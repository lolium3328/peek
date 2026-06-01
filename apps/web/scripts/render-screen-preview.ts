#!/usr/bin/env bun
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { deflateSync } from "node:zlib";

import { glcdFont } from "../src/glcdfont";
import type { AppSnapshot } from "../src/shared";

type PreviewScreenMode = "home" | "homeFrame" | "boot" | "status";
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

interface GfxGlyph {
  bitmapOffset: number;
  width: number;
  height: number;
  xAdvance: number;
  xOffset: number;
  yOffset: number;
}

interface GfxFont {
  bitmaps: number[];
  glyphs: GfxGlyph[];
  first: number;
  last: number;
  yAdvance: number;
}

interface TextBounds {
  x1: number;
  y1: number;
  w: number;
  h: number;
}

const DEG_TO_RAD = Math.PI / 180;
const kScreenSize = 240;
const kScreenCenter = 120;
const kBatteryArcRadius = 109;
const kLeftBatteryStartDeg = 142;
const kRightBatteryStartDeg = 38;
const kBatteryArcSweepDeg = 76;
const kBatteryArcThickness = 1;
const kBatteryTrackThickness = 1;
const kArcStepDeg = 1;
const kBatteryTrackColor = 0x18e3;
const kBatteryHighColor = 0x05f4;
const kBatteryMidColor = 0xfdc0;
const kBatteryLowColor = 0xf9c6;

const kBlack = 0x0000;
const kWhite = 0xffff;
const kMuted = 0x8c71;
const kLine = 0x2945;
const kPanel = 0x1082;
const kGreen = 0x05f4;
const kBlue = 0x3d7f;
const kAmber = 0xfdc0;
const kRed = 0xf9c6;
const kCubeCenterY = kScreenCenter;
const kPetAreaX = 61;
const kPetAreaY = 61;
const kPetAreaSize = 118;
const kDefaultCubeScale = 32.0;

const modes = ["home", "homeFrame", "boot", "status"] as const;
const scriptDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = normalize(join(scriptDir, "..", "..", ".."));
const fontHeaderPath = join(repoRoot, "include", "assets", "fonts", "magicalmond_ogyg820pt7b.h");
const magicalmond = parseGfxFont(readFileSync(fontHeaderPath, "utf8"));

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const snapshot = args.snapshot ? readSnapshot(args.snapshot) : null;
  const selectedMode = args.mode ?? await chooseMode();
  const selectedModes = selectedMode === "all" ? modes : [selectedMode];
  const outDir = args.out && selectedModes.length > 1 ? args.out : args.out ? dirname(args.out) : join(repoRoot, ".peek-preview");

  mkdirSync(outDir, { recursive: true });

  const outputs = selectedModes.map((mode) => {
    const outputPath = args.out && selectedModes.length === 1 ? args.out : join(outDir, `${mode}.png`);
    const display = new PngDisplayDriver();
    const preview = new FirmwareScreenPreview(display);
    preview.setSnapshot(snapshot);
    preview.setMode(mode);
    writeFileSync(outputPath, display.toPng());
    return outputPath;
  });

  console.log(`Generated ${outputs.length} firmware screen preview${outputs.length === 1 ? "" : "s"}:`);
  for (const output of outputs) {
    console.log(`- ${output}`);
  }
}

class FirmwareScreenPreview {
  private mode: PreviewScreenMode = "home";
  private snapshot: AppSnapshot | null = null;

  constructor(private readonly display: PngDisplayDriver) {
    this.render();
  }

  setMode(mode: PreviewScreenMode) {
    this.mode = mode;
    this.render();
  }

  setSnapshot(snapshot: AppSnapshot | null) {
    this.snapshot = snapshot;
    this.render();
  }

  private render() {
    const snapshot = this.snapshot;
    if (this.mode === "boot") {
      this.renderBoot({
        title: "Peek",
        message: snapshot?.status.connected ? "imu ok" : "imu missing"
      });
      return;
    }

    if (this.mode === "status") {
      this.renderStatus(toStatusScreenModel(snapshot));
      return;
    }

    const model = toHomeScreenModel(snapshot);
    if (this.mode === "homeFrame") {
      this.renderHome(model);
      this.renderHomeFrame(model);
      return;
    }
    this.renderHome(model);
  }

