#pragma once

#include <WString.h>
#include <stdint.h>

struct DeviceConfig {
  String deviceId = "peek-dev";
  String deviceToken = "";
  String wifiSsid = "";
  String wifiPassword = "";
  String backendUrl = "";
  uint32_t backendPollIntervalMs = 5000;
  uint32_t touchSampleIntervalMs = 50;
  uint32_t longPressMs = 2000;
  uint32_t extraLongPressMs = 5000;
  uint32_t sleepTimeoutMs = 120000;
  String longPressText = "good touch!";
};

inline DeviceConfig defaultDeviceConfig() {
  return DeviceConfig{};
}
