import { useState } from "preact/hooks";
import { ConfirmModal } from "./ConfirmModal";

interface Props {
  onReboot: () => void;
  onReset: () => void;
}

export function Actions({ onReboot, onReset }: Props) {
  const [confirm, setConfirm] = useState<null | "reboot" | "reset">(null);

  return (
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
    </div>
  );
}
