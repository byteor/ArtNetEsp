import type { FullConfig, Info, ConfigPatch } from "./types";

const sleep = (ms: number) => new Promise((res) => setTimeout(res, ms));

async function jsonGet<T>(path: string): Promise<T> {
  const r = await fetch(path, { headers: { Accept: "application/json" } });
  if (!r.ok) throw new Error(`${path}: HTTP ${r.status}`);
  return (await r.json()) as T;
}

export const getConfig = () => jsonGet<FullConfig>("/config");
export const getStatus = () => jsonGet<Info>("/status");

/**
 * POST a section-scoped config patch. The firmware stages it (HTTP 202) and
 * applies it on its next loop() iteration, so we wait briefly and re-GET
 * /config to confirm and return the fresh config (incl. `_needReboot`).
 */
export async function saveSection(patch: ConfigPatch): Promise<FullConfig> {
  const r = await fetch("/config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(patch),
  });
  if (r.status === 401) throw new Error("Unauthorized — HTTP auth is enabled");
  if (r.status === 500) {
    const t = await r.text().catch(() => "");
    throw new Error(
      t.includes("too large") ? "Config update too large to save" : "Save failed (500)",
    );
  }
  if (!r.ok && r.status !== 202) throw new Error(`Save failed (HTTP ${r.status})`);

  await sleep(500); // let applyPendingUpdate() run on the device's main loop
  let last: unknown;
  for (let i = 0; i < 3; i++) {
    try {
      return await getConfig();
    } catch (e) {
      last = e;
      await sleep(400);
    }
  }
  throw last instanceof Error ? last : new Error("Could not re-read config after save");
}

async function post(path: string): Promise<void> {
  const r = await fetch(path, { method: "POST" });
  if (r.status === 401) throw new Error("Unauthorized — HTTP auth is enabled");
  if (!r.ok) throw new Error(`${path}: HTTP ${r.status}`);
}

export const reboot = () => post("/reboot");
export const resetWifi = () => post("/reset-wifi");
