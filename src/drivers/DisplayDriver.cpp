#include "drivers/DisplayDriver.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "Pins.h"
#include "assets/fonts/magicalmond_ogyg820pt7b.h"

namespace {
constexpr int16_t kScreenSize = 240;
constexpr int16_t kBatteryBarY = 58;
constexpr int16_t kBatteryBarWidth = 8;
constexpr int16_t kBatteryBarHeight = 124;
constexpr int16_t kLeftBatteryBarX = 15;
constexpr int16_t kRightBatteryBarX = kScreenSize - kLeftBatteryBarX - kBatteryBarWidth;
constexpr uint16_t kBatteryTrackColor = 0x18E3;
constexpr uint16_t kBatteryTrackOutline = 0x4208;
constexpr uint16_t kBatteryHighColor = 0x05F4;
constexpr uint16_t kBatteryMidColor = 0xFDC0;
constexpr uint16_t kBatteryLowColor = 0xF9C6;

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
  return gfx_->begin();
}

void DisplayDriver::drawTextCentered(const char *text) {
  gfx_->fillScreen(BLACK);
  drawBatteryBars();
  gfx_->setFont(&magicalmond_ogyg820pt7b);
  gfx_->setTextColor(WHITE);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  gfx_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  const int16_t x = (kScreenSize - static_cast<int16_t>(w)) / 2 - x1;
  const int16_t y = (kScreenSize - static_cast<int16_t>(h)) / 2 - y1;
  gfx_->setCursor(x, y);
  gfx_->println(text);
}

void DisplayDriver::setBatteryBars(uint8_t leftPercent, uint8_t rightPercent) {
  leftBatteryPercent_ = clampPercent(leftPercent);
  rightBatteryPercent_ = clampPercent(rightPercent);
}

void DisplayDriver::drawBatteryBars() {
  drawBatteryBar(kLeftBatteryBarX, leftBatteryPercent_);
  drawBatteryBar(kRightBatteryBarX, rightBatteryPercent_);
}

void DisplayDriver::drawBatteryBar(int16_t x, uint8_t percent) {
  const uint8_t clampedPercent = clampPercent(percent);
  const int16_t fillHeight = (kBatteryBarHeight * clampedPercent) / 100;
  const int16_t fillY = kBatteryBarY + kBatteryBarHeight - fillHeight;

  gfx_->drawRoundRect(
      x - 2,
      kBatteryBarY - 2,
      kBatteryBarWidth + 4,
      kBatteryBarHeight + 4,
      6,
      kBatteryTrackOutline);
  gfx_->fillRoundRect(
      x,
      kBatteryBarY,
      kBatteryBarWidth,
      kBatteryBarHeight,
      4,
      kBatteryTrackColor);

  if (fillHeight <= 0) {
    return;
  }

  gfx_->fillRoundRect(
      x,
      fillY,
      kBatteryBarWidth,
      fillHeight,
      4,
      batteryColor(clampedPercent));
}
