export const DEG_TO_RAD = Math.PI / 180;
export const kScreenSize = 240;

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

export const kBlack = 0x0000;
export const kWhite = 0xffff;
export const kMuted = 0x8c71;
export const kLine = 0x2945;
export const kPanel = 0x1082;
export const kGreen = 0x05f4;
export const kBlue = 0x3d7f;
export const kAmber = 0xfdc0;
export const kRed = 0xf9c6;

export function batteryColor(percent: number) {
  if (percent < 24) return kRed;
  if (percent < 55) return kAmber;
  return kGreen;
}

function scaleColor(color: number, amount: number) {
  const r = (((color >> 11) & 0x1f) * amount) / 255;
  const g = (((color >> 5) & 0x3f) * amount) / 255;
  const b = ((color & 0x1f) * amount) / 255;
  return (Math.trunc(r) << 11) | (Math.trunc(g) << 5) | Math.trunc(b);
}

export function rgb565(color: number) {
  const r = Math.round(((color >> 11) & 0x1f) * 255 / 31);
  const g = Math.round(((color >> 5) & 0x3f) * 255 / 63);
  const b = Math.round((color & 0x1f) * 255 / 31);
  return `rgb(${r} ${g} ${b})`;
}

export function stateColor(active: boolean) {
  return active ? kGreen : kMuted;
}

export function batteryArcColor(percent: number) {
  if (percent < 24) return kBatteryLowColor;
  if (percent < 55) return kBatteryMidColor;
  return kBatteryHighColor;
}

export function clampPercent(percent: number) {
  const rounded = Math.trunc(percent);
  return rounded > 100 ? 100 : Math.max(0, rounded);
}

export class CanvasDisplayDriver {
  private fillCss = rgb565(kWhite);

  constructor(public readonly context: CanvasRenderingContext2D) {}

  setFill(color: number) { this.fillCss = rgb565(color); }

  clear(color: number) {
    this.context.fillStyle = rgb565(color);
    this.context.fillRect(0, 0, kScreenSize, kScreenSize);
  }

  drawTextCentered(text: string, centerY: number, style: "Small" | "Primary", color: number, opts?: { getTextBounds: (text: string, x: number, y: number) => { x1: number; y1: number; w: number; h: number } }) {
    const fn = opts?.getTextBounds ?? this.glcdTextBounds;
    const bounds = fn(text, 0, 0);
    const x = Math.trunc((kScreenSize - bounds.w) / 2) - bounds.x1;
    const y = centerY - Math.trunc(bounds.h / 2) - bounds.y1;
    this.drawText(text, x, y, style, color, opts);
  }

  drawText(text: string, x: number, y: number, style: "Small" | "Primary", color: number, opts?: { getTextBounds?: (text: string, x: number, y: number) => { x1: number; y1: number; w: number; h: number }; drawChar?: (x: number, y: number, code: number) => void; charWidth?: number; charHeight?: number }) {
    this.setFill(color);
    const drawChar = opts?.drawChar;
    if (drawChar) {
      let cx = x, cy = y;
      for (const char of text) {
        const code = char.charCodeAt(0);
        if (code === 10) { cx = 0; cy += (opts?.charHeight ?? 8); continue; }
        if (code === 13) continue;
        drawChar(cx, cy, code);
        cx += (opts?.charWidth ?? 6);
      }
      return;
    }
    if (style === "Primary") {
      this.gfxDrawText(text, x, y);
    } else {
      this.glcdDrawText(text, x, y);
    }
  }

  drawBatteryBars(leftPercent: number, rightPercent: number) {
    this.drawBatteryArc(true, leftPercent);
    this.drawBatteryArc(false, rightPercent);
  }

  drawCircle(x: number, y: number, radius: number, color: number) {
    this.setFill(color);
    let f = 1 - radius, ddF_x = 1, ddF_y = -2 * radius, px = 0, py = radius;
    this.writePixel(x, y + radius); this.writePixel(x, y - radius);
    this.writePixel(x + radius, y); this.writePixel(x - radius, y);
    while (px < py) {
      if (f >= 0) { py--; ddF_y += 2; f += ddF_y; }
      px++; ddF_x += 2; f += ddF_x;
      this.writePixel(x + px, y + py); this.writePixel(x - px, y + py);
      this.writePixel(x + px, y - py); this.writePixel(x - px, y - py);
      this.writePixel(x + py, y + px); this.writePixel(x - py, y + px);
      this.writePixel(x + py, y - px); this.writePixel(x - py, y - px);
    }
  }

  fillCircle(x: number, y: number, radius: number, color: number) {
    this.setFill(color);
    this.writeFastVLine(x, y - radius, 2 * radius + 1);
    this.fillCircleHelper(x, y, radius, 3, 0);
  }

