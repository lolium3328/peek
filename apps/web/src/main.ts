import "./styles.css";
import { icon, refreshIcons } from "./icons";
import { FirmwareScreenPreview, type PreviewScreenMode } from "./screenPreview";
import { initManage, setAssetManifest, applyManageSnapshot, renderAssets, type ManageRefs } from "./manage";
import { type AppSnapshot, type AssetManifest, type ServerMessage } from "./shared";
import { byId, request, wsUrl, degree, timeLabel, storageLabel, errorMessage } from "./util";

const app = document.querySelector<HTMLDivElement>("#app");
if (!app) throw new Error("Missing #app");
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
  storageFree: byId<HTMLElement>("storage-free"),
  imuPitch: byId<HTMLElement>("imu-pitch"),
  imuRoll: byId<HTMLElement>("imu-roll"),
  imuYaw: byId<HTMLElement>("imu-yaw"),
  motorState: byId<HTMLElement>("motor-state"),
  lastEvent: byId<HTMLElement>("last-event"),
  updatedAt: byId<HTMLElement>("updated-at"),
  urlList: byId<HTMLElement>("url-list"),
  pulseMotor: byId<HTMLButtonElement>("pulse-motor"),
  applyConfig: byId<HTMLButtonElement>("apply-config"),
  assetForm: byId<HTMLFormElement>("asset-form"),
  assetList: byId<HTMLElement>("asset-list"),
  firmwarePreviewCanvas: byId<HTMLCanvasElement>("firmware-preview-canvas"),
  firmwarePreviewState: byId<HTMLElement>("firmware-preview-state")
};

let currentSnapshot: AppSnapshot | null = null;
const firmwarePreview = new FirmwareScreenPreview(refs.firmwarePreviewCanvas);
let assetManifest: AssetManifest = { version: 1, revision: 0, pet2AssetId: null, assets: [] };
let firmwarePreviewMode: PreviewScreenMode = "home";
let socket: WebSocket | null = null;
let socketRetryTimer: number | undefined;

bindTabs();
initManage(refs as unknown as ManageRefs, assetManifest, currentSnapshot);
bindFirmwarePreview();
void loadSnapshot();
connectSocket();
refreshIcons();

function bindTabs() {
  for (const tab of Array.from(document.querySelectorAll<HTMLButtonElement>(".mode-tab"))) {
    tab.addEventListener("click", () => setMode(tab.dataset.mode ?? "manage"));
  }
}

function bindFirmwarePreview() {
  for (const button of Array.from(document.querySelectorAll<HTMLButtonElement>("[data-preview-screen]"))) {
    button.addEventListener("click", () => {
      const mode = button.dataset.previewScreen as PreviewScreenMode;
      firmwarePreviewMode = mode;
      firmwarePreview.setMode(mode);
      refs.firmwarePreviewState.textContent = previewModeLabel(mode);
      for (const item of Array.from(document.querySelectorAll<HTMLButtonElement>("[data-preview-screen]"))) {
        item.classList.toggle("is-active", item === button);
      }
    });
  }
  byId<HTMLButtonElement>("preview-back").addEventListener("click", () => setMode("manage"));
  firmwarePreview.setMode(firmwarePreviewMode);
}

