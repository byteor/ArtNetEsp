import type { Info } from "../types";

export function Header({ host }: { host: string }) {
  return (
    <div class="head">
      <div class="badge">
        <svg width="30" height="30" viewBox="0 0 24 24" fill="#fff">
          <circle cx="12" cy="6.5" r="1.6" />
          <circle cx="7.5" cy="12.5" r="1.6" />
          <circle cx="16.5" cy="12.5" r="1.6" />
          <circle cx="12" cy="17.5" r="1.6" />
          <circle cx="12" cy="12" r="7.5" fill="none" stroke="#fff" stroke-width="1.6" />
        </svg>
      </div>
      <div class="head-text">
        <h1>ArtNet Node</h1>
        {host ? (
          <a class="head-link" href={`http://${host}.local/`}>
            {host}.local
          </a>
        ) : (
          <span class="head-link muted">—</span>
        )}
      </div>
    </div>
  );
}

export function Footer({ info }: { info?: Info }) {
  return (
    <div class="foot">
      <div>
        <div class="ic">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="7" y="3" width="10" height="18" rx="2" />
            <path d="M11 18h2" />
          </svg>
        </div>
        <b>{info?.id || "—"}</b>
        <span>{info?.chip || "Device"}</span>
      </div>
      <div>
        <div class="ic">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="9" />
            <path d="M3 12h18M12 3a14 14 0 0 1 0 18 14 14 0 0 1 0-18z" />
          </svg>
        </div>
        <b>{info?.ssid || "—"}</b>
        <span>Network</span>
      </div>
      <div>
        <div class="ic">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="7" y="7" width="10" height="10" rx="1.5" />
            <path d="M10 3v3M14 3v3M10 18v3M14 18v3M3 10h3M3 14h3M18 10h3M18 14h3" />
          </svg>
        </div>
        <b>{info?.version || "—"}</b>
        <span>Firmware</span>
      </div>
    </div>
  );
}
