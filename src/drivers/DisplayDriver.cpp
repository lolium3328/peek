#include "drivers/DisplayDriver.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "assets/fonts/magicalmond_ogyg820pt7b.h"

namespace {
constexpr int16_t kScreenSize = 240;
constexpr int16_t kScreenCenter = kScreenSize / 2;
constexpr int16_t kBatteryArcRadius = 109;
constexpr int16_t kLeftBatteryStartDeg = 142;
constexpr int16_t kRightBatteryStartDeg = 38;
constexpr int16_t kBatteryArcSweepDeg = 76;
constexpr uint8_t kBatteryArcThickness = 2;
constexpr uint8_t kBatteryTrackThickness = 1;
constexpr uint16_t kBatteryTrackColor = 0x18E3;
constexpr uint16_t kBatteryHighColor = 0x05F4;
constexpr uint16_t kBatteryMidColor = 0xFDC0;
constexpr uint16_t kBatteryLowColor = 0xF9C6;
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;

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

uint32_t toTftArcAngle(int16_t mathAngle) {
  int32_t angle = static_cast<int32_t>(mathAngle) + 270;
  angle %= 360;
  if (angle < 0) {
    angle += 360;
  }
  return static_cast<uint32_t>(angle);
}
} // namespace

DisplayDriver::DisplayDriver() : tft_(new TFT_eSPI()) {}

bool DisplayDriver::begin() {
  tft_->begin();
  tft_->setRotation(0);
  return true;
}

void DisplayDriver::clear(uint16_t color) {
  tft_->fillScreen(color);
}

void DisplayDriver::drawTextCentered(const char *text) {
  clear(kBlack);
  drawBatteryBars();
  drawTextCentered(text, kScreenCenter, DisplayTextStyle::Primary, kWhite);
}

void DisplayDriver::drawTextCentered(
    const char *text,
    int16_t centerY,
    DisplayTextStyle style,
    uint16_t color) {
  applyTextStyle(style, color);
  tft_->setTextDatum(MC_DATUM);
  tft_->drawString(text, kScreenCenter, centerY);
  tft_->setTextDatum(TL_DATUM);
}

void DisplayDriver::drawText(
    const char *text,
    int16_t x,
    int16_t y,
    DisplayTextStyle style,
    uint16_t color) {
  applyTextStyle(style, color);
  tft_->setTextDatum(TL_DATUM);
  tft_->drawString(text, x, y);
}

void DisplayDriver::setBatteryBars(uint8_t leftPercent, uint8_t rightPercent) {
  leftBatteryPercent_ = clampPercent(leftPercent);
  rightBatteryPercent_ = clampPercent(rightPercent);
}

void DisplayDriver::drawBatteryBars() {
  drawBatteryArc(true, leftBatteryPercent_);
  drawBatteryArc(false, rightBatteryPercent_);
}

void DisplayDriver::drawBatteryBars(uint8_t leftPercent, uint8_t rightPercent) {
  setBatteryBars(leftPercent, rightPercent);
  drawBatteryBars();
}

void DisplayDriver::drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color) {
  tft_->drawCircle(x, y, radius, color);
}

void DisplayDriver::fillCircle(int16_t x, int16_t y, int16_t radius, uint16_t color) {
  tft_->fillCircle(x, y, radius, color);
}

void DisplayDriver::drawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color) {
  tft_->drawLine(x0, y0, x1, y1, color);
}

void DisplayDriver::drawRoundRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    int16_t radius,
    uint16_t color) {
  tft_->drawRoundRect(x, y, width, height, radius, color);
}

void DisplayDriver::fillRoundRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    int16_t radius,
    uint16_t color) {
  tft_->fillRoundRect(x, y, width, height, radius, color);
}

void DisplayDriver::drawRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint16_t color) {
  tft_->drawRect(x, y, width, height, color);
}

void DisplayDriver::fillRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint16_t color) {
  tft_->fillRect(x, y, width, height, color);
}

void DisplayDriver::applyTextStyle(DisplayTextStyle style, uint16_t color) {
  tft_->setTextColor(color);
  tft_->setTextSize(1);

  if (style == DisplayTextStyle::Primary) {
    tft_->setFreeFont(&magicalmond_ogyg820pt7b);
    return;
  }

  tft_->setFreeFont(nullptr);
  tft_->setTextFont(1);
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
  if (sweepDeg == 0) {
    return;
  }

  const int16_t endDeg = startDeg + sweepDeg;
  const uint32_t arcStart = sweepDeg > 0 ? toTftArcAngle(startDeg) : toTftArcAngle(endDeg);
  const uint32_t arcEnd = sweepDeg > 0 ? toTftArcAngle(endDeg) : toTftArcAngle(startDeg);
  const uint32_t innerRadius = kBatteryArcRadius > thickness
                                   ? kBatteryArcRadius - thickness + 1
                                   : kBatteryArcRadius;

  if (thickness == 1) {
    tft_->drawSmoothArc(
        kScreenCenter,
        kScreenCenter,
        kBatteryArcRadius,
        kBatteryArcRadius,
        arcStart,
        arcEnd,
        color,
        kBlack,
        true);
    return;
  }

  tft_->drawSmoothArc(
      kScreenCenter,
      kScreenCenter,
      kBatteryArcRadius,
      innerRadius,
      arcStart,
      arcEnd,
      color,
      kBlack,
      true);
}
