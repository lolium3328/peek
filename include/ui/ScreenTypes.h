#pragma once

#include <stdint.h>

struct HomeScreenModel {
  const char *primaryText = "";
  const char *hintText = "";
  const char *localWeather = "--";
  const char *peerWeather = "--";
  const char *localLabel = "A";
  const char *peerLabel = "B";
  uint8_t localBatteryPercent = 92;
  uint8_t peerBatteryPercent = 79;
  bool wifiConnected = false;
  bool backendConnected = false;
  bool poseAlert = false;
  bool cubeVisible = false;
  float cubeRollDeg = 0.0f;
  float cubePitchDeg = 0.0f;
  float cubeYawDeg = 0.0f;
  float cubeOffsetX = 0.0f;
  float cubeOffsetY = 0.0f;
  float cubeScale = 32.0f;
  bool petAnimationVisible = false;
  const char *petAnimationPath = "";
};

struct BootScreenModel {
  const char *title = "Peek";
  const char *message = "booting";
};

struct StatusScreenModel {
  bool buttonPressed = false;
  int8_t wifiRssi = 0;
  uint8_t localBatteryPercent = 92;
  uint8_t peerBatteryPercent = 79;
  bool backendConnected = false;
  bool imuReady = false;
  uint8_t imuAddress = 0;
  int16_t imuAccelZ = 0;
  float imuRollDeg = 0.0f;
  float imuPitchDeg = 0.0f;
};

enum class RadialMenuItem : uint8_t {
  Cancel = 0,
  Info = 1,
  PreviousPet = 2,
  NextPet = 3
};

struct RadialMenuModel {
  RadialMenuItem selectedItem = RadialMenuItem::Cancel;
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  bool imuReady = false;
  bool calibratingHint = false;
};

struct RadialCalibrationModel {
  RadialMenuItem targetItem = RadialMenuItem::Info;
  uint8_t completedCount = 0;
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  float holdProgress = 0.0f;
  bool failed = false;
};