  private renderBoot(model: BootScreenModel) {
    this.display.clear(kBlack);
    this.display.drawCircle(kScreenCenter, kScreenCenter, 110, kLine);
    this.display.drawTextCentered(model.title, 105, "Primary", kWhite);
    this.drawStatusPill(82, 145, model.message, kBlue);
  }

  private renderHome(model: HomeScreenModel) {
    this.display.clear(kBlack);
    this.display.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
    this.display.drawCircle(kScreenCenter, kScreenCenter, 88, kLine);
    this.display.drawCircle(kScreenCenter, kScreenCenter, 89, 0x0841);
    this.drawTopStatus(model);

    if (model.poseAlert) {
      this.display.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
    }

    if (model.cubeVisible) {
      this.drawPetCube(model);
    } else {
      this.display.drawTextCentered(model.primaryText, 122, "Primary", kWhite);
    }
    this.drawBottomHint(model.hintText);
  }

  private renderHomeFrame(model: HomeScreenModel) {
    this.clearPetArea();

    if (model.poseAlert) {
      this.display.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
    }

    if (model.cubeVisible) {
      this.drawPetCube(model);
    } else {
      this.display.drawTextCentered(model.primaryText, 122, "Primary", kWhite);
    }
  }

  private renderStatus(model: StatusScreenModel) {
    const buttonText = `button ${model.buttonPressed ? "down" : "up"}`;
    const rssiText = `rssi ${model.wifiRssi}`;
    const imuText = model.imuReady
      ? `imu ok 0x${model.imuAddress.toString(16).toUpperCase().padStart(2, "0")}`
      : "imu missing";
    const accelText = `az ${model.imuAccelZ}`;
    const poseText = `rp ${Math.round(model.imuRollDeg)} ${Math.round(model.imuPitchDeg)}`;
    const localBatteryText = `A ${model.localBatteryPercent}%`;
    const peerBatteryText = `B ${model.peerBatteryPercent}%`;

    this.display.clear(kBlack);
    this.display.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
    this.display.drawTextCentered("status", 48, "Small", kBlue);
    this.display.drawTextCentered(buttonText, 76, "Small", kWhite);
    this.display.drawTextCentered(imuText, 99, "Small", stateColor(model.imuReady));
    this.display.drawTextCentered(accelText, 122, "Small", kWhite);
    this.display.drawTextCentered(poseText, 145, "Small", kWhite);
    this.display.drawTextCentered(rssiText, 161, "Small", kWhite);
    this.display.drawTextCentered(localBatteryText, 184, "Small", batteryColor(model.localBatteryPercent));
    this.display.drawTextCentered(peerBatteryText, 199, "Small", batteryColor(model.peerBatteryPercent));
    this.drawStatusPill(78, 211, model.backendConnected ? "backend ok" : "backend off", stateColor(model.backendConnected));
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
    const vertices = [
      [-1, -1, -1],
      [1, -1, -1],
      [1, 1, -1],
      [-1, 1, -1],
      [-1, -1, 1],
      [1, -1, 1],
      [1, 1, 1],
      [-1, 1, 1]
    ] as const;
    const edges = [
      [0, 1], [1, 2], [2, 3], [3, 0],
      [4, 5], [5, 6], [6, 7], [7, 4],
      [0, 4], [1, 5], [2, 6], [3, 7]
    ] as const;

    const roll = model.cubeRollDeg * DEG_TO_RAD;
    const pitch = model.cubePitchDeg * DEG_TO_RAD;
    const yaw = model.cubeYawDeg * DEG_TO_RAD;
    const sr = Math.sin(roll);
    const cr = Math.cos(roll);
    const sp = Math.sin(pitch);
    const cp = Math.cos(pitch);
    const sy = Math.sin(yaw);
    const cy = Math.cos(yaw);
    const scale = model.cubeScale > 0 ? model.cubeScale : kDefaultCubeScale;
    const centerX = kScreenCenter + Math.round(model.cubeOffsetX);
    const centerY = kCubeCenterY + Math.round(model.cubeOffsetY);

    const points = vertices.map(([x, y, z]) => {
      const yRoll = y * cr - z * sr;
      const zRoll = y * sr + z * cr;
      const xPitch = x * cp + zRoll * sp;
      const zPitch = -x * sp + zRoll * cp;
      const xYaw = xPitch * cy - yRoll * sy;
      const yYaw = xPitch * sy + yRoll * cy;

      return {
        x: centerX + Math.round(xYaw * scale),
        y: centerY + Math.round(yYaw * scale),
        z: zPitch
      };
    });

    for (const [from, to] of edges) {
      const a = points[from];
      const b = points[to];
      this.display.drawLine(a.x, a.y, b.x, b.y, a.z + b.z > 0 ? kGreen : kMuted);
    }
  }

