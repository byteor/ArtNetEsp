import { useState, useEffect, useCallback } from "preact/hooks";
import type { FullConfig, ConfigPatch, Info } from "./types";
import { getConfig, saveSection, reboot, resetWifi } from "./api";
import { useToast } from "./components/Toast";
import { Header, Footer } from "./components/Header";
import { Tabs } from "./components/Tabs";
import { GeneralSection } from "./components/GeneralSection";
import { DevicesSection } from "./components/DevicesSection";
import { WifiSection } from "./components/WifiSection";
import { AdvancedSection } from "./components/AdvancedSection";
import { Actions } from "./components/Actions";

const TABS = ["General", "Devices", "WiFi", "System"];

export function App() {
  const notify = useToast();
  // `draft` is the editable working copy and the single source of truth for the
  // form fields - it lives here (not in each section) so unsaved edits survive
  // tab switches. `info`/`needReboot` are read-only runtime state, refreshed
  // from the server on load and after each save.
  const [draft, setDraft] = useState<FullConfig | null>(null);
  const [info, setInfo] = useState<Info | undefined>(undefined);
  const [needReboot, setNeedReboot] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [tab, setTab] = useState("General");

  const load = useCallback(async () => {
    try {
      const cfg = await getConfig();
      setDraft(cfg);
      setInfo(cfg.info);
      setNeedReboot(cfg._needReboot);
      setLoadError(null);
    } catch (e) {
      setLoadError((e as Error).message);
    }
  }, []);

  useEffect(() => {
    void load();
  }, [load]);

  // Merge a partial edit into the working copy (does not touch the server).
  const patch = useCallback((p: Partial<FullConfig>) => {
    setDraft((d) => (d ? { ...d, ...p } : d));
  }, []);

  // Persist one section. Only refreshes meta (info/_needReboot) from the
  // response - the draft keeps the user's edits (incl. other unsaved sections).
  const save = useCallback(
    async (section: ConfigPatch) => {
      setBusy(true);
      try {
        const fresh = await saveSection(section);
        setInfo(fresh.info);
        setNeedReboot(fresh._needReboot);
        notify(fresh._needReboot ? "Saved — reboot required" : "Saved");
      } catch (e) {
        notify((e as Error).message, true);
      } finally {
        setBusy(false);
      }
    },
    [notify],
  );

  const doReboot = useCallback(async () => {
    try {
      await reboot();
      notify("Rebooting…");
    } catch (e) {
      notify((e as Error).message, true);
    }
  }, [notify]);

  const doReset = useCallback(async () => {
    try {
      await resetWifi();
      notify("Resetting WiFi — entering setup portal");
    } catch (e) {
      notify((e as Error).message, true);
    }
  }, [notify]);

  if (loadError) {
    return (
      <div class="wrap">
        <Header host="" />
        <div class="body">
          <div class="warn">Couldn't load configuration: {loadError}</div>
          <button class="btn" onClick={() => void load()}>
            Retry
          </button>
        </div>
      </div>
    );
  }

  if (!draft) {
    return (
      <div class="wrap">
        <Header host="" />
        <div class="center">Loading…</div>
      </div>
    );
  }

  const sectionProps = { draft, patch, save, busy };

  return (
    <div class="wrap">
      {needReboot && (
        <div class="banner">
          <span>Changes need a reboot to take effect.</span>
          <button class="btn" onClick={() => void doReboot()}>
            Reboot now
          </button>
        </div>
      )}

      <Header host={draft.host} version={info?.version} />
      <Tabs tabs={TABS} active={tab} onChange={setTab} />

      <div class="body">
        {tab === "General" && <GeneralSection {...sectionProps} />}
        {tab === "Devices" && <DevicesSection {...sectionProps} />}
        {tab === "WiFi" && <WifiSection {...sectionProps} />}
        {tab === "System" && (
          <>
            <AdvancedSection {...sectionProps} />
            <Actions onReboot={() => void doReboot()} onReset={() => void doReset()} />
          </>
        )}
      </div>

      <Footer info={info} />
    </div>
  );
}
