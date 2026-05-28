import "./styles.css";
import { icon, refreshIcons } from "./icons";
import {
  defaultDeviceConfig,
  type AppSnapshot,
  type DeviceCommand,
  type DeviceConfig,
  type DeviceSlot,
  type ServerMessage
} from "./shared";

const app = document.querySelector<HTMLDivElement>("#app");

if (!app) {
  throw new Error("Missing #app");
}

app.innerHTML = shell();

const refs = {
  form: byId<HTMLFormElement>("config-form"),
  resetButton: byId<HTMLButtonElement>("reset-config"),
  saveState: byId<HTMLElement>("save-state"),
  socketState: byId<HTMLElement>("socket-state"),
  deviceState: byId<HTMLElement>("device-state"),
  deviceBadge: byId<HTMLElement>("device-badge"),
  firmwareVersion: byId<HTMLElement>("firmware-version"),
  ipAddress: byId<HTMLElement>("ip-address"),
  wifiRssi: byId<HTMLElement>("wifi-rssi"),
  batteryPercent: byId<HTMLElement>("battery-percent"),
  touchAnalog: byId<HTMLElement>("touch-analog"),
  imuPitch: byId<HTMLElement>("imu-pitch"),
  imuRoll: byId<HTMLElement>("imu-roll"),
  imuYaw: byId<HTMLElement>("imu-yaw"),
  motorState: byId<HTMLElement>("motor-state"),
  lastEvent: byId<HTMLElement>("last-event"),
  updatedAt: byId<HTMLElement>("updated-at"),
  brightnessValue: byId<HTMLElement>("brightness-value"),
  motorValue: byId<HTMLElement>("motor-value"),
  urlList: byId<HTMLElement>("url-list"),
  pulseMotor: byId<HTMLButtonElement>("pulse-motor"),
  applyConfig: byId<HTMLButtonElement>("apply-config")
};

let currentSnapshot: AppSnapshot | null = null;
let socket: WebSocket | null = null;
let socketRetryTimer: number | undefined;
let draftDirty = false;

bindForm();
void loadSnapshot();
connectSocket();
refreshIcons();

function bindForm() {
  refs.form.addEventListener("input", () => {
    draftDirty = true;
    setSaveState("未保存");
    syncRangeLabels();
  });

  refs.form.addEventListener("submit", (event) => {
    event.preventDefault();
    void saveConfig();
  });

  refs.resetButton.addEventListener("click", () => {
    void resetConfig();
  });

  refs.pulseMotor.addEventListener("click", () => {
    void sendCommand({ kind: "motor.pulse", value: getNumber("motorStrength"), at: Date.now() });
  });

  refs.applyConfig.addEventListener("click", () => {
    void sendCommand({ kind: "config.apply", at: Date.now() });
  });

  syncRangeLabels();
}

async function loadSnapshot() {
  try {
    const snapshot = await request<AppSnapshot>("/api/snapshot");
    applySnapshot(snapshot);
    setSaveState("就绪");
  } catch (error) {
    setSaveState(errorMessage(error));
  }
}

async function saveConfig() {
  refs.form.classList.add("is-busy");
  setSaveState("保存中");

  try {
    const config = await request<DeviceConfig>("/api/config", {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(readConfigForm())
    });
    draftDirty = false;
    fillConfigForm(config);
    setSaveState("已保存");
  } catch (error) {
    setSaveState(errorMessage(error));
  } finally {
    refs.form.classList.remove("is-busy");
  }
}

async function resetConfig() {
  refs.resetButton.disabled = true;
  setSaveState("重置中");

  try {
    const config = await request<DeviceConfig>("/api/config/reset", {
      method: "POST"
    });
    draftDirty = false;
    fillConfigForm(config);
    setSaveState("已重置");
  } catch (error) {
    setSaveState(errorMessage(error));
  } finally {
    refs.resetButton.disabled = false;
  }
}

async function sendCommand(command: DeviceCommand) {
  setSaveState("发送中");
  try {
    const result = await request<{ delivered: number }>("/api/device/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(command)
    });
    setSaveState(result.delivered > 0 ? "已发送" : "设备离线");
  } catch (error) {
    setSaveState(errorMessage(error));
  }
}