  private drawWeatherChip(x: number, label: string, weather: string) {
    this.display.fillRoundRect(x, 35, 48, 22, 9, kPanel);
    this.display.drawRoundRect(x, 35, 48, 22, 9, kLine);
    this.display.drawText(label, x + 7, 50, "Small", kMuted);
    this.display.drawText(weather, x + 24, 50, "Small", kWhite);
  }

  private drawConnectionDots(wifiConnected: boolean, backendConnected: boolean) {
    this.display.fillCircle(kScreenCenter - 7, 46, 3, stateColor(wifiConnected));
    this.display.fillCircle(kScreenCenter + 7, 46, 3, stateColor(backendConnected));
    this.display.drawLine(kScreenCenter - 3, 46, kScreenCenter + 3, 46, kLine);
  }

  private drawBottomHint(hintText: string) {
    this.display.fillRoundRect(58, 179, 124, 24, 10, kPanel);
    this.display.drawRoundRect(58, 179, 124, 24, 10, kLine);
    this.display.drawTextCentered(hintText, 195, "Small", kMuted);
  }

  private drawStatusPill(x: number, y: number, text: string, color: number) {
    this.display.fillRoundRect(x, y, 84, 23, 10, kPanel);
    this.display.drawRoundRect(x, y, 84, 23, 10, kLine);
    this.display.fillCircle(x + 12, y + 11, 3, color);
    this.display.drawText(text, x + 22, y + 15, "Small", kWhite);
  }
}

class PngDisplayDriver {
  private readonly pixels = new Uint8Array(kScreenSize * kScreenSize * 4);
  private fillColor = rgb565ToRgba(kWhite);

  clear(color: number) {
    const rgba = rgb565ToRgba(color);
    for (let offset = 0; offset < this.pixels.length; offset += 4) {
      this.pixels[offset] = rgba.r;
      this.pixels[offset + 1] = rgba.g;
      this.pixels[offset + 2] = rgba.b;
      this.pixels[offset + 3] = 255;
    }
  }

  drawTextCentered(text: string, centerY: number, style: DisplayTextStyle, color: number) {
    const bounds = this.getTextBounds(text, 0, 0, style);
    const x = Math.trunc((kScreenSize - bounds.w) / 2) - bounds.x1;
    const y = centerY - Math.trunc(bounds.h / 2) - bounds.y1;
    this.drawText(text, x, y, style, color);
  }

  drawText(text: string, x: number, y: number, style: DisplayTextStyle, color: number) {
    this.fillColor = rgb565ToRgba(color);
    if (style === "Primary") {
      this.drawGfxText(text, x, y);
      return;
    }
    this.drawGlcdText(text, x, y);
  }

  drawBatteryBars(leftPercent: number, rightPercent: number) {
    this.drawBatteryArc(true, leftPercent);
    this.drawBatteryArc(false, rightPercent);
  }

