# ArtNetEsp — REST API & UI Design Contract

**Contract version: 1.0.0** · Last updated: 2026-06-16 · Applies to firmware 2026.2.x

## 1. Purpose & authority

This is the **canonical specification** of ArtNetEsp's HTTP REST API and its
configuration single-page app (SPA). Any agent (or person) building an API
client, an alternate UI, or reconstructing this UI **MUST** follow this document
so the user-facing behavior (UX) stays identical across implementations.

On conflict, **this document is authoritative for *intended* behavior**; the
source code is the implementation and may contain incidental detail. The
implementation lives in `src/net/webApi.cpp` (REST), `src/config.cpp` +
`src/core/configModel.h` + `src/core/configCodec.h` (config schema),
`src/core/dmxTypes.h` (device-type enum), and `web/src/**` (the SPA). README has
human-facing prose for the same surfaces; **this doc wins on conflict**.

Scope of this contract: the **REST API** and the **configuration SPA** served at
`/`. The captive-portal WiFi-setup page is intentionally **out of scope** here
(see `README.md` → "WiFi connection and Captive Portal").

## 2. Versioning & how to catch up

The contract carries a **semantic version** `MAJOR.MINOR.PATCH`, independent of
the firmware version and of `CONFIG_SCHEMA_VERSION` (§6):

| Bump | When |
|---|---|
| **MAJOR** | A backwards-incompatible change (removed/renamed/retyped field or endpoint, changed status code or semantics). Avoid — see §3. |
| **MINOR** | A backwards-compatible addition (new endpoint, new optional field, new UI control/section, new enum value). |
| **PATCH** | Clarification, wording, or a purely cosmetic UI change that does not alter information architecture or behavior. |

