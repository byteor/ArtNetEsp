# AGENTS.md

ArtNetEsp is an ESP8266/ESP32 Arduino firmware project (built with PlatformIO) that turns a cheap dev board into an Art-Net node — receiving DMX-over-WiFi from a lighting console (typically [QLC+](https://www.qlcplus.org/)) and driving relays, PWM dimmers, servos, or a real DMX512 line via an RS485 "repeater". It was built for community theater: cheap, wireless, no commercial lighting console required. Current version: 2026.1.2.

For user-facing docs — device types, REST API, WiFi/captive portal behavior, OTA updates, version history, and the project TODO/backlog — see [README.md](README.md). This file is for anyone (human or AI) working on the firmware itself.

## 1. Build & Flash Quick Reference

```bash
git submodule update --init --recursive   # required once - pulls in platformio_version_increment

pio run -e <env>                          # build (see environment table below for <env> values)
pio run -e <env> -t upload                # flash firmware over USB
pio run -e <env> -t uploadfs              # flash data/ (LittleFS/SPIFFS) - default config + web UI
pio run -e <env> -t monitor               # serial monitor @ 115200

pio run                                   # build ALL 12 environments - good sanity check
                                           # before committing a cross-cutting change
```

There are no unit tests (`test/` is just the PlatformIO placeholder) and no CI. A successful `pio run` plus watching the serial monitor is the only verification available — see Gotcha #12 for which environments to build for a given change.

## 2. Critical Gotchas (read this first)

1. **Never hand-edit `include/Version.h`, `version`, `.vscode/c_cpp_properties.json`, or `.vscode/launch.json`.** All four are auto-generated — `Version.h` and `version` by the `platformio_version_increment` pre/post build scripts, the two `.vscode/*.json` files by the PlatformIO IDE extension (their headers literally say "AUTO-GENERATED FILE! PLEASE DO NOT MODIFY"). Any manual edit gets silently overwritten on the next build/IDE refresh.

2. **The build depends on a git submodule.** `platformio_version_increment/` is referenced by `extra_scripts` in every environment. If it's empty (fresh clone without `--recurse-submodules`), `pio run` fails with an error that doesn't obviously point at "missing submodule" — run `git submodule update --init --recursive` first.

3. **Each environment's hardware identity is a single `-D BOARD_<NAME>` flag, dispatched by `src/boards/board.h`.** `platformio.ini` passes exactly one `BOARD_<NAME>` define per environment (e.g. `-DBOARD_LOLIN_S2_MINI`); `src/boards/board.h` `#include`s the matching `src/boards/<name>.h`, a small self-contained header defining that board's `LED_PIN`, `DEFAULT_BUTTON_PIN`, and — for the 6 ESP32 envs — `DMX_TX_PIN`/`DMX_RX_PIN`/`DMX_ENABLE_PIN`/`DMX_PORT` (plus `SCREEN_WIDTH`/`SCREEN_HEIGHT`/`SCREEN_ADDRESS`/`OLED_RESET` for the 3 OLED boards). There's no more `#ifndef`-based fallback: `lolin_s2_mini.h` now spells out its DMX pins explicitly (17/16/21/1) instead of silently inheriting them from a shared default. To add a new board: create `src/boards/<name>.h`, add an `#elif defined(BOARD_<NAME>) #include "boards/<name>.h"` branch to `src/boards/board.h`, and give the new environment exactly one `-D BOARD_<NAME>` in `platformio.ini`.

   **Caveat — flags read by third-party library code must stay global `-D` flags.** A board header is only visible to files that `#include "boards/board.h"`, but a library's own `.cpp` (a separate translation unit) only sees `platformio.ini`'s `build_flags`. `ELEGANTOTA_USE_ASYNC_WEBSERVER=1` is the flag that bites: it must remain a `-D` in `platformio.ini` (ElegantOTA's own source reads it to pick its `begin(AsyncWebServer*, ...)` overload), not a `#define` in a board header — moving it produced a link error (`undefined reference to ElegantOTAClass::begin(AsyncWebServer*, const char*, const char*)`) because the library compiled without the macro while `main.cpp` compiled with it. `DISABLE_OTA` (sonoff_basic/sonoff_s31) is fine as a header-only `#define` because `main.cpp` includes `boards/board.h` *before* `<ElegantOTA.h>` and ElegantOTA isn't in those envs' `lib_deps` at all.

4. **`SONOFF_BASIC` excludes whole modules, not just a feature flag.** When defined (`sonoff_basic`, `sonoff_s31` environments):
   - `src/dmx/dmx.h` — and therefore all of `src/dmx/` — is excluded entirely (the whole header is wrapped in `#ifndef SONOFF_BASIC`).
   - `device/strobe.h`, `device/repeater.h`, and `device/dmxServo.h` are not `#include`d in `main.cpp`.
   - The device-type `switch` in `main.cpp::setup()` only handles `DmxType::Binary` for these boards; `Servo`/`Dimmable`/`Repeater` config entries fall to `default:` and just log "Incompatible DMX device type".
   - `lib_ignore` drops `Wire`, `SPI`, and `Servo` — new code that unconditionally includes these will fail to link on Sonoff boards.

   **Any new device type or feature that needs Servo/SPI/Wire/extra flash must be wrapped in `#ifndef SONOFF_BASIC`.**

5. **The ESP32 core-1 DMX sender task (`src/dmx/dmx32.cpp`) is fragile and load-bearing.** It's a FreeRTOS task pinned to core 1 (priority 2), looping every `refreshIntervalMs` (40ms), double-buffered through `dataMutex`. The existing pattern copies the buffer under the mutex, **releases the mutex, then** does the blocking `dmx_write`/`dmx_send_num`/`dmx_wait_sent` calls — preserve this ordering; doing blocking I/O while holding `dataMutex` would stall `write()` callers on core 0 (the Art-Net frame handler). `write()` itself uses a 10ms mutex-timeout and **silently drops** the value if it can't get the lock — no error is logged, so "DMX channel not updating" can be this. This code only landed in v2026.1.2 (commit `9d82329`) after the repeater was previously broken on ESP32 — treat it as fragile, not a casual refactor target. `dmx8266.cpp` is a completely different, simple synchronous wrapper around `ESPDMX` — don't assume the two `DmxProxy` implementations behave the same beyond their shared interface.

6. **`MAX_DMX_DEVICES` differs by platform** (8 on ESP32, 4 on ESP8266 — `config.h`, via `#ifdef ESP32`). The fixed-size `dmx_devices[MAX_DMX_DEVICES]` array in `main.cpp` and all the `for (... && i < MAX_DMX_DEVICES; ...)` loop bounds must stay consistent with this if it's ever changed.

7. **The config JSON buffer is a fixed `StaticJsonDocument<CONFIG_BUFFER_SIZE>` (1024 bytes)** — `config.h`. Adding new config fields, especially per-DMX-channel fields that get multiplied by up to 8 devices on ESP32, risks silently overflowing/truncating this buffer. ArduinoJson won't necessarily throw — check `doc.overflowed()` if you grow the schema.

8. **`server.begin()` must only be called after WiFi connects.** `main.cpp` has an explicit comment: `// Call ONLY AFTER WiFi gets connected !!!!! (or reboot loop)`. Preserve this ordering if you reorganize `setup()`.

9. **`.vscode/` and `.pio/` are gitignored but exist locally** with PlatformIO-generated content (build cache, IDE IntelliSense config). Don't be alarmed they're "untracked", and don't try to add or clean them up.

10. **Default DMX-silence blackout**: `Device::handle()` (`device.h`) calls `set(channel, 0)` if no `frame()` has arrived in `DMX_SILENCE_TIMEOUT` (5000ms). This is shared by all four device types. README's TODO lists "make blackout on DMX timeout optional" — if you implement that, it's a base-class change affecting everything.

11. **Multi-device configs are lightly tested.** README itself says running multiple DMX devices "wasn't properly tested other than 3 DIMMER channels on ESP8266". Be extra careful with assumptions about concurrent device behavior, especially an ESP32 repeater (core-1 task) running alongside other device types.

12. **Verification = build the affected environments.** For anything touching `board.h`, `device/`, `dmx/`, or `main.cpp`, build at minimum one ESP8266 env (e.g. `d1_mini_oled`), one full-featured ESP32 env (e.g. `esp32-devkitc-v4`), and `sonoff_basic` (to catch `SONOFF_BASIC` exclusion breakage). For pin/board-specific changes, also build the specific environment(s) affected.

## 3. Repository Layout

```
src/
├── main.cpp          # setup()/loop(), global instances, button handler, device instantiation switch
├── artnetHandler.h   # ArtnetHandler - Art-Net UDP receive, dispatches frames to Device[] via lambda
├── config.h/.cpp     # art::Config - JSON load/save (LittleFS/SPIFFS), DmxType enum,
│                      #   DmxChannel/WiFiNet/HardwareConfig structs
├── connect.h/.cpp    # Connect - WiFi + ESPAsyncWiFiManager captive portal (192.168.4.1),
│                      #   reconnect check every 1s
├── api.h             # REST API: GET/POST /config, POST /reboot, POST /reset-wifi, GET /heap
├── boards/
│   ├── board.h       # Dispatches on -D BOARD_<NAME> to the matching per-board header - see Gotcha #3
│   └── <env>.h       # One self-contained header per environment: pins, LED, button, OLED geometry
├── hw/
│   ├── logger.h      # LOG() macro = Serial.println
│   ├── statusLed.h   # StatusLed - Ticker-based blink patterns (connecting/portal/on/off)
│   └── oledDisplay.h # StatusDisplay (only if OLED_SSD1306) - auto-cycling status pages,
│                      #   64px and 32px SSD1306 variants
├── device/
│   ├── device.h      # Abstract Device base - start/stop/flip/set/get/isEnabled/handle/
│   │                  #   frame/getNumberOfChannels
│   ├── relay.h       # DmxRelay - binary on/off via threshold (header-only)
│   ├── strobe.h/cpp  # Strobe - PWM dimmer + flash timing (name is misleading, see Conventions)
│   ├── dmxServo.h    # DmxServo - DMX 0-255 -> servo microseconds (header-only)
│   └── repeater.h    # DmxRepeater - Art-Net -> DMX512 gateway, 512ch (header-only)
└── dmx/
    ├── dmx.h         # DmxProxy declaration; entire file is #ifndef SONOFF_BASIC
    ├── dmx32.cpp     # ESP32: core-1 FreeRTOS sender task (esp_dmx, mutex double-buffer)
    └── dmx8266.cpp   # ESP8266: simple ESPDMX wrapper

data/
├── config/default.json  # initial/fallback config, flashed via `pio run -t uploadfs`
└── www/                  # web UI (index.html / index.css)

include/Version.h     # AUTO-GENERATED - do not edit (Gotcha #1)
assets/*.jpg          # board pinout reference images
platformio.ini        # 12 build environments - see table below
```

## 4. Build Environments

`platformio.ini` defines 12 environments. Every environment passes exactly one `-D BOARD_<NAME>` flag (e.g. `-DBOARD_LOLIN_S2_MINI`), selecting the header in the "Board Header" column below — see Gotcha #3 for what each header defines and for the `ELEGANTOTA_USE_ASYNC_WEBSERVER` caveat. The columns cover what differs *architecturally* between environments — `platformio.ini` itself remains the source of truth for exact `lib_deps` versions and the full `build_flags` list.

| Environment | Chip | Board | Board Header | Other `-D` Flags | DMX Repeater | Notes |
|---|---|---|---|---|---|---|
| `d1_mini_oled` | ESP8266 | d1_mini | `boards/d1_mini_oled.h` | `OLED_SSD1306=1` | n/a (ESPDMX) | 64px OLED |
| `d1_mini` | ESP8266 | d1_mini | `boards/d1_mini.h` | — | n/a | baseline |
| `nodemcuv2` | ESP8266 | nodemcuv2 | `boards/nodemcuv2.h` | — | n/a | |
| `heltec_wifi_kit_8` | ESP8266 | heltec_wifi_kit_8 | `boards/heltec_wifi_kit_8.h` | `OLED_SSD1306=1` | n/a | 32px OLED variant |
| `sonoff_basic` | ESP8266 | sonoff_basic | `boards/sonoff_basic.h` | `SONOFF_BASIC` | No (`dmx/` excluded) | relay-only, no Wire/SPI/Servo |
| `sonoff_s31` | ESP8266 | sonoff_basic | `boards/sonoff_s31.h` | `SONOFF_BASIC` | No | smart-plug variant |
| `lolin32` | ESP32 | lolin32 | `boards/lolin32.h` | — | Yes | |
| `esp32-devkitc-v4` | ESP32 | esp32dev | `boards/esp32-devkitc-v4.h` | — | Yes | |
| `lolin_s2_mini` | ESP32-S2 | lolin_s2_mini | `boards/lolin_s2_mini.h` | — | Yes | |
| `esp32-s3-devkitc-1` | ESP32-S3 | esp32-s3-devkitc-1 | `boards/esp32-s3-devkitc-1.h` | — | Yes | |
| `seeed_xiao_esp32s3` | ESP32-S3 | seeed_xiao_esp32s3 | `boards/seeed_xiao_esp32s3.h` | — | Yes | see `assets/Xiao-Repeater.jpg` |
| `seeed_xiao_esp32s3_oled` | ESP32-S3 | seeed_xiao_esp32s3 | `boards/seeed_xiao_esp32s3_oled.h` | `OLED_SSD1306=1` | Yes | with I2S OLED |

### Conditional compilation flags

| Flag | Effect |
|---|---|
| `BOARD_<NAME>` (e.g. `BOARD_D1_MINI_OLED`) | Selects the per-board header in `src/boards/` — see Gotcha #3 |
| `ESP8266` / `ESP32` | Platform — set automatically by PlatformIO based on `platform =` |
| `SONOFF_BASIC` | Excludes DMX/servo/strobe/repeater modules entirely — see Gotcha #4 |
| `OLED_SSD1306` | Compiles in `StatusDisplay` (`hw/oledDisplay.h`) |
| `DISABLE_OTA` | Strips ElegantOTA from the build |
| `ELEGANTOTA_USE_ASYNC_WEBSERVER=1` | Set on most envs so ElegantOTA uses AsyncWebServer mode |

## 5. Architecture & Data Flow

**`setup()`** (in `main.cpp`), roughly in order:
1. Mount the filesystem — LittleFS on ESP8266, SPIFFS on ESP32 (`ESP_FS`)
2. `config.load()` — reads `/config/config.json` (falling back to `/config/default.json`)
3. Create `StatusLed`
4. Init the button (AceButton) — short press flips DMX devices, long press (5s, configurable) resets WiFi
5. If `OLED_SSD1306`, create `StatusDisplay`
6. Instantiate `dmx_devices[MAX_DMX_DEVICES]` — a `Device*` array, populated via a `switch` on `config.dmx[i]->type` (`Binary` → `DmxRelay`, and under `#ifndef SONOFF_BASIC`: `Servo` → `DmxServo`, `Dimmable` → `Strobe`, `Repeater` → `DmxRepeater`)
7. `connect.init()` / `connect.connect()` — WiFi connection + captive portal fallback
8. `artnet.init()` — subscribes a lambda to the configured universe
9. Start the web server (REST API + ElegantOTA unless `DISABLE_OTA`) — **`server.begin()` must come last, after WiFi is up** (Gotcha #8)

**`loop()`** is short and non-blocking:
```cpp
void loop() {
  button.check();
  artnet.loop();                    // parses incoming Art-Net UDP
  for (...) dmx_devices[i]->handle();
  ElegantOTA.loop();
  connect.loop();                   // checks WiFi every 1s, reconnects if needed
}
```

**Art-Net dispatch**: the lambda registered in `artnetHandler.h` runs for every incoming frame on the subscribed universe. For each configured device, it calls `device->frame(universe, data, size)` and then `device->set(channel, value)` for each channel in that device's range (`getChannel()` .. `getChannel() + getNumberOfChannels()`).

**ESP32 core-1 DMX repeater task** (only spun up when a `Repeater` device is configured): `DmxProxy::init()` (`dmx32.cpp`) creates a FreeRTOS task pinned to core 1 that continuously transmits the 512-channel DMX buffer via `esp_dmx` every ~40ms. `DmxRepeater::frame()` writes incoming Art-Net data into that buffer through `DmxProxy::write()`, which is mutex-protected against the sender task. Core 0 keeps running everything else (WiFi, Art-Net receive, web server, main loop). See Gotcha #5 for the concurrency rules.

## 6. The Device Abstraction

All DMX outputs implement `Device` (`src/device/device.h`):

```cpp
virtual void start();                 // allocate resources, init
virtual void stop();                  // deallocate resources
virtual void flip();                  // toggle state (button press)
virtual void set(uint8_t channel, uint8_t data);
virtual uint8_t get(uint8_t channel);
virtual bool isEnabled();
virtual void handle();                // called from loop(); default = blackout on silence
virtual void frame();                 // marks "frame received" (resets silence timer)
virtual void frame(uint32_t univ, const uint8_t *data, uint16_t size);
virtual uint16_t getNumberOfChannels();
```

The default `handle()` blacks out the device (`set(channel, 0)`) if `frame()` hasn't been called in `DMX_SILENCE_TIMEOUT` (5000ms) — see Gotcha #10.

**The four concrete types:**

| Type (config) | Class | File | What it does |
|---|---|---|---|
| `BINARY` | `DmxRelay` | `device/relay.h` | One channel, on/off via a `threshold`, drives a pin to `level` (HIGH/LOW) |
| `DIMMER` | `Strobe` | `device/strobe.h`/`.cpp` | PWM dimming on a pin, plus flash/strobe timing. **The class name is misleading** — it's primarily a dimmer; README's TODO flags renaming it |
| `SERVO` | `DmxServo` | `device/dmxServo.h` | Maps DMX 0–255 to servo pulse width (500–2500µs) via `getAngleValue()` |
| `REPEATER` | `DmxRepeater` | `device/repeater.h` | Forwards the full 512-byte Art-Net frame to a physical DMX512 line via `DmxProxy`. Universe is currently ignored (README TODO) |

**Adding a new device type** (README's TODO lists "NeoPixel strip" and "single-channel Dimmer" as open items — this is a likely future task):
1. Add an enum value to `art::DmxType` in `config.h`
2. Add the string mapping in `dmxTypeToString()` / `dmxTypeFromString()`
3. Implement a class extending `Device` (header-only or `.h`/`.cpp`, following the existing patterns)
4. Wire it into the `switch` in `main.cpp::setup()`
5. If it needs Servo/SPI/Wire/extra flash, wrap it in `#ifndef SONOFF_BASIC` (Gotcha #4)

## 7. Configuration System

- `art::Config` (`config.h`/`.cpp`) loads/saves JSON via ArduinoJson, using a `StaticJsonDocument<CONFIG_BUFFER_SIZE>` where `CONFIG_BUFFER_SIZE` is **1024 bytes** (Gotcha #7).
- `data/config/default.json` is the initial/fallback config (flashed via `pio run -t uploadfs`); the runtime config persists to `/config/config.json` on the device's filesystem.
- The `dmx` and `wifi` collections are `LinkedList<DmxChannel*>` / `LinkedList<WiFiNet*>`.
- REST surface (full spec in README): `GET/POST /config` (partial updates supported, but `dmx[]` is index-based so **all** elements must be sent together when updating that array; `_needReboot` flags pending changes), `POST /reboot`, `POST /reset-wifi`, `GET /heap`.

## 8. Conventions

These describe the patterns already used in the codebase — useful for staying consistent, not hard rules:

- **Naming**: classes are PascalCase (`DmxRelay`, `StatusDisplay`, `ArtnetHandler`, `DmxProxy`); functions and variables are camelCase; constants and macros are `UPPER_SNAKE_CASE`.
- **Namespace**: config-related types (`Config`, `DmxType`, `DmxChannel`, `WiFiNet`, `HardwareConfig`) live in `art::`.
- **Logging**: use the `LOG(...)` macro from `hw/logger.h` (= `Serial.println`) rather than raw `Serial.print`.
- **File organization**: small device classes tend to be header-only (`relay.h`, `dmxServo.h`, `repeater.h`, `oledDisplay.h`, `statusLed.h`); larger modules get `.h`/`.cpp` pairs (`config`, `connect`, `strobe`, `dmx32`/`dmx8266`). This is an observed tendency, not enforced.
- **Known rough edges** (don't "fix" these unprompted — they're known and tracked in README's TODO):
  - `Strobe` is really a PWM dimmer; the name is considered a mistake.
  - `DmxRepeater` ignores the Art-Net `universe` parameter.
  - The stroboscope/flash behavior in `Strobe` is flagged as broken/TBD.

## 9. Further Reading

- **[README.md](README.md)** — device type details (DIMMER/RELAY/SERVO/REPEATER configs), full REST API reference, WiFi/captive-portal behavior, OTA via `/update`, version history, and the **TODO list**, which doubles as the project backlog.
- **`assets/*.jpg`** — pinout reference images for ESP32 DevKitC v4, Lolin D32, and the XIAO ESP32-S3 repeater wiring.
