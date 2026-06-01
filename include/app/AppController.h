#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app/PetState.h"
#include "config/DeviceConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/ImuDriver.h"
#include "drivers/TouchSensor.h"
#include "events/TouchEvent.h"
#include "services/BackendClient.h"
#include "services/ConfigStore.h"
#include "services/NetworkService.h"
#include "services/ProvisioningService.h"
#include "storage/AssetStore.h"
#include "storage/FileSystem.h"
#include "storage/LayoutStore.h"
#include "ui/ScreenRenderer.h"

class AppController {
public:
  AppController();

  void begin();
  void loop();

private:
  enum class AppMode {
    Normal,
    StatusView,
    Sleeping,
    ImuLocked,
    RadialMenu,
    RadialCalibration
  };

  struct BatteryStatus {
    bool available = false;
    uint8_t percent = 92;
    bool low = false;
  };

  void resetPet();
  void renderHomeText(const char *hintText);
  void renderHomeFrame();
  void renderStatus();
  void renderRadialMenuFrame();
  void renderRadialCalibrationFrame();
  void fillHomeModel(HomeScreenModel &model, const char *hintText);
  const char *currentHomeHint() const;
  bool isOffline(uint32_t now) const;
  const BatteryStatus &localBattery() const;
  bool isLowBattery() const;
  void recordActivity(uint32_t now);
  bool readMotionDelta(int32_t &deltaX, int32_t &deltaY, int32_t &deltaZ);
  bool detectWakeMotion(uint32_t now);
  void enterSleep(uint32_t now);
  void wakeFromSleep(uint32_t now);
  void enterImuLocked(uint32_t now);
  void exitImuLocked(uint32_t now);
  bool updateShortPressSequence(uint32_t now);
  void resetShortPressSequence();
  void updateCubeThrow(uint32_t now);
  void updateCubeScale(uint32_t now);
  void startCubeRecovery(uint32_t now);
  void updateCubeRecovery(uint32_t now, float dt, float frameScale);
  void detectHeldPetGesture(uint32_t now);
  void detectCubeThrow(uint32_t now);
  void resetMotionBaseline();
  void stopCubeThrow();
  void startCubeThrow(uint32_t now, int32_t accelDeltaX, int32_t accelDeltaY, int32_t accelDeltaZ);
  void applyCubeMotion(HomeScreenModel &model, const ImuPose &pose) const;
  void loadScreenCalibration();
  bool saveScreenCalibration();
  void centerCube();
  void loadRadialCalibration();
  bool saveRadialCalibration(bool swapAxes, bool flipX, bool flipY, float angleOffsetDeg);
  bool radialCursor(float &cursorX, float &cursorY, float &rawAngleDeg, float &mappedAngleDeg) const;
  bool radialRawVector(float &rawX, float &rawY) const;
  void transformRadialVector(float rawX, float rawY, float &mappedX, float &mappedY) const;
  RadialMenuItem radialItemForAngle(float angleDeg) const;
  void enterRadialMenu(uint32_t now);
  void updateRadialMenu(uint32_t now);
  void completeRadialMenu(uint32_t now);
  void enterRadialCalibration(uint32_t now);
  void updateRadialCalibration(uint32_t now);
  void confirmRadialCalibrationSample(uint32_t now);
  bool finishRadialCalibration();
  void resetRadialSpinTracking();
  void updateRadialSpinTracking(float rawAngleDeg);
  void triggerRadialItem(RadialMenuItem item, uint32_t now);
  void handleCompletedClick();
  void handleLongPress();
  void handleExtraLongPress();

  DeviceConfig config_ = defaultDeviceConfig();
  ConfigStore configStore_;
  FileSystem fileSystem_;
  LayoutStore layoutStore_;
  AssetStore assetStore_;
  NetworkService network_;
  BackendClient backend_;
  ProvisioningService provisioning_;
  DisplayDriver display_;
  ScreenRenderer screen_;
  TouchSensor touch_;
  ImuDriver imu_;
  PetState pet_;
  AppMode mode_ = AppMode::Normal;
  BatteryStatus localBattery_;
  uint32_t lastActivityMs_ = 0;
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
  uint32_t lastPetGestureMs_ = 0;
  uint32_t lastCubeThrowLogMs_ = 0;
  uint32_t lastCubeScaleUpdateMs_ = 0;
  uint32_t lastShortPressMs_ = 0;
  uint32_t cubeScaleRecoverStartMs_ = 0;
  float cubeOffsetX_ = 0.0f;
  float cubeOffsetY_ = 0.0f;
  float cubeRenderScale_ = 32.0f;
  float cubeRecoverScaleStart_ = 32.0f;
  float cubeVelocityX_ = 0.0f;
  float cubeVelocityY_ = 0.0f;
  float cubeSpinRollDeg_ = 0.0f;
  float cubeSpinPitchDeg_ = 0.0f;
  float cubeSpinYawDeg_ = 0.0f;
  float cubeSpinRollVelocity_ = 0.0f;
  float cubeSpinPitchVelocity_ = 0.0f;
  float cubeSpinYawVelocity_ = 0.0f;
  bool holdGestureConsumed_ = false;
  bool radialCalibrated_ = false;
  bool radialSpinTracking_ = false;
  bool radialCalibrationFailed_ = false;
  uint8_t shortPressCount_ = 0;
  uint8_t calibrationStep_ = 0;
  RadialMenuItem radialSelectedItem_ = RadialMenuItem::Cancel;
  RadialMenuItem calibrationTargetItem_ = RadialMenuItem::Info;
  float radialAngleOffsetDeg_ = 0.0f;
  float radialSpinPreviousAngleDeg_ = 0.0f;
  float radialSpinAccumulatedDeg_ = 0.0f;
  bool radialSwapAxes_ = false;
  bool radialFlipX_ = false;
  bool radialFlipY_ = false;
  float calibrationRawX_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float calibrationRawY_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};
