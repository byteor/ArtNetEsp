import type { WifiNet, SectionProps } from "../types";

export function WifiSection({ draft, patch, save, busy }: SectionProps) {
  const nets = draft.wifi;

  const setNets = (n: WifiNet[]) => patch({ wifi: n });
  const update = (i: number, n: WifiNet) =>
    setNets(nets.map((x, j) => (j === i ? n : x)));
  const remove = (i: number) => setNets(nets.filter((_, j) => j !== i));
  const add = () =>
    setNets([...nets, { ssid: "", pass: "", dhcp: true, order: nets.length + 1 }]);

  return (
    <div class="section">
      <h2>WiFi Networks</h2>
      <span class="hint">
        Saved networks stored in the config. The active connection is set through the
        captive portal (Reset WiFi).
      </span>

      {nets.map((n, i) => (
        <div class="card" key={i}>
          <div class="card-head">
            <b>Network {i + 1}</b>
            <button class="iconbtn danger" type="button" onClick={() => remove(i)}>
              Remove
            </button>
          </div>
          <div class="grid">
            <div class="field full">
              <label>SSID</label>
              <input
                value={n.ssid}
                onInput={(e) => update(i, { ...n, ssid: e.currentTarget.value })}
              />
            </div>
            <div class="field full">
              <label>Password</label>
              <input
                type="password"
                autocomplete="off"
                value={n.pass}
                onInput={(e) => update(i, { ...n, pass: e.currentTarget.value })}
              />
            </div>
          </div>
        </div>
      ))}

      <div class="actions" style="margin:6px 0 16px">
        <button class="btn ghost sm" type="button" onClick={add}>
          + Add network
        </button>
      </div>

      <button
        class="btn"
        disabled={busy || nets.length === 0}
        onClick={() => save({ wifi: nets })}
      >
        Save networks
      </button>
    </div>
  );
}
