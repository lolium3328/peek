export type DeviceSlot = "A" | "B";

export interface DeviceConfig {
  deviceName: string;
  deviceId: string;
  deviceToken: string;
  pairSlot: DeviceSlot;
  weatherCity: string;
  wifiSsid: string;
  wifiPassword: string;
  backendUrl: string;
  backendPollIntervalMs: number;
  sleepStartMinutes: number;
  sleepEndMinutes: number;
  touchIdleThreshold: number;
  touchPressThreshold: number;
  touchSampleIntervalMs: number;
  longPressMs: number;
  sleepTimeoutMs: number;
  screenBrightness: number;
  motorStrength: number;
  imuEnabled: boolean;
  motorEnabled: boolean;
}

export interface DeviceImuStatus {
  pitch: number | null;
  roll: number | null;
  yaw: number | null;
}

export interface DeviceStatus {
  connected: boolean;
  firmwareVersion: string | null;
  ipAddress: string | null;
  wifiRssi: number | null;
  batteryPercent: number | null;
  charging: boolean | null;
  state: string;
  touchAnalog: number | null;
  lastEvent: string | null;
  imu: DeviceImuStatus;
  storage: {
    totalBytes: number | null;
    usedBytes: number | null;
    freeBytes: number | null;
  };
  motorActive: boolean;
  updatedAt: number | null;
}

export type PetAssetKind = "sprite" | "image" | "package";

export interface PetAsset {
  id: string;
  name: string;
  kind: PetAssetKind;
  format: string;
  width: number;
  height: number;
  sourceWidth?: number;
  sourceHeight?: number;
  deviceWidth?: number;
  deviceHeight?: number;
  frames: number;
  fps: number;
  durationMs?: number;
  frameDelaysMs?: number[];
  size: number;
  encodedSize?: number;
  path: string;
  sourcePath?: string;
  devicePath?: string;
  deviceFormat?: "pka-rgb565-rle";
  createdAt: number;
  updatedAt: number;
}

export interface AssetManifest {
  version: number;
  revision: number;
  pet2AssetId: string | null;
  assets: PetAsset[];
}

export interface AppSnapshot {
  type: "snapshot";
  serverTime: number;
  config: DeviceConfig;
  status: DeviceStatus;
  assets: AssetManifest;
  lanUrls: string[];
}

export interface NoticeMessage {
  type: "notice";
  message: string;
}

export interface ConfigMessage {
  type: "config";
  config: DeviceConfig;
}

export interface AssetManifestMessage {
  type: "assets";
  assets: AssetManifest;
}

export interface DeviceCommand {
  kind: "motor.pulse" | "display.refresh" | "config.apply";
  value?: number;
  at: number;
}

export interface DeviceCommandMessage {
  type: "device.command";
  command: DeviceCommand;
}

export type ServerMessage =
  | AppSnapshot
  | NoticeMessage
  | ConfigMessage
  | AssetManifestMessage
  | DeviceCommandMessage;

export type ClientMessage =
  | {
      type: "browser.hello";
    }
  | {
      type: "config.patch";
      patch: Partial<DeviceConfig>;
    }
  | {
      type: "device.hello";
      status?: Partial<DeviceStatus>;
    }
  | {
      type: "device.status";
      status: Partial<DeviceStatus>;
    };

export const deviceSlots: DeviceSlot[] = ["A", "B"];

export const defaultDeviceConfig = (): DeviceConfig => ({
  deviceName: "Peek",
  deviceId: "peek-dev",
  deviceToken: "",
  pairSlot: "A",
  weatherCity: "Shanghai",
  wifiSsid: "",
  wifiPassword: "",
  backendUrl: "",
  backendPollIntervalMs: 5000,
  sleepStartMinutes: 23 * 60,
  sleepEndMinutes: 7 * 60,
  touchIdleThreshold: 3980,
  touchPressThreshold: 3300,
  touchSampleIntervalMs: 50,
  longPressMs: 2000,
  sleepTimeoutMs: 120000,
  screenBrightness: 80,
  motorStrength: 45,
  imuEnabled: true,
  motorEnabled: true
});

export const defaultDeviceStatus = (): DeviceStatus => ({
  connected: false,
  firmwareVersion: null,
  ipAddress: null,
  wifiRssi: null,
  batteryPercent: null,
  charging: null,
  state: "offline",
  touchAnalog: null,
  lastEvent: null,
  imu: {
    pitch: null,
    roll: null,
    yaw: null
  },
  storage: {
    totalBytes: null,
    usedBytes: null,
    freeBytes: null
  },
  motorActive: false,
  updatedAt: null
});

export const defaultAssetManifest = (): AssetManifest => ({
  version: 1,
  revision: 0,
  pet2AssetId: null,
  assets: []
});

export const otherSlot = (slot: DeviceSlot): DeviceSlot => (slot === "A" ? "B" : "A");

