import type { SectionProps } from "../types";

export function GeneralSection({ draft, patch, save, busy }: SectionProps) {
  return (
    <div class="section">
      <h2>General</h2>
      <span class="hint">Device name and the Art-Net universe it listens on.</span>
      <div class="grid">
        <div class="field full">
          <label>Hostname</label>
          <input
            value={draft.host}
            maxLength={31}
            onInput={(e) => patch({ host: e.currentTarget.value })}
          />
          <span class="unit">Reachable as &lt;hostname&gt;.local</span>
        </div>
        <div class="field">
          <label>Art-Net universe</label>
          <input
            type="number"
            min={0}
            value={draft.universe}
            onInput={(e) => patch({ universe: Number(e.currentTarget.value) })}
          />
        </div>
      </div>
      <button
        class="btn"
        disabled={busy}
        onClick={() => save({ host: draft.host, universe: draft.universe })}
      >
        Save
      </button>
    </div>
  );
}
