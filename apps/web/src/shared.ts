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
  motorActive: boolean;
  updatedAt: number | null;
}

export type ScreenShape = "circle";
export type LayoutComponentType = "cube" | "sprite" | "arc" | "text" | "statusDot";

export interface LayoutComponent {
  id: string;
  type: LayoutComponentType;
  label: string;
  x: number;
  y: number;
  width?: number;
  height?: number;
  scale?: number;
  radius?: number;
  startAngle?: number;
  endAngle?: number;
  binding?: string;
  assetId?: string;
  text?: string;
  color?: string;
}

export interface ScreenLayout {
  version: number;
  revision: number;
  updatedAt: number;
  screen: {
    width: number;
    height: number;
    shape: ScreenShape;
  };
  components: LayoutComponent[];
}

export type PetAssetKind = "sprite" | "image" | "package";

export interface PetAsset {
  id: string;
  name: string;
  kind: PetAssetKind;
  format: string;
  width: number;
  height: number;
  frames: number;
  fps: number;
  size: number;
  path: string;
  createdAt: number;
  updatedAt: number;
}

export interface AssetManifest {
  version: number;
  revision: number;
  assets: PetAsset[];
}

export interface AppSnapshot {
  type: "snapshot";
  serverTime: number;
  config: DeviceConfig;
  status: DeviceStatus;
  layout: ScreenLayout;
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

export interface LayoutMessage {
  type: "layout" | "layout.preview";
  layout: ScreenLayout;
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
  | LayoutMessage
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
      type: "layout.preview";
      layout: Partial<ScreenLayout>;
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
  motorActive: false,
  updatedAt: null
});

export const defaultScreenLayout = (): ScreenLayout => ({
  version: 1,
  revision: 0,
  updatedAt: 0,
  screen: {
    width: 240,
    height: 240,
    shape: "circle"
  },
  components: [
    {
      id: "pet",
      type: "cube",
      label: "Pet",
      x: 120,
      y: 116,
      scale: 1
    },
    {
      id: "batteryA",
      type: "arc",
      label: "A",
      x: 120,
      y: 120,
      radius: 106,
      startAngle: 136,
      endAngle: 224,
      binding: "battery.local",
      color: "#46c7a5"
    },
    {
      id: "batteryB",
      type: "arc",
      label: "B",
      x: 120,
      y: 120,
      radius: 106,
      startAngle: -44,
      endAngle: 44,
      binding: "battery.peer",
      color: "#46c7a5"
    }
  ]
});

export const defaultAssetManifest = (): AssetManifest => ({
  version: 1,
  revision: 0,
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

export function normalizeScreenLayout(
  patch: Partial<ScreenLayout>,
  base: ScreenLayout = defaultScreenLayout()
): ScreenLayout {
  const nextScreen = patch.screen ?? base.screen;
  const now = Date.now();

  return {
    version: 1,
    revision: intValue(patch.revision, base.revision, 0, Number.MAX_SAFE_INTEGER),
    updatedAt: intValue(patch.updatedAt, now, 0, Number.MAX_SAFE_INTEGER),
    screen: {
      width: intValue(nextScreen.width, base.screen.width, 120, 720),
      height: intValue(nextScreen.height, base.screen.height, 120, 720),
      shape: "circle"
    },
    components: normalizeLayoutComponents(patch.components, base.components)
  };
}

export function normalizeAssetManifest(
  patch: Partial<AssetManifest>,
  base: AssetManifest = defaultAssetManifest()
): AssetManifest {
  return {
    version: 1,
    revision: intValue(patch.revision, base.revision, 0, Number.MAX_SAFE_INTEGER),
    assets: Array.isArray(patch.assets)
      ? patch.assets.map(normalizePetAsset).filter((asset): asset is PetAsset => asset !== null)
      : base.assets
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
    "layout.preview",
    "device.hello",
    "device.status"
  ].includes(String((value as { type: unknown }).type));
}

function normalizeLayoutComponents(value: unknown, fallback: LayoutComponent[]) {
  if (!Array.isArray(value)) {
    return fallback;
  }

  const components = value.map(normalizeLayoutComponent).filter(Boolean) as LayoutComponent[];
  return components.length > 0 ? components : fallback;
}

function normalizeLayoutComponent(value: unknown): LayoutComponent | null {
  if (!value || typeof value !== "object") {
    return null;
  }

  const body = value as Record<string, unknown>;
  const type = normalizeComponentType(body.type);
  if (!type) {
    return null;
  }

  const fallbackId = `${type}-${Math.random().toString(36).slice(2, 8)}`;
  const component: LayoutComponent = {
    id: textValue(body.id, fallbackId, fallbackId),
    type,
    label: textValue(body.label, String(body.id ?? type), type),
    x: intValue(body.x, 120, 0, 240),
    y: intValue(body.y, 120, 0, 240)
  };

  const width = optionalInt(body.width, 4, 240);
  const height = optionalInt(body.height, 4, 240);
  const scale = optionalNumber(body.scale, 0.1, 8);
  const radius = optionalInt(body.radius, 4, 140);
  const startAngle = optionalInt(body.startAngle, -360, 360);
  const endAngle = optionalInt(body.endAngle, -360, 360);

  if (width !== undefined) component.width = width;
  if (height !== undefined) component.height = height;
  if (scale !== undefined) component.scale = scale;
  if (radius !== undefined) component.radius = radius;
  if (startAngle !== undefined) component.startAngle = startAngle;
  if (endAngle !== undefined) component.endAngle = endAngle;
  if (typeof body.binding === "string") component.binding = body.binding.trim();
  if (typeof body.assetId === "string") component.assetId = body.assetId.trim();
  if (typeof body.text === "string") component.text = body.text.trim();
  if (typeof body.color === "string") component.color = body.color.trim();

  return component;
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
    frames: intValue(body.frames, 1, 1, 240),
    fps: intValue(body.fps, 6, 1, 60),
    size: intValue(body.size, 0, 0, Number.MAX_SAFE_INTEGER),
    path,
    createdAt: intValue(body.createdAt, Date.now(), 0, Number.MAX_SAFE_INTEGER),
    updatedAt: intValue(body.updatedAt, Date.now(), 0, Number.MAX_SAFE_INTEGER)
  };
}

function normalizeComponentType(value: unknown): LayoutComponentType | null {
  return value === "cube" || value === "sprite" || value === "arc" || value === "text" || value === "statusDot"
    ? value
    : null;
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
