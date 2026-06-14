import { useState, useEffect } from "preact/hooks";
import type { HardwareConfig, SectionProps } from "../types";

export function AdvancedSection({ cfg, save, busy }: SectionProps) {
  const [hw, setHw] = useState<HardwareConfig>(cfg.hw);
  useEffect(() => setHw(cfg.hw), [cfg.hw]);

  const set = <K extends keyof HardwareConfig>(k: K, v: HardwareConfig[K]) =>
    setHw({ ...hw, [k]: v });

  return (
    <details class="advanced">
      <summary>Advanced hardware &amp; security</summary>

      <div class="warn">
        Changing pin assignments, PWM frequency, or the button can stop the device
        working or lock you out. Only change these if you know your board's wiring.
      </div>

      <div class="grid">
        <div class="field">
          <label>PWM frequency</label>
          <input
            type="number"
            min={100}
            value={hw.freq}
            onInput={(e) => set("freq", Number(e.currentTarget.value))}
          />
          <span class="unit">Hz</span>
        </div>
        <div class="field">
          <label>Button long-press</label>
          <input
            type="number"
            min={500}
            step={500}
            value={hw.longPressDelay}
            onInput={(e) => set("longPressDelay", Number(e.currentTarget.value))}
          />
          <span class="unit">ms (resets WiFi)</span>
        </div>
        <div class="field">
          <label>Status LED pin</label>
          <input
            type="number"
            min={0}
            value={hw.ledPin}
            onInput={(e) => set("ledPin", Number(e.currentTarget.value))}
          />
        </div>
        <div class="field">
          <label>Button pin</label>
          <input
            type="number"
            min={0}
            value={hw.buttonPin}
            onInput={(e) => set("buttonPin", Number(e.currentTarget.value))}
          />
        </div>
      </div>

      <label class="chk" style="margin:12px 0">
        <input
          type="checkbox"
          checked={hw.authEnabled}
          onChange={(e) => set("authEnabled", e.currentTarget.checked)}
        />{" "}
        Require HTTP authentication for changes
      </label>

      {hw.authEnabled && (
        <>
          <div class="warn">
            After enabling auth and saving, the browser will ask for these credentials
            on the next change/reboot/reset.
          </div>
          <div class="grid">
            <div class="field">
              <label>Username</label>
              <input
                value={hw.authUser}
                onInput={(e) => set("authUser", e.currentTarget.value)}
              />
            </div>
            <div class="field">
              <label>Password</label>
              <input
                type="password"
                autocomplete="off"
                value={hw.authPass}
                onInput={(e) => set("authPass", e.currentTarget.value)}
              />
            </div>
          </div>
        </>
      )}

      <button class="btn" disabled={busy} onClick={() => save({ hw })}>
        Save advanced
      </button>
    </details>
  );
}
