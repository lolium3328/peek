#include "drivers/DisplayDriver.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

#include <Arduino.h>

#include "HostPreview.h"
#include "assets/fonts/magicalmond_ogyg820pt7b.h"

struct HostDisplayAccess {
  static Arduino_DataBus *bus(DisplayDriver &display) {
    return display.bus_;
  }

  static const Arduino_DataBus *bus(const DisplayDriver &display) {
    return display.bus_;
  }
};

namespace {
constexpr int16_t kScreenSize = 240;
constexpr int16_t kScreenCenter = kScreenSize / 2;
constexpr int16_t kBatteryArcRadius = 109;
constexpr int16_t kLeftBatteryStartDeg = 142;
constexpr int16_t kRightBatteryStartDeg = 38;
constexpr int16_t kBatteryArcSweepDeg = 76;
constexpr uint8_t kBatteryArcThickness = 1;
constexpr uint8_t kBatteryTrackThickness = 1;
constexpr int16_t kArcStepDeg = 1;
constexpr uint16_t kBatteryTrackColor = 0x18E3;
constexpr uint16_t kBatteryHighColor = 0x05F4;
constexpr uint16_t kBatteryMidColor = 0xFDC0;
constexpr uint16_t kBatteryLowColor = 0xF9C6;

struct HostState {
  std::array<uint16_t, kScreenSize * kScreenSize> pixels{};
  uint16_t textColor = 0xFFFF;
  DisplayTextStyle textStyle = DisplayTextStyle::Small;
  uint8_t leftBatteryPercent = 92;
  uint8_t rightBatteryPercent = 79;
};

constexpr uint8_t kGlcdFont[] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0x00,0x00,0x00,0x07,0x00,0x07,0x00,0x14,
  0x7f,0x14,0x7f,0x14,0x24,0x2a,0x7f,0x2a,0x12,0x23,0x13,0x08,0x64,0x62,0x36,0x49,
  0x56,0x20,0x50,0x00,0x08,0x07,0x03,0x00,0x00,0x1c,0x22,0x41,0x00,0x00,0x41,0x22,
  0x1c,0x00,0x2a,0x1c,0x7f,0x1c,0x2a,0x08,0x08,0x3e,0x08,0x08,0x00,0x80,0x70,0x30,
  0x00,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x60,0x60,0x00,0x20,0x10,0x08,0x04,0x02,
  0x3e,0x51,0x49,0x45,0x3e,0x00,0x42,0x7f,0x40,0x00,0x72,0x49,0x49,0x49,0x46,0x21,
  0x41,0x49,0x4d,0x33,0x18,0x14,0x12,0x7f,0x10,0x27,0x45,0x45,0x45,0x39,0x3c,0x4a,
  0x49,0x49,0x31,0x41,0x21,0x11,0x09,0x07,0x36,0x49,0x49,0x49,0x36,0x46,0x49,0x49,
  0x29,0x1e,0x00,0x00,0x14,0x00,0x00,0x00,0x40,0x34,0x00,0x00,0x00,0x08,0x14,0x22,
  0x41,0x14,0x14,0x14,0x14,0x14,0x00,0x41,0x22,0x14,0x08,0x02,0x01,0x59,0x09,0x06,
  0x3e,0x41,0x5d,0x59,0x4e,0x7c,0x12,0x11,0x12,0x7c,0x7f,0x49,0x49,0x49,0x36,0x3e,
  0x41,0x41,0x41,0x22,0x7f,0x41,0x41,0x41,0x3e,0x7f,0x49,0x49,0x49,0x41,0x7f,0x09,
  0x09,0x09,0x01,0x3e,0x41,0x41,0x51,0x73,0x7f,0x08,0x08,0x08,0x7f,0x00,0x41,0x7f,
  0x41,0x00,0x20,0x40,0x41,0x3f,0x01,0x7f,0x08,0x14,0x22,0x41,0x7f,0x40,0x40,0x40,
  0x40,0x7f,0x02,0x1c,0x02,0x7f,0x7f,0x04,0x08,0x10,0x7f,0x3e,0x41,0x41,0x41,0x3e,
  0x7f,0x09,0x09,0x09,0x06,0x3e,0x41,0x51,0x21,0x5e,0x7f,0x09,0x19,0x29,0x46,0x26,
  0x49,0x49,0x49,0x32,0x03,0x01,0x7f,0x01,0x03,0x3f,0x40,0x40,0x40,0x3f,0x1f,0x20,
  0x40,0x20,0x1f,0x3f,0x40,0x38,0x40,0x3f,0x63,0x14,0x08,0x14,0x63,0x03,0x04,0x78,
  0x04,0x03,0x61,0x59,0x49,0x4d,0x43,0x00,0x7f,0x41,0x41,0x41,0x02,0x04,0x08,0x10,
  0x20,0x00,0x41,0x41,0x41,0x7f,0x04,0x02,0x01,0x02,0x04,0x40,0x40,0x40,0x40,0x40,
  0x00,0x03,0x07,0x08,0x00,0x20,0x54,0x54,0x78,0x40,0x7f,0x28,0x44,0x44,0x38,0x38,
  0x44,0x44,0x44,0x28,0x38,0x44,0x44,0x28,0x7f,0x38,0x54,0x54,0x54,0x18,0x00,0x08,
  0x7e,0x09,0x02,0x18,0xa4,0xa4,0x9c,0x78,0x7f,0x08,0x04,0x04,0x78,0x00,0x44,0x7d,
  0x40,0x00,0x20,0x40,0x40,0x3d,0x00,0x7f,0x10,0x28,0x44,0x00,0x00,0x41,0x7f,0x40,
  0x00,0x7c,0x04,0x78,0x04,0x78,0x7c,0x08,0x04,0x04,0x78,0x38,0x44,0x44,0x44,0x38,
  0xfc,0x18,0x24,0x24,0x18,0x18,0x24,0x24,0x18,0xfc,0x7c,0x08,0x04,0x04,0x08,0x48,
  0x54,0x54,0x54,0x24,0x04,0x04,0x3f,0x44,0x24,0x3c,0x40,0x40,0x20,0x7c,0x1c,0x20,
  0x40,0x20,0x1c,0x3c,0x40,0x30,0x40,0x3c,0x44,0x28,0x10,0x28,0x44,0x4c,0x90,0x90,
  0x90,0x7c,0x44,0x64,0x54,0x4c,0x44,0x00,0x08,0x36,0x41,0x00,0x00,0x00,0x77,0x00,
  0x00,0x00,0x41,0x36,0x08,0x00,0x02,0x01,0x02,0x04,0x02,
};

