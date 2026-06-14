import { useState, useEffect, useCallback } from "preact/hooks";
import type { FullConfig, ConfigPatch, Info } from "./types";
import { getConfig, getStatus, saveSection, reboot, resetWifi } from "./api";
import { useToast } from "./components/Toast";
import { Header, Footer } from "./components/Header";
import { StatusLine } from "./components/StatusLine";
import { Tabs } from "./components/Tabs";
import { GeneralSection } from "./components/GeneralSection";
import { DevicesSection } from "./components/DevicesSection";
import { WifiSection } from "./components/WifiSection";
import { AdvancedSection } from "./components/AdvancedSection";
import { Actions } from "./components/Actions";

const TABS = ["General", "Devices", "WiFi", "System"];
const eq = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

export function App() {
  const notify = useToast();
  // `draft` is the editable working copy (the source of truth for the form,
  // survives tab switches). `saved` is the last-persisted baseline, used to
  // flag unsaved changes. `info`/`needReboot` are read-only runtime state.
  const [draft, setDraft] = useState<FullConfig | null>(null);
  const [saved, setSaved] = useState<FullConfig | null>(null);
  const [info, setInfo] = useState<Info | undefined>(undefined);
  const [needReboot, setNeedReboot] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [tab, setTab] = useState("General");
  const [advancedOpen, setAdvancedOpen] = useState(false);

  const load = useCallback(async () => {
    try {
      const cfg = await getConfig();
      setDraft(cfg);
      setSaved(cfg);
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

  // Light background refresh of runtime info (heap/uptime/rssi). Never touches
  // draft/saved, so it can't clobber edits; failures (e.g. mid-reboot) are
  // ignored.
  useEffect(() => {
    const id = setInterval(() => {
      getStatus()
        .then((s) => {
          setInfo(s);
          // Keep the reboot banner in sync - in particular, clear it once the
          // device has actually rebooted (_needReboot back to false).
          if (typeof s._needReboot === "boolean") setNeedReboot(s._needReboot);
        })
        .catch(() => {});
    }, 5000);
    return () => clearInterval(id);
  }, []);

  const patch = useCallback((p: Partial<FullConfig>) => {
    setDraft((d) => (d ? { ...d, ...p } : d));
  }, []);

  // Persist one section, then mark it clean (saved <- the sent values) and
  // refresh meta. Other sections' unsaved edits are untouched.
  const save = useCallback(
    async (section: ConfigPatch) => {
      setBusy(true);
      try {
        const fresh = await saveSection(section);
        setSaved((s) => (s ? { ...s, ...section } : s));
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

  if (!draft || !saved) {
    return (
      <div class="wrap">
        <Header host="" />
        <div class="center">Loading…</div>
      </div>
    );
  }

  const dirty: Record<string, boolean> = {
    General: draft.host !== saved.host || draft.universe !== saved.universe,
    Devices: !eq(draft.dmx, saved.dmx),
    WiFi: !eq(draft.wifi, saved.wifi),
    System: !eq(draft.hw, saved.hw),
  };
  const base = { draft, patch, save, busy };

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
      <StatusLine info={info} />
      <Tabs tabs={TABS} active={tab} dirty={dirty} onChange={setTab} />

      <div class="body">
        {tab === "General" && <GeneralSection {...base} dirty={dirty.General} />}
        {tab === "Devices" && <DevicesSection {...base} dirty={dirty.Devices} />}
        {tab === "WiFi" && <WifiSection {...base} dirty={dirty.WiFi} />}
        {tab === "System" && (
          <>
            <AdvancedSection
              {...base}
              dirty={dirty.System}
              open={advancedOpen}
              onToggle={setAdvancedOpen}
            />
            <Actions onReboot={() => void doReboot()} onReset={() => void doReset()} />
          </>
        )}
      </div>

      <Footer info={info} />
    </div>
  );
}
