import type { Info } from "../types";

function fmtUptime(ms: number): string {
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  if (d) return `${d}d ${h}h`;
  if (h) return `${h}h ${m}m`;
  if (m) return `${m}m ${sec}s`;
  return `${sec}s`;
}

export function StatusLine({ info }: { info?: Info }) {
  if (!info) return null;
  return (
    <div class="statusline">
      Up {fmtUptime(info.uptime)} · {Math.round(info.free_heap / 1024)} KB free
      {info.rssi ? ` · ${info.rssi} dBm` : ""}
    </div>
  );
}
