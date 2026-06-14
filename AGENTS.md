# AGENTS.md

ArtNetEsp is an ESP8266/ESP32 Arduino firmware project (built with PlatformIO) that turns a cheap dev board into an Art-Net node — receiving DMX-over-WiFi from a lighting console (typically [QLC+](https://www.qlcplus.org/)) and driving relays, PWM dimmers, servos, or a real DMX512 line via an RS485 "repeater". It was built for community theater: cheap, wireless, no commercial lighting console required. Current version: 2026.1.x (auto-bumped every build — Gotcha #1).

The `big-refactor` branch (see `REFACTORING_PLAN.md`) restructured most of `src/` without changing any externally-visible behavior (REST schema, FS paths, board pin assignments) — `src/app/`, `src/core/`, `src/net/`, and `src/platform/` are all new since that refactor, and `main.cpp` is now a 14-line shell. The sections below describe the post-refactor layout.

For user-facing docs — device types, REST API, WiFi/captive portal behavior, OTA updates, version history, and the project TODO/backlog — see [README.md](README.md). This file is for anyone (human or AI) working on the firmware itself.

## 1. Build & Flash Quick Reference

```bash
git submodule update --init --recursive   # required once - pulls in platformio_version_increment

pio run -e <env>                          # build (see environment table below for <env> values)
pio run -e <env> -t upload                # flash firmware over USB
pio run -e <env> -t uploadfs              # flash data/ (LittleFS) - config + web UI, minified first (Gotcha #13)
pio run -e <env> -t monitor               # serial monitor @ 115200

pio run                                   # build ALL 12 environments - good sanity check
                                           # before committing a cross-cutting change
```

```bash
pio test -e native                        # run the platform-free native test suite (test/test_*)
```

`[env:native]` (`platformio.ini`) runs `test/test_*` (Unity, ArduinoJson, plain C++ - no Arduino headers) against `src/core/` and is also the `native-test` job in `.github/workflows/ci.yml`. There's still no on-device test suite and no hardware-in-the-loop CI — `pio run` (the 12-env firmware matrix) plus watching the serial monitor, and the `tools/verify.sh`/`tools/hil_smoke.py` bench harness (Phase 0.5, see `REFACTORING_PLAN.md`) for live HIL checks, are the only further verification. See Gotcha #12 for which environments to build for a given change.

## 2. Critical Gotchas (read this first)

1. **Never hand-edit `include/Version.h`, `version`, `.vscode/c_cpp_properties.json`, or `.vscode/launch.json`.** All four are auto-generated — `Version.h` and `version` by the `platformio_version_increment` pre/post build scripts, the two `.vscode/*.json` files by the PlatformIO IDE extension (their headers literally say "AUTO-GENERATED FILE! PLEASE DO NOT MODIFY"). Any manual edit gets silently overwritten on the next build/IDE refresh.

2. **The build depends on a git submodule.** `platformio_version_increment/` is referenced by `extra_scripts` in every environment. If it's empty (fresh clone without `--recurse-submodules`), `pio run` fails with an error that doesn't obviously point at "missing submodule" — run `git submodule update --init --recursive` first.

3. **Each environment's hardware identity is a single `-D BOARD_<NAME>` flag, dispatched by `src/boards/board.h`.** `platformio.ini` passes exactly one `BOARD_<NAME>` define per environment (e.g. `-DBOARD_LOLIN_S2_MINI`); `src/boards/board.h` `#include`s the matching `src/boards/<name>.h`, a small self-contained header defining that board's `LED_PIN`, `DEFAULT_BUTTON_PIN`, and — for the 6 ESP32 envs — `DMX_TX_PIN`/`DMX_RX_PIN`/`DMX_ENABLE_PIN`/`DMX_PORT` (plus `SCREEN_WIDTH`/`SCREEN_HEIGHT`/`SCREEN_ADDRESS`/`OLED_RESET` for the 3 OLED boards). There's no more `#ifndef`-based fallback: `lolin_s2_mini.h` now spells out its DMX pins explicitly (17/16/21/1) instead of silently inheriting them from a shared default. To add a new board: create `src/boards/<name>.h`, add an `#elif defined(BOARD_<NAME>) #include "boards/<name>.h"` branch to `src/boards/board.h`, and give the new environment exactly one `-D BOARD_<NAME>` in `platformio.ini`.

   **Caveat — flags read by third-party library code must stay global `-D` flags.** A board header is only visible to files that `#include "boards/board.h"`, but a library's own `.cpp` (a separate translation unit) only sees `platformio.ini`'s `build_flags`. `ELEGANTOTA_USE_ASYNC_WEBSERVER=1` is the flag that bites: it must remain a `-D` in `platformio.ini` (ElegantOTA's own source reads it to pick its `begin(AsyncWebServer*, ...)` overload), not a `#define` in a board header — moving it produced a link error (`undefined reference to ElegantOTAClass::begin(AsyncWebServer*, const char*, const char*)`) because the library compiled without the macro while `main.cpp` compiled with it. `DISABLE_OTA` (sonoff_basic/sonoff_s31) is fine as a header-only `#define` because `main.cpp` includes `boards/board.h` *before* `<ElegantOTA.h>` and ElegantOTA isn't in those envs' `lib_deps` at all.

4. **`src/boards/features.h` derives four always-defined `FEATURE_*` flags from `BOARD_<NAME>`, gating whole modules.** `FEATURE_DMX_PORT`, `FEATURE_SERVO`, and `FEATURE_DIMMER` are `0` on `BOARD_SONOFF_BASIC`/`BOARD_SONOFF_S31` (the `sonoff_basic`/`sonoff_s31` environments) and `1` everywhere else; `FEATURE_OLED` is `1` on `BOARD_D1_MINI_OLED`/`BOARD_HELTEC_WIFI_KIT_8`/`BOARD_SEEED_XIAO_ESP32S3_OLED` and `0` everywhere else. Because these are value macros (always defined), code tests them with `#if FEATURE_X`, never `#ifdef`/`#ifndef`.
   - `src/dmx/dmx.h` and `src/dmx/dmx8266.cpp` — and therefore all of `src/dmx/` — are wrapped in `#if FEATURE_DMX_PORT`.
   - `device/repeater.h` is wrapped in `#if FEATURE_DMX_PORT`; `device/dmxServo.h` in `#if FEATURE_SERVO`; `device/dimmer.h`/`.cpp` (`PwmDimmer`) self-guard with `#if FEATURE_DIMMER`.
   - `src/hw/oledDisplay.h` (`StatusDisplay`) is wrapped in `#if FEATURE_OLED`, as is `src/boards/board.h`'s `<Wire.h>`/`<SPI.h>` include block.
   - `src/app/deviceFactory.cpp` mirrors this: it includes `device/relay.h` unconditionally and `device/dmxServo.h`/`device/dimmer.h`/`device/repeater.h` under their respective `#if FEATURE_SERVO`/`FEATURE_DIMMER`/`FEATURE_DMX_PORT` guards, and each non-`Relay` `case` in `makeDevice()`'s `switch (cfg.type)` (`Servo`/`Dimmer`/`Repeater`) gets its own `#if FEATURE_*`/`#endif` pair; `Relay` and `default: return nullptr` are unguarded.
   - `lib_ignore` on `sonoff_basic`/`sonoff_s31` still drops `Wire`, `SPI`, and `Servo` as belt-and-suspenders — new code that unconditionally includes these will fail to link on Sonoff boards even though `FEATURE_OLED=0`/`FEATURE_SERVO=0` already keep the referencing headers out of those builds.

   **Any new device type or feature that needs Servo/SPI/Wire/extra flash must be wrapped in the matching `#if FEATURE_*`** (add a new flag to `features.h` if none of the existing four fit).

5. **The ESP32 core-1 DMX sender task (`src/dmx/dmx32.cpp`) is fragile and load-bearing.** It's a FreeRTOS task pinned to core 1 (priority 2), looping every `refreshIntervalMs` (40ms), double-buffered through `dataMutex`. The existing pattern copies the buffer under the mutex, **releases the mutex, then** does the blocking `dmx_write`/`dmx_send_num`/`dmx_wait_sent` calls — preserve this ordering; doing blocking I/O while holding `dataMutex` would stall `write()` callers on core 0 (the Art-Net frame handler). `write()` itself uses a 10ms mutex-timeout and **silently drops** the value if it can't get the lock — no error is logged, so "DMX channel not updating" can be this. This code only landed in v2026.1.2 (commit `9d82329`) after the repeater was previously broken on ESP32 — treat it as fragile, not a casual refactor target. `dmx8266.cpp` is a completely different, simple synchronous wrapper around `ESPDMX` — don't assume the two `DmxProxy` implementations behave the same beyond their shared interface.

6. **`MAX_DMX_DEVICES` differs by platform** (8 on ESP32, 4 on ESP8266 — `config.h`, via `#ifdef ESP32`). The fixed-size `dmx_devices[MAX_DMX_DEVICES]` array in `main.cpp` and all the `for (... && i < MAX_DMX_DEVICES; ...)` loop bounds must stay consistent with this if it's ever changed.

7. **`config.load()`/`save()`/`serialize()`/`applyPendingUpdate()` use an elastic ArduinoJson 7 `JsonDocument`** (heap-allocated, no fixed capacity) — `config.cpp`, since Phase 6 item 1's ArduinoJson 6→7 upgrade. There's no more `doc.overflowed()` (AJ7 doesn't have it); a malformed/huge `POST /config` payload now fails via `DeserializationError` from `deserializeJson()`. The one surviving fixed-size buffer is `_pendingJson[CONFIG_BUFFER_SIZE]` (still 1024 bytes, `config.h`) — B11's staged-update copy of a raw `POST /config` body; `stageUpdate()` rejects (`written < sizeof(_pendingJson)` fails) anything that doesn't fit, so a single `POST /config` payload is still capped at ~1024 bytes even though the in-memory `JsonDocument` itself can grow. Adding new config fields no longer risks silent truncation of the persisted config, but very large `dmx[]`/`wifi[]` arrays can still hit this `POST /config` payload ceiling.

8. **`server.begin()` must only be called after WiFi connects.** `main.cpp` has an explicit comment: `// Call ONLY AFTER WiFi gets connected !!!!! (or reboot loop)`. Preserve this ordering if you reorganize `setup()`.

9. **`.vscode/` and `.pio/` are gitignored but exist locally** with PlatformIO-generated content (build cache, IDE IntelliSense config). Don't be alarmed they're "untracked", and don't try to add or clean them up.

10. **Default DMX-silence blackout**: `Device::handle()` (`device.h`) calls `set(channel, 0)` if no `frame()` has arrived in `DMX_SILENCE_TIMEOUT` (5000ms). This is shared by all four device types. README's TODO lists "make blackout on DMX timeout optional" — if you implement that, it's a base-class change affecting everything.

11. **Multi-device configs are lightly tested.** README itself says running multiple DMX devices "wasn't properly tested other than 3 DIMMER channels on ESP8266". Be extra careful with assumptions about concurrent device behavior, especially an ESP32 repeater (core-1 task) running alongside other device types.

12. **Verification = build the affected environments.** For anything touching `boards/`, `device/`, `dmx/`, or `app/`, build at minimum one ESP8266 env (e.g. `d1_mini_oled`), one full-featured ESP32 env (e.g. `esp32-devkitc-v4`), and `sonoff_basic` (to catch `FEATURE_*` exclusion breakage). For pin/board-specific changes, also build the specific environment(s) affected. For anything touching `core/`, run `pio test -e native` first — it's much faster than the firmware matrix and catches platform-free logic bugs in isolation.

13. **Web assets are minified into a throwaway dir at `uploadfs`/`buildfs` time — edit `data/`, never `.fs_build/`.** The files under `data/` (`www/index.html`, `www/portal.html`, `www/style.css`, `config/default.json`) are kept human-readable. `tools/minify_fs.py` — a `pre:` extra-script in `[common].extra_scripts` — runs **only** when `buildfs`/`uploadfs` is a command-line target (plain `pio run` skips it): it mirrors `data/` into the gitignored `.fs_build/` with HTML/CSS/JS/JSON minified, then `env.Replace(PROJECT_DATA_DIR=...)` points the LittleFS image builder at it. HTML goes through `minify-html` with **`minify_js=False`** (inline `onclick=` handlers call top-level functions a JS mangler would rename and break) and **`preserve_brace_template_syntax=True`** (so `portal.html`'s `{{HOST}}`/`{{VERSION}}`/`{{IP}}`/`{{NETWORKS}}` placeholders survive); CSS via `rcssmin`, JS via `rjsmin`, JSON via compact `json.dumps`. The minifiers auto-`pip install` into the PlatformIO penv on first use, with a copy-as-is fallback so a build never breaks. So: edit the readable sources; `.fs_build/` is generated output. This matters most for the captive portal, which reads `portal.html` whole into RAM on memory-tight ESP8266 boards. (Don't re-introduce a `.prettierignore` to hand-compact these — that approach was replaced by this script.)

## 3. Repository Layout

```
src/
├── main.cpp          # 14 lines: App app; app.setup(); app.loop();
├── config.h/.cpp     # art::Config - JSON load/save (LittleFS via ESP_FS), DeviceConfig/WiFiNet/
│                      #   HardwareConfig structs; DmxType is an alias for core::DmxType
├── connect.h/.cpp    # Connect - WiFi + in-house captive portal (192.168.4.1) serving
│                      #   data/www/portal.html (placeholder-substituted in portalPage()) +
│                      #   /style.css from LittleFS; non-blocking reconnect every 1s; reset()
│                      #   writes /force_portal.flag so the next boot forces the portal (B29).
│                      #   StatusLed* is injected via Connect::init, not an extern global
├── app/
│   ├── app.h/.cpp    # App - owns every top-level instance (server, connect, config, artnet,
│   │                  #   status, button, devices[]) and encodes the boot order - see §5
│   ├── deviceFactory.h/.cpp  # app::makeDevice(DeviceConfig, universe) -> unique_ptr<Device>,
│   │                  #   the FEATURE_*-guarded switch that used to live in main.cpp
│   └── safeMode.h/.cpp  # safeMode(reason) - LED-blink-forever halt if the filesystem can't mount
├── core/             # Platform-free C++ (no Arduino.h/String/millis) - natively unit-tested
│   │                  #   under [env:native], see test/test_*
│   ├── dmxTypes.h    # enum class DmxType, toWireString/fromWireString, DMX_CHANNELS/DMX_PACKET_SIZE
│   ├── dimmerLogic.h # DimmerLogic - PWM dimmer/strobe timing state machine (used by PwmDimmer)
│   ├── configModel.h # DeviceConfig/HardwareConfig/WifiNet PODs (aliased as art:: in config.h) - header-only
│   └── configCodec.h # inline ArduinoJson <-> configModel.h conversions (dmxChannelToJson/FromJson,
│                      #   hardwareToJson/FromJson, wifiNetToJson/FromJson) - header-only (Phase 5
│                      #   binding rule: [env:native] doesn't compile src/*.cpp, so no configCodec.cpp)
├── net/
│   ├── artnetService.h/.cpp  # ArtnetService - Art-Net UDP receive; single-path slice dispatch
│   │                  #   to device->onDmx(universe, slice) - see §5
│   └── webApi.h/.cpp  # REST API (replaces the old top-level api.h): GET/POST /config, GET /status,
│                      #   POST /reboot, POST /reset-wifi, GET /heap - see §7
├── platform/
│   ├── filesystem.h  # ESP_FS - unified LittleFS (ESP8266) / LittleFS (ESP32, since R2) wrapper
│   ├── pwm.h/.cpp    # Pwm::init/write - LEDC on ESP32, analogWrite on ESP8266
│   ├── mdns.h        # platform::mdnsBegin/mdnsLoop - advertises <host>.local (_http._tcp:80);
│   │                  #   started in App::setup() after WiFi connects (ESP8266 needs mdnsLoop())
│   ├── netbios.h     # platform::netbiosBegin - NetBIOS name responder (ESP8266NetBIOS/ESP32
│   │                  #   NetBIOS, bundled); router-independent LAN name (mainly Windows)
│   └── servo.h       # <Servo.h> include wrapper (only relevant under FEATURE_SERVO)
├── boards/
│   ├── board.h       # Dispatches on -D BOARD_<NAME> to the matching per-board header - see Gotcha #3
│   ├── features.h    # Derives FEATURE_DMX_PORT/FEATURE_SERVO/FEATURE_DIMMER/FEATURE_OLED - Gotcha #4
│   └── <env>.h       # One self-contained header per environment: pins, LED, button, OLED geometry
├── hw/
│   ├── logger.h      # LOG() macro = Serial.println
│   ├── statusLed.h   # StatusLed - Ticker-based blink patterns (connecting/portal/on/off)
│   └── oledDisplay.h # StatusDisplay (only if FEATURE_OLED) - auto-cycling status pages,
│                      #   64px and 32px SSD1306 variants, driven from loop() via millis()
├── device/
│   ├── device.h      # Abstract Device base v2 - start/stop/flip/set/get/isEnabled/tick/frame/
│   │                  #   onDmx/firstChannel/channelCount/setBlackout - see §6
│   ├── relay.h       # DmxRelay - binary on/off via threshold (header-only)
│   ├── dimmer.h/cpp  # PwmDimmer - thin shell over core::DimmerLogic; 2 DMX channels
│   │                  #   (level, strobe speed); #if FEATURE_DIMMER self-guarded
│   ├── dmxServo.h    # DmxServo - DMX 0-255 -> servo microseconds (header-only)
│   └── repeater.h    # DmxRepeater - Art-Net -> DMX512 gateway via DmxPort, 512ch (header-only)
└── dmx/
    ├── dmx.h         # DmxPort declaration; entire src/dmx/ tree is #if FEATURE_DMX_PORT
    ├── dmx32.cpp     # ESP32: core-1 FreeRTOS sender task (esp_dmx, mutex double-buffer)
    └── dmx8266.cpp   # ESP8266: simple ESPDMX wrapper

test/
├── test_sanity/      # [env:native] smoke test
├── test_dmxtypes/    # native tests for src/core/dmxTypes.h
└── test_dimmerlogic/ # native tests for src/core/dimmerLogic.h

data/                    # readable source assets - flashed *minified* via `uploadfs` (Gotcha #13)
├── config/default.json  # initial/fallback config
└── www/                  # web UI: index.html (home page), portal.html (captive portal),
                         #   style.css (one stylesheet shared by both)

tools/                   # HIL harness (verify.sh / boot_check.py / hil_smoke.py) +
                         #   minify_fs.py (build-time FS minifier - Gotcha #13)
.fs_build/            # GENERATED minified mirror of data/ (gitignored) - never edit (Gotcha #13)
include/Version.h     # AUTO-GENERATED - do not edit (Gotcha #1)
assets/*.jpg          # board pinout reference images
platformio.ini        # 12 firmware environments + [env:native] - see table below
```

## 4. Build Environments

`platformio.ini` defines 12 environments. Every environment passes exactly one `-D BOARD_<NAME>` flag (e.g. `-DBOARD_LOLIN_S2_MINI`), selecting the header in the "Board Header" column below — see Gotcha #3 for what each header defines and for the `ELEGANTOTA_USE_ASYNC_WEBSERVER` caveat. The columns cover what differs *architecturally* between environments — `platformio.ini` itself remains the source of truth for exact `lib_deps` versions and the full `build_flags` list.

| Environment | Chip | Board | Board Header | DMX Repeater | Notes |
|---|---|---|---|---|---|
| `d1_mini_oled` | ESP8266 | d1_mini | `boards/d1_mini_oled.h` | n/a (ESPDMX) | 64px OLED |
| `d1_mini` | ESP8266 | d1_mini | `boards/d1_mini.h` | n/a | baseline |
| `nodemcuv2` | ESP8266 | nodemcuv2 | `boards/nodemcuv2.h` | n/a | |
| `heltec_wifi_kit_8` | ESP8266 | heltec_wifi_kit_8 | `boards/heltec_wifi_kit_8.h` | n/a | 32px OLED variant |
| `sonoff_basic` | ESP8266 | sonoff_basic | `boards/sonoff_basic.h` | No (`dmx/` excluded) | relay-only, no Wire/SPI/Servo |
| `sonoff_s31` | ESP8266 | sonoff_basic | `boards/sonoff_s31.h` | No | smart-plug variant |
| `lolin32` | ESP32 | lolin32 | `boards/lolin32.h` | Yes | |
| `esp32-devkitc-v4` | ESP32 | esp32dev | `boards/esp32-devkitc-v4.h` | Yes | |
| `lolin_s2_mini` | ESP32-S2 | lolin_s2_mini | `boards/lolin_s2_mini.h` | Yes | |
| `esp32-s3-devkitc-1` | ESP32-S3 | esp32-s3-devkitc-1 | `boards/esp32-s3-devkitc-1.h` | Yes | |
| `seeed_xiao_esp32s3` | ESP32-S3 | seeed_xiao_esp32s3 | `boards/seeed_xiao_esp32s3.h` | Yes | see `assets/Xiao-Repeater.jpg` |
| `seeed_xiao_esp32s3_oled` | ESP32-S3 | seeed_xiao_esp32s3 | `boards/seeed_xiao_esp32s3_oled.h` | Yes | with I2S OLED |

### Conditional compilation flags

| Flag | Effect |
|---|---|
| `BOARD_<NAME>` (e.g. `BOARD_D1_MINI_OLED`) | Selects the per-board header in `src/boards/` — see Gotcha #3 |
| `ESP8266` / `ESP32` | Platform — set automatically by PlatformIO based on `platform =` |
| `FEATURE_DMX_PORT` / `FEATURE_SERVO` / `FEATURE_DIMMER` / `FEATURE_OLED` | Always-defined 0/1 flags from `src/boards/features.h`, derived from `BOARD_<NAME>` — gate the DMX port, servo, dimmer, and OLED modules — see Gotcha #4 |
| `DISABLE_OTA` | Strips ElegantOTA from the build |
| `ELEGANTOTA_USE_ASYNC_WEBSERVER=1` | Set on most envs so ElegantOTA uses AsyncWebServer mode |

## 5. Architecture & Data Flow

`main.cpp` is just:
```cpp
App app;
void setup() { app.setup(); }
void loop()  { app.loop(); }
```

`App` (`src/app/app.{h,cpp}`) owns every top-level instance by value or `std::unique_ptr` (`server`, `dnsServer`, `connect`, `config`, `artnet`, `status`, `buttonConfig`/`button`, `std::array<std::unique_ptr<Device>, MAX_DMX_DEVICES> devices` + a parallel raw `Device *deviceList[MAX_DMX_DEVICES]` for `ArtnetService::init`'s `Device**` API, and `#if FEATURE_OLED statusDisplay`).

**`App::setup()`**, roughly in order:
1. Mount the filesystem — LittleFS on both ESP8266 and ESP32 (`ESP_FS`, `src/platform/filesystem.h`); `safeMode()` (`app/safeMode.{h,cpp}`) halts if this fails: it brings up an open diagnostic AP (`ArtNet-<chip id>-SAFE`, 192.168.4.1) serving an inline-HTML page with the chip ID/failure reason/re-flash instructions, then blinks `LED_PIN` forever (the AP is best-effort - if WiFi/AsyncWebServer are also broken, the LED blink remains the last-resort signal)
2. `config.load()` — reads `/config/config.json` (falling back to `/config/default.json`)
3. Create `StatusLed`
4. Init the button (AceButton) — `buttonConfig.setIEventHandler(this)` registers `App::handleEvent` (App implements `ace_button::IEventHandler`); short press flips DMX devices, long press (5s, configurable) resets WiFi
5. If `FEATURE_OLED`, create `StatusDisplay`
6. For each configured `config.dmx[i]`: `devices[i] = app::makeDevice(config.dmx[i], 1)` (the `FEATURE_*`-guarded factory, `src/app/deviceFactory.{h,cpp}`), then `setBlackout(config.dmx[i].blackout)` and **`devices[i]->start()`** — `deviceList[i] = devices[i].get()`
7. `connect.init(&server, &dnsServer, status.get())` / `connect.connect(config.host)` — WiFi connection + in-house captive-portal fallback (note: `status` is passed in, not read via an `extern`). `connect.connect()` only *returns* on the STA-success path; the portal path ends in `ESP.restart()`, so anything after it runs only once WiFi is up.
8. `if (config.host.length()) platform::mdnsBegin(config.host)` + `platform::netbiosBegin(config.host)` — advertise `<host>.local` (mDNS) and the NetBIOS name (only reachable now that WiFi connected; the portal path never gets here). The DHCP hostname itself is set earlier, inside `Connect` between `WiFi.mode(WIFI_STA)` and `WiFi.begin()` (`Connect::applyHostname()` — ESP32 drops it if set before `WIFI_STA`, which left LAN scanners showing no name)
9. `artnet.init(config.universe, config.host, longName, deviceList, deviceCount)` — subscribes a lambda to the configured universe (this no longer calls `device->start()` — that happened in step 6)
10. Start the web server (REST API + ElegantOTA unless `DISABLE_OTA`) — **`server.begin()` must come last, after WiFi is up** (Gotcha #8)

**`App::loop()`** is short and non-blocking:
```cpp
void App::loop() {
  config.applyPendingUpdate();      // apply any config staged by POST /config (B11)
  button.check();
  artnet.loop();                    // parses incoming Art-Net UDP
  for (...) devices[i]->tick();
  ElegantOTA.loop();
  connect.loop();                   // non-blocking WiFi reconnect check every 1s
  platform::mdnsLoop();             // MDNS.update() on ESP8266; no-op on ESP32
  statusDisplay->loop();            // #if FEATURE_OLED
}
```

**Art-Net dispatch** (`src/net/artnetService.cpp`, `ArtnetService::init`'s `subscribeArtDmxUniverse` lambda): for each configured device, the incoming frame is sliced to that device's channel range — `start = device->firstChannel() - 1`, `len = min(device->channelCount(), size - start)` — and `device->onDmx(universe, data + start, len)` is called once with that slice (skipped if `start >= size` or `len == 0`). `Device::onDmx`'s default implementation (§6) does the change-detect `get()`/`set()` loop relative to `firstChannel()`; `DmxRepeater` overrides `onDmx` directly for a bulk `writeFrame`.

**ESP32 core-1 DMX repeater task** (only spun up when a `Repeater` device is configured): `DmxPort::init()` (`dmx32.cpp`) creates a FreeRTOS task pinned to core 1 that continuously transmits the 512-channel DMX buffer via `esp_dmx` every ~40ms. `DmxRepeater::onDmx()` writes incoming Art-Net data into that buffer via `DmxPort::write()`, which is mutex-protected against the sender task. Core 0 keeps running everything else (WiFi, Art-Net receive, web server, main loop). See Gotcha #5 for the concurrency rules.

## 6. The Device Abstraction

All DMX outputs implement `Device` (`src/device/device.h`, interface v2):

```cpp
virtual void start();                 // allocate resources, init
virtual void stop();                  // deallocate resources
virtual void flip();                  // toggle state (button press); sets manualOverride
virtual void set(uint16_t channel, uint8_t data);
virtual uint8_t get(uint16_t channel);
virtual bool isEnabled();
virtual void tick();                  // called from loop(); default = blackout on silence
virtual void frame();                 // marks "frame received" (resets silence timer)
virtual void frame(uint32_t univ, const uint8_t *data, uint16_t size);
virtual void onDmx(uint32_t univ, const uint8_t *data, uint16_t len);  // see §5 dispatch
virtual uint16_t firstChannel();      // default: return channel
virtual uint16_t channelCount() = 0;
void setBlackout(bool enabled);       // sets blackoutOnSilence
```

The default `tick()` blacks out the device (`set(channel, 0)`) if `frame()` hasn't been called in `DMX_SILENCE_TIMEOUT` (5000ms) — unless `manualOverride` is set (by `flip()`, until the next real `frame()`) or `blackoutOnSilence` is `false` (set via `setBlackout()`, wired from the per-device `"blackout"` config flag, default `true`) — see Gotcha #10. The default `onDmx()` does a relative change-detect loop: for `i in [0, min(len, channelCount()))`, channel `firstChannel() + i` gets `set()` if `data[i] != get()`.

**The four concrete types:**

| Type (config) | Class | File | What it does |
|---|---|---|---|
| `BINARY` | `DmxRelay` | `device/relay.h` | One channel, on/off via a `threshold`, drives a pin to `level` (HIGH/LOW) |
| `DIMMER` | `PwmDimmer` | `device/dimmer.h`/`.cpp` | Two DMX channels (`channel`, `channel+1`): PWM dimming on a pin via `core::DimmerLogic`, plus strobe-speed control on the second channel |
| `SERVO` | `DmxServo` | `device/dmxServo.h` | Maps DMX 0–255 to servo pulse width (500–2500µs) via `getAngleValue()` |
| `REPEATER` | `DmxRepeater` | `device/repeater.h` | Overrides `onDmx` directly: bulk-writes the full 512-byte Art-Net frame to a physical DMX512 line via `DmxPort::writeFrame`. Universe is currently ignored (README TODO) |

`"BINARY"`/`"RELAY"` (alias) → `Relay`, `"DIMMER"` → `Dimmer`, `"SERVO"` → `Servo`, `"REPEATER"` → `Repeater` — `core::fromWireString`/`toWireString` (`src/core/dmxTypes.h`) own this mapping; `Relay` always serializes back out as `"BINARY"` (compat contract).

**Adding a new device type** (README's TODO lists "NeoPixel strip" and "single-channel Dimmer" as open items — this is a likely future task):
1. Add an enum value to `core::DmxType` in `src/core/dmxTypes.h`, plus its `toWireString`/`fromWireString` mapping (and a native test in `test/test_dmxtypes/`)
2. Implement a class extending `Device` (header-only or `.h`/`.cpp`, following the existing patterns)
3. Add a `case` to the `switch (cfg.type)` in `app::makeDevice` (`src/app/deviceFactory.cpp`), returning `std::make_unique<YourDevice>(...)`
4. If it needs Servo/SPI/Wire/extra flash, wrap that `case` (and the corresponding `#include`) in the matching `#if FEATURE_*` (Gotcha #4)

## 7. Configuration System

- `art::Config` (`config.h`/`.cpp`) loads/saves JSON via ArduinoJson 7's elastic `JsonDocument` (Gotcha #7) — `configFromJson`/`configToJson` (the persisted-config envelope: `configVersion`/`_needReboot`/`hw`/`host`/`universe`/`wifi`/`dmx`, deliberately *not* `info`) delegate per-section conversions to `core::configCodec` (`src/core/configCodec.h`, header-only, natively tested under `test/test_configcodec/`): `dmxChannelToJson`/`dmxChannelFromJson`, `hardwareToJson`/`hardwareFromJson`, `wifiNetToJson`/`wifiNetFromJson`. The PODs they convert (`DeviceConfig`, `HardwareConfig`, `WifiNet`) live in `core::configModel` and are aliased into `art::` (`config.h`) for spelling compatibility.
- `configVersion` (`CONFIG_SCHEMA_VERSION`, currently `1`, `config.h`): `configToJson()` always writes the current constant; `configFromJson()` reads `"configVersion"` defaulting to `1` for configs that predate the field. No migration logic exists yet — this just establishes the hook for one.
- `data/config/default.json` is the initial/fallback config (flashed via `pio run -t uploadfs`); the runtime config persists to `/config/config.json` on the device's filesystem (LittleFS on both ESP8266 and ESP32, since R2).
- The `dmx` and `wifi` collections are `std::vector<DeviceConfig>` / `std::vector<WiFiNet>` by value.
- Each `DeviceConfig` has an additive `bool blackout = true` field — wired to `Device::setBlackout()` (§6); old configs without the key default to `true` (unchanged behavior).
- `HardwareConfig` (`hw`) has additive `bool authEnabled = false`, `std::string authUser`, `std::string authPass` fields (R5 compat: configs without these keys parse with auth disabled) — see "HTTP basic-auth" below.
- `POST /config` (B11): the async-context handler only `stageUpdate()`s the raw JSON and returns `202 {"status":"pending"}` immediately; `App::loop()`'s `config.applyPendingUpdate()` does the actual `update()`/`save()` on the main task on the next iteration. Use `GET /config` afterward to confirm.
- REST surface (full spec in README, `src/net/webApi.{h,cpp}`): `GET/POST /config`, `GET /status` (lightweight `info`-only poll endpoint), `POST /reboot`, `POST /reset-wifi`, `GET /heap`. `POST /config` partial updates are supported, but `dmx[]` is index-based so **all** elements must be sent together when updating that array; `_needReboot` flags pending changes.
- **HTTP basic-auth** (optional, off by default): if `hw.authEnabled` is `true`, `webApi.cpp`'s `checkAuth()` helper requires HTTP basic-auth (`hw.authUser`/`hw.authPass`) for `POST /config`, `POST /reboot`, `POST /reset-wifi`, and `app/app.cpp` calls `ElegantOTA.setAuth(...)` to also gate `/update`. `GET /config`, `GET /status`, `GET /heap`, and the static web UI stay unauthenticated.

## 8. Conventions

These describe the patterns already used in the codebase — useful for staying consistent, not hard rules:

- **Naming**: classes are PascalCase (`DmxRelay`, `PwmDimmer`, `StatusDisplay`, `ArtnetService`, `DmxPort`, `App`); functions and variables are camelCase; constants and macros are `UPPER_SNAKE_CASE`.
- **Namespace**: config-related types (`Config`, `DeviceConfig`, `WiFiNet`, `HardwareConfig`) live in `art::`; `DmxType` is `core::DmxType`, aliased into `art::` for spelling compatibility. Platform-free types/constants live in `core::` (`src/core/`).
- **Logging**: use the `LOG(...)` macro from `hw/logger.h` (= `Serial.println`) rather than raw `Serial.print`.
- **File organization**: small device classes tend to be header-only (`relay.h`, `dmxServo.h`, `repeater.h`, `oledDisplay.h`, `statusLed.h`); larger modules get `.h`/`.cpp` pairs (`config`, `connect`, `dimmer`, `dmx32`/`dmx8266`, `app`, `artnetService`). This is an observed tendency, not enforced.
- **`core/` extractions ship with native tests in the same commit** (the Phase 5 "binding rule") — `src/core/` must stay Arduino-free (no `Arduino.h`/`String`/`millis()`; pass time as a parameter) so it's includable and testable under `[env:native]`.
- **Known rough edges** (don't "fix" these unprompted — they're tracked in README's TODO):
  - `DmxRepeater` ignores the Art-Net `universe` parameter.
  - Multi-device configs are lightly tested beyond a single REPEATER (the bench) and 3×DIMMER on ESP8266 (Gotcha #11).

## 9. Further Reading

- **[README.md](README.md)** — device type details (DIMMER/RELAY/SERVO/REPEATER configs), full REST API reference, WiFi/captive-portal behavior, OTA via `/update`, version history, and the **TODO list**, which doubles as the project backlog.
- **`assets/*.jpg`** — pinout reference images for ESP32 DevKitC v4, Lolin D32, and the XIAO ESP32-S3 repeater wiring.
- **`REFACTORING_PLAN.md`** — the `big-refactor` branch's working document: 24 catalogued pre-refactor bugs (B1-B24), the phase-by-phase plan (Phases 0-8), and a per-item "Done" log describing exactly what changed and how each was verified. Phases 0-5 (CI/hygiene, bug fixes, renames, platform layer, board layer, core restructure) are complete as of this revision — the layout and interfaces described above (§3, §5, §6) are Phase 5's end state. Phases 6-8 (config robustness, dependency refresh, broader test coverage) are not yet started.
