#pragma once

#include <stdint.h>

struct DeviceConfig {
  int touchIdleThreshold = 3980;
  int touchPressThreshold = 3300;
  uint32_t touchSampleIntervalMs = 50;
  uint32_t longPressMs = 2000;
  uint32_t sleepTimeoutMs = 120000;
  const char *longPressText = "good touch!";
};

inline DeviceConfig defaultDeviceConfig() {
  return DeviceConfig{};
}
