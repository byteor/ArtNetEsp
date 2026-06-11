# ArtNetEsp — Architectural Review & Refactoring Plan

Senior-level review of the firmware as of v2026.1.2 (branch `big-refactor`), and a phased plan to get to a clean, portable, modern-C++ codebase without breaking devices in the field.

Guiding constraints honored throughout:

- **Field compatibility**: JSON config schema (type strings `BINARY`/`DIMMER`/`SERVO`/`REPEATER`), REST routes, and OTA must keep working for already-deployed devices until a deliberate major-version break.
- **Every phase ends green**: each phase builds all 11 environments and is independently flashable/testable. No "big bang" rewrite.
- **The ESP32 core-1 DMX sender pattern is load-bearing** (copy under mutex → release → blocking TX). It gets improved, never casually rewritten.

---

## Part 1 — Findings

### 1.1 Correctness bugs and undefined behavior (fix first, on the current structure)

These are concrete defects found by reading the code, not style opinions. Several explain known symptoms from the README TODO list.

| # | Issue | Where | Consequence |
|---|-------|-------|-------------|
| B1 | **Dangling reference capture in the Art-Net dispatch lambda.** `[&]` captures `init()`'s `universe` *parameter* by reference; the lambda runs long after `init()` returned. | `src/artnetHandler.h:44` | UB on every received frame. Harmless today only because all `frame(univ, …)` overloads ignore the universe — which is also *why* "Repeater ignores universe" can't be fixed without fixing this. |
| B2 | **Same bug in the API:** the `/reset-wifi` handler captures the `connect` *pointer parameter* of `setupApi()` by reference and dereferences it after the function returned. | `src/api.h:68-74` | UB; likely related to "BUG: Reboot doesn't always work correctly on Sonoff Basic" (README TODO). |
| B3 | **Out-of-bounds read `data[-1]`.** Repeater registers `channel = 0`, 512 channels; the dispatch loop reads `data[i - 1]` starting at `i = 0`. | `src/artnetHandler.h:54-58`, `src/device/repeater.h:32` | OOB read each frame, plus 512 pointless virtual `get()`/`set()` calls per frame per repeater (data already delivered via `frame()`). |
| B4 | **DMX channel truncated to `uint8_t`.** Config stores `uint16_t channel` (1–512), but `Device::channel`, all device constructors, and `set()/get()` use `uint8_t`. | `src/config.h:56` vs `src/device/device.h:10,17` | Any device configured on channel ≥ 256 silently wraps to the wrong channel. |
| B5 | **Repeater drops DMX channel 512 (off-by-one).** Buffer is `data[512]` where slot 0 is the start code, and `write()` rejects `channel >= 512`; ESP8266 side inits ESPDMX with 511. The correct packet size is 513 (start code + 512 slots). | `src/dmx/dmx.h:17`, `src/dmx/dmx32.cpp:75`, `src/dmx/dmx8266.cpp:9` | A full 512-channel universe loses its last channel on both platforms. |
| B6 | **Uninitialized members read before write.** `Device::lastChange` is never initialized (no ctor sets it); `Strobe::value`, `adjustedActiveValue`, `adjustedInactiveValue` likewise; `Config::universe` is garbage if JSON lacks the key. | `src/device/device.h:11`, `src/device/strobe.h:26-31`, `src/config.h:129` | UB; spurious "DMX timeout" blackout right after boot; `flip()` before any DMX frame writes a garbage PWM value to the pin. |
| B7 | **Null-pointer dereference on button press.** `dmx_devices[0]->isEnabled()` runs whenever `config.dmx.size() > 0`, but slot 0 is `NULL` if the first entry's type is `DISABLED`/unknown — or any non-`BINARY` type on Sonoff builds. | `src/main.cpp:80-90` | Crash/reboot on button press for valid-but-unsupported configs. |
| B8 | **Device count not clamped against `MAX_DMX_DEVICES`.** `configFromJson()` adds *every* JSON entry; `artnet.init(…, config.dmx.size())` then iterates `devices[k]` past the end of `dmx_devices[MAX_DMX_DEVICES]`. | `src/config.cpp:136-155`, `src/main.cpp:188`, `src/artnetHandler.h:48` | A config with more entries than the platform max reads past the array — garbage `Device*`, crash. |
| B9 | **`Config::save()` overwrites `default.json`.** If `/config/config.json` doesn't exist at boot, `_fileName` stays `/config/default.json` and the first `POST /config` writes there. `config.json` is never created; factory defaults are destroyed. | `src/config.cpp:5-17,40-47` | "Reset to defaults" semantics silently broken on every fresh device. |
| B10 | **Missing vtable anchor / not-quite-pure virtual.** `Device::getNumberOfChannels()` is declared, never defined, and not `= 0`. The build links only because the optimizer elides the unused `Device` vtable. | `src/device/device.h:39` | Any `-O0`/debug build fails to link; fragile by construction. Should be pure. Also: **no virtual destructor** on `Device`. |
| B11 | **Cross-context use-after-free window.** On ESP32, AsyncWebServer handlers run in the `async_tcp` task. `POST /config` → `cleanupDmx()` deletes `DmxChannel` objects while the OLED `Ticker` callback and `loop()` read `config->dmx[i]` concurrently. | `src/api.h:38-56`, `src/config.cpp:89-96`, `src/hw/oledDisplay.h:140,178` | Race; worst case crash during reconfiguration on OLED boards. |
| B12 | **Blocking captive portal inside `loop()`.** On WiFi loss, `Connect::loop()` calls `connect()` → `wifiManager->autoConnect()`, which blocks (possibly forever, in portal mode). The portal also `reset()`s the shared server, so REST/OTA handlers are gone after a runtime reconnect cycle. | `src/connect.cpp:20-33,46-57` | Mid-show WiFi blip freezes every dimmer/relay (`handle()` stops running); web API lost until reboot. |
| B13 | **~20 ms of I2C inside a `Ticker` callback.** `display->display()` flushes the whole framebuffer over I2C from timer context (SYS context on ESP8266) every second. The commented-out `attach_scheduled` shows this was already fought once. | `src/hw/oledDisplay.h:114-115` | WiFi jitter/WDT risk on ESP8266; blocks the shared `esp_timer` task on ESP32. |
| B14 | **Per-channel mutex churn + frame tearing in the repeater path.** `DmxRepeater::frame()` calls `dmx.write()` 512 times; each take/give of `dataMutex` (≈20k lock ops/s at 40 fps). The sender task can transmit a frame that is half-old/half-new since the 512 writes aren't atomic as a set. | `src/device/repeater.h:39-46`, `src/dmx/dmx32.cpp:72-83` | Wasted CPU on core 0 and no frame coherence on the DMX line. Needs a bulk `writeFrame()` under one lock. |
| B15 | **`DmxProxy` is a per-device value member wrapping a singleton hardware resource.** Two `REPEATER` entries → `dmx_driver_install()` twice + two core-1 sender tasks on one UART. | `src/device/repeater.h:16`, `src/dmx/dmx32.cpp:37-70` | UART contention/undefined output. Hardware ports must be owned once. |
| B16 | **Blocking serial logging in the DMX hot path.** Every `set()` on relay/dimmer/servo does Arduino-`String` concatenation + `Serial.println` (≈2–3 ms per line at 115200) — per channel change, at up to 44 fps. | `src/device/strobe.cpp:33`, `src/device/relay.h:56-57`, `src/device/dmxServo.h:112` | Visible fade stutter and frame jitter under real console traffic. |
| B17 | **Strobe speed sub-channel is unreachable.** `set()` listens on `channel - 1`, but the dispatcher only delivers `getChannel() … getChannel()+getNumberOfChannels()-1` (and `getNumberOfChannels()` returns 1). Also wraps at channel 0. This is the root cause of "Fix Stroboscope" in the TODO. | `src/device/strobe.cpp:39-44`, `src/artnetHandler.h:54` | Dead feature; confirms the dispatch design needs rework, not the Strobe code alone. |
| B18 | **`flip()`-to-full-brightness is dead code.** `valueOverride` is computed on flip but `update()` never reads it. | `src/device/strobe.cpp:143-158` vs `47-58` | "Manual flip should bring the full light on" doesn't. |
| B19 | **Inverted `isEnabled()` for active-LOW relays.** Returns the raw pin `value`, so an active-LOW relay that is ON reports disabled (status LED shows the opposite state). | `src/device/relay.h:76-79` | Wrong LED feedback on active-low hardware. |
| B20 | **Silence blackout fights the button.** `Device::handle()` re-blacks-out every 5 s of DMX silence (and `frame()` resets the timer so it repeats forever, logging each time). A relay flipped ON by button turns itself OFF ≤5 s later if no console is running. | `src/device/device.h:20-28` | Surprising behavior in exactly the standalone-test scenario the button exists for; README TODO "make blackout optional" is the fix vehicle. |
| B21 | **`#include "version.h"` vs actual `include/Version.h`.** Builds only on case-insensitive filesystems (macOS). `board.h` spells it correctly. | `src/config.h:18`, `src/main.cpp:27` | Linux/CI builds fail. Must be fixed before CI exists. |
| B22 | **Zombie boot on FS-mount failure.** `setup()` `return`s early; `loop()` then runs `button.check()`/`artnet.loop()` against half-initialized state. | `src/main.cpp:104-113` | Undefined behavior instead of a deliberate safe mode. |
| B23 | **`default.json` carries board-specific pins.** `ledPin: 2`, `buttonPin: 0` in the shared `data/` image override the per-board `-D LED_PIN/DEFAULT_BUTTON_PIN` defaults at runtime on every board that flashes `uploadfs`. | `data/config/default.json`, `src/config.cpp:162-171` | e.g. `esp32-devkitc-v4` (LED 14, button 27) gets LED 2 / button 0 until manually reconfigured. Defaults belong to the board layer, not the shared data image. |
| B24 | **`StatusLed` drives PWM with out-of-range magic (1000 on an 8-bit range) and a baked-in active-LOW assumption.** Works today only via clamping behavior of two different `analogWrite` implementations. | `src/hw/statusLed.h:19,62-69` | Wrong/undefined brightness on active-HIGH wiring; semantics by accident. |
| B25 | **PlatformIO's LDF "leaks" `esp_dmx` into ESP8266 builds.** LDF's default `chain` mode textually scans `#include` lines without evaluating `#ifdef`s, so `dmx32.cpp`'s `#ifdef ESP32`-guarded body (→ `dmx.h` → `#include <esp_dmx.h>`) still registers `esp_dmx` as a dependency on every ESP8266 env — even `sonoff_basic`/`sonoff_s31`, where `SONOFF_BASIC` excludes `dmx.h` from `main.cpp` entirely. `esp_dmx` needs ESP-IDF headers (`driver/uart.h`, `freertos/*`) absent from the ESP8266 Arduino core, so LDF then fails to build it. | `platformio.ini` (all 6 ESP8266 envs), `src/dmx/dmx32.cpp:1`, `src/dmx/dmx.h:7-13` | `pio run` failed for every ESP8266 env, not just Sonoff. Fixed in Phase 0 via `lib_ignore = esp_dmx` on each ESP8266 env — should be hoisted into `[common_esp8266]` once Phase 0 item 3 lands. |
| B26 | **`lolin_s2_mini` fails to build out of the box.** `mathieucarbou/ESPAsyncWebServer@^3.4.5`'s `AsyncWebServer::state() const` calls non-`const` `AsyncServer::status()` (from the transitive `AsyncTCP@3.3.2`) — a `-fpermissive`-eligible "passing 'const AsyncServer' as 'this' argument discards qualifiers" hard error across 8 translation units (`AsyncJson`, `AsyncWebServerRequest`, `AsyncWebHeader`, `WebHandlers`, `AsyncEventSource`, `AsyncWebSocket`, `Middleware`, `WebRequest`). **Confirmed pre-existing**: control-tested against the pre-Phase-0 `platformio.ini` (unpinned `platform = espressif32`, no `-std=` override at all) — identical failure, so it predates and is independent of the Phase 0 `gnu++17` unification (item 4). | `platformio.ini` (`[env:lolin_s2_mini]`), `.pio/libdeps/lolin_s2_mini/ESPAsyncWebServer/src/ESPAsyncWebServer.h:1691` | `pio run -e lolin_s2_mini` has never produced firmware on a clean checkout — not a Phase 0 regression, `pio run` is 11/12 both before and after. Real fix belongs with the §1.3.7 webserver-fork unification (deferred — needs `lolin_s2_mini` hardware to validate a fork swap or exact-version pin). |

