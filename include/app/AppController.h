#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app/PetState.h"
#include "config/DeviceConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/ImuDriver.h"
#include "drivers/TouchSensor.h"
#include "events/TouchEvent.h"
#include "ui/ScreenRenderer.h"

class AppController {
public:
  AppController();

  void begin();
  void loop();

private:
  void showText(size_t index);
  void renderHomeText(const char *text, const char *hintText);
  void renderStatus();
  void handleCompletedClick();
  void handleLongPress();

  DeviceConfig config_ = defaultDeviceConfig();
  DisplayDriver display_;
  ScreenRenderer screen_;
  TouchSensor touch_;
  ImuDriver imu_;
  PetState pet_;
  uint32_t lastTouchMs_ = 0;
};
