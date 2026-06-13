#include "drivers/DisplayDriver.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "Pins.h"
#include "assets/fonts/magicalmond_ogyg820pt7b.h"

namespace {
constexpr int16_t kScreenSize = 240;
constexpr int16_t kScreenCenter = kScreenSize / 2;
} // namespace

DisplayDriver::DisplayDriver()
    : bus_(new Arduino_ESP32SPI(
          Pins::TFT_DC,
          Pins::TFT_CS,
          Pins::TFT_SCK,
          Pins::TFT_MOSI,
          GFX_NOT_DEFINED)),
      gfx_(new Arduino_GC9A01(
          bus_,
          Pins::TFT_RST,
          0,
          true,
          240,
          240)) {}

bool DisplayDriver::begin() {
  return gfx_->begin(80000000);
}

void DisplayDriver::setSleep(bool sleeping) {
  if (sleeping_ == sleeping) {
    return;
  }

  sleeping_ = sleeping;
  if (sleeping_) {
    gfx_->displayOff();
  } else {
    gfx_->displayOn();
  }
}

void DisplayDriver::clear(uint16_t color) {
  gfx_->fillScreen(color);
}

void DisplayDriver::drawTextCentered(const char *text) {
  clear(BLACK);
  drawTextCentered(text, kScreenCenter, DisplayTextStyle::Primary, WHITE);
}

void DisplayDriver::drawTextCentered(
    const char *text,
    int16_t centerY,
    DisplayTextStyle style,
    uint16_t color) {
  applyTextStyle(style, color);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  gfx_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  const int16_t x = (kScreenSize - static_cast<int16_t>(w)) / 2 - x1;
  const int16_t y = centerY - static_cast<int16_t>(h) / 2 - y1;
  gfx_->setCursor(x, y);
  gfx_->println(text);
}

void DisplayDriver::drawText(
    const char *text,
    int16_t x,
    int16_t y,
    DisplayTextStyle style,
    uint16_t color) {
  applyTextStyle(style, color);
  gfx_->setCursor(x, y);
  gfx_->print(text);
}

void DisplayDriver::drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color) {
  gfx_->drawCircle(x, y, radius, color);
}

void DisplayDriver::fillCircle(int16_t x, int16_t y, int16_t radius, uint16_t color) {
  gfx_->fillCircle(x, y, radius, color);
}

void DisplayDriver::drawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color) {
  gfx_->drawLine(x0, y0, x1, y1, color);
}

void DisplayDriver::drawRoundRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    int16_t radius,
    uint16_t color) {
  gfx_->drawRoundRect(x, y, width, height, radius, color);
}

void DisplayDriver::fillRoundRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    int16_t radius,
    uint16_t color) {
  gfx_->fillRoundRect(x, y, width, height, radius, color);
}

void DisplayDriver::drawRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint16_t color) {
  gfx_->drawRect(x, y, width, height, color);
}

void DisplayDriver::fillRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint16_t color) {
  gfx_->fillRect(x, y, width, height, color);
}

void DisplayDriver::drawRgb565Bitmap(
    int16_t x,
    int16_t y,
    const uint16_t *pixels,
    int16_t width,
    int16_t height) {
  gfx_->draw16bitRGBBitmap(x, y, const_cast<uint16_t *>(pixels), width, height);
}

void DisplayDriver::applyTextStyle(DisplayTextStyle style, uint16_t color) {
  gfx_->setTextColor(color);
  gfx_->setTextSize(1);

  if (style == DisplayTextStyle::Primary) {
    gfx_->setFont(&magicalmond_ogyg820pt7b);
    return;
  }

  gfx_->setFont(nullptr);
}