Smaller correctness notes, fixed in passing: `config.update()` always returns `true` so the reload branch in `api.h:46-50` is dead; `WiFi.hostname()` is the ESP8266 spelling — verify what it actually does on ESP32 (`setHostname` is the native API) (`src/main.cpp:119`); `DmxRepeater::handle()`'s `lastRefreshTime += interval` catch-up can burst after a stall (`src/device/repeater.h:50-53`); `heltec_wifi_kit_8` env sets no `board_build.filesystem` — verify the produced `uploadfs` image is LittleFS like the code expects.

### 1.2 Structural anti-patterns

1. **Includes inside namespaces.** `#include <LinkedList.h>` inside `namespace art` (`src/config.h:31-33`) and `#include "hw/board.h"` inside `namespace art` (`src/config.cpp:2-4`). This only compiles because include guards happen to have fired at global scope first — a change in include order would wrap third-party/Arduino headers in `art::`. Remove; never include inside a namespace.
2. **Header-only modules with non-inline, out-of-class definitions and missing guards.** `api.h`, `hw/logger.h`, `hw/board.h`, `hw/oledDisplay.h` have **no include guard at all**; `relay.h`, `dmxServo.h`, `repeater.h`, `artnetHandler.h`, `oledDisplay.h`, `api.h` define functions/globals (`Ticker apiTicker`, `getAngleValue`, `StatusDisplay::setStatus64`, …) that violate ODR the moment a second translation unit includes them. The codebase survives on the convention "only `main.cpp` includes these."
3. **Global-variable coupling across modules.** `connect.cpp` drives `extern StatusLed *status` declared in `connect.h:16` and owned by `main.cpp`; `dmx32.cpp:6-9` has file-scope mutable `int transmitPin…` globals with collision-prone names. Dependencies should be injected, not reached for.
4. **Raw owning pointers, no RAII, no virtual destructor.** `new` everywhere, never `delete` (acceptable for boot-once objects, but then make them values/`unique_ptr` and say so); `LinkedList<DmxChannel*>` owns by convention; `Device` deleted through base would be UB.
5. **Three names for every concept.** Enum `Dimmable` / class `Strobe` / JSON `"DIMMER"` / README "DIMMER"; enum `Binary` / class `DmxRelay` / JSON `"BINARY"` / README heading "RELAY"; `DmxChannel` is actually a *device config*, not a channel; `DmxProxy` is a *DMX output port*. Naming is the cheapest readability fix available here.
6. **Two parallel data paths into devices** — per-channel `set()` with a `get()`-based change-detect hack, *plus* bulk `frame(universe, data, size)` — and every device receives both on every frame. The change-detect (`data[i-1] != get(i)`) only works because `get()` ignores its argument for 1-channel devices; it can never work for multi-channel devices (which is what blocks NeoPixel, the multi-channel dimmer, and the strobe sub-channel).
7. **Inconsistent device lifecycle.** Relay does `pinMode` in `start()`; Strobe in its constructor; Repeater spawns a FreeRTOS task from its constructor (during the config switch in `setup()`); Servo attaches in `start()`. And `start()` is called by… `ArtnetHandler::init()` (`src/artnetHandler.h:32-34`) — the network class initializes output hardware.
8. **Dead code and drift.** Unused `getRotationalValue()` (`dmxServo.h:65-91`), empty `#ifdef ESP32` block (`dmxServo.h:38-40`), unused `valueOverride`, unused `TICK_DURATION`, commented-out experiments (`main.cpp:107,139-140`, `oledDisplay.h:114`). Removed in Phase 0: dead `-DBUTTON_PIN=0` flag (nothing read `BUTTON_PIN`; code uses `DEFAULT_BUTTON_PIN`), unused `durydevelop/OLED` lib in the heltec env, and `me-no-dev/ESPAsyncUDP` (confirmed unused — only a comment/URL reference inside `ESPAsyncWiFiManager.h`, the ArtNet lib uses `WiFiUDP`).
9. **Layering violation in config.** `configToJson()` reads `WiFi.SSID()/RSSI()` (`src/config.cpp:192-193`) — the persistence model serializes live network state. Info/status belongs in the API layer.
10. **Magic numbers & macro constants.** `CONFIG_BUFFER_SIZE`, `DMX_SILENCE_TIMEOUT`, `40` ms in two places (`repeater.h:10` and `dmx32.cpp` field — drift risk), `0.5f` ticks, `1000`/`200` PWM, `5`/`10`/`127` defaults inline in the JSON parser.
11. **Mixed formatting and logging.** 2-space (`main.cpp`) vs 4-space (everything else); `LOG(String + String)` vs raw `Serial.print` chains (`relay.h:37-42`); no `.clang-format`.