  drawCircle(x: number, y: number, radius: number, color: number) {
    this.fillColor = rgb565ToRgba(color);
    let f = 1 - radius;
    let ddF_x = 1;
    let ddF_y = -2 * radius;
    let px = 0;
    let py = radius;

    this.writePixel(x, y + radius);
    this.writePixel(x, y - radius);
    this.writePixel(x + radius, y);
    this.writePixel(x - radius, y);

    while (px < py) {
      if (f >= 0) {
        py--;
        ddF_y += 2;
        f += ddF_y;
      }
      px++;
      ddF_x += 2;
      f += ddF_x;
      this.writePixel(x + px, y + py);
      this.writePixel(x - px, y + py);
      this.writePixel(x + px, y - py);
      this.writePixel(x - px, y - py);
      this.writePixel(x + py, y + px);
      this.writePixel(x - py, y + px);
      this.writePixel(x + py, y - px);
      this.writePixel(x - py, y - px);
    }
  }

  fillCircle(x: number, y: number, radius: number, color: number) {
    this.fillColor = rgb565ToRgba(color);
    this.writeFastVLine(x, y - radius, 2 * radius + 1);
    this.fillCircleHelper(x, y, radius, 3, 0);
  }

  drawLine(x0: number, y0: number, x1: number, y1: number, color: number) {
    this.fillColor = rgb565ToRgba(color);
    let steep = Math.abs(y1 - y0) > Math.abs(x1 - x0);
    if (steep) {
      [x0, y0] = [y0, x0];
      [x1, y1] = [y1, x1];
    }
    if (x0 > x1) {
      [x0, x1] = [x1, x0];
      [y0, y1] = [y1, y0];
    }
    const dx = x1 - x0;
    const dy = Math.abs(y1 - y0);
    let err = Math.trunc(dx / 2);
    const ystep = y0 < y1 ? 1 : -1;
    let y = y0;

    for (let x = x0; x <= x1; x++) {
      if (steep) {
        this.writePixel(y, x);
      } else {
        this.writePixel(x, y);
      }
      err -= dy;
      if (err < 0) {
        y += ystep;
        err += dx;
      }
    }
  }

  drawRoundRect(x: number, y: number, width: number, height: number, radius: number, color: number) {
    this.fillColor = rgb565ToRgba(color);
    this.writeFastHLine(x + radius, y, width - 2 * radius);
    this.writeFastHLine(x + radius, y + height - 1, width - 2 * radius);
    this.writeFastVLine(x, y + radius, height - 2 * radius);
    this.writeFastVLine(x + width - 1, y + radius, height - 2 * radius);
    this.drawCircleHelper(x + radius, y + radius, radius, 1);
    this.drawCircleHelper(x + width - radius - 1, y + radius, radius, 2);
    this.drawCircleHelper(x + width - radius - 1, y + height - radius - 1, radius, 4);
    this.drawCircleHelper(x + radius, y + height - radius - 1, radius, 8);
  }

  fillRoundRect(x: number, y: number, width: number, height: number, radius: number, color: number) {
    this.fillColor = rgb565ToRgba(color);
    this.writeFillRect(x + radius, y, width - 2 * radius, height);
    this.fillCircleHelper(x + width - radius - 1, y + radius, radius, 1, height - 2 * radius - 1);
    this.fillCircleHelper(x + radius, y + radius, radius, 2, height - 2 * radius - 1);
  }

  fillRect(x: number, y: number, width: number, height: number, color: number) {
    this.fillColor = rgb565ToRgba(color);
    this.writeFillRect(x, y, width, height);
  }

  toPng() {
    const raw = Buffer.alloc((kScreenSize * 4 + 1) * kScreenSize);
    for (let y = 0; y < kScreenSize; y++) {
      const targetOffset = y * (kScreenSize * 4 + 1);
      raw[targetOffset] = 0;
      raw.set(this.pixels.subarray(y * kScreenSize * 4, (y + 1) * kScreenSize * 4), targetOffset + 1);
    }

    return Buffer.concat([
      Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
      pngChunk("IHDR", ihdr(kScreenSize, kScreenSize)),
      pngChunk("IDAT", deflateSync(raw)),
      pngChunk("IEND", Buffer.alloc(0))
    ]);
  }

