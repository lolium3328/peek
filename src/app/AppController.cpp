#include "app/AppController.h"

#include <Arduino.h>
#include <stdlib.h>

#include "Pins.h"

namespace {
constexpr uint32_t kHomeFrameIntervalMs = 75;
constexpr uint32_t kShortPressSequenceGapMs = 1500;
constexpr uint8_t kImuLockShortPressCount = 4;
constexpr uint32_t kWakeMotionThreshold = 6000;
}

AppController::AppController() : screen_(display_) {}

void AppController::begin() {
  Serial.begin(115200);

  configStore_.begin();
  config_ = configStore_.load();
  fileSystem_.begin();
  assetStore_.begin(fileSystem_);
  radialMenu_.loadCalibration();

  if (!display_.begin()) {
    Serial.println("GC9A01 init failed");
    while (true) delay(1000);
  }

  pinMode(Pins::BUTTON, INPUT_PULLUP);
  const bool forceProvisioning = digitalRead(Pins::BUTTON) == LOW;
  if (forceProvisioning) {
    config_.wifiSsid = "";
    Serial.println("Provisioning forced by boot button");
  }

  touch_.begin(config_);
  const bool imuReady = imu_.begin();
  provisioning_.begin(config_, configStore_);
  if (!provisioning_.isActive()) {
    network_.begin(config_);
    backend_.begin(config_, assetStore_);
  }
  loadScreenCalibration();

  BootScreenModel bootModel;
  bootModel.title = "Peek";
  bootModel.message = provisioning_.isActive() ? "setup ap" : (imuReady ? "imu ok" : "imu missing");
  screen_.renderBoot(bootModel);
  delay(500);

  Serial.println("Button input start");
  resetPet();
  lastActivityMs_ = millis();
}

void AppController::loop() {
  const uint32_t now = millis();
  provisioning_.loop(now);
  const bool provisioningActive = provisioning_.isActive();
  if (!provisioningActive) network_.loop(now);
  imu_.update(now);

  if (provisioningActive) {
    mode_ = AppMode::Normal;
    display_.setSleep(false);
    holdGestureConsumed_ = false;
    recordActivity(now);
    if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) renderHomeFrame();
    return;
  }

  backend_.loop(now, network_, imu_.pose(), imu_.isReady());

  const TouchEvent event = touch_.update(now);
  const bool releasedNow = event.sampled && !event.pressed;
  if (event.pressed) recordActivity(now);

  if (mode_ == AppMode::Sleeping) {
    if ((event.sampled && event.pressed) || detectWakeMotion(now)) wakeFromSleep(now);
    return;
  }

  if (mode_ == AppMode::RadialMenu) {
    updateRadialMenu(now);
    if (releasedNow && radialAwaitingInitialRelease_) {
      radialAwaitingInitialRelease_ = false;
      holdGestureConsumed_ = false;
      return;
    }
    if (event.type == TouchEventType::ShortPress) {
      enterRadialCalibration(now);
      holdGestureConsumed_ = false;
      return;
    }
    if (event.type == TouchEventType::LongPress || event.type == TouchEventType::ExtraLongPress) {
      completeRadialMenu(now);
      holdGestureConsumed_ = true;
      return;
    }
    if (releasedNow) holdGestureConsumed_ = false;
    return;
  }

  if (mode_ == AppMode::RadialCalibration) {
    if (event.type == TouchEventType::ShortPress) {
      confirmRadialCalibrationSample(now);
      holdGestureConsumed_ = false;
      return;
    }
    if (event.type == TouchEventType::LongPress || event.type == TouchEventType::ExtraLongPress) {
      mode_ = AppMode::Normal;
      holdGestureConsumed_ = true;
      renderHomeText("cal cancel");
      return;
    }
    updateRadialCalibration(now);
    return;
  }

  if (mode_ != AppMode::ImuLocked) {
    cubePhysics_.update(now);
    if (cubePhysics_.detectThrow(now, imu_.lastSample(),
                                  pet_.isCubePet(), touch_.isPressed())) {
      recordActivity(now);
    }
  }

  if (event.sampled && !holdGestureConsumed_) {
    if (event.type == TouchEventType::ExtraLongPress) {
      recordActivity(now);
      resetShortPressSequence();
      handleExtraLongPress();
    } else if (event.type == TouchEventType::LongPress) {
      recordActivity(now);
      resetShortPressSequence();
      handleLongPress();
    } else if (event.type == TouchEventType::ShortPress) {
      recordActivity(now);
      handleCompletedClick();
    }
  }

  if (releasedNow) {
    holdGestureConsumed_ = false;
  }

  if (mode_ == AppMode::StatusView) {
    if (!provisioningActive && !touch_.isPressed()
        && config_.sleepTimeoutMs > 0
        && now - lastActivityMs_ >= config_.sleepTimeoutMs) {
      enterSleep(now);
    }
    return;
  }

  if (mode_ == AppMode::ImuLocked) return;

  if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
    detectHeldPetGesture(now);
    renderHomeFrame();
  }

  if (!provisioningActive && !touch_.isPressed()
      && config_.sleepTimeoutMs > 0
      && now - lastActivityMs_ >= config_.sleepTimeoutMs) {
    enterSleep(now);
  }
}