### 1.3 Portability & conditional-compilation problems

1. **`#ifdef` sprawl.** 27 preprocessor conditionals in `main.cpp` alone. The ESP8266-vs-ESP32 WiFi-include dance is copy-pasted in four files; `ESP_FS` is defined independently in `main.cpp` and `config.h`. There is no platform layer — every module knows about every chip.
2. **`SONOFF_BASIC` is a board name used as a feature system.** It excludes whole modules (`dmx/`, strobe, servo, repeater) and is tested in 6+ places. Adding "a board without servo but with DMX" today means inventing another `SONOFF_X`-style macro. Needs granular capability flags (`FEATURE_DMX_PORT`, `FEATURE_SERVO`, `FEATURE_PWM_DIMMER`, `FEATURE_OLED`, `FEATURE_OTA`) derived in one place from a board profile.
3. **Pin configuration split across two weakly-coupled mechanisms.** `board.h` `#ifndef` defaults vs scattered `-D` in `platformio.ini`; `lolin_s2_mini` silently inherits defaults (documented as Gotcha #3 in AGENTS.md — it should not need documenting; it should be impossible).
4. **Divergent C++ standards.** espressif8266 (core 3.x) compiles gnu++17; espressif32 (core 2.x) compiles gnu++11. The same headers compile under two different language versions depending on target.
5. **Platform-specific PWM is broken on ESP32.** The dead third-party `erropix/ESP32 AnalogWrite@0.2` (2020) is pulled in for `analogWrite`, the configured `pwmFreq` is silently ignored on ESP32 (`main.cpp:138-144` TODO), and this dependency is what blocks moving to Arduino core 3.x (which has native `analogWrite`/LEDC). A 20-line `Pwm` wrapper over LEDC removes the lib, honors `pwmFreq`, and unblocks the platform upgrade.
6. **Filesystems differ per platform** (LittleFS on 8266, SPIFFS on ESP32) for no architectural reason. SPIFFS is deprecated in arduino-esp32. Unify on LittleFS behind the platform layer (with an explicit migration decision — see Risks).
7. **Dependency chaos across the 11 envs** (see `platformio.ini`): three different AsyncWebServer sources — `ottowinter/ESPAsyncWebServer-esphome` (8266 envs), `mathieucarbou/ESPAsyncWebServer` (s2_mini only), and *unpinned transitive* (all other ESP32 envs); `ESP32Servo ^0.11.0` vs `^3.0.6` (major-version skew between lolin32/devkitc and XIAO!); `Adafruit SSD1306 ^2.4.3` vs `^2.5.10`; `BusIO ^1.7.2` vs `^1.16.0`. Platforms themselves are unpinned (`platform = espressif32` floats). No `[common]` section — every env repeats `lib_deps`, `extra_scripts`, `monitor_speed`. *Phase 0 update*: `[common]`/`[common_esp8266]`/`[common_esp32]` introduced, platforms pinned, SSD1306/BusIO aligned; the `ESP32Servo` and AsyncWebServer-fork unifications remain — `lolin_s2_mini`'s fork is independently broken regardless (B26), so that fork swap and the `lolin_s2_mini` fix are now one combined hardware-gated task.
8. **`board.h` unconditionally includes `Wire.h`/`SPI.h`** for all non-Sonoff builds, dragging I2C/SPI into boards with no display.
9. **Servo libs differ per platform** (`ESP32Servo` vs `Servo`) — fine, but the `#ifdef` lives in the device class instead of a platform header.

### 1.4 Modern C++ gaps (C++17 is available on every target once §1.3.4 is fixed)

- `enum class DmxType` (currently a plain enum that implicitly converts; `LOG(config.dmx[i]->type)` prints raw ints).
- Default member initializers everywhere a member exists (kills the entire B6 class of bugs); constructor init-lists instead of assignment bodies.
- `std::array<std::unique_ptr<Device>, MAX_DMX_DEVICES>` (or fixed-capacity vector) instead of raw `Device*[]`; factory function returning `std::unique_ptr<Device>`.
- `std::vector<DeviceConfig>` *by value* instead of `LinkedList<DmxChannel*>` (removes a dependency, the O(n) `operator[]`, and manual `delete` loops).
- `override`/`final` on every override (none today — `Strobe::handle()` is the only `override` in the codebase); `virtual ~Device() = default`; `= 0` for the interface.
- `constexpr`/`inline constexpr` constants instead of `#define`; `static constexpr char[]` (or PROGMEM) instead of eight `const String` members per `Config` instance.
- `uint16_t` channel types end-to-end; `nullptr` over `NULL`; `snprintf` over `sprintf`.
- Explicit lambda captures (`[this]`, by-value for locals) — outlaw `[&]` on long-lived callbacks (root cause of B1/B2).
- A tiny RAII lock guard for the FreeRTOS mutex in `dmx32.cpp`.
- Keep it embedded-sane: no exceptions, no RTTI-dependent code, no `std::function` in hot paths, no dynamic allocation after `setup()` (already true in spirit — make it a stated rule).

---

## Part 2 — Target architecture

```
src/
├── main.cpp                 # ~20 lines: construct App, app.setup(), app.loop()
├── app/
│   ├── app.h/.cpp           # owns all subsystems as values/unique_ptr; explicit wiring & boot order
│   ├── deviceFactory.h/.cpp # DmxType -> unique_ptr<Device> (replaces the switch in main)
│   └── safeMode.h/.cpp      # FS-failure / panic boot path (LED pattern + diagnostic AP)
├── core/                    # platform-free, natively unit-testable (no Arduino.h)
│   ├── configModel.h        # DeviceConfig, HardwareConfig, NetworkConfig — value types, defaulted members
│   ├── configCodec.h/.cpp   # ArduinoJson <-> model (ArduinoJson runs on native; testable)
│   ├── dmxTypes.h           # enum class DmxType + to/from string; channel math; DMX constants (513!)
│   └── dimmerLogic.h        # pure strobe/dimmer timing state machine (testable)
├── devices/
│   ├── device.h             # pure interface: uint16 channels, virtual dtor, single dispatch path
│   ├── relay.h/.cpp  dimmer.h/.cpp  servoDevice.h/.cpp  repeater.h/.cpp
├── net/
│   ├── artnetService.h/.cpp # receive + dispatch (slices frame per device span; universe-aware)
│   ├── webApi.h/.cpp        # REST handlers; owns "info" serialization (moves WiFi.SSID out of config)
│   └── wifiService.h/.cpp   # non-blocking reconnect state machine; portal only at boot
├── platform/                # the ONLY place allowed to #if ESP8266/ESP32
│   ├── platform.h           # chip id, restart, heap, hostname, filesystem accessor (one ESP_FS)
│   ├── pwm.h/.cpp           # LEDC on ESP32 (honors pwmFreq), analogWrite on 8266
│   ├── dmxPort.h            # DmxPort interface (renamed DmxProxy) + writeFrame(span) bulk API
│   ├── dmxPort32.cpp        # core-1 sender task (existing pattern, bulk-locked)
│   └── dmxPort8266.cpp      # ESPDMX wrapper
├── hw/
│   ├── statusLed.h          # polarity-correct, range-correct
│   └── oledDisplay.h/.cpp   # rendering only; updates driven from loop(), not Ticker
└── boards/
    ├── board.h              # selects boards/<X>.h from one BOARD_* define; final fallback defaults
    ├── lolin32.h, esp32_devkitc_v4.h, lolin_s2_mini.h, …   # ALL pins + FEATURE_* per board
    └── features.h           # capability flags; SONOFF profile = a set of features, not a code fork
```

Key design decisions:

1. **One dispatch path.** `Device` gets a single `void onDmx(uint16_t universe, const uint8_t* data, uint16_t size)` taking the slice for its declared span (`firstChannel()`, `channelCount()`); the dispatcher does bounds-checking and slicing once, correctly. Per-device change detection becomes the device's own business. This single change unblocks: strobe speed channel, multi-channel dimmer, NeoPixel, per-device universe (repeater TODO), and deletes B3/B17.
2. **The `#ifdef` firewall.** Grep-enforceable rule: `#if defined(ESP8266|ESP32)` may appear only under `src/platform/` and `boards/`. Everything else compiles identically for all targets.
3. **Capabilities, not board names, gate features.** `#ifdef FEATURE_SERVO` etc.; `sonoff_basic` becomes a board profile that turns features off. New boards are a header + an env section, nothing else.
4. **Ownership is explicit.** `App` owns subsystems by value or `unique_ptr`; `DmxPort` is owned once by `App` and handed to the repeater (fixes B15); nothing reaches for externs.
5. **Silence-blackout is policy, not hard-coded behavior**: base-class helper, per-device default, `"blackout": true|false` config flag (delivers a README TODO; fixes the B20 interaction by suspending blackout while manually flipped).
6. **Boot order stays sacred** and gets encoded in `App::setup()` with the WiFi-before-`server.begin()` constraint asserted in one commented place instead of a `!!!!!` comment.

---

## Part 3 — Phased plan

Each phase = one PR-sized unit, all 11 envs built (`pio run`), plus the on-device smoke tests listed. Phases 1–2 deliberately precede file moves so bug fixes stay reviewable as small diffs. From Phase 0.5 onward, "bench test" means the HIL harness: **T1** (`tools/verify.sh --quick`) after every significant change, **T2** (`--full`) at the end of each phase — see Phase 0.5 for the tier definitions.

### Phase 0 — Safety net (no behavior change) — S
1. Fix `version.h` → `Version.h` includes (B21) — prerequisite for any Linux CI.
2. Add include guards / `#pragma once` to `api.h`, `logger.h`, `board.h`, `oledDisplay.h`.
3. `platformio.ini`: introduce `[common]`, `[common_esp8266]`, `[common_esp32]` with shared `lib_deps`/`extra_scripts`/`monitor_speed` via `${...}` interpolation; align skewed lib versions (SSD1306 → `^2.5.10`, BusIO → `^1.16.0`, both confirmed to resolve identically to the old constraints); **pin** `platform =` versions (`espressif8266@4.2.1`, `espressif32@6.6.0`); remove dead deps (`durydevelop/OLED`, dead `-DBUTTON_PIN=0`, `ESPAsyncUDP` — audited, confirmed unused, removed). **Done** for all 12 envs (`pio run` → 11/12, see item 4 + B26 for the 12th).
   - **Deferred** (out of Phase 0's no-behavior-change mandate — unifying these blind across 4 ESP32 boards with no bench hardware risks trading "doesn't compile" for "compiles but misbehaves at runtime"): `ESP32Servo` major-version unification (`^0.11.0` vs `^3.0.6`) and the 3-way AsyncWebServer-fork split (§1.3.7). Bundle with the B26 fix as a future hardware-gated task.
4. Unify the language level: `-std=gnu++17` (+ matching `build_unflags = -std=gnu++11`) on ESP32 envs. **Done**, applied uniformly via `[common_esp32]` to all 6 ESP32 envs including `lolin_s2_mini` — no per-env exception needed, since B26 is a control-tested pre-existing failure independent of this flag.
5. Add `.clang-format` (match dominant 4-space Allman-ish style), `.editorconfig`; one pure-formatting commit. **Done** — `clang-format -i` (Apple clang-format 17, via Xcode toolchain) across all 20 files in `src/`; only 8 needed changes (12 already matched the new style), all whitespace/indentation only — `config.h`/`config.cpp`'s `namespace art { ... }` body dedented one level (`NamespaceIndentation: None`), `main.cpp`'s 2-space indent → 4-space, one stray `(){}`→`() {}` in `artnetHandler.h`. No logic changes (diffs reviewed). Verified: `d1_mini_oled` + `esp32-devkitc-v4` + `sonoff_basic` build clean, bench T1 (`tools/verify.sh --quick`) passes (`BOOT_RESULT=ok`, v2026.1.7).
6. GitHub Actions: matrix build of all 11 envs (with `submodules: recursive`), `cppcheck`/`clang-tidy` job (advisory at first). **Done** — `.github/workflows/ci.yml`: `build` job is a 12-env matrix (`fail-fast: false`, `submodules: recursive`, `~/.platformio` cached per-env keyed on `platformio.ini`'s hash); `lolin_s2_mini` runs with `continue-on-error: true` and a comment pointing at B26, so it stays visible without blocking the other 11. Separate `lint` job runs `cppcheck` over `src/` (`continue-on-error: true`, `missingInclude`/`unusedFunction` suppressed for Arduino-framework noise) — advisory only. `clang-tidy` deferred: needs a `compile_commands.json` per env (`pio run -t compiledb`), which is a heavier lift than this item warrants on its own; revisit alongside Phase 1.
   *Note: the version-increment pre-script bumps `version` on every build — CI's checkout is ephemeral and nothing commits it back, so no special handling needed (documented inline in the workflow).*
7. **Boot sentinel:** last line of `setup()` logs `BOOT OK v<version>` — the deterministic success marker the Phase 0.5 harness keys on (heuristics like "Connected :)" are too fragile). Add `monitor_filters = esp32_exception_decoder` to the ESP32 `[common_esp32]` section for interactive debugging.

**Verify:** `pio run` (all envs) before/after produce working firmware — **11/12 both before and after** (`lolin_s2_mini` is pre-existing-broken, B26; not a regression). Flashed `seeed_xiao_esp32s3` bench + `tools/verify.sh --full` passed clean (build → flash → boot → REST contract → Art-Net E2E → 4 min soak, no heap-leak trend) after the consolidation — no functional diff.

### Phase 0.5 — Hardware-in-the-loop harness (black-box; needs only the Phase 0 sentinel) — M

The bench device becomes the regression gate for every phase that follows. The harness is **deterministic CLI scripts** (exit-code driven) so any agent or human can run one command after a change and trust the answer. It characterizes today's 2026.1.2 behavior at the network/serial boundary — no firmware seams required.

**Bench device: `seeed_xiao_esp32s3`** (XIAO ESP32-S3 — the repeater board from `assets/Xiao-Repeater.jpg`; DMX TX = GPIO3, LED = GPIO21, button = GPIO0 = the XIAO's `B`/BOOT button). Key property: **native USB-CDC, no USB-UART bridge** — the serial port disappears and re-enumerates on every reset, which shapes the harness design below. All HIL commands default to this env via `tools/hil.env`.

**Bench setup (one-time onboarding):**
1. `pio device list` → the XIAO enumerates as `/dev/cu.usbmodem*`; record port + `env=seeed_xiao_esp32s3` in `tools/hil.env`.
2. `pio run -e seeed_xiao_esp32s3 -t upload && pio run -e seeed_xiao_esp32s3 -t uploadfs` (uploadfs once — default config + web UI). *Until B23 lands, `default.json` overrides the LED to pin 2, so the XIAO's built-in LED (GPIO21) won't blink — harmless: the harness keys on serial + HTTP, not the LED. The `B` button doubles as the device button (GPIO0) for manual flip/long-press tests.*
3. Join the captive portal once to store WiFi credentials. **Creds live in NVS and survive reflashes — never run `pio run -t erase` in the routine loop** (that wipes WiFi creds *and* the filesystem; it's a recovery step only, followed by re-onboarding).
4. Once an RS485 adapter is wired to GPIO3, the repeater soak becomes a real-line test exercising the core-1 sender task (R1) on the bench — the single riskiest code path gets hardware coverage.

**Deliverables (`tools/`, all stdlib + pyserial; run with PlatformIO's bundled interpreter `~/.platformio/penv/bin/python` so nothing needs installing):**

| Tool | Job |
|------|-----|
| `boot_check.py` | Resets the board (USB-CDC DTR/RTS sequence), watches serial for N s (default 30). **Pass** = `BOOT OK` sentinel + zero crash signatures + no boot-loop evidence; parses and prints `DEVICE_IP=` from the boot log for the network stages. Writes `.hil/last-boot.log`. Exit codes: 0 ok / 1 crash / 2 timeout / 3 no-port (see recovery note). Built for native USB: reopens the port with retries and counts re-enumerations across the watch window. |
| `artnet_send.py` | Minimal ArtDmx sender (UDP 6454, trivial packet): single frame, ramp, or sustained flood at a given fps. |
| `hil_smoke.py` | REST contract: `GET /heap`, `GET /config` schema check, `POST /config` round-trip on a scratch field, `POST /reboot` → re-run boot check. ArtNet end-to-end: set a configured channel via `artnet_send`, assert the device's serial echo of the `set()` within 1 s. Soak mode: flood at 44 fps × N min, assert windowed-min `/heap` is non-decreasing. |
| `decode_backtrace.py` | Feeds `Backtrace: 0x…` lines through the toolchain's `addr2line` against `.pio/build/<env>/firmware.elf` — failures come back as symbolized stacks, not hex. |
| `verify.sh [env] [--quick\|--full]` | Orchestrator; `env` defaults to the bench device from `tools/hil.env`: build → flash → `boot_check` (`--quick`, ~90 s) → plus `hil_smoke` and a 3–5 min soak (`--full`). On crash, auto-runs the decoder and tails the log. |

**Crash signatures `boot_check.py` fails on** (ESP32 now, ESP8266 set included for when an 8266 joins the bench): `Guru Meditation Error`, `abort() was called`, `assert failed`, `Backtrace:`, `Task watchdog got triggered`, `Brownout detector was triggered`, `CORRUPT HEAP`; 8266: `Exception (`, `wdt reset`, `Soft WDT reset`, `Stack smashing`.

**Boot-loop detection on native USB:** ROM `rst:0x…` banners can be missed entirely on USB-CDC because the port vanishes during reset, so the XIAO check uses firmware-level evidence instead — the early `Starting...` marker (or a second `BOOT OK`) appearing more than once in one watch window, and/or repeated port re-enumeration, fails the check.

*Triage notes:* `Brownout` usually means USB power/cable, not code — labeled separately so we don't chase ghosts. The worst native-USB failure mode is **silent**: firmware that crashes before the USB stack comes up never enumerates at all. `boot_check` reports that distinctly (exit 3, "no port"), and recovery needs a physical hand — hold `B` (BOOT), tap `R` (reset) to enter the ROM bootloader, reflash. The harness says so explicitly when it needs you; an agent can't press the buttons.

**Verification tiers (the cadence for "after every significant change"):**

| Tier | When | Command | Time |
|------|------|---------|------|
| T0 | every build-affecting edit | `pio run -e seeed_xiao_esp32s3` (compile only) | seconds |
| T1 | every significant change / each Phase-1 bug commit | `tools/verify.sh --quick` | ~90 s |
| T2 | end of each phase, before merge | `tools/verify.sh --full` + `pio run` (all 11 envs) | ~10 min |

**Operational constraints:** the serial port is an exclusive resource — flashing, monitoring, and soaking must be serialized (the orchestrator owns the port; never flash while a soak holds it). Long soaks run in a background shell; everything else is foreground because the next step depends on the result.

**Verify (of the harness itself):** run T2 against unmodified 2026.1.2 — it must pass clean; then deliberately break boot (early `abort()` on a scratch branch) — it must fail with a decoded stack. A harness that has never failed isn't trusted.

**Known harness flake:** `tools/verify.sh --quick` occasionally reports `BOOT_RESULT=crash # boot-loop` (`STARTING_COUNT=2`) immediately after `pio run -t upload`, on firmware that boots fine. Cause: the XIAO's native USB-CDC re-enumerates on every reset, and the tail of the post-upload auto-reboot's "Starting..." can land in the OS receive buffer before `boot_check.py` opens the port and issues its own DTR/RTS reset, so both boots get counted. `boot_check.py` now calls `ser.reset_input_buffer()` before resetting (mitigates but doesn't fully eliminate it - it's USB-enumeration timing, not a buffering bug per se). If `--quick` reports a boot-loop with no actual crash signature and a clean-looking log, re-run `tools/boot_check.py` (no reflash needed) or re-run `--quick`; this is a harness artifact, not a regression to chase.

### Phase 1 — Correctness fixes on the current structure — M
One commit per bug, in this order (independent unless noted):
1. B10: `virtual ~Device() = default`, `getNumberOfChannels() = 0`, `override` everywhere. **Done** — `device.h` gets the virtual destructor and a pure-virtual `getNumberOfChannels()` (`Device` is now formally abstract; confirmed no code constructs a bare `Device`). All four subclasses (`DmxRelay`, `DmxServo`, `DmxRepeater`, `Strobe`) mark every overridden method `override` (the pre-existing `Strobe::handle() override` was already correct). `Strobe::start(uint8_t)` remains a declared-but-undefined overload — pre-existing, out of scope for B10. Verified: d1_mini_oled + esp32-devkitc-v4 + sonoff_basic build clean; bench T1 passes (`BOOT_RESULT=ok`, v2026.1.10, IP/WiFi/ArtNet/Repeater all correct).
2. B6: default member initializers across `Device`, `Strobe`, `Config`.
3. B1/B2: lambda captures — `[this]` / by-value; never `[&]` on stored callbacks.
4. B4: `uint16_t` channels end-to-end (config → ctor → `set`/`get` → dispatch loop).
5. B3: dispatcher skips the per-channel loop for frame-consuming devices (`channelCount() == 512` interim test; properly typed in Phase 5) and clamps `i >= 1`.
6. B8: clamp config parsing to `MAX_DMX_DEVICES` at load; pass the clamped count everywhere.
7. B7: null-check `dmx_devices[0]` in the button handler.
8. B9: `save()` always writes `/config/config.json`; `load()` falls back to default.
9. B5 + B14 + B15: `DmxPort` buffer 513, channels 1–512 valid; add `writeFrame(const uint8_t*, uint16_t)` doing one `memcpy` under one lock (preserve copy→release→send pattern in the task); repeater uses it; make the port a single shared instance.
10. B16: logging discipline — introduce leveled `LOG_DEBUG/INFO/WARN` in `logger.h` (compile-time threshold per env); hot-path `set()` logs become `LOG_DEBUG`, default threshold INFO.
11. B12: `wifiService` reconnect state machine — runtime loss → periodic non-blocking `WiFi.reconnect()` (lights keep running); captive portal only at cold boot. *(Behavior change vs README — deliberate; document.)*
12. B13: OLED refresh from `loop()` via millis-schedule; Ticker only sets a "due" flag (or is removed).
13. B11: `POST /config` handler only parses + stages; apply/save happens on the next `loop()` iteration (single-threaded apply; removes the cross-task mutation).
14. B19, B24: relay `isEnabled()` polarity; `StatusLed` gets explicit on/off duty values within range and respects `_onValue`.
15. B18/B17 minimal: wire `valueOverride` into `update()`; leave full strobe redesign to Phase 5 (root cause is dispatch).
16. B22: FS-failure → `safeMode()` loop (LED pattern + serial message) instead of falling through.
17. B23: strip `hw` pins from `data/config/default.json` (board defaults then apply); keep `freq`.
18. B20: suspend silence-blackout while a device is manually flipped on; log timeout once, not every 5 s.

**Verify:** build matrix (at minimum the Gotcha-#12 trio: d1_mini_oled, esp32-devkitc-v4, sonoff_basic); HIL T1 on the XIAO after each bug commit, plus targeted bench checks: QLC+ fade smoothness (B16), button flip via the XIAO `B` button with console stopped (B20), WiFi-router-off-mid-fade test (B12), POST /config under ArtNet load (B11 — the OLED-race half waits for an OLED board on the bench), 512-channel frame on the repeater with channel 512 patched (B5; RS485 on XIAO GPIO3).

### Phase 2 — Naming & taxonomy (mechanical, JSON/REST unchanged) — S
- `Strobe` → `PwmDimmer` (`devices/dimmer.*`); `DmxChannel` → `DeviceConfig`; `DmxProxy` → `DmxPort`; `enum class DmxType { Disabled, Relay, Dimmer, Servo, Repeater }`; legacy JSON strings (`"BINARY"`, `"DIMMER"`, …) preserved in the codec, with new canonical aliases (`"RELAY"`) accepted on input.
- Kill the third name everywhere (README "RELAY" heading vs `"BINARY"` type string gets an explicit alias note).
- Decide the namespace: everything project-owned under `artnode::` (or keep `art::` but apply it to all modules, not just config). Fix the includes-inside-namespace hazard (§1.2.1) while touching these files.

**Verify:** build matrix; `GET /config` byte-compatible with Phase 1 output; old config.json files load.

### Phase 3 — Platform layer — M
- Create `src/platform/`; move every `#if ESP8266/ESP32` from `main.cpp`, `config.*`, `connect.*`, `oledDisplay.h`, device headers into it. Single `ESP_FS`/filesystem accessor; hostname set via one platform call (resolves the `WiFi.hostname` question on ESP32).
- `Pwm` class: LEDC on ESP32 (channel allocation + `pwmFreq` honored — fixes the silent TODO at `main.cpp:139`), `analogWrite/analogWriteFreq` on 8266. Drop `erropix/ESP32 AnalogWrite`.
- Unify LittleFS on ESP32 (`board_build.filesystem = littlefs` + code), **gated by the migration decision in Risks (R2)**.
- Servo include/`#ifdef` moves behind `platform/servo.h`.

**Verify:** build matrix; dimmer PWM frequency measured on a scope/LED flicker on both platforms; FS read/write + OTA on ESP32 after the LittleFS switch.

### Phase 4 — Board layer — S/M
- `boards/<env>.h` per environment carrying *all* pins and `FEATURE_*` flags; `platformio.ini` passes exactly one `-D BOARD_<NAME>`; `boards/board.h` dispatches. `lolin_s2_mini` gets an explicit header (kills AGENTS Gotcha #3).
- `features.h`: `SONOFF_BASIC` becomes `BOARD_SONOFF_*` profiles that set `FEATURE_*=0`; all `#ifndef SONOFF_BASIC` in code becomes `#if FEATURE_DMX_PORT`, `#if FEATURE_SERVO`, etc. `lib_ignore` stays per-env.
- `Wire/SPI` includes move under `FEATURE_OLED`.

**Verify:** build matrix **including both sonoff envs**; `pio run -e sonoff_basic` flash-size unchanged or smaller; relay smoke test on a Sonoff.

### Phase 5 — Core restructure — L
- `App` class absorbs `main.cpp` globals and boot order; `deviceFactory` replaces the switch; `std::array<std::unique_ptr<Device>, MAX_DMX_DEVICES>`; `std::vector<DeviceConfig>` values in config (drop `LinkedList`).
- **Device interface v2** (single dispatch path, §Part 2.1): `onDmx(universe, slice)`, `firstChannel()`, `channelCount()`, `tick()` (renamed `handle`), `setEnabled/flip`, blackout policy hook. Dispatcher in `artnetService` does slicing/universe routing — repeater universe support lands here (README TODO).
- Strobe/dimmer timing extracted to `core/dimmerLogic.h` as a pure state machine; `PwmDimmer` becomes a thin shell (fixes the stroboscope properly, with tests).
- Devices no longer started by the network class; `App` calls `start()` after construction, before `artnet.init()`.
- **Extraction rule (binding):** nothing moves into `core/` without native tests arriving in the same commit — pin the behavior at the new seam first, then refactor against it. This is how unit coverage grows: with the restructure, not after it.

**Verify:** build matrix; full regression of all four device types on bench (per AGENTS Gotcha #11/12 — multi-device config 3×DIMMER on 8266 + repeater-with-relay on ESP32); native tests for dimmer logic + dispatch slicing.

### Phase 6 — Config & robustness — M
- ArduinoJson 7 (elastic `JsonDocument` kills the fixed-1024 gotcha) **or** stay on 6 with measured capacity + `doc.overflowed()` checks — decide by flash budget on the 1M Sonoff; add config-version field for future migrations.
- API: `info` block (version/ssid/rssi/heap/uptime) built in `webApi`, not in the persistence codec; `_needReboot` semantics kept; optional `GET /status`.
- Optional basic-auth for `/update` and mutating routes (theater LAN threat model = low; make it a config flag, default off).
- Safe-mode page served from the diagnostic AP.

**Verify:** build matrix; config round-trip native tests (save→load→deep-equal, legacy strings, oversized input → clean rejection not truncation); 1M-flash budget check.

### Phase 7 — Dependency refresh (one dep per commit) — M
- One AsyncWebServer for all envs: `ESP32Async/ESPAsyncWebServer` (the maintained successor org), pinned.
- `hideakitai/ArtNet` 0.4.4 → current (subscription API changed; isolated in `artnetService`).
- espressif32 platform → pioarduino / Arduino core 3.x (now unblocked: no `erropix` analogWrite, LEDC wrapper adapts); re-pin `esp_dmx`, `ESP32Servo` for core 3.
- Confirm removal of vestigial `ESPAsyncUDP`; ESPAsyncWiFiManager: evaluate replacement by a ~150-line in-house portal in Phase 8 (it's the staleest dependency and owns the server lifecycle problem from B12) — keep for now.

**Verify:** build matrix after each bump; OTA from previous firmware to this build (the upgrade path users will take); portal, REST, ArtNet discovery in QLC+.

### Phase 8 — Tests, CI hardening, stretch — M
- `pio test -e native` completes the coverage begun by Phase 5's extraction rule (config codec, dmx channel math, dimmer state machine, dispatch slicing); CI promotes native tests + build matrix + format check to required gates.
- Hardware smoke checklist in `docs/TESTING.md` (manual, honest about what CI can't cover).
- Versioning rethink (optional): replace per-build increment with git-describe injected via build flag — eliminates perpetual `version`-file churn in commits; keep the submodule if the workflow is preferred.
- Backlog features now cheap on the new architecture: NeoPixel device (factory + slice dispatch proof), single-channel dimmer type, mDNS, README/AGENTS.md rewrite to match reality (most AGENTS "gotchas" should be deleted because the trap no longer exists).

---

## Part 4 — Risk register

| # | Risk | Mitigation |
|---|------|------------|
| R1 | **Core-1 DMX task regressions** (Phase 1.9, 7). | Never hold `dataMutex` across `dmx_*` calls (pattern preserved verbatim); bench-test with RS485 fixture + 512-ch frames before/after; keep 40 ms cadence constant in exactly one place. |
| R2 | **SPIFFS→LittleFS on ESP32 wipes stored config** on already-deployed devices (Phase 3). | Decision needed: (a) accept (DIY fleet, config is 5 lines — re-enter via portal/REST), (b) one-release migration shim (mount SPIFFS read-only, copy config.json, format), or (c) keep SPIFFS behind a board flag for legacy units. Recommend (b) for one release, then delete the shim. |
| R3 | **Behavior change: no captive portal on runtime WiFi loss** (Phase 1.11). | Deliberate (a blocked `loop()` mid-show is worse); README updated; long-press still forces portal. |
| R4 | **ArtNet/webserver lib bumps change APIs** (Phase 7). | All isolated behind `artnetService`/`webApi` after Phase 5; one dep per commit with matrix build. |
| R5 | **Fielded-device compatibility** (config schema, REST, OTA). | Compatibility contract: JSON keys/strings and routes frozen through Phase 8; new fields additive with defaults; OTA upgrade tested from 2026.1.2 image at Phases 1, 5, 7. |
| R6 | **1M-flash Sonoff budget** as C++17/new libs land. | CI records per-env flash/RAM deltas; Sonoff envs already exclude OTA + heavy modules; ArduinoJson 7 decision (Phase 6) made against this budget. |
| R7 | **No unit-level regression safety until Phase 5.** | Mitigated up front: Phase 0 CI matrix catches cross-env compile breakage (this codebase's #1 regression class) and the Phase 0.5 HIL harness gates every significant change on a real device (T1) and every phase on the full suite (T2). Phase 1 stays one-commit-per-bug. |

## Part 5 — Compatibility contract (frozen until a declared 2.x break)

- JSON config: existing keys and type strings parse forever; new keys optional with defaults; `dmx[]` stays index-based (README-documented behavior).
- REST: `GET/POST /config`, `POST /reboot`, `POST /reset-wifi`, `GET /heap`, `/update` (OTA) unchanged.
- Filesystem paths: `/config/config.json`, `/config/default.json`, `/www/`.
- Serial: 115200, human-readable boot log.

## Appendix — Quick wins (could land today, independent of phases)

`#pragma once` on the four unguarded headers · `Version.h` casing · delete `getRotationalValue()`/empty `#ifdef`/commented-out blocks · null-check in `handleEvent` · default member initializers · `[common]` in platformio.ini · `.clang-format`.
