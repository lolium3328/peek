import "./styles.css";
import { icon, refreshIcons } from "./icons";
import { FirmwareScreenPreview, type PreviewScreenMode } from "./screenPreview";
import {
  defaultDeviceConfig,
  defaultScreenLayout,
  type AppSnapshot,
  type AssetManifest,
  type DeviceCommand,
  type DeviceConfig,
  type DeviceSlot,
  type LayoutComponent,
  type LayoutComponentType,
  type ScreenLayout,
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
  storageFree: byId<HTMLElement>("storage-free"),
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
  applyConfig: byId<HTMLButtonElement>("apply-config"),
  assetForm: byId<HTMLFormElement>("asset-form"),
  assetList: byId<HTMLElement>("asset-list"),
  layoutCanvas: byId<HTMLElement>("layout-canvas"),
  componentList: byId<HTMLElement>("component-list"),
  layoutState: byId<HTMLElement>("layout-state"),
  saveLayout: byId<HTMLButtonElement>("save-layout"),
  previewLayout: byId<HTMLButtonElement>("preview-layout"),
  firmwarePreviewCanvas: byId<HTMLCanvasElement>("firmware-preview-canvas"),
  firmwarePreviewState: byId<HTMLElement>("firmware-preview-state")
};

let currentSnapshot: AppSnapshot | null = null;
const firmwarePreview = new FirmwareScreenPreview(refs.firmwarePreviewCanvas);
let assetManifest: AssetManifest = { version: 1, revision: 0, pet2AssetId: null, assets: [] };
let layoutDraft: ScreenLayout = defaultScreenLayout();
let firmwarePreviewMode: PreviewScreenMode = "home";
let socket: WebSocket | null = null;
let socketRetryTimer: number | undefined;
let previewTimer: number | undefined;
let draftDirty = false;
let layoutDirty = false;
let dragging:
  | {
      id: string;
      pointerId: number;
      offsetX: number;
      offsetY: number;
    }
  | null = null;

bindTabs();
bindForm();
bindAssets();
bindLayout();
bindFirmwarePreview();
void loadSnapshot();
connectSocket();
refreshIcons();

function bindTabs() {
  for (const tab of Array.from(document.querySelectorAll<HTMLButtonElement>(".mode-tab"))) {
    tab.addEventListener("click", () => {
      setMode(tab.dataset.mode ?? "config");
    });
  }
}

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

function bindAssets() {
  refs.assetForm.addEventListener("submit", (event) => {
    event.preventDefault();
    void uploadAsset();
  });
}

function bindLayout() {
  for (const button of Array.from(document.querySelectorAll<HTMLButtonElement>("[data-add-component]"))) {
    button.addEventListener("click", () => {
      addComponent(button.dataset.addComponent as LayoutComponentType);
    });
  }

  refs.saveLayout.addEventListener("click", () => {
    void saveLayout();
  });

  refs.previewLayout.addEventListener("click", () => {
    void sendLayoutPreview();
  });

  window.addEventListener("pointermove", (event) => {
    if (!dragging) {
      return;
    }

    const component = layoutDraft.components.find((item) => item.id === dragging?.id);
    if (!component) {
      return;
    }

    const rect = refs.layoutCanvas.getBoundingClientRect();
    component.x = clampToScreen(((event.clientX - rect.left - dragging.offsetX) / rect.width) * 240);
    component.y = clampToScreen(((event.clientY - rect.top - dragging.offsetY) / rect.height) * 240);
    layoutDirty = true;
    renderLayout();
    queueLayoutPreview();
  });

  window.addEventListener("pointerup", (event) => {
    if (dragging?.pointerId === event.pointerId) {
      dragging = null;
    }
  });
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
  firmwarePreview.setMode(firmwarePreviewMode);
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
    setSaveState(result.delivered > 0 ? "已发送" : "等待设备同步");
  } catch (error) {
    setSaveState(errorMessage(error));
  }
}

