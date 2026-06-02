#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app/PetState.h"
#include "config/DeviceConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/ImuDriver.h"
#include "drivers/TouchSensor.h"
#include "events/TouchEvent.h"
#include "physics/CubePhysics.h"
#include "services/BackendClient.h"
#include "services/ConfigStore.h"
#include "services/NetworkService.h"
#include "services/ProvisioningService.h"
#include "storage/AssetStore.h"
#include "storage/FileSystem.h"
#include "ui/RadialMenuController.h"
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

  bool detectWakeMotion(uint32_t now);
  void enterSleep(uint32_t now);
  void wakeFromSleep(uint32_t now);
  void enterImuLocked(uint32_t now);
  void exitImuLocked(uint32_t now);
  bool updateShortPressSequence(uint32_t now);
  void resetShortPressSequence();
  void handleCompletedClick();
  void handleLongPress();
  void handleExtraLongPress();

  void detectHeldPetGesture(uint32_t now);

  void loadScreenCalibration();
  bool saveScreenCalibration();
  void centerCube();

  void enterRadialMenu(uint32_t now);
  void updateRadialMenu(uint32_t now);
  void completeRadialMenu(uint32_t now);
  void enterRadialCalibration(uint32_t now);
  void updateRadialCalibration(uint32_t now);
  void confirmRadialCalibrationSample(uint32_t now);
  void triggerRadialItem(RadialMenuItem item, uint32_t now);

  DeviceConfig config_ = defaultDeviceConfig();
  ConfigStore configStore_;
  FileSystem fileSystem_;
  AssetStore assetStore_;
  NetworkService network_;
  BackendClient backend_;
  ProvisioningService provisioning_;
  DisplayDriver display_;
  ScreenRenderer screen_;
  TouchSensor touch_;
  ImuDriver imu_;
  PetState pet_;
  CubePhysics cubePhysics_;
  RadialMenuController radialMenu_;
  AppMode mode_ = AppMode::Normal;
  BatteryStatus localBattery_;
  uint32_t lastActivityMs_ = 0;
  uint32_t lastHomeRenderMs_ = 0;
  float cubeRollZeroDeg_ = 0.0f;
  float cubePitchZeroDeg_ = 0.0f;
  float cubeYawZeroDeg_ = 0.0f;
  uint32_t lastShortPressMs_ = 0;
  uint32_t lastPetGestureMs_ = 0;
  uint8_t shortPressCount_ = 0;
  bool holdGestureConsumed_ = false;
  bool radialAwaitingInitialRelease_ = false;
  bool wakeBaselineSet_ = false;
  int16_t wakePrevX_ = 0, wakePrevY_ = 0, wakePrevZ_ = 0;
};