HostState &state(DisplayDriver &display) {
  return *reinterpret_cast<HostState *>(HostDisplayAccess::bus(display));
}

const HostState &state(const DisplayDriver &display) {
  return *reinterpret_cast<const HostState *>(HostDisplayAccess::bus(display));
}

uint8_t clampPercent(uint8_t percent) {
  return percent > 100 ? 100 : percent;
}

uint16_t batteryColor(uint8_t percent) {
  if (percent < 24) {
    return kBatteryLowColor;
  }
  if (percent < 55) {
    return kBatteryMidColor;
  }
  return kBatteryHighColor;
}

uint16_t scaleColor(uint16_t color, uint8_t amount) {
  const uint8_t r = ((color >> 11) & 0x1F) * amount / 255;
  const uint8_t g = ((color >> 5) & 0x3F) * amount / 255;
  const uint8_t b = (color & 0x1F) * amount / 255;
  return (static_cast<uint16_t>(r) << 11) | (static_cast<uint16_t>(g) << 5) | b;
}

void writePixel(HostState &s, int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= kScreenSize || y >= kScreenSize) {
    return;
  }
  s.pixels[static_cast<size_t>(y) * kScreenSize + static_cast<size_t>(x)] = color;
}

void writeFillRect(HostState &s, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color) {
  if (width <= 0 || height <= 0) {
    return;
  }
  const int16_t startX = std::max<int16_t>(0, x);
  const int16_t startY = std::max<int16_t>(0, y);
  const int16_t endX = std::min<int16_t>(kScreenSize, x + width);
  const int16_t endY = std::min<int16_t>(kScreenSize, y + height);
  for (int16_t yy = startY; yy < endY; ++yy) {
    for (int16_t xx = startX; xx < endX; ++xx) {
      writePixel(s, xx, yy, color);
    }
  }
}

