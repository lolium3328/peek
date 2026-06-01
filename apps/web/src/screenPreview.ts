import magicalmondHeader from "../../../include/assets/fonts/magicalmond_ogyg820pt7b.h?raw";
import { glcdFont } from "./glcdfont";
import {
  CanvasDisplayDriver,
  kBlack, kWhite, kMuted, kLine, kPanel, kGreen, kBlue, kAmber, kRed,
  kScreenSize, stateColor, batteryColor, DEG_TO_RAD,
  type GfxGlyph
} from "./canvas";
import type { AppSnapshot } from "./shared";

export type PreviewScreenMode = "home" | "homeFrame" | "boot" | "status";

type DisplayTextStyle = "Small" | "Primary";

interface HomeScreenModel {
  primaryText: string;
  hintText: string;
  localWeather: string;
  peerWeather: string;
  localLabel: string;
  peerLabel: string;
  localBatteryPercent: number;
  peerBatteryPercent: number;
  wifiConnected: boolean;
  backendConnected: boolean;
  poseAlert: boolean;
  cubeVisible: boolean;
  cubeRollDeg: number;
  cubePitchDeg: number;
  cubeYawDeg: number;
  cubeOffsetX: number;
  cubeOffsetY: number;
  cubeScale: number;
}

interface BootScreenModel {
  title: string;
  message: string;
}

interface StatusScreenModel {
  buttonPressed: boolean;
  wifiRssi: number;
  localBatteryPercent: number;
  peerBatteryPercent: number;
  backendConnected: boolean;
  imuReady: boolean;
  imuAddress: number;
  imuAccelZ: number;
  imuRollDeg: number;
  imuPitchDeg: number;
}

const kScreenCenter = 120;
const kCubeCenterY = kScreenCenter;
const kPetAreaX = 61;
const kPetAreaY = 61;
const kPetAreaSize = 118;
const kDefaultCubeScale = 32.0;

const magicalmond = parseGfxFont(magicalmondHeader);

export class FirmwareScreenPreview {
  private readonly display: CanvasDisplayDriver;
  private mode: PreviewScreenMode = "home";
  private snapshot: AppSnapshot | null = null;

  constructor(canvas: HTMLCanvasElement) {
    canvas.width = kScreenSize;
    canvas.height = kScreenSize;
    const context = canvas.getContext("2d");
    if (!context) throw new Error("Missing canvas context");
    context.imageSmoothingEnabled = false;
    this.display = new CanvasDisplayDriver(context);
    this.display.setGfxFont(magicalmond.glyphs, magicalmond.bitmaps, magicalmond.first, magicalmond.last, magicalmond.yAdvance);
    this.render();
  }

  setMode(mode: PreviewScreenMode) { this.mode = mode; this.render(); }
  setSnapshot(snapshot: AppSnapshot | null) { this.snapshot = snapshot; this.render(); }

  render() {
    const snapshot = this.snapshot;
    if (this.mode === "boot") {
      this.renderBoot({ title: "Peek", message: snapshot?.status.connected ? "imu ok" : "imu missing" });
      return;
    }
    if (this.mode === "status") {
      this.renderStatus(toStatusScreenModel(snapshot));
      return;
    }
    const model = toHomeScreenModel(snapshot);
    this.renderHome(model);
    if (this.mode === "homeFrame") this.renderHomeFrame(model);
  }

  private renderBoot(model: BootScreenModel) {
    this.display.clear(kBlack);
    this.display.drawCircle(kScreenCenter, kScreenCenter, 110, kLine);
    this.drawTextCentered(model.title, 105, "Primary", kWhite);
    this.drawStatusPill(82, 145, model.message, kBlue);
  }

  private renderHome(model: HomeScreenModel) {
    this.display.clear(kBlack);
    this.display.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
    this.display.drawCircle(kScreenCenter, kScreenCenter, 88, kLine);
    this.display.drawCircle(kScreenCenter, kScreenCenter, 89, 0x0841);
    this.drawTopStatus(model);
    if (model.poseAlert) this.display.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
    if (model.cubeVisible) {
      this.drawPetCube(model);
    } else {
      this.drawTextCentered(model.primaryText, 122, "Primary", kWhite);
    }
    this.drawBottomHint(model.hintText);
  }

  private renderHomeFrame(model: HomeScreenModel) {
    this.clearPetArea();
    if (model.poseAlert) this.display.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
    if (model.cubeVisible) {
      this.drawPetCube(model);
    } else {
      this.drawTextCentered(model.primaryText, 122, "Primary", kWhite);
    }
  }

