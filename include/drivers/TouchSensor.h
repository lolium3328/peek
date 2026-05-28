#pragma once

#include <stdint.h>

#include "config/DeviceConfig.h"
#include "events/TouchEvent.h"

class TouchSensor {
public:
  void begin(const DeviceConfig &config);
  TouchEvent update(uint32_t now);

  bool isPressed() const;
  int lastValue() const;

private:
  DeviceConfig config_ = defaultDeviceConfig();
  bool pressInProgress_ = false;
  bool longPressTriggered_ = false;
  bool extraLongPressTriggered_ = false;
  uint32_t lastSampleMs_ = 0;
  uint32_t pressStartMs_ = 0;
  int lastValue_ = 0;
};