void drawCircleHelper(HostState &s, int16_t x0, int16_t y0, int16_t radius, uint8_t corner, uint16_t color) {
  int16_t f = 1 - radius;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * radius;
  int16_t x = 0;
  int16_t y = radius;

  while (x < y) {
    if (f >= 0) {
      --y;
      ddF_y += 2;
      f += ddF_y;
    }
    ++x;
    ddF_x += 2;
    f += ddF_x;
    if (corner & 0x4) {
      writePixel(s, x0 + x, y0 + y, color);
      writePixel(s, x0 + y, y0 + x, color);
    }
    if (corner & 0x2) {
      writePixel(s, x0 + x, y0 - y, color);
      writePixel(s, x0 + y, y0 - x, color);
    }
    if (corner & 0x8) {
      writePixel(s, x0 - y, y0 + x, color);
      writePixel(s, x0 - x, y0 + y, color);
    }
    if (corner & 0x1) {
      writePixel(s, x0 - y, y0 - x, color);
      writePixel(s, x0 - x, y0 - y, color);
    }
  }
}

void fillCircleHelper(HostState &s, int16_t x0, int16_t y0, int16_t radius, uint8_t corners, int16_t delta, uint16_t color) {
  int16_t f = 1 - radius;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * radius;
  int16_t x = 0;
  int16_t y = radius;
  int16_t px = x;
  int16_t py = y;

  ++delta;
  while (x < y) {
    if (f >= 0) {
      --y;
      ddF_y += 2;
      f += ddF_y;
    }
    ++x;
    ddF_x += 2;
    f += ddF_x;
    if (x < y + 1) {
      if (corners & 1) {
        writeFillRect(s, x0 + x, y0 - y, 1, 2 * y + delta, color);
      }
      if (corners & 2) {
        writeFillRect(s, x0 - x, y0 - y, 1, 2 * y + delta, color);
      }
    }
    if (y != py) {
      if (corners & 1) {
        writeFillRect(s, x0 + py, y0 - px, 1, 2 * px + delta, color);
      }
      if (corners & 2) {
        writeFillRect(s, x0 - py, y0 - px, 1, 2 * px + delta, color);
      }
      py = y;
    }
    px = x;
  }
}

void glcdBounds(const char *text, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  const size_t len = std::strlen(text);
  *x1 = x;
  *y1 = y;
  *w = len == 0 ? 0 : static_cast<uint16_t>(len * 6 - 1);
  *h = len == 0 ? 0 : 8;
}

void gfxBounds(const char *text, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  int16_t minX = kScreenSize;
  int16_t minY = kScreenSize;
  int16_t maxX = -1;
  int16_t maxY = -1;
  int16_t cursorX = x;
  int16_t cursorY = y;

  for (const char *ptr = text; *ptr; ++ptr) {
    const uint8_t code = static_cast<uint8_t>(*ptr);
    if (code == '\n') {
      cursorX = 0;
      cursorY += magicalmond_ogyg820pt7b.yAdvance;
      continue;
    }
    if (code == '\r' || code < magicalmond_ogyg820pt7b.first || code > magicalmond_ogyg820pt7b.last) {
      continue;
    }
    const GFXglyph &glyph = magicalmond_ogyg820pt7b.glyph[code - magicalmond_ogyg820pt7b.first];
    const int16_t gx1 = cursorX + glyph.xOffset;
    const int16_t gy1 = cursorY + glyph.yOffset;
    const int16_t gx2 = gx1 + glyph.width - 1;
    const int16_t gy2 = gy1 + glyph.height - 1;
    if (glyph.width > 0 && glyph.height > 0) {
      minX = std::min(minX, gx1);
      minY = std::min(minY, gy1);
      maxX = std::max(maxX, gx2);
      maxY = std::max(maxY, gy2);
    }
    cursorX += glyph.xAdvance;
  }

  if (maxX < minX || maxY < minY) {
    *x1 = x;
    *y1 = y;
    *w = 0;
    *h = 0;
    return;
  }
  *x1 = minX;
  *y1 = minY;
  *w = static_cast<uint16_t>(maxX - minX + 1);
  *h = static_cast<uint16_t>(maxY - minY + 1);
}

void drawGlcdChar(HostState &s, int16_t x, int16_t y, uint8_t code) {
  if (code < 0x20 || code > 0x7e) {
    return;
  }
  const size_t offset = static_cast<size_t>(code - 0x20) * 5;
  for (uint8_t i = 0; i < 5; ++i) {
    uint8_t line = kGlcdFont[offset + i];
    for (uint8_t j = 0; j < 8; ++j) {
      if ((line & 0x1) != 0) {
        writePixel(s, x + i, y + j, s.textColor);
      }
      line >>= 1;
    }
  }
}

