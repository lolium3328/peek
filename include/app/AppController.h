#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app/PetState.h"
#include "config/DeviceConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/TouchSensor.h"
#include "events/TouchEvent.h"

class AppController {
public:
  void begin();
  void loop();

private:
  void showText(size_t index);
  void handleCompletedClick();
  void handleLongPress();

  DeviceConfig config_ = defaultDeviceConfig();
  DisplayDriver display_;
  TouchSensor touch_;
  PetState pet_;
  uint32_t lastTouchMs_ = 0;
};
