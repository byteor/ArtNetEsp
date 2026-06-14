import { useState, useEffect, useCallback } from "preact/hooks";
import type { FullConfig, ConfigPatch } from "./types";
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
  const [cfg, setCfg] = useState<FullConfig | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [tab, setTab] = useState("General");

  const load = useCallback(async () => {
    try {
      setCfg(await getConfig());
      setLoadError(null);
    } catch (e) {
      setLoadError((e as Error).message);
    }
  }, []);

  useEffect(() => {
    void load();
  }, [load]);

  const save = useCallback(
    async (patch: ConfigPatch) => {
      setBusy(true);
      try {
        const fresh = await saveSection(patch);
        setCfg(fresh);
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

  if (!cfg) {
    return (
      <div class="wrap">
        <Header host="" />
        <div class="center">Loading…</div>
      </div>
    );
  }

  const sectionProps = { cfg, save, busy };

  return (
    <div class="wrap">
      {cfg._needReboot && (
        <div class="banner">
          <span>Changes need a reboot to take effect.</span>
          <button class="btn" onClick={() => void doReboot()}>
            Reboot now
          </button>
        </div>
      )}

      <Header host={cfg.host} version={cfg.info?.version} />
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

      <Footer info={cfg.info} />
    </div>
  );
}