**Catching up:** the [Changelog](#10-changelog) is append-only, newest first,
and every entry is tagged `Compatible` or `Breaking`. An agent that last synced
at version *X* reads every entry **newer than *X*** to learn exactly what changed
and whether its existing client/UI still conforms. If you only know the firmware
version, map it via the changelog's "firmware" note.

This contract version is **separate** from:
- `CONFIG_SCHEMA_VERSION` (the persisted-config schema integer, §6) — bumped only
  when the on-disk/JSON config shape changes in a way needing migration.
- the firmware version (`2026.2.x`) — auto-bumped on every build/flash.

## 3. Backwards-compatibility policy (best-effort)

The REST API is kept **backwards-compatible on a best-effort basis**. Follow
these rules — each already has precedent in the codebase:

- **Additive only.** Do not remove, rename, or change the type/units/semantics of
  an existing JSON field, endpoint, or status code.
- **New fields are optional with safe defaults.** A request that omits a new key
  must behave exactly as before. Precedent: `hardwareFromJson` leaves any missing
  `hw.*` key at its existing value (`src/core/configCodec.h`); additive fields
  like `hw.authEnabled`, `hw.wifiPowerSave`, `dmx[].blackout` all default such
  that pre-existing configs are unaffected.
- **Keep accepting legacy values.** Precedent: device type accepts both `BINARY`
  and `RELAY` on input, and any unrecognized type maps to `DISABLED` rather than
  erroring (`src/core/dmxTypes.h`). Never reject input that an older client
  legitimately sent.
- **Responses only grow.** Add fields to responses; never drop or repurpose one.
  Clients must ignore unknown fields.
- **Breaking change of last resort:** if unavoidable, bump this contract's
  **MAJOR**, bump `CONFIG_SCHEMA_VERSION`, and add migration logic keyed on the
  incoming `configVersion` (the hook already exists in
  `Config::configFromJson`). Document the migration in the changelog entry.

## 4. Change process — *from now on*

Whenever you change **any** of: an endpoint (path/method/request/response/status/
semantics), the config JSON schema (a field, type, default, or enum value), the
`info`/`status` fields, the auth model, **or** the SPA's information architecture
/ per-section field set / save semantics / visibility rules / user-visible
behavior — you **MUST**, in the same change:

1. Update the relevant section(s) of this document.
2. Bump the **contract version** per §2.
3. Add a **changelog** entry (§10) tagged `Compatible` or `Breaking`.

Pure visual/CSS tweaks that don't change IA or behavior → PATCH (or no bump).
This rule is mirrored in `AGENTS.md` so contributors and agents see it at the
point of change.

## 5. REST API

### 5.1 Conventions

- **Base URL:** the device root — `http://<host>.local/` (mDNS/NetBIOS name from
  `config.host`) or `http://<device-ip>/`. HTTP, port **80**. No HTTPS.
- **Content type:** JSON request/response unless noted (`GET /heap` is plain
  text). The SPA is same-origin with the API (served from the same device).
- **Static UI:** `GET /` serves `index.html`; the bundle (`/app.js`, `/style.css`)
  and `/portal.html` are static files under the device FS (`/www/`).
- **Unknown route:** `404 text/plain "Not found"`.

### 5.2 Authentication

Optional HTTP **Basic** auth, off by default, gated by `hw.authEnabled`
(credentials `hw.authUser` / `hw.authPass`):

| Route | Auth required when `authEnabled` |
|---|---|
| `POST /config`, `POST /reboot`, `POST /reset-wifi`, `/update` (OTA) | **Yes** |
| `GET /config`, `GET /status`, `GET /heap`, static UI (`/`, `/app.js`, …) | No |

When required and missing/wrong, the server replies **`401`** with a
`WWW-Authenticate` challenge. Same-origin browser `fetch` lets the browser handle
the challenge; an API client should send an `Authorization: Basic …` header.

### 5.3 Endpoints

| Method | Path | Auth | Success | Notes |
|---|---|---|---|---|
| GET | `/config` | no | `200` JSON | Full config envelope (§6) **plus** nested `info` (§6.5). |
| POST | `/config` | yes | **`202`** `{"status":"pending"}` | **Async-staged** — see §5.4. `500 {"error":"update too large"}` if body too big; `401` if auth. |
| GET | `/status` | no | `200` JSON | The `info` fields (§6.5) **plus** `_needReboot`. Lightweight poll. |
| POST | `/reboot` | yes | `200` `{"reboot":"OK"}` | Device restarts ~200 ms after replying. |
| POST | `/reset-wifi` | yes | `200` `{"reset":"OK"}` | Clears saved WiFi and reboots into the captive portal. |
| GET | `/heap` | no | `200` `text/plain` | Free-heap bytes as a decimal string. |
| any | `/update` | yes* | ElegantOTA UI/API | **Present only when `info.ota === true`** (compiled out via `DISABLE_OTA` on sonoff builds). *Auth-gated only when `authEnabled`. |

### 5.4 `POST /config` semantics (read carefully)

- **Async, staged.** The handler validates size, stages the raw body, and returns
  **`202 {"status":"pending"}`** immediately. The device applies it on its next
  main-loop iteration (`Config::applyPendingUpdate`). The response does **not**
  mean "applied" — the client must **re-`GET /config`** after a short delay
  (~500 ms) to confirm the persisted result and read the new `_needReboot`.
- **Partial / section-scoped.** The body may contain any subset of top-level keys
  (`host`, `universe`, `hw`, `dmx`). Omitted keys are untouched. `hw` is merged
  per-field. The SPA always saves **one section at a time** to keep payloads
  small.
- **`dmx[]` is whole-array, index-based.** If present and non-empty, the entire
  array **replaces** the stored one. To change one entry, send the **whole**
  array. An **empty array is ignored** (no-op) — you cannot delete the last entry
  this way; instead set a device's `type` to `DISABLED`.
- **Size limits.** The staged body must be `< CONFIG_BUFFER_SIZE` (**4096** bytes
  on ESP32, **2048** on ESP8266) or the server replies
  `500 {"error":"update too large"}`. The JSON handler also enforces a **16 KB**
  content-length ceiling. Keep payloads per-section.

### 5.5 Error responses

| Status | Body | Meaning |
|---|---|---|
| `401` | (challenge) | Auth required/failed on a mutating route. |
| `404` | `Not found` | Unknown route. |
| `500` | `{"error":"update too large"}` | `POST /config` body exceeded `CONFIG_BUFFER_SIZE`. |

## 6. Config JSON schema

The `GET /config` body and the `POST /config` body share one envelope. `info` is
**output-only** (added by the API on `GET /config`; never read back or persisted).
`_needReboot` is **output-only**. All other top-level keys are read/write.

### 6.1 Envelope (top level)

| Key | Type | R/W | Default | Notes |
|---|---|---|---|---|
| `configVersion` | int | r/w | `1` | `CONFIG_SCHEMA_VERSION`. Written as the current constant; on read, missing → `1`. Migration hook only — no migrations defined yet. |
| `_needReboot` | bool | **out** | — | Server's pending-change flag (`_dirty`). Set by any applied update; **not persisted**; resets to `false` on boot. The UI uses it to show/clear its reboot banner. |
| `host` | string | r/w | board default | Device name (mDNS/NetBIOS, `<host>.local`). Practical max **31** chars (UI-enforced). |
| `universe` | int | r/w | `0` | Art-Net universe the node listens on. `≥ 0`. |
| `hw` | object | r/w | see §6.2 | Hardware/advanced settings. |
| `dmx` | array | r/w | `[]` | DMX output devices (§6.3). Whole-array, index-based, capped at `info.max_dmx_devices`. |
| `info` | object | **out** | — | Runtime identity/health (§6.5). Present in `GET /config` (nested) and as the `GET /status` body. |

### 6.2 `hw` object

| Key | Type | Default | Notes |
|---|---|---|---|
| `freq` | int | board (`DEFAULT_PWM_FREQ`, e.g. 600) | PWM frequency (Hz). |
| `ledPin` | int | board (`LED_PIN`) | Status-LED GPIO. |
| `buttonPin` | int | board (`DEFAULT_BUTTON_PIN`) | Config-button GPIO. |
| `longPressDelay` | int | board (ms) | Long-press duration that triggers WiFi reset (ms). |
| `wifiPowerSave` | bool | `false` | `false` = WiFi radio always on (lowest Art-Net latency/jitter, tightest multi-device sync); `true` = re-enable modem sleep to save power. Missing key → `false`. |
| `authEnabled` | bool | `false` | Enables HTTP Basic auth (§5.2). |
| `authUser` | string | `""` | Basic-auth username (used only if `authEnabled`). |
| `authPass` | string | `""` | Basic-auth password (used only if `authEnabled`). |

Pin/freq defaults are **board-specific** (set in `Config::Config()` from board
macros), not fixed constants.

### 6.3 `dmx[]` entry

| Key | Type | Default | Used by types | Notes |
|---|---|---|---|---|
| `channel` | int | `0` | all | Start DMX channel. UI range **1–512**. |
| `type` | enum string | `DISABLED` | — | See §6.4. |
| `pin` | int | board | BINARY, DIMMER, SERVO | Output GPIO. |
| `level` | `"HIGH"`/`"LOW"` | `"LOW"` | BINARY, DIMMER | Active level. Any value other than (case-insensitive) `"high"` reads as `LOW`. |
| `threshold` | int | `127` | BINARY | On/off threshold, `0–255`. |
| `pulse` | int | `10` | DIMMER | Strobe pulse length. |
| `multiplier` | int | `1` | DIMMER | Strobe period multiplier. |
| `blackout` | bool | `true` | all (except DISABLED in UI) | Black out this device on DMX-signal loss (~5 s). |

Fields not used by a given `type` are still stored/round-tripped; the UI simply
hides them.

### 6.4 `DmxType` (device type) wire strings

| Wire string | Meaning | Notes |
|---|---|---|
| `BINARY` | Relay (on/off via `threshold`) | **Canonical.** `RELAY` is accepted on input as an alias but always **serialized back as `BINARY`**. |
| `DIMMER` | PWM dimmer (2 channels: level + strobe) | |
| `SERVO` | DMX→servo pulse-width | |
| `REPEATER` | Art-Net→DMX512 gateway (full universe) | |
| `DISABLED` | No output | Unrecognized values map here. |

### 6.5 `info` object (output-only)

Populated fresh per request. Nested under `info` in `GET /config`; the **body** of
`GET /status` (which also adds `_needReboot`).

| Key | Type | Notes |
|---|---|---|
| `id` | string | Chip ID. |
| `chip` | string | Architecture (e.g. `ESP32`, `ESP8266`). |
| `version` | string | Firmware version. |
| `built` | string | Build timestamp. |
| `max_dmx_devices` | int | Max `dmx[]` entries: **8** (ESP32) / **4** (ESP8266). |
| `ssid` | string | Connected SSID. |
| `rssi` | int | WiFi RSSI (dBm). |
| `uptime` | int | Milliseconds since boot. |
| `free_heap` | int | Free heap bytes. |
| `ota` | bool | Whether OTA `/update` is compiled in. UI hides the OTA link when `false`. |

## 7. UI / UX specification (configuration SPA)

The SPA is served at `/` (Preact). On mount it loads `GET /config`; it then polls
`GET /status` every **5 s**. This section is detailed enough to reconstruct an
identical UX with any framework.

### 7.1 Information architecture (top → bottom)

1. **Reboot banner** — shown only while `_needReboot` is true: text *"Changes need
   a reboot to take effect."* + a **Reboot now** button (→ `POST /reboot`).
2. **Header** (compact, single row): a round badge icon on the **left**; to its
   right the title **"ArtNet Node"** and, beneath it, the device's
   **`<host>.local` as a hyperlink** to `http://<host>.local/` (a muted "—" when
   `host` is empty). No version or "Configuration" text here.
3. **Status line**: `Up <uptime> · <KB> free[ · <rssi> dBm]` (rssi omitted if 0).
4. **Tabs**: `General` · `Devices` · `System`. A tab shows a small **dot**
   when that section has unsaved edits.
5. **Active section body** (per §7.3).
6. **Footer**: three tiles — `info.id`/chip, `info.ssid`/"Network",
   `info.version`/"Firmware".
7. **Toasts** (transient messages) and **confirmation modals** overlay as needed.

**Loading state:** header + centered *"Loading…"*. **Load-error state:** header +
*"Couldn't load configuration: &lt;message&gt;"* + a **Retry** button.

### 7.2 State & save model

- **`draft`** = the editable working copy (survives tab switches). **`saved`** =
  the last-persisted baseline. A section is **dirty** when its `draft` slice
  differs from `saved` (deep compare).
- Edits call `patch(partial)` which merges into `draft` only (never auto-saves).
- Each section has its own **Save** button: it `POST`s **only that section's**
  keys, then re-`GET`s `/config`, updates `saved` + `info` + `needReboot`, and
  shows a toast. A `busy` flag disables controls during a save.
- **Save button label/state:** reads **"Save"** when dirty, **"Saved"** when
  clean; disabled when `busy` or not dirty (Devices is also disabled when the
  list is empty).
- **Save toast:** *"Saved"*, or *"Saved — reboot required"* when the result has
  `_needReboot`. Errors show the error text (e.g. *"Unauthorized — HTTP auth is
  enabled"*, *"Config update too large to save"*).

### 7.3 Sections — fields → config keys → save payload

**General** → saves `{host, universe}`

| Control | Key | Notes |
|---|---|---|
| Hostname (text, maxlen 31) | `host` | Hint: *"Reachable as `<host>.local`"* (live), or a prompt when empty. |
| Art-Net universe (number, min 0) | `universe` | |

**Devices** → saves `{dmx: [all cards]}`

- Hint: *"Up to `<max>` device(s). Saving replaces the whole list."*
  (`max = info.max_dmx_devices`).
- If the list is empty, show a warning that the API can't store an empty list —
  add a device or set one to `DISABLED` instead of removing the last.
- **+ Add device** (disabled at `max`); each card has **Remove**.
- New-device defaults: `{channel:1, type:"BINARY", pin:2, level:"HIGH",
  multiplier:1, pulse:10, threshold:127, blackout:true}`.
- **Per-card controls** (type-driven visibility):

  | Control | Key | Shown for types |
  |---|---|---|
  | Type (select) | `type` | always — options `BINARY, DIMMER, SERVO, REPEATER, DISABLED` |
  | Start channel (number 1–512) | `channel` | always |
  | GPIO pin (number ≥0) | `pin` | BINARY, DIMMER, SERVO |
  | Active level (HIGH/LOW) | `level` | BINARY, DIMMER |
  | On/off threshold (0–255) | `threshold` | BINARY |
  | Strobe pulse (≥0) | `pulse` | DIMMER |
  | Strobe multiplier (≥1) | `multiplier` | DIMMER |
  | "Blackout on DMX signal loss" (checkbox) | `blackout` | all except DISABLED |

**System** → three groups:

- **Firmware** (only when `info.ota === true`): an **"Open firmware updater"**
  link → `/update`.
- **Advanced** (collapsible `<details>`; open state persists across tab switches)
  → saves `{hw}`. Warning that wrong pins/freq can brick or lock out. Controls:

  | Control | Key | Notes |
  |---|---|---|
  | PWM frequency (number, min 100) | `hw.freq` | Hz |
  | Button long-press (number, min 500, step 500) | `hw.longPressDelay` | ms (resets WiFi) |
  | Status LED pin (number ≥0) | `hw.ledPin` | |
  | Button pin (number ≥0) | `hw.buttonPin` | |
  | "WiFi power saving (higher latency)" (checkbox) | `hw.wifiPowerSave` | Hint: off = lowest Art-Net latency / tightest sync; on = save power |
  | "Require HTTP authentication for changes" (checkbox) | `hw.authEnabled` | When checked, reveals Username/Password (`hw.authUser`/`hw.authPass`) + a warning |

- **Power & WiFi**: **Reboot** (confirm modal *"Reboot device?"* → `POST /reboot`)
  and **Reset WiFi** (danger confirm modal *"Reset WiFi?"* → `POST /reset-wifi`).

### 7.4 Behaviors & rules

- **Reboot banner auto-clears**: the 5 s `/status` poll updates `needReboot`;
  once the device reports `_needReboot:false` (after it actually reboots), the
  banner disappears without user action.
- **Destructive actions** (Reboot, Reset WiFi) always go through a confirmation
  modal first.
- **Auth**: a `401` from any mutating call surfaces as an "Unauthorized" toast.
- **OTA link visibility** is driven solely by `info.ota`.
- **Visual system**: the look is defined by the shared `style.css` (the same
  design as the captive portal) and must be reused; this contract does not
  re-specify CSS.

## 8. Source-of-truth pointers

| Concern | Files |
|---|---|
| REST endpoints, auth, `info` | `src/net/webApi.cpp` |
| Config envelope (load/save/update/stage) | `src/config.cpp`, `src/config.h` |
| Config model + JSON codec | `src/core/configModel.h`, `src/core/configCodec.h` |
| Device-type enum + wire strings | `src/core/dmxTypes.h` |
| OTA wiring (`/update`, `DISABLE_OTA`) | `src/app/app.cpp` |
| SPA | `web/src/app.tsx`, `web/src/api.ts`, `web/src/types.ts`, `web/src/components/**` |

## 9. Quick reference for client authors

1. `GET /config` once for the full state (incl. `info.max_dmx_devices`,
   `info.ota`).
2. To change settings, `POST /config` with **only the section(s)** you changed;
   for `dmx`, send the **whole array**. Expect **`202`**; then re-`GET /config`
   (~500 ms later) to confirm and read `_needReboot`.
3. Poll `GET /status` for health and to detect when a reboot has completed
   (`_needReboot` flips back to `false`).
4. `POST /reboot` / `POST /reset-wifi` for lifecycle; both reply, then act.
5. If `hw.authEnabled`, send Basic-auth on all mutating calls (handle `401`).
6. Treat unknown response fields as forward-compatible additions (ignore them).

## 10. Changelog

> Append-only, newest first. One entry per contract-version bump. Tag each
> `Compatible` or `Breaking`. See §4 for when an entry is required.

### 1.0.0 — 2026-06-16 — `Compatible` (baseline)
Initial contract. Documents the REST API and configuration SPA as of firmware
**2026.2.x**: endpoints `GET/POST /config`, `GET /status`, `POST /reboot`,
`POST /reset-wifi`, `GET /heap`, `/update`; the config envelope incl.
`hw.wifiPowerSave` and `info.ota`; `DmxType` wire strings; and the three-tab SPA
(General/Devices/System) with its save semantics and reboot-banner polling. The
config envelope carries no saved-WiFi-networks list — WiFi is configured only via
the captive portal + `POST /reset-wifi` (out of scope, see §1).
