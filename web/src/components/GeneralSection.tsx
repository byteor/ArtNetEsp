import { useState, useEffect } from "preact/hooks";
import type { SectionProps } from "../types";

export function GeneralSection({ cfg, save, busy }: SectionProps) {
  const [host, setHost] = useState(cfg.host);
  const [universe, setUniverse] = useState(cfg.universe);

  useEffect(() => {
    setHost(cfg.host);
    setUniverse(cfg.universe);
  }, [cfg.host, cfg.universe]);

  return (
    <div class="section">
      <h2>General</h2>
      <span class="hint">Device name and the Art-Net universe it listens on.</span>
      <div class="grid">
        <div class="field full">
          <label>Hostname</label>
          <input
            value={host}
            maxLength={31}
            onInput={(e) => setHost(e.currentTarget.value)}
          />
          <span class="unit">Reachable as &lt;hostname&gt;.local</span>
        </div>
        <div class="field">
          <label>Art-Net universe</label>
          <input
            type="number"
            min={0}
            value={universe}
            onInput={(e) => setUniverse(Number(e.currentTarget.value))}
          />
        </div>
      </div>
      <button class="btn" disabled={busy} onClick={() => save({ host, universe })}>
        Save
      </button>
    </div>
  );
}