async function uploadAsset() {
  const formData = new FormData(refs.assetForm);
  const file = formData.get("file");
  if (!(file instanceof File) || file.size === 0) {
    renderAssetError("请选择文件");
    return;
  }

  renderAssetError("上传中");
  try {
    assetManifest = await request<AssetManifest>("/api/assets", {
      method: "POST",
      body: formData
    });
    refs.assetForm.reset();
    renderAssets();
    renderAssetError("已上传");
  } catch (error) {
    renderAssetError(errorMessage(error));
  }
}

async function deleteAsset(id: string) {
  renderAssetError("删除中");
  try {
    assetManifest = await request<AssetManifest>(`/api/assets/${encodeURIComponent(id)}`, {
      method: "DELETE"
    });
    renderAssets();
    renderAssetError("已删除");
  } catch (error) {
    renderAssetError(errorMessage(error));
  }
}

async function saveLayout() {
  refs.layoutState.textContent = "保存中";
  try {
    layoutDraft = await request<ScreenLayout>("/api/layout", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(layoutDraft)
    });
    layoutDirty = false;
    renderLayout();
    refs.layoutState.textContent = "已保存";
  } catch (error) {
    refs.layoutState.textContent = errorMessage(error);
  }
}

async function sendLayoutPreview() {
  refs.layoutState.textContent = "预览中";
  try {
    await request<ScreenLayout>("/api/layout/preview", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(layoutDraft)
    });
    refs.layoutState.textContent = "已下发预览";
  } catch (error) {
    refs.layoutState.textContent = errorMessage(error);
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
    } else if (message.type === "layout" && !layoutDirty) {
      layoutDraft = structuredClone(message.layout);
      renderLayout();
    } else if (message.type === "assets") {
      assetManifest = message.assets;
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

  if (!layoutDirty) {
    layoutDraft = structuredClone(snapshot.layout);
  }

  if (!draftDirty && !refs.form.matches(":focus-within")) {
    fillConfigForm(snapshot.config);
  }

  renderStatus(snapshot);
  renderUrls(snapshot.lanUrls);
  renderAssets();
  renderLayout();
  firmwarePreview.setSnapshot(snapshot);
}

async function setPet2Asset(id: string) {
  renderAssetError("设置中");
  try {
    assetManifest = await request<AssetManifest>(`/api/assets/${encodeURIComponent(id)}/pet2`, {
      method: "POST"
    });
    renderAssets();
    renderAssetError("已设为 Pet2");
  } catch (error) {
    renderAssetError(errorMessage(error));
  }
}

