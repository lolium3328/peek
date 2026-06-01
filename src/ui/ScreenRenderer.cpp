#include "ui/ScreenRenderer.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
constexpr uint8_t kPkaHeaderSize = 12;
constexpr uint8_t kPkaFrameEntrySize = 10;
uint16_t petBuffer[kPetAreaSize * kPetAreaSize];

// 双缓存：pet2 动画专用，帧间只推脏矩形
uint16_t* pet2BufA = nullptr;
uint16_t* pet2BufB = nullptr;
uint16_t pet2BufWidth = 0;
uint16_t pet2BufHeight = 0;
bool pet2BufToggle = false;  // false=用A渲染, true=用B渲染
bool pet2WasActive = false;  // 上一帧 pet2 是否活跃，用于检测模式切换

void releasePet2Buffers() {
  delete[] pet2BufA;
  delete[] pet2BufB;
  pet2BufA = nullptr;
  pet2BufB = nullptr;
  pet2BufWidth = 0;
  pet2BufHeight = 0;
}

struct CubePoint {
  int16_t x = 0;
  int16_t y = 0;
  float z = 0.0f;
};

uint16_t readU16(File &file) {
  uint8_t bytes[2];
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes)) {
    return 0;
  }
  return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readU32(File &file) {
  uint8_t bytes[4];
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes)) {
    return 0;
  }
  return static_cast<uint32_t>(bytes[0])
         | (static_cast<uint32_t>(bytes[1]) << 8)
         | (static_cast<uint32_t>(bytes[2]) << 16)
         | (static_cast<uint32_t>(bytes[3]) << 24);
}

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
  resetHomeCache();
  display_.clear(kBlack);
  display_.drawCircle(kScreenCenter, kScreenCenter, 110, kLine);
  display_.drawTextCentered(model.title, 105, DisplayTextStyle::Primary, kWhite);
  drawStatusPill(82, 145, model.message, kBlue);
}

void ScreenRenderer::renderHome(const HomeScreenModel &model) {
  resetHomeCache();
  display_.clear(kBlack);
  drawHomeChrome(model);
  renderHomeContent(model, true);
}

void ScreenRenderer::renderHomeFrame(const HomeScreenModel &model) {
  if (!homeChromeDrawn_) {
    renderHome(model);
    return;
  }
  if (updateHomeChrome(model)) {
    return;
  }
  renderHomeContent(model, false);
}

