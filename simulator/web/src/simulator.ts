import {
  defaultSimulatorState,
  normalizeSimulatorState,
  type SimulatorRadialItem,
  type SimulatorScreenMode,
  type SimulatorState
} from "../../protocol/simulator-state";

export function initialState() {
  return defaultSimulatorState();
}

export function updateState(state: SimulatorState, patch: Partial<SimulatorState>) {
  return normalizeSimulatorState({
    ...state,
    ...patch
  });
}

export function appendEvent(state: SimulatorState, message: string) {
  return updateState(state, {
    eventLog: [...state.eventLog, `${timeLabel()} ${message}`].slice(-40)
  });
}

export function setScreenMode(state: SimulatorState, screenMode: SimulatorScreenMode) {
  const sleeping = screenMode === "sleeping";
  return appendEvent(updateState(state, {
    screenMode,
    primaryText: sleeping ? "zzz..." : `Pet ${state.petIndex}`,
    hintText: sleeping ? "sleeping" : screenMode === "radialMenu" ? "menu" : "hold + shake",
    touchPressed: false,
    petThrowActive: false
  }), `mode ${screenMode}`);
}

export function nextPet(state: SimulatorState) {
  const petIndex = state.petIndex >= 3 ? 1 : state.petIndex + 1;
  return appendEvent(updateState(state, {
    petIndex,
    primaryText: `Pet ${petIndex}`,
    hintText: "next",
    screenMode: "home",
    petThrowActive: false
  }), `pet ${petIndex}`);
}

export function previousPet(state: SimulatorState) {
  const petIndex = state.petIndex <= 1 ? 3 : state.petIndex - 1;
  return appendEvent(updateState(state, {
    petIndex,
    primaryText: `Pet ${petIndex}`,
    hintText: "previous",
    screenMode: "home",
    petThrowActive: false
  }), `pet ${petIndex}`);
}

export function shortPress(state: SimulatorState) {
  return appendEvent(updateState(state, {
    screenMode: "home",
    hintText: "centered",
    touchPressed: false,
    cubeOffsetX: 0,
    cubeOffsetY: 0,
    petThrowActive: false
  }), "short press");
}

export function longPress(state: SimulatorState) {
  return appendEvent(updateState(state, {
    screenMode: "radialMenu",
    hintText: "menu",
    selectedItem: "info",
    cursorX: 32,
    cursorY: -32,
    touchPressed: false,
    petThrowActive: false
  }), "long press");
}

export function shake(state: SimulatorState) {
  const nextRoll = state.rollDeg > 0 ? -22 : 22;
  const nextPitch = state.pitchDeg > 0 ? -14 : 14;
  return appendEvent(updateState(state, {
    screenMode: "home",
    hintText: "shaken",
    rollDeg: nextRoll,
    pitchDeg: nextPitch,
    cubeOffsetX: nextRoll > 0 ? 18 : -18,
    cubeOffsetY: nextPitch > 0 ? 10 : -10,
    petThrowActive: false
  }), "shake");
}

export function throwPet(state: SimulatorState) {
  return appendEvent(updateState(state, {
    screenMode: "home",
    hintText: "hold + shake",
    primaryText: `Pet ${state.petIndex}`,
    petThrowActive: true,
    cubeOffsetX: 25,
    cubeOffsetY: -15,
    cubeScale: 18,
    rollDeg: 12,
    pitchDeg: -8
  }), "throw");
}

export function selectRadialItem(state: SimulatorState, selectedItem: SimulatorRadialItem) {
  const cursor = radialCursor(selectedItem);
  return appendEvent(updateState(state, {
    screenMode: "radialMenu",
    selectedItem,
    cursorX: cursor.x,
    cursorY: cursor.y,
    hintText: "menu",
    petThrowActive: false
  }), `select ${selectedItem}`);
}

function radialCursor(item: SimulatorRadialItem) {
  if (item === "previousPet") return { x: -46, y: 0 };
  if (item === "nextPet") return { x: 46, y: 0 };
  if (item === "cancel") return { x: 0, y: 46 };
  return { x: 32, y: -32 };
}

function timeLabel() {
  return new Date().toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  });
}