async function loadSnapshot() {
  try {
    const snapshot = await request<AppSnapshot>("/api/snapshot");
    applySnapshot(snapshot);
    refs.saveState.textContent = "就绪";
  } catch (error) {
    refs.saveState.textContent = errorMessage(error);
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
    } else if (message.type === "assets") {
      assetManifest = message.assets;
      setAssetManifest(assetManifest);
      renderAssets();
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
  assetManifest = snapshot.assets;
  setAssetManifest(assetManifest);
  applyManageSnapshot(snapshot);
  renderStatus(snapshot);
  renderUrls(snapshot.lanUrls);
  firmwarePreview.setSnapshot(snapshot);
}

function renderStatus(snapshot: AppSnapshot) {
  const status = snapshot.status;
  refs.deviceState.textContent = status.state;
  refs.deviceBadge.textContent = status.connected ? "在线" : "离线";
  refs.deviceBadge.dataset.state = status.connected ? "online" : "offline";
  refs.firmwareVersion.textContent = status.firmwareVersion ?? "--";
  refs.ipAddress.textContent = status.ipAddress ?? "--";
  refs.wifiRssi.textContent = status.wifiRssi === null ? "--" : `${Math.round(status.wifiRssi)} dBm`;
  refs.batteryPercent.textContent = status.batteryPercent === null ? "--" : `${Math.round(status.batteryPercent)}%`;
  refs.storageFree.textContent = storageLabel(status.storage.freeBytes, status.storage.totalBytes);
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

function setMode(mode: string) {
  for (const tab of Array.from(document.querySelectorAll<HTMLButtonElement>(".mode-tab"))) {
    tab.classList.toggle("is-active", tab.dataset.mode === mode);
  }
  for (const view of Array.from(document.querySelectorAll<HTMLElement>("[data-view]"))) {
    view.hidden = view.dataset.view !== mode;
  }
}

function previewModeLabel(mode: PreviewScreenMode) {
  if (mode === "homeFrame") return "renderHomeFrame";
  if (mode === "boot") return "renderBoot";
  if (mode === "status") return "renderStatus";
  return "renderHome";
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

      <nav class="mode-tabs" aria-label="管理模式">
        <button class="mode-tab is-active" type="button" data-mode="manage">${icon("settings")}<span>管理</span></button>
      </nav>

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
          <div class="card-icon">${icon("hard-drive")}</div>
          <span>设备空间</span>
          <strong id="storage-free">--</strong>
        </article>
      </section>

      <section data-view="manage">
        <div class="workspace">
          <form class="config-panel" id="config-form">
            <section class="panel-section">
              <div class="section-heading">
                <h2>${icon("settings")} 基础</h2>
                <span id="save-state">就绪</span>
              </div>
              <div class="form-grid">
                <label class="field"><span>设备名</span><input id="deviceName" type="text" autocomplete="off" /></label>
                <label class="field"><span>设备 ID</span><input id="deviceId" type="text" autocomplete="off" /></label>
                <label class="field"><span>令牌</span><input id="deviceToken" type="password" autocomplete="off" /></label>
                <label class="field"><span>槽位</span><select id="pairSlot"><option value="A">A</option><option value="B">B</option></select></label>
                <label class="field"><span>天气城市</span><input id="weatherCity" type="text" autocomplete="address-level2" /></label>
                <label class="field"><span>Wi-Fi SSID</span><input id="wifiSsid" type="text" autocomplete="off" /></label>
                <label class="field"><span>Wi-Fi 密码</span><input id="wifiPassword" type="password" autocomplete="off" /></label>
                <label class="field"><span>同步间隔 ms</span><input id="backendPollIntervalMs" type="number" min="1000" max="600000" step="1000" /></label>
                <label class="field field--wide"><span>后端地址</span><input id="backendUrl" type="text" inputmode="url" autocomplete="off" /></label>
              </div>
            </section>
            <section class="panel-section">
              <div class="section-heading"><h2>${icon("clock-3")} 休眠</h2></div>
              <div class="form-grid form-grid--compact">
                <label class="field"><span>开始</span><input id="sleepStartMinutes" type="time" /></label>
                <label class="field"><span>结束</span><input id="sleepEndMinutes" type="time" /></label>
                <label class="field"><span>无操作超时 ms</span><input id="sleepTimeoutMs" type="number" min="5000" max="900000" step="1000" /></label>
              </div>
            </section>
            <section class="panel-section">
              <div class="section-heading"><h2>${icon("sliders-horizontal")} 传感器</h2></div>
              <div class="form-grid form-grid--compact">
                <label class="field"><span>空闲阈值</span><input id="touchIdleThreshold" type="number" min="0" max="4095" /></label>
                <label class="field"><span>按压阈值</span><input id="touchPressThreshold" type="number" min="0" max="4095" /></label>
                <label class="field"><span>采样间隔 ms</span><input id="touchSampleIntervalMs" type="number" min="10" max="1000" /></label>
                <label class="field"><span>长按 ms</span><input id="longPressMs" type="number" min="300" max="10000" step="100" /></label>
              </div>
            </section>
            <section class="panel-section">
              <div class="section-heading"><h2>${icon("vibrate")} 输出</h2></div>
              <div class="range-grid">
                <label class="range-field"><span>屏幕亮度 <strong id="brightness-value">80%</strong></span><input id="screenBrightness" type="range" min="1" max="100" /></label>
                <label class="range-field"><span>马达力度 <strong id="motor-value">45%</strong></span><input id="motorStrength" type="range" min="0" max="100" /></label>
              </div>
              <div class="toggle-row">
                <label class="toggle-field"><input id="imuEnabled" type="checkbox" />${icon("smartphone")}<span>IMU</span></label>
                <label class="toggle-field"><input id="motorEnabled" type="checkbox" />${icon("vibrate")}<span>马达</span></label>
              </div>
            </section>
            <div class="form-actions">
              <button class="secondary-button" id="reset-config" type="button">${icon("rotate-ccw")}<span>重置</span></button>
              <button class="primary-button" type="submit">${icon("save")}<span>保存</span></button>
            </div>
          </form>

          <aside class="diagnostic-panel">
            <section class="panel-section">
              <div class="section-heading"><h2>${icon("cpu")} 诊断</h2><span id="updated-at">--</span></div>
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
              <div class="section-heading"><h2>${icon("zap")} 命令</h2></div>
              <div class="command-grid">
                <button class="secondary-button" id="pulse-motor" type="button">${icon("vibrate")}<span>马达</span></button>
                <button class="secondary-button" id="apply-config" type="button">${icon("check")}<span>应用</span></button>
                <button class="secondary-button" id="dev-preview" type="button">${icon("monitor")}<span>预览</span></button>
              </div>
            </section>
            <section class="panel-section">
              <div class="section-heading"><h2>${icon("map-pin")} 地址</h2></div>
              <div class="url-list" id="url-list"></div>
            </section>
          </aside>
        </div>

        <section class="panel-section asset-section">
          <form class="asset-uploader" id="asset-form">
            <div class="section-heading"><h2>${icon("upload")} 动画文件</h2><span id="asset-state">就绪</span></div>
            <div class="form-grid">
              <label class="field field--wide"><span>文件</span><input name="file" type="file" /></label>
              <label class="field"><span>名称</span><input name="name" type="text" autocomplete="off" /></label>
              <label class="field"><span>类型</span><select name="kind"><option value="sprite">sprite</option><option value="image">image</option><option value="package">package</option></select></label>
              <p class="form-note field--wide">GIF 会自动识别尺寸、帧数和帧延迟，并等比缩放到 Pet2 显示区域。</p>
            </div>
            <div class="form-actions"><button class="primary-button" type="submit">${icon("upload")}<span>上传</span></button></div>
          </form>
          <section class="panel-section">
            <div class="section-heading"><h2>${icon("hard-drive")} 资源清单</h2></div>
            <div class="asset-list" id="asset-list"></div>
          </section>
        </section>
      </section>

      <section data-view="preview" hidden>
        <div class="firmware-preview-workspace">
          <section class="panel-section firmware-preview-panel">
            <div class="section-heading"><h2>${icon("monitor")} Firmware Canvas Preview</h2><span id="firmware-preview-state">renderHome</span></div>
            <div class="firmware-preview-stage">
              <canvas class="firmware-preview-canvas" id="firmware-preview-canvas" width="240" height="240" aria-label="Firmware screen preview"></canvas>
            </div>
            <div class="preview-toolbar">
              <button class="secondary-button" type="button" id="preview-back">${icon("arrow-left")}<span>管理</span></button>
              <button class="secondary-button is-active" type="button" data-preview-screen="home">${icon("home")}<span>Home</span></button>
              <button class="secondary-button" type="button" data-preview-screen="homeFrame">${icon("scan-line")}<span>Frame</span></button>
              <button class="secondary-button" type="button" data-preview-screen="boot">${icon("power")}<span>Boot</span></button>
              <button class="secondary-button" type="button" data-preview-screen="status">${icon("cpu")}<span>Status</span></button>
            </div>
          </section>
          <section class="panel-section">
            <div class="section-heading"><h2>${icon("list-checks")} Mirror Source</h2></div>
            <div class="preview-notes">
              <p>Read-only development preview. It mirrors <code>DisplayDriver.cpp</code>, <code>ScreenRenderer.cpp</code>, <code>glcdfont.h</code>, and <code>magicalmond_ogyg820pt7b.h</code>.</p>
              <p>Canvas coordinates stay at the hardware 240x240 pixel grid. CSS only scales the rendered bitmap.</p>
            </div>
          </section>
        </div>
      </section>
    </main>
  `;
}