void AppController::resetPet() {
  pet_.reset();
  mode_ = AppMode::Normal;
  display_.setSleep(false);
  renderHomeText(pet_.isSleeping() ? "sleeping" : "hold + shake");
}

void AppController::renderHomeText(const char *hintText) {
  HomeScreenModel model;
  fillHomeModel(model, hintText);
  screen_.renderHomeFrame(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderHomeFrame() {
  HomeScreenModel model;
  fillHomeModel(model, currentHomeHint());
  screen_.renderHomeFrame(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderStatus() {
  const ImuPose &pose = imu_.pose();
  StatusScreenModel model;
  model.buttonPressed = touch_.isPressed();
  model.wifiRssi = static_cast<int8_t>(network_.rssi());
  model.localBatteryPercent = localBattery().percent;
  model.peerBatteryPercent = 79;
  model.backendConnected = !provisioning_.isActive() && backend_.isConnected(millis());
  model.imuReady = imu_.isReady();
  model.imuAddress = imu_.address();
  model.imuAccelZ = imu_.lastSample().accelZ;
  model.imuRollDeg = pose.rollDeg;
  model.imuPitchDeg = pose.pitchDeg;
  screen_.renderStatus(model);
}

void AppController::fillHomeModel(HomeScreenModel &model, const char *hintText) {
  const uint32_t now = millis();
  const ImuPose &pose = imu_.pose();
  model.primaryText = provisioning_.isActive()
                          ? "setup"
                          : (mode_ == AppMode::ImuLocked
                                 ? "imu?"
                                 : (pet_.isCubePet()
                                        ? (pose.valid ? "" : "imu?")
                                        : pet_.currentPetText()));
  model.hintText = provisioning_.isActive()
                       ? provisioning_.apSsid().c_str()
                       : ((mode_ == AppMode::Normal)
                              ? (isLowBattery() ? "low battery" : (isOffline(now) ? "offline" : hintText))
                              : hintText);
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localBatteryPercent = localBattery().percent;
  model.peerBatteryPercent = 79;
  model.wifiConnected = network_.isConnected();
  model.backendConnected = !provisioning_.isActive() && backend_.isConnected(now);
  model.poseAlert = isLowBattery();
  if (!provisioning_.isActive() && mode_ != AppMode::ImuLocked) {
    cubePhysics_.applyToModel(model, pose, cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  }
  if (mode_ != AppMode::ImuLocked && pet_.isPet2() && assetStore_.hasPet2Animation()) {
    model.petAnimationVisible = true;
    model.petAnimationPath = assetStore_.pet2AnimationPath().c_str();
  }
}

const char *AppController::currentHomeHint() const {
  if (mode_ == AppMode::ImuLocked) return "imu locked";
  if (mode_ == AppMode::RadialMenu) return "menu";
  if (mode_ == AppMode::RadialCalibration) return "cal";
  if (touch_.isPressed()) return "gesture";
  return pet_.isSleeping() ? "sleeping" : "hold + shake";
}

bool AppController::isOffline(uint32_t now) const {
  if (provisioning_.isActive()) return false;
  const bool wifiOffline = network_.isEnabled() && !network_.isConnected();
  const bool backendOffline = backend_.isEnabled() && !backend_.isConnected(now);
  return wifiOffline || backendOffline;
}

const AppController::BatteryStatus &AppController::localBattery() const {
  return localBattery_;
}

bool AppController::isLowBattery() const {
  return localBattery_.available && localBattery_.low;
}

void AppController::recordActivity(uint32_t now) {
  lastActivityMs_ = now;
}

bool AppController::detectWakeMotion(uint32_t now) {
  const ImuSample &sample = imu_.lastSample();
  if (!sample.valid) return false;

  if (!wakeBaselineSet_) {
    wakePrevX_ = sample.accelX;
    wakePrevY_ = sample.accelY;
    wakePrevZ_ = sample.accelZ;
    wakeBaselineSet_ = true;
    return false;
  }

  const int32_t dx = static_cast<int32_t>(sample.accelX) - wakePrevX_;
  const int32_t dy = static_cast<int32_t>(sample.accelY) - wakePrevY_;
  const int32_t dz = static_cast<int32_t>(sample.accelZ) - wakePrevZ_;
  wakePrevX_ = sample.accelX;
  wakePrevY_ = sample.accelY;
  wakePrevZ_ = sample.accelZ;

  const int32_t motion = labs(dx) + labs(dy) + labs(dz);
  if (motion < static_cast<int32_t>(kWakeMotionThreshold)) return false;

  Serial.print("Wake motion "); Serial.println(motion);
  return true;
}

void AppController::enterSleep(uint32_t now) {
  mode_ = AppMode::Sleeping;
  holdGestureConsumed_ = false;
  resetShortPressSequence();
  cubePhysics_.stopThrow();
  wakeBaselineSet_ = false;
  recordActivity(now);
  renderHomeText("sleeping");
  display_.setSleep(true);
  Serial.print("Sleep timeout at "); Serial.println(now);
}

void AppController::wakeFromSleep(uint32_t now) {
  display_.setSleep(false);
  mode_ = AppMode::Normal;
  recordActivity(now);
  renderHomeText(currentHomeHint());
  Serial.println("Wake -> normal");
}

void AppController::enterImuLocked(uint32_t now) {
  mode_ = AppMode::ImuLocked;
  holdGestureConsumed_ = false;
  resetShortPressSequence();
  cubePhysics_.stopThrow();
  recordActivity(now);
  renderHomeText("imu locked");
  Serial.println("IMU input locked");
}

void AppController::exitImuLocked(uint32_t now) {
  mode_ = AppMode::Normal;
  resetShortPressSequence();
  recordActivity(now);
  renderHomeText("imu restored");
  Serial.println("IMU input restored");
}

bool AppController::updateShortPressSequence(uint32_t now) {
  if (lastShortPressMs_ == 0 || now - lastShortPressMs_ > kShortPressSequenceGapMs) {
    shortPressCount_ = 0;
  }
  lastShortPressMs_ = now;
  ++shortPressCount_;
  Serial.print("Short press sequence "); Serial.println(shortPressCount_);
  if (shortPressCount_ >= kImuLockShortPressCount) {
    enterImuLocked(now);
    return true;
  }
  return false;
}

void AppController::resetShortPressSequence() {
  lastShortPressMs_ = 0;
  shortPressCount_ = 0;
}

void AppController::detectHeldPetGesture(uint32_t now) {
  if (mode_ != AppMode::Normal || holdGestureConsumed_ || !touch_.isPressed()) return;

  if (!cubePhysics_.detectHeldShake(now, imu_.lastSample(), touch_.isPressed())) return;

  pet_.advancePet();
  holdGestureConsumed_ = true;
  recordActivity(now);
  cubePhysics_.stopThrow();
  renderHomeText("switched");
  Serial.print("Hold shake -> pet "); Serial.println(pet_.currentPetText());
}

void AppController::loadScreenCalibration() {
  cubePhysics_.loadZeroCalibration(cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  Serial.print("Screen calibration loaded ");
  Serial.print(cubeRollZeroDeg_); Serial.print(",");
  Serial.print(cubePitchZeroDeg_); Serial.print(",");
  Serial.println(cubeYawZeroDeg_);
}

bool AppController::saveScreenCalibration() {
  const bool saved = cubePhysics_.saveZeroCalibration(
      imu_.pose(), cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  if (saved) {
    Serial.print("Screen calibration saved ");
    Serial.print(cubeRollZeroDeg_); Serial.print(",");
    Serial.print(cubePitchZeroDeg_); Serial.print(",");
    Serial.println(cubeYawZeroDeg_);
  }
  return saved;
}

void AppController::centerCube() {
  const ImuPose &pose = imu_.pose();
  if (!pose.valid) {
    Serial.println("Cube center skipped: imu pose invalid");
    return;
  }
  cubeRollZeroDeg_ = pose.rollDeg;
  cubePitchZeroDeg_ = pose.pitchDeg;
  cubeYawZeroDeg_ = pose.yawDeg;
  cubePhysics_.centerPose(cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  Serial.print("Cube centered at ");
  Serial.print(cubeRollZeroDeg_); Serial.print(",");
  Serial.print(cubePitchZeroDeg_); Serial.print(",");
  Serial.println(cubeYawZeroDeg_);
}

void AppController::renderRadialMenuFrame() {
  RadialCursorResult cr = radialMenu_.computeCursor(
      imu_.pose(), cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  RadialMenuModel model;
  model.selectedItem = radialMenu_.itemForAngle(cr.mappedAngleDeg);
  model.cursorX = cr.cursorX;
  model.cursorY = cr.cursorY;
  model.imuReady = cr.valid;
  model.calibratingHint = fabsf(radialMenu_.spinAccumulatedDeg()) > 180.0f;
  screen_.renderRadialMenu(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderRadialCalibrationFrame() {
  RadialCursorResult cr = radialMenu_.computeCursor(
      imu_.pose(), cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  RadialCalibrationModel model;
  model.targetItem = radialMenu_.calibrationTarget();
  model.completedCount = radialMenu_.calibrationStep();
  model.cursorX = cr.cursorX;
  model.cursorY = cr.cursorY;
  model.failed = radialMenu_.calibrationFailed();
  screen_.renderRadialCalibration(model);
  lastHomeRenderMs_ = millis();
}

void AppController::enterRadialMenu(uint32_t now) {
  mode_ = AppMode::RadialMenu;
  radialMenu_.enterMenu(now);
  radialAwaitingInitialRelease_ = touch_.isPressed();
  holdGestureConsumed_ = false;
  resetShortPressSequence();
  cubePhysics_.stopThrow();
  recordActivity(now);
  renderRadialMenuFrame();
  Serial.println("Long press -> radial menu");
}

void AppController::updateRadialMenu(uint32_t now) {
  RadialCursorResult cr = radialMenu_.computeCursor(
      imu_.pose(), cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  if (cr.valid) {
    radialAwaitingInitialRelease_ = false;
  }
  if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
    renderRadialMenuFrame();
  }
}

void AppController::completeRadialMenu(uint32_t now) {
  RadialCursorResult cr = radialMenu_.computeCursor(
      imu_.pose(), cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  const RadialMenuItem selected = radialMenu_.itemForAngle(cr.mappedAngleDeg);
  radialMenu_.completeMenu();
  mode_ = AppMode::Normal;
  recordActivity(now);
  triggerRadialItem(selected, now);
}

void AppController::enterRadialCalibration(uint32_t now) {
  mode_ = AppMode::RadialCalibration;
  radialMenu_.startCalibration(now);
  holdGestureConsumed_ = false;
  recordActivity(now);
  renderRadialCalibrationFrame();
  Serial.println("Radial calibration started");
}

void AppController::updateRadialCalibration(uint32_t now) {
  radialMenu_.updateCalibration(now);
  if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
    renderRadialCalibrationFrame();
  }
}

void AppController::confirmRadialCalibrationSample(uint32_t now) {
  radialMenu_.confirmSample(now, imu_.pose(), cubeRollZeroDeg_, cubePitchZeroDeg_, cubeYawZeroDeg_);
  recordActivity(now);

  if (!radialMenu_.isCalibrating()) {
    mode_ = AppMode::Normal;
    renderHomeText(radialMenu_.calibrationFailed() ? "menu cal fail" : "menu cal ok");
    return;
  }

  renderRadialCalibrationFrame();
  Serial.print("Radial calibration sample "); Serial.println(radialMenu_.calibrationStep());
}

void AppController::triggerRadialItem(RadialMenuItem item, uint32_t now) {
  resetShortPressSequence();
  switch (item) {
    case RadialMenuItem::Info:
      mode_ = AppMode::StatusView;
      pet_.wakeForLongPress();
      renderStatus();
      Serial.println("Radial menu -> status");
      return;
    case RadialMenuItem::PreviousPet:
      pet_.previousPet();
      cubePhysics_.stopThrow();
      renderHomeText("previous");
      Serial.println("Radial menu -> previous pet");
      return;
    case RadialMenuItem::NextPet:
      pet_.advancePet();
      cubePhysics_.stopThrow();
      renderHomeText("next");
      Serial.println("Radial menu -> next pet");
      return;
    case RadialMenuItem::Cancel:
    default:
      renderHomeText(currentHomeHint());
      Serial.println("Radial menu -> cancel");
      return;
  }
}

void AppController::handleCompletedClick() {
  const uint32_t now = millis();
  if (mode_ == AppMode::ImuLocked) {
    exitImuLocked(now);
    return;
  }
  if (mode_ == AppMode::StatusView) {
    mode_ = AppMode::Normal;
    resetShortPressSequence();
    renderHomeText(currentHomeHint());
    return;
  }
  if (updateShortPressSequence(now)) return;
  centerCube();
  renderHomeText("centered");
}

void AppController::handleLongPress() {
  if (mode_ == AppMode::ImuLocked) return;
  enterRadialMenu(millis());
}

void AppController::handleExtraLongPress() {
  if (mode_ == AppMode::ImuLocked) return;
  resetShortPressSequence();
  renderHomeText("hold menu");
  Serial.println("Extra long press ignored; use radial calibration");
}