function fillConfigForm(config: DeviceConfig) {
  setInput("deviceName", config.deviceName);
  setInput("deviceId", config.deviceId);
  setInput("deviceToken", config.deviceToken);
  setInput("pairSlot", config.pairSlot);
  setInput("weatherCity", config.weatherCity);
  setInput("wifiSsid", config.wifiSsid);
  setInput("wifiPassword", config.wifiPassword);
  setInput("backendUrl", config.backendUrl);
  setInput("backendPollIntervalMs", String(config.backendPollIntervalMs));
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
    deviceId: getInput("deviceId").value,
    deviceToken: getInput("deviceToken").value,
    pairSlot: getInput("pairSlot").value as DeviceSlot,
    weatherCity: getInput("weatherCity").value,
    wifiSsid: getInput("wifiSsid").value,
    wifiPassword: getInput("wifiPassword").value,
    backendUrl: getInput("backendUrl").value,
    backendPollIntervalMs: getNumber("backendPollIntervalMs"),
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
  refs.storageFree.textContent = storageLabel(status.storage.freeBytes, status.storage.totalBytes);
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

function renderAssets() {
  if (assetManifest.assets.length === 0) {
    refs.assetList.innerHTML = `<p class="empty-row">暂无动画资源</p>`;
    return;
  }

  refs.assetList.replaceChildren(
    ...assetManifest.assets.map((asset) => {
      const row = document.createElement("article");
      row.className = "asset-row";
      row.innerHTML = `
        <div>
          <strong>${escapeHtml(asset.name)}</strong>
          <span>${assetMetaLabel(asset)}</span>
          <span>${storageFitLabel(asset)}</span>
        </div>
        <div>
          <em>${formatBytes(asset.encodedSize ?? asset.size)}</em>
          <button class="secondary-button asset-action" type="button" data-action="pet2">
            ${assetManifest.pet2AssetId === asset.id ? "Pet2" : "设为 Pet2"}
          </button>
          <button class="icon-button" type="button" title="删除">${icon("trash-2")}</button>
        </div>
      `;
      row.querySelector<HTMLButtonElement>("[data-action='pet2']")?.addEventListener("click", () => {
        void setPet2Asset(asset.id);
      });
      row.querySelector<HTMLButtonElement>(".icon-button")?.addEventListener("click", () => {
        void deleteAsset(asset.id);
      });
      return row;
    })
  );
  refreshIcons();
}

function renderAssetError(text: string) {
  byId<HTMLElement>("asset-state").textContent = text;
}

function renderLayout() {
  refs.layoutCanvas.replaceChildren(
    ...layoutDraft.components.map((component) => renderLayoutComponent(component))
  );
  refs.componentList.replaceChildren(
    ...layoutDraft.components.map((component) => renderComponentRow(component))
  );
  refs.layoutState.textContent = layoutDirty ? "未保存" : "就绪";
  refreshIcons();
}

function renderLayoutComponent(component: LayoutComponent) {
  const node = document.createElement("button");
  node.type = "button";
  node.className = `layout-component layout-component--${component.type}`;
  node.dataset.componentId = component.id;
  node.style.left = `${(component.x / 240) * 100}%`;
  node.style.top = `${(component.y / 240) * 100}%`;
  node.title = component.label;

  if (component.type === "arc") {
    const radius = component.radius ?? 96;
    node.style.width = `${(radius * 2 / 240) * 100}%`;
    node.style.height = `${(radius * 2 / 240) * 100}%`;
    node.style.borderColor = component.color ?? "#46c7a5";
    node.textContent = component.label;
  } else if (component.type === "sprite") {
    const asset = assetManifest.assets.find((item) => item.id === component.assetId);
    node.style.width = `${((component.width ?? 64) / 240) * 100}%`;
    node.style.height = `${((component.height ?? 64) / 240) * 100}%`;
    node.textContent = asset ? asset.name.slice(0, 8) : component.label;
  } else if (component.type === "text") {
    node.textContent = component.text || component.label;
  } else if (component.type === "statusDot") {
    node.textContent = "";
  } else {
    node.innerHTML = `<span class="preview-cube"></span>`;
  }

  node.addEventListener("pointerdown", (event) => {
    const rect = refs.layoutCanvas.getBoundingClientRect();
    dragging = {
      id: component.id,
      pointerId: event.pointerId,
      offsetX: event.clientX - rect.left - (component.x / 240) * rect.width,
      offsetY: event.clientY - rect.top - (component.y / 240) * rect.height
    };
    node.setPointerCapture(event.pointerId);
  });

  return node;
}

function renderComponentRow(component: LayoutComponent) {
  const row = document.createElement("article");
  row.className = "component-row";
  row.innerHTML = `
    <strong>${escapeHtml(component.label)}</strong>
    <span>${component.type}</span>
    <label>X <input type="number" min="0" max="240" value="${component.x}" data-axis="x" /></label>
    <label>Y <input type="number" min="0" max="240" value="${component.y}" data-axis="y" /></label>
    <button class="icon-button" type="button" title="删除">${icon("trash-2")}</button>
  `;

  for (const input of Array.from(row.querySelectorAll<HTMLInputElement>("input"))) {
    input.addEventListener("input", () => {
      const axis = input.dataset.axis === "x" ? "x" : "y";
      component[axis] = clampToScreen(Number(input.value));
      layoutDirty = true;
      renderLayout();
      queueLayoutPreview();
    });
  }

  row.querySelector("button")?.addEventListener("click", () => {
    layoutDraft.components = layoutDraft.components.filter((item) => item.id !== component.id);
    layoutDirty = true;
    renderLayout();
    queueLayoutPreview();
  });

  return row;
}

function addComponent(type: LayoutComponentType) {
  const id = `${type}-${Date.now().toString(36)}`;
  const component: LayoutComponent = {
    id,
    type,
    label: type,
    x: 120,
    y: 120,
    width: type === "sprite" ? 64 : undefined,
    height: type === "sprite" ? 64 : undefined,
    radius: type === "arc" ? 96 : undefined,
    startAngle: type === "arc" ? 135 : undefined,
    endAngle: type === "arc" ? 225 : undefined,
    text: type === "text" ? "Peek" : undefined,
    color: type === "arc" || type === "statusDot" ? "#46c7a5" : undefined
  };
  layoutDraft.components.push(component);
  layoutDirty = true;
  renderLayout();
  queueLayoutPreview();
}

function queueLayoutPreview() {
  window.clearTimeout(previewTimer);
  previewTimer = window.setTimeout(() => {
    void sendLayoutPreview();
  }, 220);
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

function setMode(mode: string) {
  for (const tab of Array.from(document.querySelectorAll<HTMLButtonElement>(".mode-tab"))) {
    tab.classList.toggle("is-active", tab.dataset.mode === mode);
  }
  for (const view of Array.from(document.querySelectorAll<HTMLElement>("[data-view]"))) {
    view.hidden = view.dataset.view !== mode;
  }
}

function previewModeLabel(mode: PreviewScreenMode) {
  if (mode === "homeFrame") {
    return "renderHomeFrame";
  }
  if (mode === "boot") {
    return "renderBoot";
  }
  if (mode === "status") {
    return "renderStatus";
  }
  return "renderHome";
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

function clampToScreen(value: number) {
  return Math.min(Math.max(Math.round(value), 0), 240);
}

function formatBytes(value: number) {
  if (value < 1024) {
    return `${value} B`;
  }
  if (value < 1024 * 1024) {
    return `${(value / 1024).toFixed(1)} KB`;
  }
  return `${(value / 1024 / 1024).toFixed(1)} MB`;
}

function storageLabel(freeBytes: number | null, totalBytes: number | null) {
  if (freeBytes === null || totalBytes === null) {
    return "--";
  }
  return `${formatBytes(freeBytes)} / ${formatBytes(totalBytes)}`;
}

function assetMetaLabel(asset: AssetManifest["assets"][number]) {
  const sourceSize =
    asset.sourceWidth && asset.sourceHeight ? `${asset.sourceWidth}x${asset.sourceHeight}` : `${asset.width}x${asset.height}`;
  const deviceSize =
    asset.deviceWidth && asset.deviceHeight ? ` -> ${asset.deviceWidth}x${asset.deviceHeight}` : "";
  return `${asset.kind} · ${asset.format}${deviceSize ? ` · ${sourceSize}${deviceSize}` : ` · ${sourceSize}`} · ${asset.frames} 帧 · ${asset.fps} fps`;
}

function storageFitLabel(asset: AssetManifest["assets"][number]) {
  const encodedSize = asset.encodedSize ?? asset.size;
  const storage = currentSnapshot?.status.storage;
  if (!storage || storage.freeBytes === null) {
    return "设备空间未确认";
  }
  const reserveBytes = 128 * 1024;
  const available = Math.max(0, storage.freeBytes - reserveBytes);
  return encodedSize <= available ? `设备可下载 · 预留 ${formatBytes(reserveBytes)}` : "设备空间不足";
}

function escapeHtml(value: string) {
  return value.replace(/[&<>"']/g, (char) => {
    const map: Record<string, string> = {
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      "\"": "&quot;",
      "'": "&#39;"
    };
    return map[char] ?? char;
  });
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
        <button class="mode-tab is-active" type="button" data-mode="config">${icon("settings")}<span>配置</span></button>
        <button class="mode-tab" type="button" data-mode="preview">${icon("monitor")}<span>Preview</span></button>
        <button class="mode-tab" type="button" data-mode="assets">${icon("image")}<span>动画</span></button>
        <button class="mode-tab" type="button" data-mode="layout">${icon("layout-dashboard")}<span>布局</span></button>
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

      <section data-view="config">
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
                  <span>设备 ID</span>
                  <input id="deviceId" type="text" autocomplete="off" />
                </label>
                <label class="field">
                  <span>令牌</span>
                  <input id="deviceToken" type="password" autocomplete="off" />
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
                <label class="field">
                  <span>Wi-Fi 密码</span>
                  <input id="wifiPassword" type="password" autocomplete="off" />
                </label>
                <label class="field">
                  <span>同步间隔 ms</span>
                  <input id="backendPollIntervalMs" type="number" min="1000" max="600000" step="1000" />
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
      </section>

      <section data-view="preview" hidden>
        <div class="firmware-preview-workspace">
          <section class="panel-section firmware-preview-panel">
            <div class="section-heading">
              <h2>${icon("monitor")} Firmware Canvas Preview</h2>
              <span id="firmware-preview-state">renderHome</span>
            </div>
            <div class="firmware-preview-stage">
              <canvas
                class="firmware-preview-canvas"
                id="firmware-preview-canvas"
                width="240"
                height="240"
                aria-label="Firmware screen preview"
              ></canvas>
            </div>
            <div class="layout-toolbar">
              <button class="secondary-button is-active" type="button" data-preview-screen="home">${icon("home")}<span>Home</span></button>
              <button class="secondary-button" type="button" data-preview-screen="homeFrame">${icon("scan-line")}<span>Frame</span></button>
              <button class="secondary-button" type="button" data-preview-screen="boot">${icon("power")}<span>Boot</span></button>
              <button class="secondary-button" type="button" data-preview-screen="status">${icon("cpu")}<span>Status</span></button>
            </div>
          </section>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("list-checks")} Mirror Source</h2>
            </div>
            <div class="preview-notes">
              <p>Read-only development preview. It mirrors <code>DisplayDriver.cpp</code>, <code>ScreenRenderer.cpp</code>, <code>glcdfont.h</code>, and <code>magicalmond_ogyg820pt7b.h</code>.</p>
              <p>Canvas coordinates stay at the hardware 240x240 pixel grid. CSS only scales the rendered bitmap.</p>
            </div>
          </section>
        </div>
      </section>

      <section data-view="assets" hidden>
        <div class="asset-workspace">
          <form class="panel-section asset-uploader" id="asset-form">
            <div class="section-heading">
              <h2>${icon("upload")} 动画文件</h2>
              <span id="asset-state">就绪</span>
            </div>
            <div class="form-grid">
              <label class="field field--wide">
                <span>文件</span>
                <input name="file" type="file" />
              </label>
              <label class="field">
                <span>名称</span>
                <input name="name" type="text" autocomplete="off" />
              </label>
              <label class="field">
                <span>类型</span>
                <select name="kind">
                  <option value="sprite">sprite</option>
                  <option value="image">image</option>
                  <option value="package">package</option>
                </select>
              </label>
              <p class="form-note field--wide">GIF 会自动识别尺寸、帧数和帧延迟，并等比缩放到 Pet2 显示区域。</p>
            </div>
            <div class="form-actions">
              <button class="primary-button" type="submit">${icon("upload")}<span>上传</span></button>
            </div>
          </form>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("hard-drive")} 资源清单</h2>
            </div>
            <div class="asset-list" id="asset-list"></div>
          </section>
        </div>
      </section>

      <section data-view="layout" hidden>
        <div class="layout-workspace">
          <section class="panel-section layout-preview-panel">
            <div class="section-heading">
              <h2>${icon("monitor")} 屏幕布局</h2>
              <span id="layout-state">就绪</span>
            </div>
            <div class="layout-editor">
              <div class="screen-preview" id="layout-canvas" aria-label="屏幕预览"></div>
            </div>
            <div class="layout-toolbar">
              <button class="secondary-button" type="button" data-add-component="cube">${icon("box")}<span>立方体</span></button>
              <button class="secondary-button" type="button" data-add-component="sprite">${icon("image")}<span>动画</span></button>
              <button class="secondary-button" type="button" data-add-component="arc">${icon("activity")}<span>圆弧</span></button>
              <button class="secondary-button" type="button" data-add-component="text">${icon("type")}<span>文本</span></button>
              <button class="secondary-button" type="button" id="preview-layout">${icon("radio")}<span>预览</span></button>
              <button class="primary-button" type="button" id="save-layout">${icon("save")}<span>保存</span></button>
            </div>
          </section>

          <section class="panel-section">
            <div class="section-heading">
              <h2>${icon("layers")} 组件</h2>
            </div>
            <div class="component-list" id="component-list"></div>
          </section>
        </div>
      </section>
    </main>
  `;
}
