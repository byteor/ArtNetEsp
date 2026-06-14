import { useState, useEffect } from "preact/hooks";
import type { DeviceConfig, SectionProps } from "../types";
import { DeviceCard } from "./DeviceCard";

const DEFAULT_DEVICE: DeviceConfig = {
  channel: 1,
  type: "BINARY",
  pin: 2,
  level: "HIGH",
  multiplier: 1,
  pulse: 10,
  threshold: 127,
  blackout: true,
};

export function DevicesSection({ cfg, save, busy }: SectionProps) {
  const [devices, setDevices] = useState<DeviceConfig[]>(cfg.dmx);
  useEffect(() => setDevices(cfg.dmx), [cfg.dmx]);

  const max = cfg.info?.max_dmx_devices ?? 4;
  const update = (i: number, d: DeviceConfig) =>
    setDevices((arr) => arr.map((x, j) => (j === i ? d : x)));
  const remove = (i: number) => setDevices((arr) => arr.filter((_, j) => j !== i));
  const add = () => setDevices((arr) => [...arr, { ...DEFAULT_DEVICE }]);

  return (
    <div class="section">
      <h2>DMX Devices</h2>
      <span class="hint">
        Up to {max} device{max === 1 ? "" : "s"}. Saving replaces the whole list.
      </span>

      {devices.length === 0 && (
        <div class="warn">
          At least one device is required — the API can't store an empty list. Add a
          device, or set one to <b>DISABLED</b> instead of removing it.
        </div>
      )}

      {devices.map((d, i) => (
        <DeviceCard
          key={i}
          index={i}
          device={d}
          onChange={(nd) => update(i, nd)}
          onRemove={() => remove(i)}
        />
      ))}

      <div class="actions" style="margin:6px 0 16px">
        <button
          class="btn ghost sm"
          type="button"
          disabled={devices.length >= max}
          onClick={add}
        >
          + Add device
        </button>
      </div>

      <button
        class="btn"
        disabled={busy || devices.length === 0}
        onClick={() => save({ dmx: devices })}
      >
        Save devices
      </button>
    </div>
  );
}