  drawLine(x0: number, y0: number, x1: number, y1: number, color: number) {
    this.setFill(color);
    let steep = Math.abs(y1 - y0) > Math.abs(x1 - x0);
    if (steep) { [x0, y0] = [y0, x0]; [x1, y1] = [y1, x1]; }
    if (x0 > x1) { [x0, x1] = [x1, x0]; [y0, y1] = [y1, y0]; }
    const dx = x1 - x0, dy = Math.abs(y1 - y0);
    let err = Math.trunc(dx / 2), y = y0, ystep = y0 < y1 ? 1 : -1;
    for (let x = x0; x <= x1; x++) {
      if (steep) this.writePixel(y, x); else this.writePixel(x, y);
      err -= dy; if (err < 0) { y += ystep; err += dx; }
    }
  }

  drawRoundRect(x: number, y: number, w: number, h: number, r: number, color: number) {
    this.setFill(color);
    this.writeFastHLine(x + r, y, w - 2 * r);
    this.writeFastHLine(x + r, y + h - 1, w - 2 * r);
    this.writeFastVLine(x, y + r, h - 2 * r);
    this.writeFastVLine(x + w - 1, y + r, h - 2 * r);
    this.drawCircleHelper(x + r, y + r, r, 1);
    this.drawCircleHelper(x + w - r - 1, y + r, r, 2);
    this.drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4);
    this.drawCircleHelper(x + r, y + h - r - 1, r, 8);
  }

  fillRoundRect(x: number, y: number, w: number, h: number, r: number, color: number) {
    this.setFill(color);
    this.writeFillRect(x + r, y, w - 2 * r, h);
    this.fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1);
    this.fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1);
  }

  drawRect(x: number, y: number, w: number, h: number, color: number) {
    this.setFill(color);
    this.writeFastHLine(x, y, w); this.writeFastHLine(x, y + h - 1, w);
    this.writeFastVLine(x, y, h); this.writeFastVLine(x + w - 1, y, h);
  }

  fillRect(x: number, y: number, w: number, h: number, color: number) {
    this.setFill(color);
    this.writeFillRect(x, y, w, h);
  }

  writePixel(x: number, y: number) {
    if (x < 0 || y < 0 || x >= kScreenSize || y >= kScreenSize) return;
    this.context.fillStyle = this.fillCss;
    this.context.fillRect(Math.round(x), Math.round(y), 1, 1);
  }

  writeFillRect(x: number, y: number, w: number, h: number) {
    if (w <= 0 || h <= 0) return;
    this.context.fillStyle = this.fillCss;
    this.context.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h));
  }

  // GLCD font (6x8 bitmap)
  glcdDrawText(text: string, x: number, y: number, font: number[]) {
    let cx = x, cy = y;
    for (const char of text) {
      const code = char.charCodeAt(0);
      if (code === 10) { cx = 0; cy += 8; continue; }
      if (code === 13) continue;
      const offset = (code & 0xff) * 5;
      for (let i = 0; i < 5; i++) {
        let line = Number(font[offset + i] ?? 0);
        for (let j = 0; j < 8; j++) {
          if ((line & 0x1) !== 0) this.writePixel(cx + i, cy + j);
          line >>= 1;
        }
      }
      cx += 6;
    }
  }

  glcdTextBounds(text: string, _x: number, _y: number) {
    if (text.length === 0) return { x1: 0, y1: 0, w: 0, h: 0 };
    return { x1: 0, y1: 0, w: text.length * 6 - 1, h: 8 };
  }

  // GFX font
  private gfxGlyphs: GfxGlyph[] = [];
  private gfxBitmaps: number[] = [];
  private gfxFirst = 0x20;
  private gfxLast = 0x7e;
  private gfxYAdvance = 48;

  setGfxFont(glyphs: GfxGlyph[], bitmaps: number[], first: number, last: number, yAdvance: number) {
    this.gfxGlyphs = glyphs; this.gfxBitmaps = bitmaps;
    this.gfxFirst = first; this.gfxLast = last; this.gfxYAdvance = yAdvance;
  }

  gfxDrawText(text: string, x: number, y: number) {
    let cx = x, cy = y;
    for (const char of text) {
      const code = char.charCodeAt(0);
      if (code === 10) { cx = 0; cy += this.gfxYAdvance; continue; }
      if (code === 13) continue;
      const glyph = this.getGlyph(code);
      if (!glyph) continue;
      let bit = 0, bits = 0, bo = glyph.bitmapOffset;
      for (let yy = 0; yy < glyph.height; yy++) {
        for (let xx = 0; xx < glyph.width; xx++) {
          if ((bit++ & 7) === 0) bits = this.gfxBitmaps[bo++] ?? 0;
          if ((bits & 0x80) !== 0) this.writePixel(cx + glyph.xOffset + xx, cy + glyph.yOffset + yy);
          bits <<= 1;
        }
      }
      cx += glyph.xAdvance;
    }
  }

  gfxTextBounds(text: string, x: number, y: number) {
    let minX = kScreenSize, minY = kScreenSize, maxX = -1, maxY = -1, cx = x, cy = y;
    for (const char of text) {
      const code = char.charCodeAt(0);
      if (code === 10) { cx = 0; cy += this.gfxYAdvance; continue; }
      if (code === 13) continue;
      const glyph = this.getGlyph(code);
      if (!glyph) continue;
      const x1 = cx + glyph.xOffset, y1 = cy + glyph.yOffset;
      const x2 = x1 + glyph.width - 1, y2 = y1 + glyph.height - 1;
      if (glyph.width > 0 && glyph.height > 0) {
        minX = Math.min(minX, x1); minY = Math.min(minY, y1);
        maxX = Math.max(maxX, x2); maxY = Math.max(maxY, y2);
      }
      cx += glyph.xAdvance;
    }
    if (maxX < minX || maxY < minY) return { x1: x, y1: y, w: 0, h: 0 };
    return { x1: minX, y1: minY, w: maxX - minX + 1, h: maxY - minY + 1 };
  }

  private getGlyph(code: number) {
    if (code < this.gfxFirst || code > this.gfxLast) return null;
    return this.gfxGlyphs[code - this.gfxFirst] ?? null;
  }

  private writeFastHLine(x: number, y: number, w: number) { this.writeFillRect(x, y, w, 1); }
  private writeFastVLine(x: number, y: number, h: number) { this.writeFillRect(x, y, 1, h); }

  private drawBatteryArc(leftSide: boolean, percent: number) {
    const p = clampPercent(percent);
    const startDeg = leftSide ? kLeftBatteryStartDeg : kRightBatteryStartDeg;
    const sweepDeg = leftSide ? kBatteryArcSweepDeg : -kBatteryArcSweepDeg;
    const fillSweepDeg = Math.trunc((sweepDeg * p) / 100);
    this.arcSegment(startDeg, sweepDeg, kBatteryTrackColor, kBatteryTrackThickness);
    if (fillSweepDeg === 0) return;
    this.arcSegment(startDeg, fillSweepDeg, batteryArcColor(p), kBatteryArcThickness);
  }

  private arcSegment(startDeg: number, sweepDeg: number, color: number, thickness: number) {
    const coreHalfWidth = thickness > 1 ? 1 : 0;
    const edgeColor = scaleColor(color, 92);
    this.arcLine(startDeg, sweepDeg, kBatteryArcRadius - coreHalfWidth - 1, edgeColor);
    this.arcLine(startDeg, sweepDeg, kBatteryArcRadius + coreHalfWidth + 1, edgeColor);
    for (let offset = -coreHalfWidth; offset <= coreHalfWidth; ++offset)
      this.arcLine(startDeg, sweepDeg, kBatteryArcRadius + offset, color);
  }

  private arcLine(startDeg: number, sweepDeg: number, radius: number, color: number) {
    const step = sweepDeg >= 0 ? kArcStepDeg : -kArcStepDeg;
    const endDeg = startDeg + sweepDeg;
    for (let deg = startDeg; sweepDeg >= 0 ? deg !== endDeg : deg !== endDeg; deg += step) {
      let nextDeg = deg + step;
      if (sweepDeg >= 0 ? nextDeg > endDeg : nextDeg < endDeg) nextDeg = endDeg;
      const rad = deg * DEG_TO_RAD, nrad = nextDeg * DEG_TO_RAD;
      this.drawLine(
        kScreenCenter + Math.round(Math.cos(rad) * radius),
        kScreenCenter + Math.round(Math.sin(rad) * radius),
        kScreenCenter + Math.round(Math.cos(nrad) * radius),
        kScreenCenter + Math.round(Math.sin(nrad) * radius), color);
    }
  }

  private drawCircleHelper(x0: number, y0: number, radius: number, corner: number) {
    let f = 1 - radius, ddF_x = 1, ddF_y = -2 * radius, x = 0, y = radius;
    while (x < y) {
      if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
      x++; ddF_x += 2; f += ddF_x;
      if (corner & 0x4) { this.writePixel(x0 + x, y0 + y); this.writePixel(x0 + y, y0 + x); }
      if (corner & 0x2) { this.writePixel(x0 + x, y0 - y); this.writePixel(x0 + y, y0 - x); }
      if (corner & 0x8) { this.writePixel(x0 - y, y0 + x); this.writePixel(x0 - x, y0 + y); }
      if (corner & 0x1) { this.writePixel(x0 - y, y0 - x); this.writePixel(x0 - x, y0 - y); }
    }
  }

  private fillCircleHelper(x0: number, y0: number, radius: number, corners: number, delta: number) {
    let f = 1 - radius, ddF_x = 1, ddF_y = -2 * radius, x = 0, y = radius, px = x, py = y;
    delta++;
    while (x < y) {
      if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
      x++; ddF_x += 2; f += ddF_x;
      if (x < y + 1) {
        if (corners & 1) this.writeFastVLine(x0 + x, y0 - y, 2 * y + delta);
        if (corners & 2) this.writeFastVLine(x0 - x, y0 - y, 2 * y + delta);
      }
      if (y !== py) {
        if (corners & 1) this.writeFastVLine(x0 + py, y0 - px, 2 * px + delta);
        if (corners & 2) this.writeFastVLine(x0 - py, y0 - px, 2 * px + delta);
        py = y;
      }
      px = x;
    }
  }
}

export interface GfxGlyph {
  bitmapOffset: number; width: number; height: number; xAdvance: number; xOffset: number; yOffset: number;
}
