interface Props {
  tabs: string[];
  active: string;
  dirty?: Record<string, boolean>;
  onChange: (tab: string) => void;
}

export function Tabs({ tabs, active, dirty, onChange }: Props) {
  return (
    <div class="tabs">
      {tabs.map((t) => (
        <button
          key={t}
          class={t === active ? "tab active" : "tab"}
          onClick={() => onChange(t)}
        >
          {t}
          {dirty?.[t] && <span class="dot" title="Unsaved changes" />}
        </button>
      ))}
    </div>
  );
}
