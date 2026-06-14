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
  cfg: FullConfig;
  save: Save;
  busy: boolean;
}