void ScreenRenderer::renderStatus(const StatusScreenModel &model) {
  resetHomeCache();
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

void ScreenRenderer::renderRadialMenu(const RadialMenuModel &model) {
  resetHomeCache();
  display_.clear(kBlack);

  drawRadialSector(270.0f, radialItemColor(RadialMenuItem::Cancel, model.selectedItem == RadialMenuItem::Cancel));
  drawRadialSector(90.0f, radialItemColor(RadialMenuItem::Info, model.selectedItem == RadialMenuItem::Info));
  drawRadialSector(180.0f,
                   radialItemColor(RadialMenuItem::PreviousPet,
                                   model.selectedItem == RadialMenuItem::PreviousPet));
  drawRadialSector(0.0f,
                   radialItemColor(RadialMenuItem::NextPet,
                                   model.selectedItem == RadialMenuItem::NextPet));

  display_.drawLine(205, 35, 35, 205, kWhite);
  display_.drawLine(35, 35, 205, 205, kWhite);
  display_.drawCircle(kScreenCenter, kScreenCenter, 116, kLine);
  display_.drawCircle(kScreenCenter, kScreenCenter, 76, kLine);
  display_.fillCircle(kScreenCenter, kScreenCenter, 31, kBlack);
  display_.drawCircle(kScreenCenter, kScreenCenter, 31, kWhite);

  display_.drawTextCentered("info", 58, DisplayTextStyle::Small,
                            model.selectedItem == RadialMenuItem::Info ? kWhite : kMuted);
  display_.drawTextCentered("cancel", 185, DisplayTextStyle::Small,
                            model.selectedItem == RadialMenuItem::Cancel ? kWhite : kMuted);
  display_.drawText("prev", 34, 124, DisplayTextStyle::Small,
                    model.selectedItem == RadialMenuItem::PreviousPet ? kWhite : kMuted);
  display_.drawText("next", 180, 124, DisplayTextStyle::Small,
                    model.selectedItem == RadialMenuItem::NextPet ? kWhite : kMuted);
  display_.drawTextCentered(radialItemLabel(model.selectedItem), 121, DisplayTextStyle::Small, kWhite);
  if (!model.imuReady) {
    display_.drawTextCentered("imu?", 141, DisplayTextStyle::Small, kAmber);
  } else if (model.calibratingHint) {
    display_.drawTextCentered("cal", 141, DisplayTextStyle::Small, kBlue);
  }

  drawRadialCursor(model.cursorX, model.cursorY, kWhite);
}

void ScreenRenderer::renderRadialCalibration(const RadialCalibrationModel &model) {
  resetHomeCache();
  display_.clear(kBlack);

  drawRadialSector(270.0f, radialItemColor(RadialMenuItem::Cancel, false));
  drawRadialSector(90.0f, radialItemColor(RadialMenuItem::Info, model.targetItem == RadialMenuItem::Info));
  drawRadialSector(180.0f, radialItemColor(RadialMenuItem::PreviousPet,
                                           model.targetItem == RadialMenuItem::PreviousPet));
  drawRadialSector(0.0f, radialItemColor(RadialMenuItem::NextPet,
                                         model.targetItem == RadialMenuItem::NextPet));

  display_.drawLine(205, 35, 35, 205, kWhite);
  display_.drawLine(35, 35, 205, 205, kWhite);
  display_.drawCircle(kScreenCenter, kScreenCenter, 116, kLine);
  display_.drawCircle(kScreenCenter, kScreenCenter, 76, kLine);
  display_.fillCircle(kScreenCenter, kScreenCenter, 34, kBlack);
  display_.drawCircle(kScreenCenter, kScreenCenter, 34, model.failed ? kRed : kBlue);

  display_.drawTextCentered(model.failed ? "retry" : "cal", 108, DisplayTextStyle::Small,
                            model.failed ? kRed : kBlue);
  char stepText[12];
  snprintf(stepText, sizeof(stepText), "%u/4", static_cast<unsigned>(model.completedCount));
  display_.drawTextCentered(stepText, 127, DisplayTextStyle::Small, kWhite);
  const int16_t progressWidth = static_cast<int16_t>(48.0f * model.holdProgress);
  display_.drawRect(96, 143, 48, 5, kLine);
  if (progressWidth > 0) {
    display_.fillRect(96, 143, progressWidth, 5, model.failed ? kRed : kGreen);
  }

  drawRadialCursor(model.cursorX, model.cursorY, model.failed ? kRed : kWhite);
}

void ScreenRenderer::resetHomeCache() {
  homeChromeDrawn_ = false;
  lastHomeContentKind_ = HomeContentKind::None;
  lastLocalBatteryPercent_ = 0;
  lastPeerBatteryPercent_ = 0;
  lastWifiConnected_ = false;
  lastBackendConnected_ = false;
  lastPoseAlert_ = false;
  lastPrimaryText_[0] = '\0';
  lastHintText_[0] = '\0';
  lastLocalWeather_[0] = '\0';
  lastPeerWeather_[0] = '\0';
  lastLocalLabel_[0] = '\0';
  lastPeerLabel_[0] = '\0';
  lastAnimationPath_[0] = '\0';
  pet2WasActive = false;
}

void ScreenRenderer::drawHomeChrome(const HomeScreenModel &model) {
  display_.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
  display_.drawCircle(kScreenCenter, kScreenCenter, 88, kLine);
  display_.drawCircle(kScreenCenter, kScreenCenter, 89, 0x0841);
  drawTopStatus(model);
  if (model.poseAlert) {
    display_.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
  }
  drawBottomHint(model.hintText);

  lastLocalBatteryPercent_ = model.localBatteryPercent;
  lastPeerBatteryPercent_ = model.peerBatteryPercent;
  lastWifiConnected_ = model.wifiConnected;
  lastBackendConnected_ = model.backendConnected;
  lastPoseAlert_ = model.poseAlert;
  copyText(lastHintText_, sizeof(lastHintText_), model.hintText);
  copyText(lastLocalWeather_, sizeof(lastLocalWeather_), model.localWeather);
  copyText(lastPeerWeather_, sizeof(lastPeerWeather_), model.peerWeather);
  copyText(lastLocalLabel_, sizeof(lastLocalLabel_), model.localLabel);
  copyText(lastPeerLabel_, sizeof(lastPeerLabel_), model.peerLabel);
  homeChromeDrawn_ = true;
}

bool ScreenRenderer::updateHomeChrome(const HomeScreenModel &model) {
  if (model.poseAlert != lastPoseAlert_) {
    renderHome(model);
    return true;
  }

  if (model.localBatteryPercent != lastLocalBatteryPercent_
      || model.peerBatteryPercent != lastPeerBatteryPercent_) {
    display_.drawBatteryBars(model.localBatteryPercent, model.peerBatteryPercent);
    lastLocalBatteryPercent_ = model.localBatteryPercent;
    lastPeerBatteryPercent_ = model.peerBatteryPercent;
  }

  if (textChanged(lastLocalWeather_, model.localWeather)
      || textChanged(lastPeerWeather_, model.peerWeather)
      || textChanged(lastLocalLabel_, model.localLabel)
      || textChanged(lastPeerLabel_, model.peerLabel)) {
    drawTopStatus(model);
    copyText(lastLocalWeather_, sizeof(lastLocalWeather_), model.localWeather);
    copyText(lastPeerWeather_, sizeof(lastPeerWeather_), model.peerWeather);
    copyText(lastLocalLabel_, sizeof(lastLocalLabel_), model.localLabel);
    copyText(lastPeerLabel_, sizeof(lastPeerLabel_), model.peerLabel);
  } else if (model.wifiConnected != lastWifiConnected_
             || model.backendConnected != lastBackendConnected_) {
    drawConnectionDots(model.wifiConnected, model.backendConnected);
  }

  if (model.wifiConnected != lastWifiConnected_) {
    lastWifiConnected_ = model.wifiConnected;
  }
  if (model.backendConnected != lastBackendConnected_) {
    lastBackendConnected_ = model.backendConnected;
  }

  if (textChanged(lastHintText_, model.hintText)) {
    drawBottomHint(model.hintText);
    copyText(lastHintText_, sizeof(lastHintText_), model.hintText);
  }

  return false;
}

void ScreenRenderer::renderHomeContent(const HomeScreenModel &model, bool force) {
  HomeContentKind kind = homeContentKind(model);
  if (kind == HomeContentKind::Animation && !model.petAnimationPath) {
    kind = HomeContentKind::Text;
  }

  const bool animationChanged =
      kind == HomeContentKind::Animation && textChanged(lastAnimationPath_, model.petAnimationPath);

  if (kind != lastHomeContentKind_ || animationChanged) {
    clearPetArea();
    pet2WasActive = false;
  }

  if (kind == HomeContentKind::Animation) {
    if (drawPetAnimation(model)) {
      copyText(lastPrimaryText_, sizeof(lastPrimaryText_), model.primaryText);
      copyText(lastAnimationPath_, sizeof(lastAnimationPath_), model.petAnimationPath);
      lastHomeContentKind_ = HomeContentKind::Animation;
      return;
    }
    kind = model.cubeVisible ? HomeContentKind::Cube : HomeContentKind::Text;
    clearPetArea();
    pet2WasActive = false;
  }

  if (kind == HomeContentKind::Cube) {
    pet2WasActive = false;
    drawPetCubeBuffered(model);
  } else {
    pet2WasActive = false;
    if (force || kind != lastHomeContentKind_ || textChanged(lastPrimaryText_, model.primaryText)) {
      clearPetArea();
      display_.drawTextCentered(model.primaryText, 122, DisplayTextStyle::Primary, kWhite);
    }
  }

  copyText(lastPrimaryText_, sizeof(lastPrimaryText_), model.primaryText);
  lastAnimationPath_[0] = '\0';
  lastHomeContentKind_ = kind;
}

ScreenRenderer::HomeContentKind ScreenRenderer::homeContentKind(const HomeScreenModel &model) const {
  if (model.petAnimationVisible) {
    return HomeContentKind::Animation;
  }
  if (model.cubeVisible) {
    return HomeContentKind::Cube;
  }
  return HomeContentKind::Text;
}

bool ScreenRenderer::textChanged(const char *cached, const char *current) const {
  const char *safeCurrent = current ? current : "";
  return strcmp(cached, safeCurrent) != 0;
}

void ScreenRenderer::copyText(char *target, uint8_t targetSize, const char *source) {
  if (targetSize == 0) {
    return;
  }
  const char *safeSource = source ? source : "";
  strncpy(target, safeSource, targetSize - 1);
  target[targetSize - 1] = '\0';
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

void ScreenRenderer::drawPetCubeBuffered(const HomeScreenModel &model) {
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

  clearPetBuffer(kBlack);

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

    points[index].x = centerX + static_cast<int16_t>(roundf(xYaw * scale)) - kPetAreaX;
    points[index].y = centerY + static_cast<int16_t>(roundf(yYaw * scale)) - kPetAreaY;
    points[index].z = zPitch;
  }

  for (uint8_t index = 0; index < 12; ++index) {
    const CubePoint &a = points[edges[index][0]];
    const CubePoint &b = points[edges[index][1]];
    const uint16_t color = (a.z + b.z) > 0.0f ? kGreen : kMuted;
    drawPetBufferLine(a.x, a.y, b.x, b.y, color);
  }

  display_.drawRgb565Bitmap(kPetAreaX, kPetAreaY, petBuffer, kPetAreaSize, kPetAreaSize);
}

void ScreenRenderer::clearPetBuffer(uint16_t color) {
  for (uint16_t index = 0; index < kPetAreaSize * kPetAreaSize; ++index) {
    petBuffer[index] = color;
  }
}

void ScreenRenderer::putPetPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= kPetAreaSize || y >= kPetAreaSize) {
    return;
  }
  petBuffer[static_cast<uint16_t>(y) * kPetAreaSize + static_cast<uint16_t>(x)] = color;
}