function connectSocket() {
  window.clearTimeout(socketRetryTimer);
  refs.socketState.textContent = "连接中";
  refs.socketState.dataset.state = "connecting";

  socket = new WebSocket(wsUrl());

  socket.addEventListener("open", () => {
    refs.socketState.textContent = "已连接";
    refs.socketState.dataset.state = "online";
    socket?.send(JSON.stringify({ type: "browser.hello" }));
  });

  socket.addEventListener("message", (event) => {
    const message = JSON.parse(String(event.data)) as ServerMessage;
    if (message.type === "snapshot") {
      applySnapshot(message);
    }
  });

  socket.addEventListener("close", () => {
    refs.socketState.textContent = "离线";
    refs.socketState.dataset.state = "offline";
    socketRetryTimer = window.setTimeout(connectSocket, 1500);
  });
}

function applySnapshot(snapshot: AppSnapshot) {
  currentSnapshot = snapshot;

  if (!draftDirty && !refs.form.matches(":focus-within")) {
    fillConfigForm(snapshot.config);
  }

  renderStatus(snapshot);
  renderUrls(snapshot.lanUrls);
}

function fillConfigForm(config: DeviceConfig) {
  setInput("deviceName", config.deviceName);
  setInput("pairSlot", config.pairSlot);
  setInput("weatherCity", config.weatherCity);
  setInput("wifiSsid", config.wifiSsid);
  setInput("backendUrl", config.backendUrl);
  setInput("sleepStartMinutes", minutesToTimeInput(config.sleepStartMinutes));
  setInput("sleepEndMinutes", minutesToTimeInput(config.sleepEndMinutes));
  setInput("touchIdleThreshold", String(config.touchIdleThreshold));
  setInput("touchPressThreshold", String(config.touchPressThreshold));
  setInput("touchSampleIntervalMs", String(config.touchSampleIntervalMs));
  setInput("longPressMs", String(config.longPressMs));
  setInput("sleepTimeoutMs", String(config.sleepTimeoutMs));
  setInput("screenBrightness", String(config.screenBrightness));
  setInput("motorStrength", String(config.motorStrength));
  setChecked("imuEnabled", config.imuEnabled);
  setChecked("motorEnabled", config.motorEnabled);
  syncRangeLabels();
}

function readConfigForm(): DeviceConfig {
  const fallback = currentSnapshot?.config ?? defaultDeviceConfig();

  return {
    ...fallback,
    deviceName: getInput("deviceName").value,
    pairSlot: getInput("pairSlot").value as DeviceSlot,
    weatherCity: getInput("weatherCity").value,
    wifiSsid: getInput("wifiSsid").value,
    backendUrl: getInput("backendUrl").value,
    sleepStartMinutes: timeInputToMinutes(getInput("sleepStartMinutes").value),
    sleepEndMinutes: timeInputToMinutes(getInput("sleepEndMinutes").value),
    touchIdleThreshold: getNumber("touchIdleThreshold"),
    touchPressThreshold: getNumber("touchPressThreshold"),
    touchSampleIntervalMs: getNumber("touchSampleIntervalMs"),
    longPressMs: getNumber("longPressMs"),
    sleepTimeoutMs: getNumber("sleepTimeoutMs"),
    screenBrightness: getNumber("screenBrightness"),
    motorStrength: getNumber("motorStrength"),
    imuEnabled: getCheckbox("imuEnabled").checked,
    motorEnabled: getCheckbox("motorEnabled").checked
  };
}

function renderStatus(snapshot: AppSnapshot) {
  const status = snapshot.status;
  refs.deviceState.textContent = status.state;
  refs.deviceBadge.textContent = status.connected ? "在线" : "离线";
  refs.deviceBadge.dataset.state = status.connected ? "online" : "offline";
  refs.firmwareVersion.textContent = status.firmwareVersion ?? "--";
  refs.ipAddress.textContent = status.ipAddress ?? "--";
  refs.wifiRssi.textContent = status.wifiRssi === null ? "--" : `${Math.round(status.wifiRssi)} dBm`;
  refs.batteryPercent.textContent =
    status.batteryPercent === null ? "--" : `${Math.round(status.batteryPercent)}%`;
  refs.touchAnalog.textContent = status.touchAnalog === null ? "--" : String(Math.round(status.touchAnalog));
  refs.imuPitch.textContent = degree(status.imu.pitch);
  refs.imuRoll.textContent = degree(status.imu.roll);
  refs.imuYaw.textContent = degree(status.imu.yaw);
  refs.motorState.textContent = status.motorActive ? "运行" : "待机";
  refs.lastEvent.textContent = status.lastEvent ?? "--";
  refs.updatedAt.textContent = status.updatedAt ? timeLabel(status.updatedAt) : "--";
}

