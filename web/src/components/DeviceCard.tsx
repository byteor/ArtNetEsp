import type { DeviceConfig, DmxType } from "../types";

const TYPES: DmxType[] = ["BINARY", "DIMMER", "SERVO", "REPEATER", "DISABLED"];

interface Props {
  index: number;
  device: DeviceConfig;
  onChange: (d: DeviceConfig) => void;
  onRemove: () => void;
}

export function DeviceCard({ index, device, onChange, onRemove }: Props) {
  const set = <K extends keyof DeviceConfig>(k: K, v: DeviceConfig[K]) =>
    onChange({ ...device, [k]: v });
  const t = device.type;
  const hasPin = t === "BINARY" || t === "DIMMER" || t === "SERVO";
  const hasLevel = t === "BINARY" || t === "DIMMER";

  return (
    <div class="card">
      <div class="card-head">
        <b>Device {index + 1}</b>
        <button class="iconbtn danger" type="button" onClick={onRemove}>
          Remove
        </button>
      </div>
      <div class="grid">
        <div class="field">
          <label>Type</label>
          <select value={t} onChange={(e) => set("type", e.currentTarget.value as DmxType)}>
            {TYPES.map((x) => (
              <option key={x} value={x}>
                {x}
              </option>
            ))}
          </select>
        </div>
        <div class="field">
          <label>Start channel</label>
          <input
            type="number"
            min={1}
            max={512}
            value={device.channel}
            onInput={(e) => set("channel", Number(e.currentTarget.value))}
          />
        </div>

        {hasPin && (
          <div class="field">
            <label>GPIO pin</label>
            <input
              type="number"
              min={0}
              value={device.pin}
              onInput={(e) => set("pin", Number(e.currentTarget.value))}
            />
          </div>
        )}
        {hasLevel && (
          <div class="field">
            <label>Active level</label>
            <select
              value={device.level}
              onChange={(e) => set("level", e.currentTarget.value as "HIGH" | "LOW")}
            >
              <option value="HIGH">HIGH</option>
              <option value="LOW">LOW</option>
            </select>
          </div>
        )}
        {t === "BINARY" && (
          <div class="field">
            <label>On/off threshold</label>
            <input
              type="number"
              min={0}
              max={255}
              value={device.threshold}
              onInput={(e) => set("threshold", Number(e.currentTarget.value))}
            />
          </div>
        )}
        {t === "DIMMER" && (
          <>
            <div class="field">
              <label>Strobe pulse</label>
              <input
                type="number"
                min={0}
                value={device.pulse}
                onInput={(e) => set("pulse", Number(e.currentTarget.value))}
              />
            </div>
            <div class="field">
              <label>Strobe multiplier</label>
              <input
                type="number"
                min={1}
                value={device.multiplier}
                onInput={(e) => set("multiplier", Number(e.currentTarget.value))}
              />
            </div>
          </>
        )}
      </div>

      {t !== "DISABLED" && (
        <label class="chk" style="margin-top:10px">
          <input
            type="checkbox"
            checked={device.blackout}
            onChange={(e) => set("blackout", e.currentTarget.checked)}
          />{" "}
          Blackout on DMX signal loss
        </label>
      )}
    </div>
  );
}
