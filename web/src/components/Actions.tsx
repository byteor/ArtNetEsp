import { useState } from "preact/hooks";
import { ConfirmModal } from "./ConfirmModal";

interface Props {
  onReboot: () => void;
  onReset: () => void;
  // Whether ElegantOTA is compiled in (info.ota). When false/undefined the
  // /update endpoint doesn't exist, so we hide the link.
  ota?: boolean;
}

export function Actions({ onReboot, onReset, ota }: Props) {
  const [confirm, setConfirm] = useState<null | "reboot" | "reset">(null);

  return (
    <>
      {ota && (
        <div class="section">
          <h2>Firmware</h2>
          <span class="hint">
            Upload a new firmware or filesystem image over WiFi (OTA).
          </span>
          <div class="actions">
            <a class="btn secondary" href="/update">
              Open firmware updater
            </a>
          </div>
        </div>
      )}

      <div class="section">
        <h2>Power &amp; WiFi</h2>
        <span class="hint">Restart the device or clear its WiFi credentials.</span>
        <div class="actions">
          <button class="btn secondary" onClick={() => setConfirm("reboot")}>
            Reboot
          </button>
          <button class="btn danger" onClick={() => setConfirm("reset")}>
            Reset WiFi
          </button>
        </div>
      </div>

      {confirm === "reboot" && (
        <ConfirmModal
          title="Reboot device?"
          body="The device will restart and be unreachable for a few seconds."
          confirmLabel="Reboot"
          onConfirm={() => {
            setConfirm(null);
            onReboot();
          }}
          onCancel={() => setConfirm(null)}
        />
      )}
      {confirm === "reset" && (
        <ConfirmModal
          title="Reset WiFi?"
          danger
          body="The device will forget its WiFi and reboot into the setup portal. You'll need to reconnect it to your network."
          confirmLabel="Reset WiFi"
          onConfirm={() => {
            setConfirm(null);
            onReset();
          }}
          onCancel={() => setConfirm(null)}
        />
      )}
    </>
  );
}