void drawGfxChar(HostState &s, int16_t x, int16_t y, uint8_t code) {
  if (code < magicalmond_ogyg820pt7b.first || code > magicalmond_ogyg820pt7b.last) {
    return;
  }
  const GFXglyph &glyph = magicalmond_ogyg820pt7b.glyph[code - magicalmond_ogyg820pt7b.first];
  uint8_t bit = 0;
  uint8_t bits = 0;
  uint16_t bo = glyph.bitmapOffset;
  for (uint8_t yy = 0; yy < glyph.height; ++yy) {
    for (uint8_t xx = 0; xx < glyph.width; ++xx) {
      if ((bit++ & 7) == 0) {
        bits = magicalmond_ogyg820pt7b.bitmap[bo++];
      }
      if ((bits & 0x80) != 0) {
        writePixel(s, x + glyph.xOffset + xx, y + glyph.yOffset + yy, s.textColor);
      }
      bits <<= 1;
    }
  }
}

void rgb565ToRgba(uint16_t color, uint8_t *rgba) {
  rgba[0] = static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
  rgba[1] = static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
  rgba[2] = static_cast<uint8_t>((color & 0x1F) * 255 / 31);
  rgba[3] = 255;
}

void writeU32(std::vector<uint8_t> &out, uint32_t value) {
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void writeChunk(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &data) {
  writeU32(out, static_cast<uint32_t>(data.size()));
  const size_t typeOffset = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  const uint32_t crc = crc32(0, out.data() + typeOffset, static_cast<uInt>(4 + data.size()));
  writeU32(out, crc);
}
} // namespace

DisplayDriver::DisplayDriver()
    : bus_(reinterpret_cast<Arduino_DataBus *>(new HostState())),
      gfx_(nullptr) {}

bool DisplayDriver::begin() {
  return true;
}

void DisplayDriver::setSleep(bool sleeping) {
  sleeping_ = sleeping;
}

void DisplayDriver::clear(uint16_t color) {
  state(*this).pixels.fill(color);
}

void DisplayDriver::drawTextCentered(const char *text) {
  clear(0x0000);
  drawBatteryBars();
  drawTextCentered(text, kScreenCenter, DisplayTextStyle::Primary, 0xFFFF);
}

void DisplayDriver::drawTextCentered(const char *text, int16_t centerY, DisplayTextStyle style, uint16_t color) {
  applyTextStyle(style, color);
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  if (style == DisplayTextStyle::Primary) {
    gfxBounds(text, 0, 0, &x1, &y1, &w, &h);
  } else {
    glcdBounds(text, 0, 0, &x1, &y1, &w, &h);
  }
  const int16_t x = (kScreenSize - static_cast<int16_t>(w)) / 2 - x1;
  const int16_t y = centerY - static_cast<int16_t>(h) / 2 - y1;
  drawText(text, x, y, style, color);
}

void DisplayDriver::drawText(const char *text, int16_t x, int16_t y, DisplayTextStyle style, uint16_t color) {
  applyTextStyle(style, color);
  HostState &s = state(*this);
  int16_t cursorX = x;
  int16_t cursorY = y;
  for (const char *ptr = text; *ptr; ++ptr) {
    const uint8_t code = static_cast<uint8_t>(*ptr);
    if (code == '\n') {
      cursorX = 0;
      cursorY += style == DisplayTextStyle::Primary ? magicalmond_ogyg820pt7b.yAdvance : 8;
      continue;
    }
    if (code == '\r') {
      continue;
    }
    if (style == DisplayTextStyle::Primary) {
      drawGfxChar(s, cursorX, cursorY, code);
      if (code >= magicalmond_ogyg820pt7b.first && code <= magicalmond_ogyg820pt7b.last) {
        cursorX += magicalmond_ogyg820pt7b.glyph[code - magicalmond_ogyg820pt7b.first].xAdvance;
      }
    } else {
      drawGlcdChar(s, cursorX, cursorY, code);
      cursorX += 6;
    }
  }
}

void DisplayDriver::setBatteryBars(uint8_t leftPercent, uint8_t rightPercent) {
  HostState &s = state(*this);
  s.leftBatteryPercent = clampPercent(leftPercent);
  s.rightBatteryPercent = clampPercent(rightPercent);
}

