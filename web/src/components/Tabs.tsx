interface Props {
  tabs: string[];
  active: string;
  onChange: (tab: string) => void;
}

export function Tabs({ tabs, active, onChange }: Props) {
  return (
    <div class="tabs">
      {tabs.map((t) => (
        <button
          key={t}
          class={t === active ? "tab active" : "tab"}
          onClick={() => onChange(t)}
        >
          {t}
        </button>
      ))}
    </div>
  );
}
