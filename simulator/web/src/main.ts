import "./styles.css";
import type { SimulatorRadialItem, SimulatorScreenMode, SimulatorState } from "../../protocol/simulator-state";
import type { SimulatorRenderResponse } from "../../protocol/render-response";
import {
  appendEvent,
  initialState,
  longPress,
  nextPet,
  previousPet,
  selectRadialItem,
  setScreenMode,
  shake,
  shortPress,
  throwPet,
  updateState
} from "./simulator";
import { renderSimulatorFrame } from "./transport";

const app = document.querySelector<HTMLDivElement>("#app");
if (!app) throw new Error("Missing #app");

app.innerHTML = shell();

const refs = {
  frame: byId<HTMLImageElement>("frame"),
  renderState: byId<HTMLElement>("render-state"),
  imagePath: byId<HTMLElement>("image-path"),
  log: byId<HTMLElement>("event-log"),
  mode: byId<HTMLSelectElement>("screenMode"),
  petIndex: byId<HTMLElement>("pet-index"),
  primaryText: byId<HTMLInputElement>("primaryText"),
  hintText: byId<HTMLInputElement>("hintText"),
  selectedItem: byId<HTMLSelectElement>("selectedItem"),
  touchPressed: byId<HTMLInputElement>("touchPressed"),
  wifiConnected: byId<HTMLInputElement>("wifiConnected"),
  backendConnected: byId<HTMLInputElement>("backendConnected"),
  lowBattery: byId<HTMLInputElement>("lowBattery"),
  imuReady: byId<HTMLInputElement>("imuReady"),
  cubeVisible: byId<HTMLInputElement>("cubeVisible"),
  rollDeg: byId<HTMLInputElement>("rollDeg"),
  pitchDeg: byId<HTMLInputElement>("pitchDeg"),
  yawDeg: byId<HTMLInputElement>("yawDeg"),
  cubeOffsetX: byId<HTMLInputElement>("cubeOffsetX"),
  cubeOffsetY: byId<HTMLInputElement>("cubeOffsetY"),
  cubeScale: byId<HTMLInputElement>("cubeScale")
};

let state = initialState();
let renderTimer: number | undefined;

bindControls();
syncControls();
void renderNow();

function bindControls() {
  refs.mode.addEventListener("change", () => {
    state = setScreenMode(state, refs.mode.value as SimulatorScreenMode);
    syncControls();
    scheduleRender();
  });

  refs.selectedItem.addEventListener("change", () => {
    state = selectRadialItem(state, refs.selectedItem.value as SimulatorRadialItem);
    syncControls();
    scheduleRender();
  });

  refs.primaryText.addEventListener("input", () => updateFromInputs("primary text"));
  refs.hintText.addEventListener("input", () => updateFromInputs("hint text"));

  for (const input of [
    refs.touchPressed,
    refs.wifiConnected,
    refs.backendConnected,
    refs.lowBattery,
    refs.imuReady,
    refs.cubeVisible,
    refs.rollDeg,
    refs.pitchDeg,
    refs.yawDeg,
    refs.cubeOffsetX,
    refs.cubeOffsetY,
    refs.cubeScale
  ]) {
    input.addEventListener("input", () => updateFromInputs("control"));
  }

  bindAction("short-press", () => shortPress(state));
  bindAction("long-press", () => longPress(state));
  bindAction("shake", () => shake(state));
  bindAction("throw", () => throwPet(state));
  bindAction("previous-pet", () => previousPet(state));
  bindAction("next-pet", () => nextPet(state));
  bindAction("sleep", () => setScreenMode(state, "sleeping"));
  bindAction("wake", () => setScreenMode(state, "home"));
  bindAction("status", () => setScreenMode(state, "status"));
  bindAction("render", () => appendEvent(state, "manual render"));
}

function bindAction(id: string, action: () => SimulatorState) {
  byId<HTMLButtonElement>(id).addEventListener("click", () => {
    state = action();
    syncControls();
    scheduleRender(0);
  });
}

function updateFromInputs(label: string) {
  state = appendEvent(updateState(state, {
    primaryText: refs.primaryText.value,
    hintText: refs.hintText.value,
    touchPressed: refs.touchPressed.checked,
    wifiConnected: refs.wifiConnected.checked,
    backendConnected: refs.backendConnected.checked,
    lowBattery: refs.lowBattery.checked,
    imuReady: refs.imuReady.checked,
    cubeVisible: refs.cubeVisible.checked,
    rollDeg: Number(refs.rollDeg.value),
    pitchDeg: Number(refs.pitchDeg.value),
    yawDeg: Number(refs.yawDeg.value),
    cubeOffsetX: Number(refs.cubeOffsetX.value),
    cubeOffsetY: Number(refs.cubeOffsetY.value),
    cubeScale: Number(refs.cubeScale.value),
    selectedItem: refs.selectedItem.value as SimulatorRadialItem
  }), label);
  syncReadouts();
  scheduleRender();
}

function scheduleRender(delay = 220) {
  window.clearTimeout(renderTimer);
  renderTimer = window.setTimeout(() => {
    void renderNow();
  }, delay);
}

async function renderNow() {
  refs.renderState.textContent = "rendering";
  refs.renderState.dataset.state = "busy";
  try {
    const response = await renderSimulatorFrame({ state });
    applyRenderResponse(response);
  } catch (error) {
    refs.renderState.textContent = error instanceof Error ? error.message : String(error);
    refs.renderState.dataset.state = "error";
  }
}