void DisplayDriver::drawBatteryBars() {
  HostState &s = state(*this);
  drawBatteryArc(true, s.leftBatteryPercent);
  drawBatteryArc(false, s.rightBatteryPercent);
}

void DisplayDriver::drawBatteryBars(uint8_t leftPercent, uint8_t rightPercent) {
  setBatteryBars(leftPercent, rightPercent);
  drawBatteryBars();
}

void DisplayDriver::drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color) {
  HostState &s = state(*this);
  int16_t f = 1 - radius;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * radius;
  int16_t px = 0;
  int16_t py = radius;
  writePixel(s, x, y + radius, color);
  writePixel(s, x, y - radius, color);
  writePixel(s, x + radius, y, color);
  writePixel(s, x - radius, y, color);
  while (px < py) {
    if (f >= 0) {
      --py;
      ddF_y += 2;
      f += ddF_y;
    }
    ++px;
    ddF_x += 2;
    f += ddF_x;
    writePixel(s, x + px, y + py, color);
    writePixel(s, x - px, y + py, color);
    writePixel(s, x + px, y - py, color);
    writePixel(s, x - px, y - py, color);
    writePixel(s, x + py, y + px, color);
    writePixel(s, x - py, y + px, color);
    writePixel(s, x + py, y - px, color);
    writePixel(s, x - py, y - px, color);
  }
}

void DisplayDriver::fillCircle(int16_t x, int16_t y, int16_t radius, uint16_t color) {
  HostState &s = state(*this);
  writeFillRect(s, x, y - radius, 1, 2 * radius + 1, color);
  fillCircleHelper(s, x, y, radius, 3, 0, color);
}

void DisplayDriver::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  HostState &s = state(*this);
  bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
  if (steep) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }
  const int16_t dx = x1 - x0;
  const int16_t dy = std::abs(y1 - y0);
  int16_t err = dx / 2;
  const int16_t ystep = y0 < y1 ? 1 : -1;
  int16_t y = y0;
  for (int16_t x = x0; x <= x1; ++x) {
    writePixel(s, steep ? y : x, steep ? x : y, color);
    err -= dy;
    if (err < 0) {
      y += ystep;
      err += dx;
    }
  }
}

void DisplayDriver::drawRoundRect(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, uint16_t color) {
  HostState &s = state(*this);
  writeFillRect(s, x + radius, y, width - 2 * radius, 1, color);
  writeFillRect(s, x + radius, y + height - 1, width - 2 * radius, 1, color);
  writeFillRect(s, x, y + radius, 1, height - 2 * radius, color);
  writeFillRect(s, x + width - 1, y + radius, 1, height - 2 * radius, color);
  drawCircleHelper(s, x + radius, y + radius, radius, 1, color);
  drawCircleHelper(s, x + width - radius - 1, y + radius, radius, 2, color);
  drawCircleHelper(s, x + width - radius - 1, y + height - radius - 1, radius, 4, color);
  drawCircleHelper(s, x + radius, y + height - radius - 1, radius, 8, color);
}

void DisplayDriver::fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, uint16_t color) {
  HostState &s = state(*this);
  writeFillRect(s, x + radius, y, width - 2 * radius, height, color);
  fillCircleHelper(s, x + width - radius - 1, y + radius, radius, 1, height - 2 * radius - 1, color);
  fillCircleHelper(s, x + radius, y + radius, radius, 2, height - 2 * radius - 1, color);
}

void DisplayDriver::drawRect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color) {
  HostState &s = state(*this);
  writeFillRect(s, x, y, width, 1, color);
  writeFillRect(s, x, y + height - 1, width, 1, color);
  writeFillRect(s, x, y, 1, height, color);
  writeFillRect(s, x + width - 1, y, 1, height, color);
}

void DisplayDriver::fillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color) {
  writeFillRect(state(*this), x, y, width, height, color);
}

void DisplayDriver::drawRgb565Bitmap(int16_t x, int16_t y, const uint16_t *pixels, int16_t width, int16_t height) {
  HostState &s = state(*this);
  for (int16_t yy = 0; yy < height; ++yy) {
    for (int16_t xx = 0; xx < width; ++xx) {
      writePixel(s, x + xx, y + yy, pixels[static_cast<size_t>(yy) * width + xx]);
    }
  }
}

