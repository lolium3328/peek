import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";
import type { ServerWebSocket } from "bun";
import { assetManifestPath, configPath, httpsEnabled, port } from "./config";
import { lanUrls } from "./network";
import type { ClientData } from "./types";
import {
  defaultAssetManifest,
  defaultDeviceConfig,
  defaultDeviceStatus,
  normalizeAssetManifest,
  normalizeDeviceConfig,
  normalizeDeviceStatus,
  type AssetManifest,
  type AppSnapshot,
  type DeviceCommand,
  type DeviceConfig,
  type DeviceStatus,
  type ServerMessage
} from "../src/shared";

const clients = new Set<ServerWebSocket<ClientData>>();
let deviceConfig = loadDeviceConfig();
let deviceStatus = defaultDeviceStatus();
let assetManifest = loadAssetManifest();

export function addClient(ws: ServerWebSocket<ClientData>) {
  clients.add(ws);
}

export function removeClient(ws: ServerWebSocket<ClientData>) {
  const wasDevice = ws.data.role === "device";
  clients.delete(ws);

  if (wasDevice && !hasDeviceClient()) {
    updateDeviceStatus({ connected: false, updatedAt: Date.now() });
    return;
  }

  broadcastSnapshot();
}

export function snapshot(): AppSnapshot {
  return {
    type: "snapshot",
    serverTime: Date.now(),
    config: deviceConfig,
    status: deviceStatus,
    assets: assetManifest,
    lanUrls: lanUrls(port, httpsEnabled ? "https" : "http")
  };
}

export function currentConfig() {
  return deviceConfig;
}

export function currentStatus() {
  return deviceStatus;
}

export function currentAssetManifest() {
  return assetManifest;
}

export function patchDeviceConfig(patch: Partial<DeviceConfig>) {
  deviceConfig = normalizeDeviceConfig(patch, deviceConfig);
  saveDeviceConfig(deviceConfig);
  broadcastSnapshot();
  broadcastToDevices({ type: "config", config: deviceConfig });
  return deviceConfig;
}

export function resetDeviceConfig() {
  deviceConfig = defaultDeviceConfig();
  saveDeviceConfig(deviceConfig);
  broadcastSnapshot();
  broadcastToDevices({ type: "config", config: deviceConfig });
  return deviceConfig;
}

export function replaceAssetManifest(manifest: Partial<AssetManifest>) {
  assetManifest = normalizeAssetManifest(
    {
      ...manifest,
      revision: Date.now()
    },
    assetManifest
  );
  saveAssetManifest(assetManifest);
  broadcastSnapshot();
  broadcastToDevices({ type: "assets", assets: assetManifest });
  return assetManifest;
}

export function upsertAsset(asset: AssetManifest["assets"][number]) {
  const assets = assetManifest.assets.filter((item) => item.id !== asset.id);
  assets.push(asset);
  return replaceAssetManifest({ assets });
}

export function setPet2Asset(assetId: string | null) {
  if (assetId !== null && !assetManifest.assets.some((asset) => asset.id === assetId)) {
    return assetManifest;
  }

  return replaceAssetManifest({ pet2AssetId: assetId });
}

export function deleteAsset(assetId: string) {
  const before = assetManifest.assets.length;
  const assets = assetManifest.assets.filter((item) => item.id !== assetId);
  if (assets.length === before) {
    return null;
  }

  return replaceAssetManifest({
    assets,
    pet2AssetId: assetManifest.pet2AssetId === assetId ? null : assetManifest.pet2AssetId
  });
}

export function updateDeviceStatus(patch: Partial<DeviceStatus>) {
  deviceStatus = normalizeDeviceStatus(patch, deviceStatus);
  broadcastSnapshot();
  return deviceStatus;
}

export function markDeviceConnected(patch: Partial<DeviceStatus> = {}) {
  return updateDeviceStatus({
    ...patch,
    connected: true,
    updatedAt: Date.now()
  });
}

export function deviceSyncPayload(statusPatch: Partial<DeviceStatus>) {
  updateDeviceStatus({
    ...statusPatch,
    connected: true,
    updatedAt: Date.now()
  });

  return {
    serverTime: Date.now(),
    config: deviceConfig,
    assets: assetManifest
  };
}

export function sendDeviceCommand(command: DeviceCommand) {
  let delivered = 0;
  const payload = JSON.stringify({ type: "device.command", command } satisfies ServerMessage);

  for (const client of clients) {
    if (client.data.role === "device") {
      client.send(payload);
      delivered += 1;
    }
  }

  return delivered;
}

export function broadcastSnapshot() {
  broadcast(snapshot());
}

export function sendSnapshot(ws: ServerWebSocket<ClientData>) {
  ws.send(JSON.stringify(snapshot()));
}

function broadcast(message: ServerMessage) {
  const payload = JSON.stringify(message);
  for (const client of clients) {
    client.send(payload);
  }
}

function broadcastToDevices(message: ServerMessage) {
  const payload = JSON.stringify(message);
  for (const client of clients) {
    if (client.data.role === "device") {
      client.send(payload);
    }
  }
}

function hasDeviceClient() {
  for (const client of clients) {
    if (client.data.role === "device") {
      return true;
    }
  }
  return false;
}

function loadDeviceConfig() {
  if (!existsSync(configPath)) {
    return defaultDeviceConfig();
  }

  try {
    const raw = JSON.parse(readFileSync(configPath, "utf8")) as Partial<DeviceConfig>;
    return normalizeDeviceConfig(raw);
  } catch {
    return defaultDeviceConfig();
  }
}

function saveDeviceConfig(config: DeviceConfig) {
  mkdirSync(dirname(configPath), { recursive: true });
  writeFileSync(configPath, `${JSON.stringify(config, null, 2)}\n`, "utf8");
}

function loadAssetManifest() {
  if (!existsSync(assetManifestPath)) {
    return defaultAssetManifest();
  }

  try {
    const raw = JSON.parse(readFileSync(assetManifestPath, "utf8")) as Partial<AssetManifest>;
    return normalizeAssetManifest(raw);
  } catch {
    return defaultAssetManifest();
  }
}

function saveAssetManifest(manifest: AssetManifest) {
  mkdirSync(dirname(assetManifestPath), { recursive: true });
  writeFileSync(assetManifestPath, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
}
