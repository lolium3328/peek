#include "ui/ScreenRenderer.h"

#include <Arduino.h>
#include <math.h>
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
constexpr int16_t kCubeCenterY = kScreenCenter;
constexpr int16_t kPetAreaX = 61;
constexpr int16_t kPetAreaY = 61;
constexpr int16_t kPetAreaSize = 118;
constexpr float kDefaultCubeScale = 32.0f;

struct CubePoint {
  int16_t x = 0;
  int16_t y = 0;
  float z = 0.0f;
};

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

  if (model.cubeVisible) {
    drawPetCube(model);
  } else {
    display_.drawTextCentered(model.primaryText, 122, DisplayTextStyle::Primary, kWhite);
  }
  drawBottomHint(model.hintText);
}

void ScreenRenderer::renderHomeFrame(const HomeScreenModel &model) {
  clearPetArea();

  if (model.poseAlert) {
    display_.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
  }

  if (model.cubeVisible) {
    drawPetCube(model);
  } else {
    display_.drawTextCentered(model.primaryText, 122, DisplayTextStyle::Primary, kWhite);
  }
}

void ScreenRenderer::renderStatus(const StatusScreenModel &model) {
  char buttonText[20];
  char rssiText[20];
  char imuText[20];
  char accelText[20];
  char poseText[24];
  char localBatteryText[20];
  char peerBatteryText[20];

  snprintf(buttonText, sizeof(buttonText), "button %s", model.buttonPressed ? "down" : "up");
  snprintf(rssiText, sizeof(rssiText), "rssi %d", model.wifiRssi);
  if (model.imuReady) {
    snprintf(imuText, sizeof(imuText), "imu ok 0x%02X", model.imuAddress);
  } else {
    snprintf(imuText, sizeof(imuText), "imu missing");
  }
  snprintf(accelText, sizeof(accelText), "az %d", model.imuAccelZ);
  snprintf(poseText, sizeof(poseText), "rp %.0f %.0f", model.imuRollDeg, model.imuPitchDeg);
  snprintf(localBatteryText, sizeof(localBatteryText), "A %u%%", model.localBatteryPercent);
  snprintf(peerBatteryText, sizeof(peerBatteryText), "B %u%%", model.peerBatteryPercent);

  display_.clear(kBlack);
  display_.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
  display_.drawTextCentered("status", 48, DisplayTextStyle::Small, kBlue);
  display_.drawTextCentered(buttonText, 76, DisplayTextStyle::Small, kWhite);
  display_.drawTextCentered(imuText, 99, DisplayTextStyle::Small, stateColor(model.imuReady));
  display_.drawTextCentered(accelText, 122, DisplayTextStyle::Small, kWhite);
  display_.drawTextCentered(poseText, 145, DisplayTextStyle::Small, kWhite);
  display_.drawTextCentered(rssiText, 161, DisplayTextStyle::Small, kWhite);
  display_.drawTextCentered(localBatteryText, 184, DisplayTextStyle::Small, batteryColor(model.localBatteryPercent));
  display_.drawTextCentered(peerBatteryText, 199, DisplayTextStyle::Small, batteryColor(model.peerBatteryPercent));
  drawStatusPill(78, 211, model.backendConnected ? "backend ok" : "backend off", stateColor(model.backendConnected));
}

void ScreenRenderer::drawTopStatus(const HomeScreenModel &model) {
  drawWeatherChip(41, model.localLabel, model.localWeather);
  drawWeatherChip(151, model.peerLabel, model.peerWeather);
  drawConnectionDots(model.wifiConnected, model.backendConnected);
}

void ScreenRenderer::clearPetArea() {
  display_.fillRect(kPetAreaX, kPetAreaY, kPetAreaSize, kPetAreaSize, kBlack);
}

void ScreenRenderer::drawPetCube(const HomeScreenModel &model) {
  static constexpr int8_t vertices[8][3] = {
      {-1, -1, -1},
      {1, -1, -1},
      {1, 1, -1},
      {-1, 1, -1},
      {-1, -1, 1},
      {1, -1, 1},
      {1, 1, 1},
      {-1, 1, 1},
  };
  static constexpr uint8_t edges[12][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0},
      {4, 5}, {5, 6}, {6, 7}, {7, 4},
      {0, 4}, {1, 5}, {2, 6}, {3, 7},
  };

  const float roll = model.cubeRollDeg * DEG_TO_RAD;
  const float pitch = model.cubePitchDeg * DEG_TO_RAD;
  const float yaw = model.cubeYawDeg * DEG_TO_RAD;
  const float sr = sinf(roll);
  const float cr = cosf(roll);
  const float sp = sinf(pitch);
  const float cp = cosf(pitch);
  const float sy = sinf(yaw);
  const float cy = cosf(yaw);
  const float scale = model.cubeScale > 0.0f ? model.cubeScale : kDefaultCubeScale;
  const int16_t centerX = kScreenCenter + static_cast<int16_t>(roundf(model.cubeOffsetX));
  const int16_t centerY = kCubeCenterY + static_cast<int16_t>(roundf(model.cubeOffsetY));

  CubePoint points[8];
  for (uint8_t index = 0; index < 8; ++index) {
    const float x = static_cast<float>(vertices[index][0]);
    const float y = static_cast<float>(vertices[index][1]);
    const float z = static_cast<float>(vertices[index][2]);

    const float yRoll = y * cr - z * sr;
    const float zRoll = y * sr + z * cr;
    const float xPitch = x * cp + zRoll * sp;
    const float zPitch = -x * sp + zRoll * cp;
    const float xYaw = xPitch * cy - yRoll * sy;
    const float yYaw = xPitch * sy + yRoll * cy;

    points[index].x = centerX + static_cast<int16_t>(roundf(xYaw * scale));
    points[index].y = centerY + static_cast<int16_t>(roundf(yYaw * scale));
    points[index].z = zPitch;
  }

  for (uint8_t index = 0; index < 12; ++index) {
    const CubePoint &a = points[edges[index][0]];
    const CubePoint &b = points[edges[index][1]];
    const uint16_t color = (a.z + b.z) > 0.0f ? kGreen : kMuted;
    display_.drawLine(a.x, a.y, b.x, b.y, color);
  }
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
