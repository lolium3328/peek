#include "drivers/DisplayDriver.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "Pins.h"
#include "assets/fonts/magicalmond_ogyg820pt7b.h"

namespace {
constexpr int16_t kScreenSize = 240;
constexpr int16_t kScreenCenter = kScreenSize / 2;
constexpr int16_t kBatteryArcRadius = 109;
constexpr int16_t kLeftBatteryStartDeg = 142;
constexpr int16_t kRightBatteryStartDeg = 38;
constexpr int16_t kBatteryArcSweepDeg = 76;
constexpr uint8_t kBatteryArcThickness = 4;
constexpr uint8_t kBatteryTrackThickness = 2;
constexpr uint16_t kBatteryTrackColor = 0x18E3;
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
  drawBatteryArc(true, leftBatteryPercent_);
  drawBatteryArc(false, rightBatteryPercent_);
}

void DisplayDriver::drawBatteryArc(bool leftSide, uint8_t percent) {
  const uint8_t clampedPercent = clampPercent(percent);
  const int16_t startDeg = leftSide ? kLeftBatteryStartDeg : kRightBatteryStartDeg;
  const int16_t sweepDeg = leftSide ? kBatteryArcSweepDeg : -kBatteryArcSweepDeg;
  const int16_t fillSweepDeg = (sweepDeg * clampedPercent) / 100;

  drawArcSegment(startDeg, sweepDeg, kBatteryTrackColor, kBatteryTrackThickness);

  if (fillSweepDeg == 0) {
    return;
  }

  drawArcSegment(startDeg, fillSweepDeg, batteryColor(clampedPercent), kBatteryArcThickness);
}

void DisplayDriver::drawArcSegment(
    int16_t startDeg,
    int16_t sweepDeg,
    uint16_t color,
    uint8_t thickness) {
  const int16_t step = sweepDeg >= 0 ? 2 : -2;
  const int16_t endDeg = startDeg + sweepDeg;

  for (int16_t deg = startDeg; sweepDeg >= 0 ? deg <= endDeg : deg >= endDeg; deg += step) {
    const float radians = deg * DEG_TO_RAD;
    const int16_t x = kScreenCenter + static_cast<int16_t>(cos(radians) * kBatteryArcRadius);
    const int16_t y = kScreenCenter + static_cast<int16_t>(sin(radians) * kBatteryArcRadius);
    gfx_->fillCircle(x, y, thickness, color);
  }
}