function renderUrls(urls: string[]) {
  refs.urlList.replaceChildren(
    ...urls.map((url) => {
      const link = document.createElement("a");
      link.href = url;
      link.textContent = url;
      return link;
    })
  );
}

function syncRangeLabels() {
  refs.brightnessValue.textContent = `${getNumber("screenBrightness")}%`;
  refs.motorValue.textContent = `${getNumber("motorStrength")}%`;
}

async function request<T>(url: string, init?: RequestInit) {
  const response = await fetch(url, init);
  const body = (await response.json()) as {
    data?: T;
    error?: { message?: string };
  };

  if (!response.ok) {
    throw new Error(body.error?.message ?? "请求失败");
  }

  return body.data as T;
}

function setSaveState(text: string) {
  refs.saveState.textContent = text;
}

function getInput(id: string) {
  return byId<HTMLInputElement | HTMLSelectElement>(id);
}

function getCheckbox(id: string) {
  return byId<HTMLInputElement>(id);
}

function getNumber(id: string) {
  return Number(getInput(id).value);
}

function setInput(id: string, value: string) {
  getInput(id).value = value;
}

function setChecked(id: string, checked: boolean) {
  byId<HTMLInputElement>(id).checked = checked;
}

function byId<T extends HTMLElement>(id: string) {
  const element = document.getElementById(id);
  if (!element) {
    throw new Error(`Missing #${id}`);
  }
  return element as T;
}

