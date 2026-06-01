#include "ui/ScreenRenderer.h"

#include <Arduino.h>
#include <LittleFS.h>
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

  if (model.petAnimationVisible && drawPetAnimation(model)) {
    // Drawn from cached asset package.
  } else if (model.cubeVisible) {
    drawPetCube(model);
  } else {
    display_.drawTextCentered(model.primaryText, 122, DisplayTextStyle::Primary, kWhite);
  }
  drawBottomHint(model.hintText);
}

void ScreenRenderer::renderHomeFrame(const HomeScreenModel &model) {
  if (model.petAnimationVisible) {
    if (drawPetAnimation(model)) {
      return;
    }
    clearPetArea();  // 动画失败，清空区域给后续路径
  }
  pet2WasActive = false;  // pet2 本帧未活跃，下一帧若恢复需全量刷新

  if (model.cubeVisible) {
    drawPetCubeBuffered(model);
  } else {
    clearPetArea();
    if (model.poseAlert) {
      display_.drawCircle(kScreenCenter, kScreenCenter, 72, kAmber);
    }
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
