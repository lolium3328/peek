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
};

struct BootScreenModel {
  const char *title = "Peek";
  const char *message = "booting";
};

struct StatusScreenModel {
  uint16_t touchAnalog = 0;
  int8_t wifiRssi = 0;
  uint8_t localBatteryPercent = 92;
  uint8_t peerBatteryPercent = 79;
  bool backendConnected = false;
  bool imuReady = false;
  uint8_t imuAddress = 0;
  int16_t imuAccelZ = 0;
};