  private renderStatus(model: StatusScreenModel) {
    this.display.clear(kBlack);
    this.display.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
    this.drawTextCentered("status", 48, "Small", kBlue);
    this.drawTextCentered(`button ${model.buttonPressed ? "down" : "up"}`, 76, "Small", kWhite);
    this.drawTextCentered(model.imuReady
      ? `imu ok 0x${model.imuAddress.toString(16).toUpperCase().padStart(2, "0")}`
      : "imu missing", 99, "Small", stateColor(model.imuReady));
    this.drawTextCentered(`az ${model.imuAccelZ}`, 122, "Small", kWhite);
    this.drawTextCentered(`rp ${Math.round(model.imuRollDeg)} ${Math.round(model.imuPitchDeg)}`, 145, "Small", kWhite);
    this.drawTextCentered(`rssi ${model.wifiRssi}`, 161, "Small", kWhite);
    this.drawTextCentered(`A ${model.localBatteryPercent}%`, 184, "Small", batteryColor(model.localBatteryPercent));
    this.drawTextCentered(`B ${model.peerBatteryPercent}%`, 199, "Small", batteryColor(model.peerBatteryPercent));
    this.drawStatusPill(78, 211, model.backendConnected ? "backend ok" : "backend off", stateColor(model.backendConnected));
  }

  private drawTextCentered(text: string, centerY: number, style: DisplayTextStyle, color: number) {
    const bounds = style === "Primary"
      ? this.display.gfxTextBounds(text, 0, 0)
      : this.display.glcdTextBounds(text, 0, 0);
    const x = Math.trunc((kScreenSize - bounds.w) / 2) - bounds.x1;
    const y = centerY - Math.trunc(bounds.h / 2) - bounds.y1;
    if (style === "Primary") {
      this.display.gfxDrawText(text, x, y);
    } else {
      this.display.glcdDrawText(text, x, y, glcdFont);
    }
  }

  private drawTopStatus(model: HomeScreenModel) {
    this.drawWeatherChip(41, model.localLabel, model.localWeather);
    this.drawWeatherChip(151, model.peerLabel, model.peerWeather);
    this.drawConnectionDots(model.wifiConnected, model.backendConnected);
  }

  private clearPetArea() {
    this.display.fillRect(kPetAreaX, kPetAreaY, kPetAreaSize, kPetAreaSize, kBlack);
  }

  private drawPetCube(model: HomeScreenModel) {
    const vertices: [number, number, number][] = [
      [-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1],
      [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1]
    ];
    const edges: [number, number][] = [
      [0, 1], [1, 2], [2, 3], [3, 0], [4, 5], [5, 6], [6, 7], [7, 4], [0, 4], [1, 5], [2, 6], [3, 7]
    ];
    const roll = model.cubeRollDeg * DEG_TO_RAD, pitch = model.cubePitchDeg * DEG_TO_RAD, yaw = model.cubeYawDeg * DEG_TO_RAD;
    const sr = Math.sin(roll), cr = Math.cos(roll), sp = Math.sin(pitch), cp = Math.cos(pitch), sy = Math.sin(yaw), cy = Math.cos(yaw);
    const scale = model.cubeScale > 0 ? model.cubeScale : kDefaultCubeScale;
    const cx = kScreenCenter + Math.round(model.cubeOffsetX), cy = kCubeCenterY + Math.round(model.cubeOffsetY);
    const points = vertices.map(([x, y_, z]) => {
      const yRoll = y_ * cr - z * sr, zRoll = y_ * sr + z * cr;
      const xPitch = x * cp + zRoll * sp, zPitch = -x * sp + zRoll * cp;
      const xYaw = xPitch * cy - yRoll * sy, yYaw = xPitch * sy + yRoll * cy;
      return { x: cx + Math.round(xYaw * scale), y: cy + Math.round(yYaw * scale), z: zPitch };
    });
    for (const [from, to] of edges) {
      const a = points[from], b = points[to];
      this.display.drawLine(a.x, a.y, b.x, b.y, a.z + b.z > 0 ? kGreen : kMuted);
    }
  }

  private drawWeatherChip(x: number, label: string, weather: string) {
    this.display.fillRoundRect(x, 35, 48, 22, 9, kPanel);
    this.display.drawRoundRect(x, 35, 48, 22, 9, kLine);
    this.display.glcdDrawText(label, x + 7, 50, glcdFont);
    this.display.glcdDrawText(weather, x + 24, 50, glcdFont);
  }