export const isDeviceSlot = (value: string): value is DeviceSlot =>
  deviceSlots.includes(value as DeviceSlot);

export const clamp = (value: number, min: number, max: number) =>
  Math.min(Math.max(value, min), max);

export function normalizeDeviceConfig(
  patch: Partial<DeviceConfig>,
  base: DeviceConfig = defaultDeviceConfig()
): DeviceConfig {
  const idleThreshold = intValue(patch.touchIdleThreshold, base.touchIdleThreshold, 0, 4095);
  const pressThreshold = Math.min(
    intValue(patch.touchPressThreshold, base.touchPressThreshold, 0, 4095),
    idleThreshold
  );
  const pairSlot = String(patch.pairSlot ?? base.pairSlot);

  return {
    deviceName: textValue(patch.deviceName, base.deviceName, "Peek"),
    deviceId: textValue(patch.deviceId, base.deviceId, "peek-dev"),
    deviceToken: textValue(patch.deviceToken, base.deviceToken, ""),
    pairSlot: isDeviceSlot(pairSlot) ? pairSlot : base.pairSlot,
    weatherCity: textValue(patch.weatherCity, base.weatherCity, "Shanghai"),
    wifiSsid: textValue(patch.wifiSsid, base.wifiSsid, ""),
    wifiPassword: textValue(patch.wifiPassword, base.wifiPassword, ""),
    backendUrl: textValue(patch.backendUrl, base.backendUrl, ""),
    backendPollIntervalMs: intValue(patch.backendPollIntervalMs, base.backendPollIntervalMs, 1000, 600000),
    sleepStartMinutes: intValue(patch.sleepStartMinutes, base.sleepStartMinutes, 0, 1439),
    sleepEndMinutes: intValue(patch.sleepEndMinutes, base.sleepEndMinutes, 0, 1439),
    touchIdleThreshold: idleThreshold,
    touchPressThreshold: pressThreshold,
    touchSampleIntervalMs: intValue(patch.touchSampleIntervalMs, base.touchSampleIntervalMs, 10, 1000),
    longPressMs: intValue(patch.longPressMs, base.longPressMs, 300, 10000),
    sleepTimeoutMs: intValue(patch.sleepTimeoutMs, base.sleepTimeoutMs, 5000, 900000),
    screenBrightness: intValue(patch.screenBrightness, base.screenBrightness, 1, 100),
    motorStrength: intValue(patch.motorStrength, base.motorStrength, 0, 100),
    imuEnabled: boolValue(patch.imuEnabled, base.imuEnabled),
    motorEnabled: boolValue(patch.motorEnabled, base.motorEnabled)
  };
}

export function normalizeAssetManifest(
  patch: Partial<AssetManifest>,
  base: AssetManifest = defaultAssetManifest()
): AssetManifest {
  const assets = Array.isArray(patch.assets)
    ? patch.assets.map(normalizePetAsset).filter((asset): asset is PetAsset => asset !== null)
    : base.assets;
  const pet2AssetId =
    typeof patch.pet2AssetId === "string" && assets.some((asset) => asset.id === patch.pet2AssetId)
      ? patch.pet2AssetId
      : patch.pet2AssetId === null
        ? null
        : assets.some((asset) => asset.id === base.pet2AssetId)
          ? base.pet2AssetId
          : null;

  return {
    version: 1,
    revision: intValue(patch.revision, base.revision, 0, Number.MAX_SAFE_INTEGER),
    pet2AssetId,
    assets
  };
}

export function normalizeDeviceStatus(
  patch: Partial<DeviceStatus>,
  base: DeviceStatus = defaultDeviceStatus()
): DeviceStatus {
  return {
    connected: boolValue(patch.connected, base.connected),
    firmwareVersion: nullableText(patch.firmwareVersion, base.firmwareVersion),
    ipAddress: nullableText(patch.ipAddress, base.ipAddress),
    wifiRssi: nullableNumber(patch.wifiRssi, base.wifiRssi, -120, 0),
    batteryPercent: nullableNumber(patch.batteryPercent, base.batteryPercent, 0, 100),
    charging: typeof patch.charging === "boolean" ? patch.charging : base.charging,
    state: textValue(patch.state, base.state, "offline"),
    touchAnalog: nullableNumber(patch.touchAnalog, base.touchAnalog, 0, 4095),
    lastEvent: nullableText(patch.lastEvent, base.lastEvent),
    imu: {
      pitch: nullableNumber(patch.imu?.pitch, base.imu.pitch, -180, 180),
      roll: nullableNumber(patch.imu?.roll, base.imu.roll, -180, 180),
      yaw: nullableNumber(patch.imu?.yaw, base.imu.yaw, -180, 180)
    },
    storage: {
      totalBytes: nullableNumber(patch.storage?.totalBytes, base.storage.totalBytes, 0, Number.MAX_SAFE_INTEGER),
      usedBytes: nullableNumber(patch.storage?.usedBytes, base.storage.usedBytes, 0, Number.MAX_SAFE_INTEGER),
      freeBytes: nullableNumber(patch.storage?.freeBytes, base.storage.freeBytes, 0, Number.MAX_SAFE_INTEGER)
    },
    motorActive: boolValue(patch.motorActive, base.motorActive),
    updatedAt: nullableNumber(patch.updatedAt, base.updatedAt, 0, Number.MAX_SAFE_INTEGER)
  };
}