  private drawBatteryArc(leftSide: boolean, percent: number) {
    const clampedPercent = clampPercent(percent);
    const startDeg = leftSide ? kLeftBatteryStartDeg : kRightBatteryStartDeg;
    const sweepDeg = leftSide ? kBatteryArcSweepDeg : -kBatteryArcSweepDeg;
    const fillSweepDeg = Math.trunc((sweepDeg * clampedPercent) / 100);

    this.drawArcSegment(startDeg, sweepDeg, kBatteryTrackColor, kBatteryTrackThickness);
    if (fillSweepDeg !== 0) {
      this.drawArcSegment(startDeg, fillSweepDeg, batteryArcColor(clampedPercent), kBatteryArcThickness);
    }
  }

  private drawArcSegment(startDeg: number, sweepDeg: number, color: number, thickness: number) {
    const coreHalfWidth = thickness > 1 ? 1 : 0;
    const edgeColor = scaleColor(color, 92);

    this.drawArcLine(startDeg, sweepDeg, kBatteryArcRadius - coreHalfWidth - 1, edgeColor);
    this.drawArcLine(startDeg, sweepDeg, kBatteryArcRadius + coreHalfWidth + 1, edgeColor);
    for (let offset = -coreHalfWidth; offset <= coreHalfWidth; ++offset) {
      this.drawArcLine(startDeg, sweepDeg, kBatteryArcRadius + offset, color);
    }
  }

  private drawArcLine(startDeg: number, sweepDeg: number, radius: number, color: number) {
    const step = sweepDeg >= 0 ? kArcStepDeg : -kArcStepDeg;
    const endDeg = startDeg + sweepDeg;
    for (let deg = startDeg; deg !== endDeg; deg += step) {
      let nextDeg = deg + step;
      if (sweepDeg >= 0 ? nextDeg > endDeg : nextDeg < endDeg) {
        nextDeg = endDeg;
      }
      const radians = deg * DEG_TO_RAD;
      const nextRadians = nextDeg * DEG_TO_RAD;
      const x0 = kScreenCenter + Math.round(Math.cos(radians) * radius);
      const y0 = kScreenCenter + Math.round(Math.sin(radians) * radius);
      const x1 = kScreenCenter + Math.round(Math.cos(nextRadians) * radius);
      const y1 = kScreenCenter + Math.round(Math.sin(nextRadians) * radius);
      this.drawLine(x0, y0, x1, y1, color);
    }
  }

  private getTextBounds(text: string, x: number, y: number, style: DisplayTextStyle): TextBounds {
    return style === "Primary" ? this.getGfxTextBounds(text, x, y) : this.getGlcdTextBounds(text, x, y);
  }

  private getGlcdTextBounds(text: string, x: number, y: number): TextBounds {
    if (text.length === 0) {
      return { x1: x, y1: y, w: 0, h: 0 };
    }
    return { x1: x, y1: y, w: text.length * 6 - 1, h: 8 };
  }

  private getGfxTextBounds(text: string, x: number, y: number): TextBounds {
    let minX = kScreenSize;
    let minY = kScreenSize;
    let maxX = -1;
    let maxY = -1;
    let cursorX = x;
    let cursorY = y;

    for (const char of text) {
      const code = char.charCodeAt(0);
      if (code === 10) {
        cursorX = 0;
        cursorY += magicalmond.yAdvance;
        continue;
      }
      if (code === 13) {
        continue;
      }
      const glyph = getGlyph(magicalmond, code);
      if (!glyph) {
        continue;
      }
      const x1 = cursorX + glyph.xOffset;
      const y1 = cursorY + glyph.yOffset;
      const x2 = x1 + glyph.width - 1;
      const y2 = y1 + glyph.height - 1;
      if (glyph.width > 0 && glyph.height > 0) {
        minX = Math.min(minX, x1);
        minY = Math.min(minY, y1);
        maxX = Math.max(maxX, x2);
        maxY = Math.max(maxY, y2);
      }
      cursorX += glyph.xAdvance;
    }

    if (maxX < minX || maxY < minY) {
      return { x1: x, y1: y, w: 0, h: 0 };
    }
    return { x1: minX, y1: minY, w: maxX - minX + 1, h: maxY - minY + 1 };
  }