  private drawConnectionDots(wifiConnected: boolean, backendConnected: boolean) {
    this.display.fillCircle(kScreenCenter - 7, 46, 3, stateColor(wifiConnected));
    this.display.fillCircle(kScreenCenter + 7, 46, 3, stateColor(backendConnected));
    this.display.drawLine(kScreenCenter - 3, 46, kScreenCenter + 3, 46, kLine);
  }

  private drawBottomHint(hintText: string) {
    this.display.fillRoundRect(58, 179, 124, 24, 10, kPanel);
    this.display.drawRoundRect(58, 179, 124, 24, 10, kLine);
    this.drawTextCentered(hintText, 195, "Small", kMuted);
  }

  private drawStatusPill(x: number, y: number, text: string, color: number) {
    this.display.fillRoundRect(x, y, 84, 23, 10, kPanel);
    this.display.drawRoundRect(x, y, 84, 23, 10, kLine);
    this.display.fillCircle(x + 12, y + 11, 3, color);
    this.display.glcdDrawText(text, x + 22, y + 15, glcdFont);
  }
}

function toHomeScreenModel(snapshot: AppSnapshot | null): HomeScreenModel {
  const status = snapshot?.status;
  const poseValid = status?.imu.pitch !== null && status?.imu.roll !== null && status?.imu.yaw !== null;
  return {
    primaryText: poseValid ? "zzz..." : "imu?",
    hintText: "sleeping",
    localWeather: "--",
    peerWeather: "--",
    localLabel: "A",
    peerLabel: "B",
    localBatteryPercent: 92,
    peerBatteryPercent: 79,
    wifiConnected: Boolean(status?.connected),
    backendConnected: Boolean(status?.connected && snapshot?.config.backendUrl),
    poseAlert: false,
    cubeVisible: poseValid,
    cubeRollDeg: status?.imu.roll ?? 0,
    cubePitchDeg: status?.imu.pitch ?? 0,
    cubeYawDeg: status?.imu.yaw ?? 0,
    cubeOffsetX: 0,
    cubeOffsetY: 0,
    cubeScale: kDefaultCubeScale
  };
}

function toStatusScreenModel(snapshot: AppSnapshot | null): StatusScreenModel {
  const status = snapshot?.status;
  const imuReady = status?.imu.pitch !== null && status?.imu.roll !== null;
  return {
    buttonPressed: false,
    wifiRssi: Math.trunc(status?.wifiRssi ?? 0),
    localBatteryPercent: 92,
    peerBatteryPercent: 79,
    backendConnected: Boolean(status?.connected && snapshot?.config.backendUrl),
    imuReady,
    imuAddress: 0,
    imuAccelZ: 0,
    imuRollDeg: status?.imu.roll ?? 0,
    imuPitchDeg: status?.imu.pitch ?? 0
  };
}

function parseGfxFont(header: string): {
  bitmaps: number[]; glyphs: GfxGlyph[]; first: number; last: number; yAdvance: number;
} {
  const bitmapBlock = extractBlock(header, "magicalmond_ogyg820pt7bBitmaps");
  const glyphBlock = extractBlock(header, "magicalmond_ogyg820pt7bGlyphs");
  const fontMatch = /0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*(\d+)\s*\}/.exec(header);
  return {
    bitmaps: parseHexNumbers(bitmapBlock),
    glyphs: Array.from(glyphBlock.matchAll(/\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}/g))
      .map(([, bo, w, h, xa, xo, yo]) => ({
        bitmapOffset: Number(bo), width: Number(w), height: Number(h),
        xAdvance: Number(xa), xOffset: Number(xo), yOffset: Number(yo)
      })),
    first: fontMatch ? Number.parseInt(fontMatch[1], 16) : 0x20,
    last: fontMatch ? Number.parseInt(fontMatch[2], 16) : 0x7e,
    yAdvance: fontMatch ? Number(fontMatch[3]) : 48
  };
}

function extractBlock(header: string, name: string) {
  const start = header.indexOf(`${name}[]`);
  if (start === -1) throw new Error(`Missing ${name}`);
  const open = header.indexOf("{", start), close = header.indexOf("};", open);
  return header.slice(open, close);
}

function parseHexNumbers(value: string) {
  return Array.from(value.matchAll(/0x([0-9A-Fa-f]{2})/g), ([, hex]) => Number.parseInt(hex, 16));
}
