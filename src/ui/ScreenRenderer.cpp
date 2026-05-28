#include "ui/ScreenRenderer.h"

#include <Arduino.h>
#include <stdio.h>

namespace {
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kMuted = 0x8C71;
constexpr uint16_t kLine = 0x2945;
constexpr uint16_t kPanel = 0x1082;
constexpr uint16_t kGreen = 0x05F4;
constexpr uint16_t kBlue = 0x3D7F;
constexpr uint16_t kAmber = 0xFDC0;
constexpr uint16_t kRed = 0xF9C6;
constexpr int16_t kScreenCenter = 120;

uint16_t stateColor(bool active) {
  return active ? kGreen : kMuted;
}

uint16_t batteryColor(uint8_t percent) {
  if (percent < 24) {
    return kRed;
  }
  if (percent < 55) {
    return kAmber;
  }
  return kGreen;
}
} // namespace

ScreenRenderer::ScreenRenderer(DisplayDriver &display) : display_(display) {}

void ScreenRenderer::renderBoot(const BootScreenModel &model) {
  display_.clear(kBlack);
  display_.drawCircle(kScreenCenter, kScreenCenter, 110, kLine);
  display_.drawTextCentered(model.title, 105, DisplayTextStyle::Primary, kWhite);
  drawStatusPill(82, 145, model.message, kBlue);
}

void ScreenRenderer::renderHome(const HomeScreenModel &model) {
  display_.clear(kBlack);
  display_.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
  display_.drawCircle(kScreenCenter, kScreenCenter, 88, kLine);
  display_.drawCircle(kScreenCenter, kScreenCenter, 89, 0x0841);
  drawTopStatus(model);

  if (model.poseAlert) {
    display_.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
  }

  display_.drawTextCentered(model.primaryText, 122, DisplayTextStyle::Primary, kWhite);
  drawBottomHint(model.hintText);
}

void ScreenRenderer::renderStatus(const StatusScreenModel &model) {
  char touchText[20];
  char rssiText[20];
  char imuText[20];
  char accelText[20];
  char localBatteryText[20];
  char peerBatteryText[20];

  snprintf(touchText, sizeof(touchText), "touch %u", model.touchAnalog);
  snprintf(rssiText, sizeof(rssiText), "rssi %d", model.wifiRssi);
  if (model.imuReady) {
    snprintf(imuText, sizeof(imuText), "imu ok 0x%02X", model.imuAddress);
  } else {
    snprintf(imuText, sizeof(imuText), "imu missing");
  }
  snprintf(accelText, sizeof(accelText), "az %d", model.imuAccelZ);
  snprintf(localBatteryText, sizeof(localBatteryText), "A %u%%", model.localBatteryPercent);
  snprintf(peerBatteryText, sizeof(peerBatteryText), "B %u%%", model.peerBatteryPercent);

  display_.clear(kBlack);
  display_.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
  display_.drawTextCentered("status", 48, DisplayTextStyle::Small, kBlue);
  display_.drawTextCentered(touchText, 76, DisplayTextStyle::Small, kWhite);
  display_.drawTextCentered(imuText, 99, DisplayTextStyle::Small, stateColor(model.imuReady));
  display_.drawTextCentered(accelText, 122, DisplayTextStyle::Small, kWhite);
  display_.drawTextCentered(rssiText, 145, DisplayTextStyle::Small, kWhite);
  display_.drawTextCentered(localBatteryText, 168, DisplayTextStyle::Small, batteryColor(model.localBatteryPercent));
  display_.drawTextCentered(peerBatteryText, 191, DisplayTextStyle::Small, batteryColor(model.peerBatteryPercent));
  drawStatusPill(78, 205, model.backendConnected ? "backend ok" : "backend off", stateColor(model.backendConnected));
}

void ScreenRenderer::drawTopStatus(const HomeScreenModel &model) {
  drawWeatherChip(41, model.localLabel, model.localWeather);
  drawWeatherChip(151, model.peerLabel, model.peerWeather);
  drawConnectionDots(model.wifiConnected, model.backendConnected);
}

void ScreenRenderer::drawWeatherChip(int16_t x, const char *label, const char *weather) {
  display_.fillRoundRect(x, 35, 48, 22, 9, kPanel);
  display_.drawRoundRect(x, 35, 48, 22, 9, kLine);
  display_.drawText(label, x + 7, 50, DisplayTextStyle::Small, kMuted);
  display_.drawText(weather, x + 24, 50, DisplayTextStyle::Small, kWhite);
}

void ScreenRenderer::drawConnectionDots(bool wifiConnected, bool backendConnected) {
  display_.fillCircle(kScreenCenter - 7, 46, 3, stateColor(wifiConnected));
  display_.fillCircle(kScreenCenter + 7, 46, 3, stateColor(backendConnected));
  display_.drawLine(kScreenCenter - 3, 46, kScreenCenter + 3, 46, kLine);
}

void ScreenRenderer::drawBottomHint(const char *hintText) {
  display_.fillRoundRect(58, 179, 124, 24, 10, kPanel);
  display_.drawRoundRect(58, 179, 124, 24, 10, kLine);
  display_.drawTextCentered(hintText, 195, DisplayTextStyle::Small, kMuted);
}

void ScreenRenderer::drawStatusPill(int16_t x, int16_t y, const char *text, uint16_t color) {
  display_.fillRoundRect(x, y, 84, 23, 10, kPanel);
  display_.drawRoundRect(x, y, 84, 23, 10, kLine);
  display_.fillCircle(x + 12, y + 11, 3, color);
  display_.drawText(text, x + 22, y + 15, DisplayTextStyle::Small, kWhite);
}

void ScreenRenderer::drawTinyBattery(int16_t x, int16_t y, uint8_t percent, uint16_t color) {
  const int16_t fillWidth = (18 * percent) / 100;
  display_.drawRect(x, y, 21, 9, kLine);
  display_.fillRect(x + 21, y + 3, 2, 3, kLine);
  if (fillWidth > 0) {
    display_.fillRect(x + 2, y + 2, fillWidth, 5, color);
  }
}
