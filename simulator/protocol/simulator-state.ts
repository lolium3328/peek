export type SimulatorScreenMode = "home" | "status" | "radialMenu" | "sleeping";

export type SimulatorRadialItem = "cancel" | "info" | "previousPet" | "nextPet";

export interface SimulatorState {
  screenMode: SimulatorScreenMode;
  petIndex: number;
  primaryText: string;
  hintText: string;
  touchPressed: boolean;
  wifiConnected: boolean;
  backendConnected: boolean;
  lowBattery: boolean;
  imuReady: boolean;
  cubeVisible: boolean;
  rollDeg: number;
  pitchDeg: number;
  yawDeg: number;
  cubeOffsetX: number;
  cubeOffsetY: number;
  cubeScale: number;
  petThrowActive: boolean;
  selectedItem: SimulatorRadialItem;
  cursorX: number;
  cursorY: number;
  eventLog: string[];
}

export const defaultSimulatorState = (): SimulatorState => ({
  screenMode: "home",
  petIndex: 1,
  primaryText: "Pet 1",
  hintText: "hold + shake",
  touchPressed: false,
  wifiConnected: true,
  backendConnected: true,
  lowBattery: false,
  imuReady: true,
  cubeVisible: true,
  rollDeg: 0,
  pitchDeg: 0,
  yawDeg: 0,
  cubeOffsetX: 0,
  cubeOffsetY: 0,
  cubeScale: 32,
  petThrowActive: false,
  selectedItem: "info",
  cursorX: 32,
  cursorY: -32,
  eventLog: ["simulator ready"]
});

export function normalizeSimulatorState(value: unknown): SimulatorState {
  const body = value && typeof value === "object" ? value as Partial<SimulatorState> : {};
  const fallback = defaultSimulatorState();
  const screenMode = isScreenMode(body.screenMode) ? body.screenMode : fallback.screenMode;
  const petIndex = intValue(body.petIndex, fallback.petIndex, 1, 9);
  return {
    screenMode,
    petIndex,
    primaryText: textValue(body.primaryText, `Pet ${petIndex}`),
    hintText: textValue(body.hintText, screenMode === "sleeping" ? "sleeping" : fallback.hintText),
    touchPressed: boolValue(body.touchPressed, fallback.touchPressed),
    wifiConnected: boolValue(body.wifiConnected, fallback.wifiConnected),
    backendConnected: boolValue(body.backendConnected, fallback.backendConnected),
    lowBattery: boolValue(body.lowBattery, fallback.lowBattery),
    imuReady: boolValue(body.imuReady, fallback.imuReady),
    cubeVisible: boolValue(body.cubeVisible, fallback.cubeVisible),
    rollDeg: numberValue(body.rollDeg, fallback.rollDeg, -180, 180),
    pitchDeg: numberValue(body.pitchDeg, fallback.pitchDeg, -180, 180),
    yawDeg: numberValue(body.yawDeg, fallback.yawDeg, -180, 180),
    cubeOffsetX: numberValue(body.cubeOffsetX, fallback.cubeOffsetX, -80, 80),
    cubeOffsetY: numberValue(body.cubeOffsetY, fallback.cubeOffsetY, -80, 80),
    cubeScale: numberValue(body.cubeScale, fallback.cubeScale, 8, 64),
    petThrowActive: boolValue(body.petThrowActive, fallback.petThrowActive),
    selectedItem: isRadialItem(body.selectedItem) ? body.selectedItem : fallback.selectedItem,
    cursorX: numberValue(body.cursorX, fallback.cursorX, -80, 80),
    cursorY: numberValue(body.cursorY, fallback.cursorY, -80, 80),
    eventLog: Array.isArray(body.eventLog)
      ? body.eventLog.map((item) => String(item)).slice(-40)
      : fallback.eventLog
  };
}

function isScreenMode(value: unknown): value is SimulatorScreenMode {
  return value === "home" || value === "status" || value === "radialMenu" || value === "sleeping";
}

function isRadialItem(value: unknown): value is SimulatorRadialItem {
  return value === "cancel" || value === "info" || value === "previousPet" || value === "nextPet";
}

function textValue(value: unknown, fallback: string) {
  return typeof value === "string" && value.trim().length > 0 ? value.trim().slice(0, 48) : fallback;
}

function boolValue(value: unknown, fallback: boolean) {
  return typeof value === "boolean" ? value : fallback;
}

function intValue(value: unknown, fallback: number, min: number, max: number) {
  return Math.round(numberValue(value, fallback, min, max));
}

function numberValue(value: unknown, fallback: number, min: number, max: number) {
  const next = Number(value);
  if (!Number.isFinite(next)) return fallback;
  return Math.min(Math.max(next, min), max);
}