void ScreenRenderer::drawPetBufferLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color) {
  const int16_t dx = abs(x1 - x0);
  const int16_t sx = x0 < x1 ? 1 : -1;
  const int16_t dy = -abs(y1 - y0);
  const int16_t sy = y0 < y1 ? 1 : -1;
  int16_t error = dx + dy;

  while (true) {
    putPetPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int16_t error2 = error * 2;
    if (error2 >= dy) {
      error += dy;
      x0 += sx;
    }
    if (error2 <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

bool ScreenRenderer::drawPetAnimation(const HomeScreenModel &model) {
  if (!model.petAnimationPath || model.petAnimationPath[0] == '\0') {
    return false;
  }

  File file = LittleFS.open(model.petAnimationPath, "r");
  if (!file) {
    return false;
  }

  char magic[4];
  if (file.read(reinterpret_cast<uint8_t *>(magic), sizeof(magic)) != sizeof(magic)
      || magic[0] != 'P' || magic[1] != 'K' || magic[2] != 'A' || magic[3] != '1') {
    file.close();
    return false;
  }

  const uint16_t width = readU16(file);
  const uint16_t height = readU16(file);
  const uint16_t frameCount = readU16(file);
  readU16(file);
  if (width == 0 || height == 0 || width > kPetAreaSize || height > kPetAreaSize || frameCount == 0) {
    file.close();
    return false;
  }

  uint32_t totalDuration = 0;
  for (uint16_t index = 0; index < frameCount; ++index) {
    file.seek(kPkaHeaderSize + index * kPkaFrameEntrySize + 8);
    totalDuration += readU16(file);
  }
  if (totalDuration == 0) {
    file.close();
    return false;
  }

  const uint32_t frameTime = millis() % totalDuration;
  uint32_t elapsed = 0;
  uint16_t frameIndex = 0;
  uint32_t frameOffset = 0;
  uint32_t frameLength = 0;
  for (; frameIndex < frameCount; ++frameIndex) {
    file.seek(kPkaHeaderSize + frameIndex * kPkaFrameEntrySize);
    frameOffset = readU32(file);
    frameLength = readU32(file);
    const uint16_t delayMs = readU16(file);
    if (frameTime < elapsed + delayMs) {
      break;
    }
    elapsed += delayMs;
  }
  if (frameIndex >= frameCount || frameOffset == 0 || frameLength == 0) {
    file.close();
    return false;
  }

  const int16_t originX = kScreenCenter - static_cast<int16_t>(width) / 2;
  const int16_t originY = kCubeCenterY - static_cast<int16_t>(height) / 2;

  // 动画尺寸变化时重新分配双缓存
  const bool sizeChanged = (width != pet2BufWidth || height != pet2BufHeight);
  if (sizeChanged) {
    releasePet2Buffers();
    const uint32_t bufSize = static_cast<uint32_t>(width) * height;
    pet2BufA = new uint16_t[bufSize];
    pet2BufB = new uint16_t[bufSize];
    pet2BufWidth = width;
    pet2BufHeight = height;
    if (!pet2BufA || !pet2BufB) {
      releasePet2Buffers();
      file.close();
      return false;
    }
  }

  // 选择目标缓存（交替写入）
  uint16_t* const curBuf = pet2BufToggle ? pet2BufB : pet2BufA;
  // 首帧条件：尺寸变化 / 上一帧 pet2 未活跃（从 cube/text 切换过来）
  const bool forceFull = sizeChanged || !pet2WasActive;

  // RLE 解码到目标缓存（线性写入，无需逐行推屏）
  uint32_t remainingPixels = static_cast<uint32_t>(width) * height;
  uint32_t remainingBytes = frameLength;
  file.seek(frameOffset);

  while (remainingPixels > 0 && remainingBytes >= 4) {
    const uint16_t runLength = readU16(file);
    const uint16_t color = readU16(file);
    remainingBytes -= 4;
    uint16_t run = runLength;
    while (run > 0 && remainingPixels > 0) {
      curBuf[static_cast<uint32_t>(width) * height - remainingPixels] = color;
      --run;
      --remainingPixels;
    }
  }

  file.close();

  if (remainingPixels != 0) {
    return false;  // 不完整帧，不推屏
  }

  // 推送策略：首帧全量，后续帧只推脏矩形
  const uint16_t* const prevBuf = pet2BufToggle ? pet2BufA : pet2BufB;

  if (forceFull) {
    display_.drawRgb565Bitmap(originX, originY, curBuf, width, height);
  } else {
    // 计算脏矩形（当前帧与上一帧之间的差异包围盒）
    int16_t dL = static_cast<int16_t>(width);
    int16_t dR = -1;
    int16_t dT = static_cast<int16_t>(height);
    int16_t dB = -1;
    const uint32_t total = static_cast<uint32_t>(width) * height;
    for (uint32_t i = 0; i < total; ++i) {
      if (curBuf[i] != prevBuf[i]) {
        const int16_t x = static_cast<int16_t>(i % width);
        const int16_t y = static_cast<int16_t>(i / width);
        if (x < dL) dL = x;
        if (x > dR) dR = x;
        if (y < dT) dT = y;
        if (y > dB) dB = y;
      }
    }

    if (dL <= dR) {
      const uint16_t dW = static_cast<uint16_t>(dR - dL + 1);
      const uint16_t dH = static_cast<uint16_t>(dB - dT + 1);
      const uint32_t dirtyArea = static_cast<uint32_t>(dW) * dH;
      const uint32_t fullArea = static_cast<uint32_t>(width) * height;

      if (dirtyArea < fullArea / 4) {
        // 脏区域小：逐行推脏矩形
        uint16_t row[kPetAreaSize];
        for (int16_t y = dT; y <= dB; ++y) {
          memcpy(row, &curBuf[static_cast<uint32_t>(y) * width + dL], dW * sizeof(uint16_t));
          display_.drawRgb565Bitmap(originX + dL, originY + y, row, dW, 1);
        }
      } else {
        // 脏区域大：全帧一次性推送
        display_.drawRgb565Bitmap(originX, originY, curBuf, width, height);
      }
    }
    // 无变化时跳过：dL > dR，不推任何像素
  }

  // 翻转双缓存
  pet2BufToggle = !pet2BufToggle;
  pet2WasActive = true;
  return true;
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

void ScreenRenderer::drawRadialSector(float centerDeg, uint16_t color) {
  const float startDeg = centerDeg - 44.0f;
  const float endDeg = centerDeg + 44.0f;
  for (float deg = startDeg; deg <= endDeg; deg += 2.0f) {
    const float radians = deg * DEG_TO_RAD;
    const int16_t x1 = kScreenCenter + static_cast<int16_t>(roundf(cosf(radians) * 34.0f));
    const int16_t y1 = kScreenCenter - static_cast<int16_t>(roundf(sinf(radians) * 34.0f));
    const int16_t x2 = kScreenCenter + static_cast<int16_t>(roundf(cosf(radians) * 116.0f));
    const int16_t y2 = kScreenCenter - static_cast<int16_t>(roundf(sinf(radians) * 116.0f));
    display_.drawLine(x1, y1, x2, y2, color);
  }
}

void ScreenRenderer::drawRadialCursor(float cursorX, float cursorY, uint16_t color) {
  const int16_t x = kScreenCenter + static_cast<int16_t>(roundf(cursorX));
  const int16_t y = kScreenCenter + static_cast<int16_t>(roundf(cursorY));
  display_.drawLine(kScreenCenter, kScreenCenter, x, y, color);
  display_.fillCircle(x, y, 5, color);
  display_.drawCircle(x, y, 8, kBlack);
  display_.drawCircle(x, y, 9, color);
}

const char *ScreenRenderer::radialItemLabel(RadialMenuItem item) const {
  switch (item) {
    case RadialMenuItem::Info:
      return "info";
    case RadialMenuItem::PreviousPet:
      return "prev pet";
    case RadialMenuItem::NextPet:
      return "next pet";
    case RadialMenuItem::Cancel:
    default:
      return "cancel";
  }
}

uint16_t ScreenRenderer::radialItemColor(RadialMenuItem item, bool selected) const {
  if (!selected) {
    switch (item) {
      case RadialMenuItem::Info:
        return 0x18C7;
      case RadialMenuItem::PreviousPet:
        return 0x120D;
      case RadialMenuItem::NextPet:
        return 0x1B46;
      case RadialMenuItem::Cancel:
      default:
        return 0x28E3;
    }
  }

  switch (item) {
    case RadialMenuItem::Info:
      return kBlue;
    case RadialMenuItem::PreviousPet:
      return 0x6D7F;
    case RadialMenuItem::NextPet:
      return kGreen;
    case RadialMenuItem::Cancel:
    default:
      return kAmber;
  }
}
