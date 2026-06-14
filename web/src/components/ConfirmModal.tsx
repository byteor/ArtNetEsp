interface Props {
  title: string;
  body: string;
  confirmLabel: string;
  danger?: boolean;
  onConfirm: () => void;
  onCancel: () => void;
}

export function ConfirmModal({ title, body, confirmLabel, danger, onConfirm, onCancel }: Props) {
  return (
    <div class="modal-overlay" onClick={onCancel}>
      <div class="modal" onClick={(e) => e.stopPropagation()}>
        <h3>{title}</h3>
        <p>{body}</p>
        <div class="actions">
          <button class="btn secondary" onClick={onCancel}>
            Cancel
          </button>
          <button class={danger ? "btn danger" : "btn"} onClick={onConfirm}>
            {confirmLabel}
          </button>
        </div>
      </div>
    </div>
  );
}