void DisplayDriver::applyTextStyle(DisplayTextStyle style, uint16_t color) {
  HostState &s = state(*this);
  s.textStyle = style;
  s.textColor = color;
}

void DisplayDriver::drawBatteryArc(bool leftSide, uint8_t percent) {
  const uint8_t clampedPercent = clampPercent(percent);
  const int16_t startDeg = leftSide ? kLeftBatteryStartDeg : kRightBatteryStartDeg;
  const int16_t sweepDeg = leftSide ? kBatteryArcSweepDeg : -kBatteryArcSweepDeg;
  const int16_t fillSweepDeg = (sweepDeg * clampedPercent) / 100;
  drawArcSegment(startDeg, sweepDeg, kBatteryTrackColor, kBatteryTrackThickness);
  if (fillSweepDeg != 0) {
    drawArcSegment(startDeg, fillSweepDeg, batteryColor(clampedPercent), kBatteryArcThickness);
  }
}

void DisplayDriver::drawArcSegment(int16_t startDeg, int16_t sweepDeg, uint16_t color, uint8_t thickness) {
  const int16_t coreHalfWidth = thickness > 1 ? 1 : 0;
  const uint16_t edgeColor = scaleColor(color, 92);
  drawArcLine(startDeg, sweepDeg, kBatteryArcRadius - coreHalfWidth - 1, edgeColor);
  drawArcLine(startDeg, sweepDeg, kBatteryArcRadius + coreHalfWidth + 1, edgeColor);
  for (int16_t offset = -coreHalfWidth; offset <= coreHalfWidth; ++offset) {
    drawArcLine(startDeg, sweepDeg, kBatteryArcRadius + offset, color);
  }
}

void DisplayDriver::drawArcLine(int16_t startDeg, int16_t sweepDeg, int16_t radius, uint16_t color) {
  const int16_t step = sweepDeg >= 0 ? kArcStepDeg : -kArcStepDeg;
  const int16_t endDeg = startDeg + sweepDeg;
  for (int16_t deg = startDeg; deg != endDeg; deg += step) {
    int16_t nextDeg = deg + step;
    if (sweepDeg >= 0 ? nextDeg > endDeg : nextDeg < endDeg) {
      nextDeg = endDeg;
    }
    const float radians = deg * DEG_TO_RAD;
    const float nextRadians = nextDeg * DEG_TO_RAD;
    const int16_t x0 = kScreenCenter + static_cast<int16_t>(std::round(std::cos(radians) * radius));
    const int16_t y0 = kScreenCenter + static_cast<int16_t>(std::round(std::sin(radians) * radius));
    const int16_t x1 = kScreenCenter + static_cast<int16_t>(std::round(std::cos(nextRadians) * radius));
    const int16_t y1 = kScreenCenter + static_cast<int16_t>(std::round(std::sin(nextRadians) * radius));
    drawLine(x0, y0, x1, y1, color);
  }
}

bool writeDisplayPng(DisplayDriver &display, const char *path) {
  const HostState &s = state(display);
  std::vector<uint8_t> raw;
  raw.resize(static_cast<size_t>(kScreenSize) * (kScreenSize * 4 + 1));
  for (int16_t y = 0; y < kScreenSize; ++y) {
    const size_t rowOffset = static_cast<size_t>(y) * (kScreenSize * 4 + 1);
    raw[rowOffset] = 0;
    for (int16_t x = 0; x < kScreenSize; ++x) {
      rgb565ToRgba(s.pixels[static_cast<size_t>(y) * kScreenSize + x],
                   &raw[rowOffset + 1 + static_cast<size_t>(x) * 4]);
    }
  }

  uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
  std::vector<uint8_t> compressed(compressedSize);
  if (compress2(compressed.data(), &compressedSize, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_COMPRESSION) != Z_OK) {
    return false;
  }
  compressed.resize(compressedSize);

  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<uint8_t> ihdr;
  writeU32(ihdr, kScreenSize);
  writeU32(ihdr, kScreenSize);
  ihdr.push_back(8);
  ihdr.push_back(6);
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);
  writeChunk(png, "IHDR", ihdr);
  writeChunk(png, "IDAT", compressed);
  writeChunk(png, "IEND", {});

  FILE *file = std::fopen(path, "wb");
  if (!file) {
    return false;
  }
  const bool ok = std::fwrite(png.data(), 1, png.size(), file) == png.size();
  std::fclose(file);
  return ok;
}