  private drawGlcdText(text: string, x: number, y: number) {
    let cursorX = x;
    let cursorY = y;
    for (const char of text) {
      const code = char.charCodeAt(0);
      if (code === 10) {
        cursorX = 0;
        cursorY += 8;
        continue;
      }
      if (code === 13) {
        continue;
      }
      this.drawGlcdChar(cursorX, cursorY, code);
      cursorX += 6;
    }
  }

  private drawGlcdChar(x: number, y: number, code: number) {
    const offset = (code & 0xff) * 5;
    for (let i = 0; i < 5; i++) {
      let line = Number(glcdFont[offset + i] ?? 0);
      for (let j = 0; j < 8; j++) {
        if ((line & 0x1) !== 0) {
          this.writePixel(x + i, y + j);
        }
        line >>= 1;
      }
    }
  }

  private drawGfxText(text: string, x: number, y: number) {
    let cursorX = x;
    let cursorY = y;
    for (const char of text) {
      const code = char.charCodeAt(0);
      if (code === 10) {
        cursorX = 0;
        cursorY += magicalmond.yAdvance;
        continue;
      }
      if (code === 13) {
        continue;
      }
      const glyph = getGlyph(magicalmond, code);
      if (!glyph) {
        continue;
      }
      let bit = 0;
      let bits = 0;
      let bo = glyph.bitmapOffset;
      for (let yy = 0; yy < glyph.height; yy++) {
        for (let xx = 0; xx < glyph.width; xx++) {
          if ((bit++ & 7) === 0) {
            bits = magicalmond.bitmaps[bo++] ?? 0;
          }
          if ((bits & 0x80) !== 0) {
            this.writePixel(cursorX + glyph.xOffset + xx, cursorY + glyph.yOffset + yy);
          }
          bits <<= 1;
        }
      }
      cursorX += glyph.xAdvance;
    }
  }