export function normalizeDeviceCommand(value: unknown): DeviceCommand {
  const body = value && typeof value === "object" ? (value as Record<string, unknown>) : {};
  const kind = body.kind === "display.refresh" || body.kind === "config.apply" ? body.kind : "motor.pulse";
  const valueNumber = Number(body.value);

  return {
    kind,
    value: Number.isFinite(valueNumber) ? clamp(valueNumber, 0, 100) : undefined,
    at: Date.now()
  };
}

export function isClientMessage(value: unknown): value is ClientMessage {
  if (!value || typeof value !== "object" || !("type" in value)) {
    return false;
  }

  return [
    "browser.hello",
    "config.patch",
    "device.hello",
    "device.status"
  ].includes(String((value as { type: unknown }).type));
}

function normalizePetAsset(value: unknown): PetAsset | null {
  if (!value || typeof value !== "object") {
    return null;
  }

  const body = value as Partial<PetAsset>;
  const id = textValue(body.id, "", "");
  const path = textValue(body.path, "", "");
  if (!id || !path) {
    return null;
  }

  return {
    id,
    name: textValue(body.name, id, id),
    kind: body.kind === "image" || body.kind === "package" ? body.kind : "sprite",
    format: textValue(body.format, "binary", "binary"),
    width: intValue(body.width, 0, 0, 4096),
    height: intValue(body.height, 0, 0, 4096),
    sourceWidth: optionalInt(body.sourceWidth, 0, 4096),
    sourceHeight: optionalInt(body.sourceHeight, 0, 4096),
    deviceWidth: optionalInt(body.deviceWidth, 0, 4096),
    deviceHeight: optionalInt(body.deviceHeight, 0, 4096),
    frames: intValue(body.frames, 1, 1, 240),
    fps: intValue(body.fps, 6, 1, 60),
    durationMs: optionalInt(body.durationMs, 1, Number.MAX_SAFE_INTEGER),
    frameDelaysMs: Array.isArray(body.frameDelaysMs)
      ? body.frameDelaysMs.map((delay) => intValue(delay, 100, 10, 60000))
      : undefined,
    size: intValue(body.size, 0, 0, Number.MAX_SAFE_INTEGER),
    encodedSize: optionalInt(body.encodedSize, 0, Number.MAX_SAFE_INTEGER),
    path,
    sourcePath: typeof body.sourcePath === "string" ? body.sourcePath.trim() : undefined,
    devicePath: typeof body.devicePath === "string" ? body.devicePath.trim() : undefined,
    deviceFormat: body.deviceFormat === "pka-rgb565-rle" ? body.deviceFormat : undefined,
    createdAt: intValue(body.createdAt, Date.now(), 0, Number.MAX_SAFE_INTEGER),
    updatedAt: intValue(body.updatedAt, Date.now(), 0, Number.MAX_SAFE_INTEGER)
  };
}

function intValue(value: unknown, fallback: number, min: number, max: number) {
  const next = Math.round(Number(value ?? fallback));
  return Number.isFinite(next) ? clamp(next, min, max) : fallback;
}

function optionalInt(value: unknown, min: number, max: number) {
  if (value === undefined || value === null || value === "") {
    return undefined;
  }
  const next = Math.round(Number(value));
  return Number.isFinite(next) ? clamp(next, min, max) : undefined;
}

function optionalNumber(value: unknown, min: number, max: number) {
  if (value === undefined || value === null || value === "") {
    return undefined;
  }
  const next = Number(value);
  return Number.isFinite(next) ? clamp(next, min, max) : undefined;
}

function boolValue(value: unknown, fallback: boolean) {
  return typeof value === "boolean" ? value : fallback;
}

function textValue(value: unknown, fallback: string, emptyFallback: string) {
  const next = typeof value === "string" ? value.trim() : fallback;
  return next.length > 0 ? next : emptyFallback;
}

function nullableText(value: unknown, fallback: string | null) {
  if (value === null) {
    return null;
  }
  if (typeof value === "string") {
    const next = value.trim();
    return next.length > 0 ? next : null;
  }
  return fallback;
}

function nullableNumber(value: unknown, fallback: number | null, min: number, max: number) {
  if (value === null) {
    return null;
  }

  const next = Number(value);
  return Number.isFinite(next) ? clamp(next, min, max) : fallback;
}