function applyRenderResponse(response: SimulatorRenderResponse) {
  state = response.state;
  refs.frame.src = response.imageUrl;
  refs.imagePath.textContent = response.imagePath;
  refs.renderState.textContent = `rendered ${new Date(response.renderedAt).toLocaleTimeString()}`;
  refs.renderState.dataset.state = "ready";
  syncControls();
}

function syncControls() {
  refs.mode.value = state.screenMode;
  refs.petIndex.textContent = String(state.petIndex);
  refs.primaryText.value = state.primaryText;
  refs.hintText.value = state.hintText;
  refs.selectedItem.value = state.selectedItem;
  refs.touchPressed.checked = state.touchPressed;
  refs.wifiConnected.checked = state.wifiConnected;
  refs.backendConnected.checked = state.backendConnected;
  refs.lowBattery.checked = state.lowBattery;
  refs.imuReady.checked = state.imuReady;
  refs.cubeVisible.checked = state.cubeVisible;
  refs.rollDeg.value = String(state.rollDeg);
  refs.pitchDeg.value = String(state.pitchDeg);
  refs.yawDeg.value = String(state.yawDeg);
  refs.cubeOffsetX.value = String(state.cubeOffsetX);
  refs.cubeOffsetY.value = String(state.cubeOffsetY);
  refs.cubeScale.value = String(state.cubeScale);
  syncReadouts();
  renderLog();
}

function syncReadouts() {
  for (const input of Array.from(document.querySelectorAll<HTMLInputElement>("[data-readout]"))) {
    const target = byId<HTMLElement>(input.dataset.readout ?? "");
    target.textContent = input.value;
  }
}

function renderLog() {
  refs.log.replaceChildren(
    ...state.eventLog.slice().reverse().map((item) => {
      const row = document.createElement("li");
      row.textContent = item;
      return row;
    })
  );
}

function byId<T extends HTMLElement>(id: string) {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Missing #${id}`);
  return element as T;
}

function shell() {
  return `
    <main class="sim-shell">
      <header class="topbar">
        <div>
          <p>Peek Simulator</p>
          <h1>Firmware Renderer Lab</h1>
        </div>
        <button class="primary-button" id="render" type="button"><span>Render</span></button>
      </header>

      <section class="sim-layout">
        <section class="preview-panel" aria-label="screen preview">
          <div class="device-frame">
            <img id="frame" alt="Peek simulated screen" width="240" height="240" />
          </div>
          <div class="render-meta">
            <strong id="render-state" data-state="busy">rendering</strong>
            <span id="image-path">--</span>
          </div>
        </section>

        <section class="control-panel" aria-label="simulator controls">
          <section class="control-section">
            <div class="section-heading"><h2>Mode</h2><strong>Pet <span id="pet-index">1</span></strong></div>
            <div class="form-grid">
              <label class="field"><span>Screen</span><select id="screenMode">
                <option value="home">Home</option>
                <option value="status">Status</option>
                <option value="radialMenu">Radial menu</option>
                <option value="sleeping">Sleeping</option>
              </select></label>
              <label class="field"><span>Menu item</span><select id="selectedItem">
                <option value="info">Info</option>
                <option value="previousPet">Previous</option>
                <option value="nextPet">Next</option>
                <option value="cancel">Cancel</option>
              </select></label>
              <label class="field"><span>Primary</span><input id="primaryText" type="text" /></label>
              <label class="field"><span>Hint</span><input id="hintText" type="text" /></label>
            </div>
            <div class="action-grid">
              <button id="short-press" type="button">Short press</button>
              <button id="long-press" type="button">Long press</button>
              <button id="shake" type="button">Shake</button>
              <button id="throw" type="button">Throw</button>
              <button id="previous-pet" type="button">Prev pet</button>
              <button id="next-pet" type="button">Next pet</button>
              <button id="sleep" type="button">Sleep</button>
              <button id="wake" type="button">Wake</button>
              <button id="status" type="button">Status</button>
            </div>
          </section>

          <section class="control-section">
            <div class="section-heading"><h2>Signals</h2></div>
            <div class="toggle-grid">
              <label><input id="touchPressed" type="checkbox" /> Touch</label>
              <label><input id="wifiConnected" type="checkbox" /> Wi-Fi</label>
              <label><input id="backendConnected" type="checkbox" /> Backend</label>
              <label><input id="lowBattery" type="checkbox" /> Low battery</label>
              <label><input id="imuReady" type="checkbox" /> IMU</label>
              <label><input id="cubeVisible" type="checkbox" /> Cube</label>
            </div>
          </section>

          <section class="control-section">
            <div class="section-heading"><h2>Pose</h2></div>
            <div class="range-grid">
              ${rangeField("rollDeg", "Roll", -180, 180)}
              ${rangeField("pitchDeg", "Pitch", -180, 180)}
              ${rangeField("yawDeg", "Yaw", -180, 180)}
              ${rangeField("cubeOffsetX", "Offset X", -80, 80)}
              ${rangeField("cubeOffsetY", "Offset Y", -80, 80)}
              ${rangeField("cubeScale", "Scale", 8, 64)}
            </div>
          </section>
        </section>

        <aside class="log-panel" aria-label="event log">
          <div class="section-heading"><h2>Event Log</h2></div>
          <ol id="event-log"></ol>
        </aside>
      </section>
    </main>
  `;
}

function rangeField(id: string, label: string, min: number, max: number) {
  return `
    <label class="range-field">
      <span>${label} <strong id="${id}-value">0</strong></span>
      <input id="${id}" data-readout="${id}-value" type="range" min="${min}" max="${max}" step="1" />
    </label>
  `;
}
