// Mirrors the firmware config schema (src/config.{h,cpp}, src/core/configModel.h,
// src/core/configCodec.h) and the GET /config / GET /status responses.

export type DmxType = "BINARY" | "DIMMER" | "SERVO" | "REPEATER" | "DISABLED";

export interface Info {
  id: string;
  chip: string;
  version: string;
  built: string;
  max_dmx_devices: number;
  ssid: string;
  rssi: number;
  uptime: number;
  free_heap: number;
  // GET /status also reports the pending-reboot flag (optional for older fw).
  _needReboot?: boolean;
  // True when ElegantOTA's /update endpoint is compiled in (false on sonoff
  // boards built with DISABLE_OTA). Optional for older fw that didn't report it.
  ota?: boolean;
}

export interface DeviceConfig {
  channel: number;
  type: DmxType;
  pin: number;
  level: "HIGH" | "LOW";
  multiplier: number;
  pulse: number;
  threshold: number;
  blackout: boolean;
}

export interface WifiNet {
  ssid: string;
  pass: string;
  dhcp?: boolean;
  order?: number;
}

export interface HardwareConfig {
  freq: number;
  ledPin: number;
  buttonPin: number;
  longPressDelay: number;
  // false (default) = WiFi radio always on, for lowest Art-Net latency and
  // tightest multi-device sync; true = re-enable modem sleep to save power.
  wifiPowerSave: boolean;
  authEnabled: boolean;
  authUser: string;
  authPass: string;
}

export interface FullConfig {
  configVersion: number;
  _needReboot: boolean;
  hw: HardwareConfig;
  host: string;
  universe: number;
  wifi: WifiNet[];
  dmx: DeviceConfig[];
  info: Info;
}

// A section-scoped POST /config body (the firmware merges partial updates).
export type ConfigPatch = Partial<
  Pick<FullConfig, "host" | "universe" | "wifi" | "dmx" | "hw">
>;

export type Save = (patch: ConfigPatch) => Promise<void>;

export interface SectionProps {
  // The editable working copy lives in App (so unsaved edits survive tab
  // switches); sections read from `draft` and mutate it via `patch`.
  draft: FullConfig;
  patch: (p: Partial<FullConfig>) => void;
  save: Save;
  busy: boolean;
  // True when this section's draft differs from the last-saved baseline.
  dirty: boolean;
}