  private drawCircleHelper(x0: number, y0: number, radius: number, corner: number) {
    let f = 1 - radius;
    let ddF_x = 1;
    let ddF_y = -2 * radius;
    let x = 0;
    let y = radius;

    while (x < y) {
      if (f >= 0) {
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;
      if (corner & 0x4) {
        this.writePixel(x0 + x, y0 + y);
        this.writePixel(x0 + y, y0 + x);
      }
      if (corner & 0x2) {
        this.writePixel(x0 + x, y0 - y);
        this.writePixel(x0 + y, y0 - x);
      }
      if (corner & 0x8) {
        this.writePixel(x0 - y, y0 + x);
        this.writePixel(x0 - x, y0 + y);
      }
      if (corner & 0x1) {
        this.writePixel(x0 - y, y0 - x);
        this.writePixel(x0 - x, y0 - y);
      }
    }
  }

  private fillCircleHelper(x0: number, y0: number, radius: number, corners: number, delta: number) {
    let f = 1 - radius;
    let ddF_x = 1;
    let ddF_y = -2 * radius;
    let x = 0;
    let y = radius;
    let px = x;
    let py = y;

    delta++;
    while (x < y) {
      if (f >= 0) {
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;
      if (x < y + 1) {
        if (corners & 1) {
          this.writeFastVLine(x0 + x, y0 - y, 2 * y + delta);
        }
        if (corners & 2) {
          this.writeFastVLine(x0 - x, y0 - y, 2 * y + delta);
        }
      }
      if (y !== py) {
        if (corners & 1) {
          this.writeFastVLine(x0 + py, y0 - px, 2 * px + delta);
        }
        if (corners & 2) {
          this.writeFastVLine(x0 - py, y0 - px, 2 * px + delta);
        }
        py = y;
      }
      px = x;
    }
  }

  private writeFastHLine(x: number, y: number, width: number) {
    this.writeFillRect(x, y, width, 1);
  }

  private writeFastVLine(x: number, y: number, height: number) {
    this.writeFillRect(x, y, 1, height);
  }

  private writeFillRect(x: number, y: number, width: number, height: number) {
    if (width <= 0 || height <= 0) {
      return;
    }
    const startX = Math.max(0, Math.round(x));
    const startY = Math.max(0, Math.round(y));
    const endX = Math.min(kScreenSize, Math.round(x + width));
    const endY = Math.min(kScreenSize, Math.round(y + height));
    for (let yy = startY; yy < endY; yy++) {
      for (let xx = startX; xx < endX; xx++) {
        this.writePixel(xx, yy);
      }
    }
  }

  private writePixel(x: number, y: number) {
    const px = Math.round(x);
    const py = Math.round(y);
    if (px < 0 || py < 0 || px >= kScreenSize || py >= kScreenSize) {
      return;
    }
    const offset = (py * kScreenSize + px) * 4;
    this.pixels[offset] = this.fillColor.r;
    this.pixels[offset + 1] = this.fillColor.g;
    this.pixels[offset + 2] = this.fillColor.b;
    this.pixels[offset + 3] = 255;
  }
}

function parseArgs(argv: string[]) {
  const args: { mode?: PreviewScreenMode | "all"; out?: string; snapshot?: string } = {};
  for (let index = 0; index < argv.length; index++) {
    const arg = argv[index];
    if (arg === "--mode" || arg === "-m") {
      args.mode = parseMode(argv[++index]);
    } else if (arg.startsWith("--mode=")) {
      args.mode = parseMode(arg.slice("--mode=".length));
    } else if (arg === "--out" || arg === "-o") {
      args.out = normalize(argv[++index]);
    } else if (arg.startsWith("--out=")) {
      args.out = normalize(arg.slice("--out=".length));
    } else if (arg === "--snapshot") {
      args.snapshot = normalize(argv[++index]);
    } else if (arg.startsWith("--snapshot=")) {
      args.snapshot = normalize(arg.slice("--snapshot=".length));
    } else if (arg === "--help" || arg === "-h") {
      printHelp();
      process.exit(0);
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }
  return args;
}

function parseMode(value: string | undefined): PreviewScreenMode | "all" {
  if (value === "all" || modes.includes(value as PreviewScreenMode)) {
    return value as PreviewScreenMode | "all";
  }
  throw new Error(`Expected --mode to be one of: all, ${modes.join(", ")}`);
}

async function chooseMode(): Promise<PreviewScreenMode | "all"> {
  if (!process.stdin.isTTY || !process.stdout.isTTY) {
    return "all";
  }

  const choices = ["all", ...modes] as const;
  let selected = 0;
  const render = () => {
    process.stdout.write("\x1b[2J\x1b[H");
    process.stdout.write("Firmware screen preview PNG\n\n");
    for (let index = 0; index < choices.length; index++) {
      process.stdout.write(`${index === selected ? ">" : " "} ${choices[index]}\n`);
    }
    process.stdout.write("\nUse arrows or j/k, Enter to generate, q to quit.\n");
  };

  return await new Promise((resolve) => {
    const stdin = process.stdin;
    stdin.setRawMode(true);
    stdin.resume();
    stdin.setEncoding("utf8");
    render();
    stdin.on("data", (key: string) => {
      if (key === "\u0003" || key === "q") {
        process.stdout.write("\n");
        process.exit(key === "\u0003" ? 130 : 0);
      }
      if (key === "\r" || key === "\n") {
        stdin.setRawMode(false);
        stdin.pause();
        process.stdout.write("\x1b[2J\x1b[H");
        resolve(choices[selected]);
        return;
      }
      if (key === "\u001b[A" || key === "k") {
        selected = (selected + choices.length - 1) % choices.length;
        render();
      } else if (key === "\u001b[B" || key === "j") {
        selected = (selected + 1) % choices.length;
        render();
      }
    });
  });
}

function readSnapshot(path: string) {
  return JSON.parse(readFileSync(path, "utf8")) as AppSnapshot;
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

function parseGfxFont(header: string): GfxFont {
  const bitmapBlock = extractBlock(header, "magicalmond_ogyg820pt7bBitmaps");
  const glyphBlock = extractBlock(header, "magicalmond_ogyg820pt7bGlyphs");
  const fontMatch = /0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*(\d+)\s*\}/.exec(header);

  return {
    bitmaps: parseHexNumbers(bitmapBlock),
    glyphs: Array.from(glyphBlock.matchAll(/\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}/g)).map(
      ([, bitmapOffset, width, height, xAdvance, xOffset, yOffset]) => ({
        bitmapOffset: Number(bitmapOffset),
        width: Number(width),
        height: Number(height),
        xAdvance: Number(xAdvance),
        xOffset: Number(xOffset),
        yOffset: Number(yOffset)
      })
    ),
    first: fontMatch ? Number.parseInt(fontMatch[1], 16) : 0x20,
    last: fontMatch ? Number.parseInt(fontMatch[2], 16) : 0x7e,
    yAdvance: fontMatch ? Number(fontMatch[3]) : 48
  };
}

function extractBlock(header: string, name: string) {
  const start = header.indexOf(`${name}[]`);
  if (start === -1) {
    throw new Error(`Missing ${name}`);
  }
  const open = header.indexOf("{", start);
  const close = header.indexOf("};", open);
  return header.slice(open, close);
}

function parseHexNumbers(value: string) {
  return Array.from(value.matchAll(/0x([0-9A-Fa-f]{2})/g), ([, hex]) => Number.parseInt(hex, 16));
}

function getGlyph(font: GfxFont, code: number) {
  if (code < font.first || code > font.last) {
    return null;
  }
  return font.glyphs[code - font.first] ?? null;
}

function clampPercent(percent: number) {
  const rounded = Math.trunc(percent);
  return rounded > 100 ? 100 : Math.max(0, rounded);
}

function batteryArcColor(percent: number) {
  if (percent < 24) {
    return kBatteryLowColor;
  }
  if (percent < 55) {
    return kBatteryMidColor;
  }
  return kBatteryHighColor;
}

function batteryColor(percent: number) {
  if (percent < 24) {
    return kRed;
  }
  if (percent < 55) {
    return kAmber;
  }
  return kGreen;
}

function stateColor(active: boolean) {
  return active ? kGreen : kMuted;
}

function scaleColor(color: number, amount: number) {
  const r = (((color >> 11) & 0x1f) * amount) / 255;
  const g = (((color >> 5) & 0x3f) * amount) / 255;
  const b = ((color & 0x1f) * amount) / 255;
  return (Math.trunc(r) << 11) | (Math.trunc(g) << 5) | Math.trunc(b);
}

function rgb565ToRgba(color: number) {
  return {
    r: Math.round(((color >> 11) & 0x1f) * 255 / 31),
    g: Math.round(((color >> 5) & 0x3f) * 255 / 63),
    b: Math.round((color & 0x1f) * 255 / 31)
  };
}

function ihdr(width: number, height: number) {
  const data = Buffer.alloc(13);
  data.writeUInt32BE(width, 0);
  data.writeUInt32BE(height, 4);
  data[8] = 8;
  data[9] = 6;
  data[10] = 0;
  data[11] = 0;
  data[12] = 0;
  return data;
}

function pngChunk(type: string, data: Buffer) {
  const typeBytes = Buffer.from(type, "ascii");
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length, 0);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([typeBytes, data])), 0);
  return Buffer.concat([length, typeBytes, data, crc]);
}

function crc32(data: Buffer) {
  let crc = 0xffffffff;
  for (const byte of data) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function printHelp() {
  console.log(`Usage: bun run preview:png -- [options]

Options:
  -m, --mode <mode>       all, home, homeFrame, boot, or status
  -o, --out <path>        output PNG path; with --mode all this is treated as an output directory
      --snapshot <path>   optional AppSnapshot JSON file
  -h, --help              show this help

Without --mode, an interactive terminal menu is shown. In non-TTY environments,
the tool defaults to --mode all and writes .peek-preview/*.png.`);
}

main().catch((error: unknown) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
});
