import { icon, refreshIcons } from "./icons";
import {
  defaultDeviceConfig,
  type AppSnapshot,
  type AssetManifest,
  type DeviceCommand,
  type DeviceConfig,
  type DeviceSlot
} from "./shared";
import { byId, request, formatBytes, escapeHtml, errorMessage } from "./util";

export interface ManageRefs {
  form: HTMLFormElement;
  resetButton: HTMLButtonElement;
  saveState: HTMLElement;
  brightnessValue: HTMLElement;
  motorValue: HTMLElement;
  pulseMotor: HTMLButtonElement;
  applyConfig: HTMLButtonElement;
  assetForm: HTMLFormElement;
  assetList: HTMLElement;
}

let refs: ManageRefs;
let assetManifest: AssetManifest;
let currentSnapshot: AppSnapshot | null;
let draftDirty = false;

export function initManage(r: ManageRefs, initialAssetManifest: AssetManifest, initialSnapshot: AppSnapshot | null) {
  refs = r;
  assetManifest = initialAssetManifest;
  currentSnapshot = initialSnapshot;
  bindForm();
  bindAssets();
}

export function setAssetManifest(m: AssetManifest) { assetManifest = m; }
export function setCurrentSnapshot(s: AppSnapshot | null) { currentSnapshot = s; }
export function getDraftDirty() { return draftDirty; }
export function setDraftDirty(v: boolean) { draftDirty = v; }
export function getAssetManifest() { return assetManifest; }
export function getCurrentSnapshot() { return currentSnapshot; }

export function applyManageSnapshot(snapshot: AppSnapshot) {
  currentSnapshot = snapshot;
  assetManifest = snapshot.assets;
  if (!draftDirty && !refs.form.matches(":focus-within")) {
    fillConfigForm(snapshot.config);
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

  byId<HTMLButtonElement>("dev-preview").addEventListener("click", () => {
    setMode("preview");
  });

  syncRangeLabels();
}

function bindAssets() {
  refs.assetForm.addEventListener("submit", (event) => {
    event.preventDefault();
    void uploadAsset();
  });
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
    const config = await request<DeviceConfig>("/api/config/reset", { method: "POST" });
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
    assetManifest = await request<AssetManifest>("/api/assets", { method: "POST", body: formData });
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
    assetManifest = await request<AssetManifest>(`/api/assets/${encodeURIComponent(id)}`, { method: "DELETE" });
    renderAssets();
    renderAssetError("已删除");
  } catch (error) {
    renderAssetError(errorMessage(error));
  }
}

async function setPet2Asset(id: string) {
  renderAssetError("设置中");
  try {
    assetManifest = await request<AssetManifest>(`/api/assets/${encodeURIComponent(id)}/pet2`, { method: "POST" });
    renderAssets();
    renderAssetError("已设为 Pet2");
  } catch (error) {
    renderAssetError(errorMessage(error));
  }
}

export function fillConfigForm(config: DeviceConfig) {
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

export function renderAssets() {
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

function syncRangeLabels() {
  refs.brightnessValue.textContent = `${getNumber("screenBrightness")}%`;
  refs.motorValue.textContent = `${getNumber("motorStrength")}%`;
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

function minutesToTimeInput(minutes: number) {
  const hour = Math.floor(minutes / 60);
  const minute = minutes % 60;
  return `${String(hour).padStart(2, "0")}:${String(minute).padStart(2, "0")}`;
}

function timeInputToMinutes(value: string) {
  const [hour = "0", minute = "0"] = value.split(":");
  return Number(hour) * 60 + Number(minute);
}

function assetMetaLabel(asset: AssetManifest["assets"][number]) {
  const sourceSize = asset.sourceWidth && asset.sourceHeight
    ? `${asset.sourceWidth}x${asset.sourceHeight}`
    : `${asset.width}x${asset.height}`;
  const deviceSize = asset.deviceWidth && asset.deviceHeight
    ? ` -> ${asset.deviceWidth}x${asset.deviceHeight}`
    : "";
  return `${asset.kind} · ${asset.format}${deviceSize ? ` · ${sourceSize}${deviceSize}` : ` · ${sourceSize}`} · ${asset.frames} 帧 · ${asset.fps} fps`;
}

function storageFitLabel(asset: AssetManifest["assets"][number]) {
  const encodedSize = asset.encodedSize ?? asset.size;
  const storage = currentSnapshot?.status.storage;
  if (!storage || storage.freeBytes === null) return "设备空间未确认";
  const reserveBytes = 128 * 1024;
  const available = Math.max(0, storage.freeBytes - reserveBytes);
  return encodedSize <= available ? `设备可下载 · 预留 ${formatBytes(reserveBytes)}` : "设备空间不足";
}
