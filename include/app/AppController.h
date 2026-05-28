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
  void renderHomeFrame();
  void renderStatus();
  void updateCubeThrow(uint32_t now);
  void updateCubeScale(uint32_t now);
  void detectCubeThrow(uint32_t now);
  void startCubeThrow(uint32_t now, int32_t accelDeltaX, int32_t accelDeltaY, int32_t accelDeltaZ);
  void applyCubeMotion(HomeScreenModel &model, const ImuPose &pose) const;
  void centerCube();
  void handleCompletedClick();
  void handleLongPress();

  DeviceConfig config_ = defaultDeviceConfig();
  DisplayDriver display_;
  ScreenRenderer screen_;
  TouchSensor touch_;
  ImuDriver imu_;
  PetState pet_;
  uint32_t lastTouchMs_ = 0;
  uint32_t lastHomeRenderMs_ = 0;
  float cubeRollZeroDeg_ = 0.0f;
  float cubePitchZeroDeg_ = 0.0f;
  float cubeYawZeroDeg_ = 0.0f;
  bool cubeThrown_ = false;
  bool cubeScaleRecovering_ = false;
  bool hasMotionBaseline_ = false;
  int16_t previousAccelX_ = 0;
  int16_t previousAccelY_ = 0;
  int16_t previousAccelZ_ = 0;
  uint32_t lastCubeThrowUpdateMs_ = 0;
  uint32_t lastCubeThrowStartMs_ = 0;
  uint32_t lastCubeThrowLogMs_ = 0;
  uint32_t lastCubeScaleUpdateMs_ = 0;
  float cubeOffsetX_ = 0.0f;
  float cubeOffsetY_ = 0.0f;
  float cubeRenderScale_ = 32.0f;
  float cubeVelocityX_ = 0.0f;
  float cubeVelocityY_ = 0.0f;
  float cubeSpinRollDeg_ = 0.0f;
  float cubeSpinPitchDeg_ = 0.0f;
  float cubeSpinYawDeg_ = 0.0f;
  float cubeSpinRollVelocity_ = 0.0f;
  float cubeSpinPitchVelocity_ = 0.0f;
  float cubeSpinYawVelocity_ = 0.0f;
  bool statusVisible_ = false;
};