function wsUrl() {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${window.location.host}/ws`;
}

function minutesToTimeInput(minutes: number) {
  const hour = Math.floor(minutes / 60);
  const minute = minutes % 60;
  return `${String(hour).padStart(2, "0")}:${String(minute).padStart(2, "0")}`;
}

function timeInputToMinutes(value: string) {
  const [hour = "0", minute = "0"] = value.split(":");
  return Number(hour) * 60 + Number(minute);
}

function degree(value: number | null) {
  return value === null ? "--" : `${Math.round(value)}°`;
}

function timeLabel(timestamp: number) {
  return new Intl.DateTimeFormat("zh-CN", {
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  }).format(timestamp);
}

function errorMessage(error: unknown) {
  return error instanceof Error ? error.message : "失败";
}

function shell() {
  return `
    <main class="app-shell">
      <header class="topbar">
        <div>
          <p class="eyebrow">Peek</p>
          <h1>Device Console</h1>
        </div>
        <div class="topbar-actions">
          <span class="state-pill" id="socket-state" data-state="connecting">
            ${icon("radio")}<span>连接中</span>
          </span>
          <button class="primary-button" form="config-form" type="submit">
            ${icon("save")}<span>保存</span>
          </button>
        </div>
      </header>

      <section class="status-strip" aria-label="设备状态">
        <article class="status-card status-card--wide">
          <div class="card-icon">${icon("power")}</div>
          <span>设备</span>
          <strong id="device-state">offline</strong>
          <em class="state-pill" id="device-badge" data-state="offline">离线</em>
        </article>
        <article class="status-card">
          <div class="card-icon">${icon("battery")}</div>
          <span>电池</span>
          <strong id="battery-percent">--</strong>
        </article>
        <article class="status-card">
          <div class="card-icon">${icon("wifi")}</div>
          <span>信号</span>
          <strong id="wifi-rssi">--</strong>
        </article>
        <article class="status-card">
          <div class="card-icon">${icon("gauge")}</div>
          <span>触摸</span>
          <strong id="touch-analog">--</strong>
        </article>
      </section>

      <div class="workspace">
        <form class="config-panel" id="config-form">
          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("settings")} 基础</h2>
              <span id="save-state">就绪</span>
            </div>
            <div class="form-grid">
              <label class="field">
                <span>设备名</span>
                <input id="deviceName" type="text" autocomplete="off" />
              </label>
              <label class="field">
                <span>槽位</span>
                <select id="pairSlot">
                  <option value="A">A</option>
                  <option value="B">B</option>
                </select>
              </label>
              <label class="field">
                <span>天气城市</span>
                <input id="weatherCity" type="text" autocomplete="address-level2" />
              </label>
              <label class="field">
                <span>Wi-Fi SSID</span>
                <input id="wifiSsid" type="text" autocomplete="off" />
              </label>
              <label class="field field--wide">
                <span>后端地址</span>
                <input id="backendUrl" type="text" inputmode="url" autocomplete="off" />
              </label>
            </div>
          </section>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("clock-3")} 休眠</h2>
            </div>
            <div class="form-grid form-grid--compact">
              <label class="field">
                <span>开始</span>
                <input id="sleepStartMinutes" type="time" />
              </label>
              <label class="field">
                <span>结束</span>
                <input id="sleepEndMinutes" type="time" />
              </label>
              <label class="field">
                <span>无操作超时 ms</span>
                <input id="sleepTimeoutMs" type="number" min="5000" max="900000" step="1000" />
              </label>
            </div>
          </section>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("sliders-horizontal")} 传感器</h2>
            </div>
            <div class="form-grid form-grid--compact">
              <label class="field">
                <span>空闲阈值</span>
                <input id="touchIdleThreshold" type="number" min="0" max="4095" />
              </label>
              <label class="field">
                <span>按压阈值</span>
                <input id="touchPressThreshold" type="number" min="0" max="4095" />
              </label>
              <label class="field">
                <span>采样间隔 ms</span>
                <input id="touchSampleIntervalMs" type="number" min="10" max="1000" />
              </label>
              <label class="field">
                <span>长按 ms</span>
                <input id="longPressMs" type="number" min="300" max="10000" step="100" />
              </label>
            </div>
          </section>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("vibrate")} 输出</h2>
            </div>
            <div class="range-grid">
              <label class="range-field">
                <span>屏幕亮度 <strong id="brightness-value">80%</strong></span>
                <input id="screenBrightness" type="range" min="1" max="100" />
              </label>
              <label class="range-field">
                <span>马达力度 <strong id="motor-value">45%</strong></span>
                <input id="motorStrength" type="range" min="0" max="100" />
              </label>
            </div>
            <div class="toggle-row">
              <label class="toggle-field">
                <input id="imuEnabled" type="checkbox" />
                <span>${icon("smartphone")} IMU</span>
              </label>
              <label class="toggle-field">
                <input id="motorEnabled" type="checkbox" />
                <span>${icon("vibrate")} 马达</span>
              </label>
            </div>
          </section>

          <div class="form-actions">
            <button class="secondary-button" id="reset-config" type="button">
              ${icon("rotate-ccw")}<span>重置</span>
            </button>
            <button class="primary-button" type="submit">
              ${icon("save")}<span>保存</span>
            </button>
          </div>
        </form>

        <aside class="diagnostic-panel">
          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("cpu")} 诊断</h2>
              <span id="updated-at">--</span>
            </div>
            <dl class="diagnostic-list">
              <div><dt>固件</dt><dd id="firmware-version">--</dd></div>
              <div><dt>IP</dt><dd id="ip-address">--</dd></div>
              <div><dt>Pitch</dt><dd id="imu-pitch">--</dd></div>
              <div><dt>Roll</dt><dd id="imu-roll">--</dd></div>
              <div><dt>Yaw</dt><dd id="imu-yaw">--</dd></div>
              <div><dt>马达</dt><dd id="motor-state">待机</dd></div>
              <div class="diagnostic-list__wide"><dt>事件</dt><dd id="last-event">--</dd></div>
            </dl>
          </section>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("zap")} 命令</h2>
            </div>
            <div class="command-grid">
              <button class="secondary-button" id="pulse-motor" type="button">
                ${icon("vibrate")}<span>马达</span>
              </button>
              <button class="secondary-button" id="apply-config" type="button">
                ${icon("check")}<span>应用</span>
              </button>
            </div>
          </section>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("map-pin")} 地址</h2>
            </div>
            <div class="url-list" id="url-list"></div>
          </section>
        </aside>
      </div>
    </main>
  `;
}
