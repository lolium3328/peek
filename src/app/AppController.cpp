#include "app/AppController.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <stdlib.h>

#include "Pins.h"

namespace {
constexpr uint32_t kHomeFrameIntervalMs = 75;
constexpr uint32_t kThrowCooldownMs = 900;
constexpr uint32_t kThrowMinSettleMs = 1400;
constexpr uint32_t kThrowLogIntervalMs = 200;
constexpr uint32_t kShortPressSequenceGapMs = 1500;
constexpr uint8_t kImuLockShortPressCount = 4;
constexpr float kRadialCursorGain = 3.0f;
constexpr float kRadialCursorRadius = 92.0f;
constexpr float kRadialCalibrationMinTurnDeg = 330.0f;
constexpr float kRadialYawMix = 0.35f;
constexpr float kRadialAdjacentMinDeg = 50.0f;
constexpr float kRadialAdjacentMaxDeg = 130.0f;
constexpr int32_t kThrowAccelDeltaThreshold = 7200;
constexpr float kCubeNormalScale = 32.0f;
constexpr float kCubeThrownScale = 18.0f;
constexpr float kCubeScaleShrinkPixelsPerSecond = 24.0f;
constexpr float kCubeScaleGrowPixelsPerSecond = 7.0f;
constexpr uint32_t kCubeScaleRecoverDurationMs = 2000;
constexpr float kCubeScaleRecoverOffsetThreshold = 28.0f;
constexpr float kCubeScaleRecoverVelocityThreshold = 80.0f;
constexpr float kCubeScaleRecoverSpinThreshold = 60.0f;
constexpr float kCubeRecoveryOffsetSpring = 8.0f;
constexpr float kCubeRecoveryVelocityDamping = 0.86f;
constexpr float kCubeRecoverySpinSpring = 7.0f;
constexpr float kCubeRecoverySpinDamping = 0.84f;
constexpr float kThrowVelocityScale = 0.040f;
constexpr float kThrowSpinScale = 0.026f;
constexpr float kThrowZProjection = 0.42f;
constexpr float kThrowMinLaunchVelocity = 190.0f;
constexpr float kThrowCircleRadius = 84.0f;
constexpr float kThrowSpring = 3.6f;
constexpr float kThrowVelocityDamping = 0.94f;
constexpr float kThrowBounceDamping = 0.72f;
constexpr float kThrowSpinSpring = 3.8f;
constexpr float kThrowSpinDamping = 0.94f;
constexpr const char *kPrefsNamespace = "peek";
constexpr const char *kPrefsScreenCalibratedKey = "screenCal";
constexpr const char *kPrefsScreenRollKey = "screenRoll";
constexpr const char *kPrefsScreenPitchKey = "screenPitch";
constexpr const char *kPrefsScreenYawKey = "screenYaw";
constexpr const char *kPrefsRadialCalibratedKey = "radialCal";
constexpr const char *kPrefsRadialOffsetKey = "radialOffset";
constexpr const char *kPrefsRadialSwapKey = "radialSwap";
constexpr const char *kPrefsRadialFlipXKey = "radialFlipX";
constexpr const char *kPrefsRadialFlipYKey = "radialFlipY";

float relativeDegrees(float value, float zero) {
  float degrees = value - zero;
  while (degrees > 180.0f) {
    degrees -= 360.0f;
  }
  while (degrees < -180.0f) {
    degrees += 360.0f;
  }
  return degrees;
}

float normalizeDegrees(float degrees) {
  while (degrees >= 360.0f) {
    degrees -= 360.0f;
  }
  while (degrees < 0.0f) {
    degrees += 360.0f;
  }
  return degrees;
}

float shortestAngleDelta(float fromDeg, float toDeg) {
  float delta = normalizeDegrees(toDeg) - normalizeDegrees(fromDeg);
  while (delta > 180.0f) {
    delta -= 360.0f;
  }
  while (delta < -180.0f) {
    delta += 360.0f;
  }
  return delta;
}

float angleDistance(float aDeg, float bDeg) {
  return fabsf(shortestAngleDelta(aDeg, bDeg));
}

float clampFloat(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

float moveFloatToward(float value, float target, float step) {
  if (value < target) {
    return value + step > target ? target : value + step;
  }
  if (value > target) {
    return value - step < target ? target : value - step;
  }
  return value;
}

float lerpFloat(float start, float end, float amount) {
  return start + (end - start) * amount;
}

float smoothStep(float value) {
  const float clamped = clampFloat(value, 0.0f, 1.0f);
  return clamped * clamped * (3.0f - 2.0f * clamped);
}
}

AppController::AppController() : screen_(display_) {}

void AppController::begin() {
  Serial.begin(115200);

  configStore_.begin();
  config_ = configStore_.load();
  fileSystem_.begin();
  layoutStore_.begin(fileSystem_);
  assetStore_.begin(fileSystem_);

  if (!display_.begin()) {
    Serial.println("GC9A01 init failed");
    while (true) {
      delay(1000);
    }
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
    backend_.begin(config_, layoutStore_, assetStore_);
  }
  loadScreenCalibration();
  loadRadialCalibration();

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
  if (!provisioningActive) {
    network_.loop(now);
  }
  imu_.update(now);

  if (provisioningActive) {
    mode_ = AppMode::Normal;
    display_.setSleep(false);
    holdGestureConsumed_ = false;
    resetMotionBaseline();
    recordActivity(now);
    if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
      renderHomeFrame();
    }
    return;
  }

  if (!provisioningActive) {
    backend_.loop(now, network_, imu_.pose(), imu_.isReady());
  }

  const TouchEvent event = touch_.update(now);
  const bool releasedNow = event.sampled && !event.pressed;
  if (event.sampled) {
    Serial.print("Button = ");
    Serial.println(event.pressed ? "down" : "up");

    if (event.pressed) {
      recordActivity(now);
      resetMotionBaseline();
    }
  }

  if (mode_ == AppMode::Sleeping) {
    if (event.sampled && event.pressed) {
      wakeFromSleep(now);
    } else if (detectWakeMotion(now)) {
      wakeFromSleep(now);
    }
    return;
  }

  if (mode_ == AppMode::RadialMenu) {
    updateRadialMenu(now);
    if (releasedNow) {
      completeRadialMenu(now);
      holdGestureConsumed_ = false;
      resetMotionBaseline();
    }
    return;
  }

  if (mode_ == AppMode::RadialCalibration) {
    if (event.type == TouchEventType::ShortPress) {
      confirmRadialCalibrationSample(now);
      holdGestureConsumed_ = false;
      resetMotionBaseline();
      return;
    }
    if (event.type == TouchEventType::LongPress || event.type == TouchEventType::ExtraLongPress) {
      mode_ = AppMode::Normal;
      radialCalibrationFailed_ = false;
      holdGestureConsumed_ = true;
      resetMotionBaseline();
      renderHomeText("cal cancel");
      Serial.println("Radial calibration canceled");
      return;
    }
    updateRadialCalibration(now);
    return;
  }

  if (mode_ != AppMode::ImuLocked) {
    updateCubeThrow(now);
    updateCubeScale(now);
    detectCubeThrow(now);
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
    resetMotionBaseline();
  }

  if (mode_ == AppMode::StatusView
      && !provisioningActive
      && !touch_.isPressed()
      && config_.sleepTimeoutMs > 0
      && now - lastActivityMs_ >= config_.sleepTimeoutMs) {
    enterSleep(now);
    return;
  }

  if (mode_ == AppMode::StatusView) {
    return;
  }

  if (mode_ == AppMode::ImuLocked) {
    return;
  }

  if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
    renderHomeFrame();
  }

  if (!provisioningActive
      && !touch_.isPressed()
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
                                 : (pet_.isCubePet() ? (pose.valid ? "" : "imu?") : pet_.currentPetText()));
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
    applyCubeMotion(model, pose);
  }
  if (mode_ != AppMode::ImuLocked && pet_.isPet2() && assetStore_.hasPet2Animation()) {
    model.petAnimationVisible = true;
    model.petAnimationPath = assetStore_.pet2AnimationPath().c_str();
  }
}

const char *AppController::currentHomeHint() const {
  if (mode_ == AppMode::ImuLocked) {
    return "imu locked";
  }
  if (mode_ == AppMode::RadialMenu) {
    return "menu";
  }
  if (mode_ == AppMode::RadialCalibration) {
    return "cal";
  }
  if (touch_.isPressed()) {
    return "gesture";
  }
  return pet_.isSleeping() ? "sleeping" : "hold + shake";
}

bool AppController::isOffline(uint32_t now) const {
  if (provisioning_.isActive()) {
    return false;
  }
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

bool AppController::readMotionDelta(int32_t &deltaX, int32_t &deltaY, int32_t &deltaZ) {
  if (!imu_.lastSample().valid) {
    return false;
  }

  const ImuSample &sample = imu_.lastSample();
  if (!hasMotionBaseline_) {
    previousAccelX_ = sample.accelX;
    previousAccelY_ = sample.accelY;
    previousAccelZ_ = sample.accelZ;
    hasMotionBaseline_ = true;
    return false;
  }

  deltaX = static_cast<int32_t>(sample.accelX) - previousAccelX_;
  deltaY = static_cast<int32_t>(sample.accelY) - previousAccelY_;
  deltaZ = static_cast<int32_t>(sample.accelZ) - previousAccelZ_;
  previousAccelX_ = sample.accelX;
  previousAccelY_ = sample.accelY;
  previousAccelZ_ = sample.accelZ;
  return true;
}

bool AppController::detectWakeMotion(uint32_t now) {
  int32_t deltaX = 0;
  int32_t deltaY = 0;
  int32_t deltaZ = 0;
  if (!readMotionDelta(deltaX, deltaY, deltaZ)) {
    return false;
  }

  const int32_t motion = labs(deltaX) + labs(deltaY) + labs(deltaZ);
  if (motion < static_cast<int32_t>(config_.wakeMotionThreshold)) {
    return false;
  }

  recordActivity(now);
  Serial.print("Wake motion ");
  Serial.println(motion);
  return true;
}

void AppController::enterSleep(uint32_t now) {
  mode_ = AppMode::Sleeping;
  holdGestureConsumed_ = false;
  resetShortPressSequence();
  stopCubeThrow();
  resetMotionBaseline();
  renderHomeText("sleeping");
  display_.setSleep(true);
  Serial.print("Sleep timeout at ");
  Serial.println(now);
}

void AppController::wakeFromSleep(uint32_t now) {
  display_.setSleep(false);
  mode_ = AppMode::Normal;
  recordActivity(now);
  resetMotionBaseline();
  renderHomeText(currentHomeHint());
  Serial.println("Wake -> normal");
}

void AppController::enterImuLocked(uint32_t now) {
  mode_ = AppMode::ImuLocked;
  holdGestureConsumed_ = false;
  resetShortPressSequence();
  stopCubeThrow();
  resetMotionBaseline();
  recordActivity(now);
  renderHomeText("imu locked");
  Serial.println("IMU input locked");
}

void AppController::exitImuLocked(uint32_t now) {
  mode_ = AppMode::Normal;
  resetShortPressSequence();
  resetMotionBaseline();
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
  Serial.print("Short press sequence ");
  Serial.println(shortPressCount_);
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

void AppController::updateCubeScale(uint32_t now) {
  if (lastCubeScaleUpdateMs_ == 0) {
    lastCubeScaleUpdateMs_ = now;
    return;
  }

  float dt = static_cast<float>(now - lastCubeScaleUpdateMs_) / 1000.0f;
  if (dt <= 0.0f) {
    return;
  }
  lastCubeScaleUpdateMs_ = now;
  if (dt > 0.12f) {
    dt = static_cast<float>(kHomeFrameIntervalMs) / 1000.0f;
  }

  if (cubeScaleRecovering_) {
    const float progress = smoothStep(
        static_cast<float>(now - cubeScaleRecoverStartMs_) / kCubeScaleRecoverDurationMs);
    cubeRenderScale_ = lerpFloat(cubeRecoverScaleStart_, kCubeNormalScale, progress);
    return;
  }

  const float targetScale = cubeThrown_ ? kCubeThrownScale : kCubeNormalScale;
  const float scaleSpeed = targetScale < cubeRenderScale_
                               ? kCubeScaleShrinkPixelsPerSecond
                               : kCubeScaleGrowPixelsPerSecond;
  cubeRenderScale_ = moveFloatToward(cubeRenderScale_, targetScale, scaleSpeed * dt);
}

void AppController::updateCubeThrow(uint32_t now) {
  if (!cubeThrown_) {
    return;
  }

  float dt = static_cast<float>(now - lastCubeThrowUpdateMs_) / 1000.0f;
  if (dt <= 0.0f) {
    return;
  }
  lastCubeThrowUpdateMs_ = now;
  if (dt > 0.12f) {
    dt = static_cast<float>(kHomeFrameIntervalMs) / 1000.0f;
  }
  const float frameScale = dt / (static_cast<float>(kHomeFrameIntervalMs) / 1000.0f);

  if (cubeScaleRecovering_) {
    updateCubeRecovery(now, dt, frameScale);
    return;
  }

  cubeVelocityX_ += -cubeOffsetX_ * kThrowSpring * dt;
  cubeVelocityY_ += -cubeOffsetY_ * kThrowSpring * dt;
  cubeOffsetX_ += cubeVelocityX_ * dt;
  cubeOffsetY_ += cubeVelocityY_ * dt;

  const float distance = sqrtf(cubeOffsetX_ * cubeOffsetX_ + cubeOffsetY_ * cubeOffsetY_);
  if (distance > kThrowCircleRadius) {
    const float normalX = cubeOffsetX_ / distance;
    const float normalY = cubeOffsetY_ / distance;
    cubeOffsetX_ = normalX * kThrowCircleRadius;
    cubeOffsetY_ = normalY * kThrowCircleRadius;

    const float normalVelocity = cubeVelocityX_ * normalX + cubeVelocityY_ * normalY;
    if (normalVelocity > 0.0f) {
      cubeVelocityX_ -= (1.0f + kThrowBounceDamping) * normalVelocity * normalX;
      cubeVelocityY_ -= (1.0f + kThrowBounceDamping) * normalVelocity * normalY;
      Serial.print("Cube throw bounce offset ");
      Serial.print(cubeOffsetX_);
      Serial.print(",");
      Serial.print(cubeOffsetY_);
      Serial.print(" velocity ");
      Serial.print(cubeVelocityX_);
      Serial.print(",");
      Serial.println(cubeVelocityY_);
    }
  }

  const float velocityDamping = powf(kThrowVelocityDamping, frameScale);
  cubeVelocityX_ *= velocityDamping;
  cubeVelocityY_ *= velocityDamping;
  const float speed = sqrtf(cubeVelocityX_ * cubeVelocityX_ + cubeVelocityY_ * cubeVelocityY_);

  cubeSpinRollVelocity_ += -cubeSpinRollDeg_ * kThrowSpinSpring * dt;
  cubeSpinPitchVelocity_ += -cubeSpinPitchDeg_ * kThrowSpinSpring * dt;
  cubeSpinYawVelocity_ += -cubeSpinYawDeg_ * kThrowSpinSpring * dt;
  cubeSpinRollDeg_ += cubeSpinRollVelocity_ * dt;
  cubeSpinPitchDeg_ += cubeSpinPitchVelocity_ * dt;
  cubeSpinYawDeg_ += cubeSpinYawVelocity_ * dt;
  const float spinDamping = powf(kThrowSpinDamping, frameScale);
  cubeSpinRollVelocity_ *= spinDamping;
  cubeSpinPitchVelocity_ *= spinDamping;
  cubeSpinYawVelocity_ *= spinDamping;

  if (now - lastCubeThrowLogMs_ >= kThrowLogIntervalMs) {
    lastCubeThrowLogMs_ = now;
    Serial.print("Cube throw frame offset ");
    Serial.print(cubeOffsetX_);
    Serial.print(",");
    Serial.print(cubeOffsetY_);
    Serial.print(" velocity ");
    Serial.print(cubeVelocityX_);
    Serial.print(",");
    Serial.print(cubeVelocityY_);
    Serial.print(" spin ");
    Serial.print(cubeSpinRollDeg_);
    Serial.print(",");
    Serial.print(cubeSpinPitchDeg_);
    Serial.print(",");
    Serial.println(cubeSpinYawDeg_);
  }

  const float spinAmount = fabsf(cubeSpinRollDeg_) + fabsf(cubeSpinPitchDeg_) + fabsf(cubeSpinYawDeg_);
  const bool readyForFinalScaleRecover = distance < kCubeScaleRecoverOffsetThreshold
                                         && speed < kCubeScaleRecoverVelocityThreshold
                                         && spinAmount < kCubeScaleRecoverSpinThreshold;
  if (!cubeScaleRecovering_
      && readyForFinalScaleRecover
      && now - lastCubeThrowStartMs_ >= kThrowMinSettleMs) {
    startCubeRecovery(now);
  }
}

void AppController::startCubeRecovery(uint32_t now) {
  cubeScaleRecovering_ = true;
  cubeScaleRecoverStartMs_ = now;
  cubeRecoverScaleStart_ = cubeRenderScale_;
  Serial.println("Cube final damping recovery");
}

void AppController::updateCubeRecovery(uint32_t now, float dt, float frameScale) {
  const float linearProgress =
      static_cast<float>(now - cubeScaleRecoverStartMs_) / kCubeScaleRecoverDurationMs;

  cubeVelocityX_ += -cubeOffsetX_ * kCubeRecoveryOffsetSpring * dt;
  cubeVelocityY_ += -cubeOffsetY_ * kCubeRecoveryOffsetSpring * dt;
  cubeOffsetX_ += cubeVelocityX_ * dt;
  cubeOffsetY_ += cubeVelocityY_ * dt;
  const float recoveryVelocityDamping = powf(kCubeRecoveryVelocityDamping, frameScale);
  cubeVelocityX_ *= recoveryVelocityDamping;
  cubeVelocityY_ *= recoveryVelocityDamping;

  cubeSpinRollVelocity_ += -cubeSpinRollDeg_ * kCubeRecoverySpinSpring * dt;
  cubeSpinPitchVelocity_ += -cubeSpinPitchDeg_ * kCubeRecoverySpinSpring * dt;
  cubeSpinYawVelocity_ += -cubeSpinYawDeg_ * kCubeRecoverySpinSpring * dt;
  cubeSpinRollDeg_ += cubeSpinRollVelocity_ * dt;
  cubeSpinPitchDeg_ += cubeSpinPitchVelocity_ * dt;
  cubeSpinYawDeg_ += cubeSpinYawVelocity_ * dt;
  const float recoverySpinDamping = powf(kCubeRecoverySpinDamping, frameScale);
  cubeSpinRollVelocity_ *= recoverySpinDamping;
  cubeSpinPitchVelocity_ *= recoverySpinDamping;
  cubeSpinYawVelocity_ *= recoverySpinDamping;

  if (now - lastCubeThrowLogMs_ >= kThrowLogIntervalMs) {
    lastCubeThrowLogMs_ = now;
    Serial.print("Cube recover pose offset ");
    Serial.print(cubeOffsetX_);
    Serial.print(",");
    Serial.print(cubeOffsetY_);
    Serial.print(" spin ");
    Serial.print(cubeSpinRollDeg_);
    Serial.print(",");
    Serial.print(cubeSpinPitchDeg_);
    Serial.print(",");
    Serial.print(cubeSpinYawDeg_);
    Serial.print(" velocity ");
    Serial.print(cubeVelocityX_);
    Serial.print(",");
    Serial.println(cubeVelocityY_);
  }

  if (linearProgress < 1.0f) {
    return;
  }

  cubeThrown_ = false;
  cubeScaleRecovering_ = false;
  cubeOffsetX_ = 0.0f;
  cubeOffsetY_ = 0.0f;
  cubeVelocityX_ = 0.0f;
  cubeVelocityY_ = 0.0f;
  cubeSpinRollDeg_ = 0.0f;
  cubeSpinPitchDeg_ = 0.0f;
  cubeSpinYawDeg_ = 0.0f;
  cubeRenderScale_ = kCubeNormalScale;
  cubeSpinRollVelocity_ = 0.0f;
  cubeSpinPitchVelocity_ = 0.0f;
  cubeSpinYawVelocity_ = 0.0f;
  lastCubeThrowLogMs_ = 0;
  cubeScaleRecoverStartMs_ = 0;
  Serial.println("Cube throw settled");
}

void AppController::detectHeldPetGesture(uint32_t now) {
  if (mode_ != AppMode::Normal || holdGestureConsumed_ || !touch_.isPressed()) {
    return;
  }

  int32_t accelDeltaX = 0;
  int32_t accelDeltaY = 0;
  int32_t accelDeltaZ = 0;
  if (!readMotionDelta(accelDeltaX, accelDeltaY, accelDeltaZ)) {
    return;
  }

  const int32_t motion = labs(accelDeltaX) + labs(accelDeltaY) + labs(accelDeltaZ);
  if (motion < kThrowAccelDeltaThreshold || now - lastPetGestureMs_ < kThrowCooldownMs) {
    return;
  }

  pet_.advancePet();
  holdGestureConsumed_ = true;
  recordActivity(now);
  lastPetGestureMs_ = now;
  stopCubeThrow();
  renderHomeText("switched");

  Serial.print("Hold shake -> pet ");
  Serial.print(pet_.isCubePet() ? "cube" : pet_.currentPetText());
  Serial.print(" motion ");
  Serial.println(motion);
}

void AppController::detectCubeThrow(uint32_t now) {
  if (mode_ != AppMode::Normal || touch_.isPressed() || !pet_.isCubePet()) {
    return;
  }

  int32_t accelDeltaX = 0;
  int32_t accelDeltaY = 0;
  int32_t accelDeltaZ = 0;
  if (!readMotionDelta(accelDeltaX, accelDeltaY, accelDeltaZ)) {
    return;
  }

  const int32_t motion = labs(accelDeltaX) + labs(accelDeltaY) + labs(accelDeltaZ);
  if (motion < kThrowAccelDeltaThreshold || now - lastCubeThrowStartMs_ < kThrowCooldownMs) {
    return;
  }

  recordActivity(now);
  startCubeThrow(now, accelDeltaX, accelDeltaY, accelDeltaZ);
}

void AppController::resetMotionBaseline() {
  hasMotionBaseline_ = false;
}

void AppController::stopCubeThrow() {
  cubeThrown_ = false;
  cubeScaleRecovering_ = false;
  cubeScaleRecoverStartMs_ = 0;
  cubeOffsetX_ = 0.0f;
  cubeOffsetY_ = 0.0f;
  cubeVelocityX_ = 0.0f;
  cubeVelocityY_ = 0.0f;
  cubeSpinRollDeg_ = 0.0f;
  cubeSpinPitchDeg_ = 0.0f;
  cubeSpinYawDeg_ = 0.0f;
  cubeSpinRollVelocity_ = 0.0f;
  cubeSpinPitchVelocity_ = 0.0f;
  cubeSpinYawVelocity_ = 0.0f;
  cubeRenderScale_ = kCubeNormalScale;
  lastCubeThrowLogMs_ = 0;
}

void AppController::startCubeThrow(
    uint32_t now,
    int32_t accelDeltaX,
    int32_t accelDeltaY,
    int32_t accelDeltaZ) {
  cubeThrown_ = true;
  cubeScaleRecovering_ = false;
  cubeScaleRecoverStartMs_ = 0;
  lastCubeThrowStartMs_ = now;
  lastCubeThrowUpdateMs_ = now;
  lastCubeThrowLogMs_ = now;

  float launchX = static_cast<float>(accelDeltaX)
                  + static_cast<float>(accelDeltaZ) * kThrowZProjection;
  float launchY = static_cast<float>(accelDeltaY)
                  - static_cast<float>(accelDeltaZ) * kThrowZProjection;
  cubeVelocityX_ = clampFloat(launchX * kThrowVelocityScale, -560.0f, 560.0f);
  cubeVelocityY_ = clampFloat(launchY * kThrowVelocityScale, -560.0f, 560.0f);

  const float launchSpeed = sqrtf(cubeVelocityX_ * cubeVelocityX_ + cubeVelocityY_ * cubeVelocityY_);
  if (launchSpeed < kThrowMinLaunchVelocity) {
    const float motion = static_cast<float>(labs(accelDeltaX) + labs(accelDeltaY) + labs(accelDeltaZ));
    const float fallbackSpeed = clampFloat(motion * kThrowVelocityScale * 0.75f,
                                           kThrowMinLaunchVelocity,
                                           560.0f);
    float directionX = launchX;
    float directionY = launchY;
    float directionLength = sqrtf(directionX * directionX + directionY * directionY);
    if (directionLength < 1.0f) {
      directionX = accelDeltaZ >= 0 ? 1.0f : -1.0f;
      directionY = -0.75f;
      directionLength = sqrtf(directionX * directionX + directionY * directionY);
    }
    cubeVelocityX_ = directionX / directionLength * fallbackSpeed;
    cubeVelocityY_ = directionY / directionLength * fallbackSpeed;
  }

  cubeSpinRollVelocity_ = clampFloat(static_cast<float>(accelDeltaY) * kThrowSpinScale, -520.0f, 520.0f);
  cubeSpinPitchVelocity_ = clampFloat(static_cast<float>(-accelDeltaX) * kThrowSpinScale, -520.0f, 520.0f);
  cubeSpinYawVelocity_ = clampFloat(static_cast<float>(accelDeltaZ) * kThrowSpinScale, -520.0f, 520.0f);

  Serial.print("Cube thrown motion ");
  Serial.print(labs(accelDeltaX) + labs(accelDeltaY) + labs(accelDeltaZ));
  Serial.print(" delta ");
  Serial.print(accelDeltaX);
  Serial.print(",");
  Serial.print(accelDeltaY);
  Serial.print(",");
  Serial.print(accelDeltaZ);
  Serial.print(" velocity ");
  Serial.print(cubeVelocityX_);
  Serial.print(",");
  Serial.println(cubeVelocityY_);
}

void AppController::applyCubeMotion(HomeScreenModel &model, const ImuPose &pose) const {
  model.cubeVisible = pet_.isCubePet() && pose.valid;
  if (!pose.valid) {
    model.cubeScale = cubeRenderScale_;
    return;
  }

  model.cubeRollDeg = relativeDegrees(pose.rollDeg, cubeRollZeroDeg_) + cubeSpinRollDeg_;
  model.cubePitchDeg = relativeDegrees(pose.pitchDeg, cubePitchZeroDeg_) + cubeSpinPitchDeg_;
  model.cubeYawDeg = relativeDegrees(pose.yawDeg, cubeYawZeroDeg_) + cubeSpinYawDeg_;
  model.cubeOffsetX = cubeOffsetX_;
  model.cubeOffsetY = cubeOffsetY_;
  model.cubeScale = cubeRenderScale_;
}

void AppController::loadScreenCalibration() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    Serial.println("Screen calibration load failed: prefs open");
    return;
  }

  const bool calibrated = prefs.getBool(kPrefsScreenCalibratedKey, false);
  if (calibrated) {
    cubeRollZeroDeg_ = prefs.getFloat(kPrefsScreenRollKey, 0.0f);
    cubePitchZeroDeg_ = prefs.getFloat(kPrefsScreenPitchKey, 0.0f);
    cubeYawZeroDeg_ = prefs.getFloat(kPrefsScreenYawKey, 0.0f);
  }
  prefs.end();

  if (!calibrated) {
    Serial.println("Screen calibration: none saved");
    return;
  }

  Serial.print("Screen calibration loaded ");
  Serial.print(cubeRollZeroDeg_);
  Serial.print(",");
  Serial.print(cubePitchZeroDeg_);
  Serial.print(",");
  Serial.println(cubeYawZeroDeg_);
}

bool AppController::saveScreenCalibration() {
  const ImuPose &pose = imu_.pose();
  if (!pose.valid) {
    Serial.println("Screen calibration save skipped: imu pose invalid");
    return false;
  }

  cubeRollZeroDeg_ = pose.rollDeg;
  cubePitchZeroDeg_ = pose.pitchDeg;
  cubeYawZeroDeg_ = pose.yawDeg;
  cubeThrown_ = false;
  cubeScaleRecovering_ = false;
  cubeScaleRecoverStartMs_ = 0;
  cubeOffsetX_ = 0.0f;
  cubeOffsetY_ = 0.0f;
  cubeVelocityX_ = 0.0f;
  cubeVelocityY_ = 0.0f;
  cubeSpinRollDeg_ = 0.0f;
  cubeSpinPitchDeg_ = 0.0f;
  cubeSpinYawDeg_ = 0.0f;
  cubeSpinRollVelocity_ = 0.0f;
  cubeSpinPitchVelocity_ = 0.0f;
  cubeSpinYawVelocity_ = 0.0f;
  lastCubeThrowLogMs_ = 0;

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    Serial.println("Screen calibration save failed: prefs open");
    return false;
  }
  prefs.putFloat(kPrefsScreenRollKey, cubeRollZeroDeg_);
  prefs.putFloat(kPrefsScreenPitchKey, cubePitchZeroDeg_);
  prefs.putFloat(kPrefsScreenYawKey, cubeYawZeroDeg_);
  prefs.putBool(kPrefsScreenCalibratedKey, true);
  prefs.end();

  Serial.print("Screen calibration saved ");
  Serial.print(cubeRollZeroDeg_);
  Serial.print(",");
  Serial.print(cubePitchZeroDeg_);
  Serial.print(",");
  Serial.println(cubeYawZeroDeg_);
  return true;
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
  cubeThrown_ = false;
  cubeScaleRecovering_ = false;
  cubeScaleRecoverStartMs_ = 0;
  cubeOffsetX_ = 0.0f;
  cubeOffsetY_ = 0.0f;
  cubeVelocityX_ = 0.0f;
  cubeVelocityY_ = 0.0f;
  cubeSpinRollDeg_ = 0.0f;
  cubeSpinPitchDeg_ = 0.0f;
  cubeSpinYawDeg_ = 0.0f;
  cubeSpinRollVelocity_ = 0.0f;
  cubeSpinPitchVelocity_ = 0.0f;
  cubeSpinYawVelocity_ = 0.0f;
  lastCubeThrowLogMs_ = 0;
  Serial.print("Cube centered at ");
  Serial.print(cubeRollZeroDeg_);
  Serial.print(",");
  Serial.print(cubePitchZeroDeg_);
  Serial.print(",");
  Serial.println(cubeYawZeroDeg_);
}

void AppController::loadRadialCalibration() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    Serial.println("Radial calibration load failed: prefs open");
    return;
  }

  radialCalibrated_ = prefs.getBool(kPrefsRadialCalibratedKey, false);
  radialAngleOffsetDeg_ = prefs.getFloat(kPrefsRadialOffsetKey, 0.0f);
  radialSwapAxes_ = prefs.getBool(kPrefsRadialSwapKey, false);
  radialFlipX_ = prefs.getBool(kPrefsRadialFlipXKey, false);
  radialFlipY_ = prefs.getBool(kPrefsRadialFlipYKey, false);
  prefs.end();

  Serial.print("Radial calibration ");
  Serial.print(radialCalibrated_ ? "loaded " : "default ");
  Serial.print(radialAngleOffsetDeg_);
  Serial.print(" swap ");
  Serial.print(radialSwapAxes_);
  Serial.print(" flip ");
  Serial.print(radialFlipX_);
  Serial.print(",");
  Serial.println(radialFlipY_);
}

bool AppController::saveRadialCalibration(
    bool swapAxes,
    bool flipX,
    bool flipY,
    float angleOffsetDeg) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    Serial.println("Radial calibration save failed: prefs open");
    return false;
  }

  radialSwapAxes_ = swapAxes;
  radialFlipX_ = flipX;
  radialFlipY_ = flipY;
  radialAngleOffsetDeg_ = normalizeDegrees(angleOffsetDeg);
  radialCalibrated_ = true;
  prefs.putFloat(kPrefsRadialOffsetKey, radialAngleOffsetDeg_);
  prefs.putBool(kPrefsRadialSwapKey, radialSwapAxes_);
  prefs.putBool(kPrefsRadialFlipXKey, radialFlipX_);
  prefs.putBool(kPrefsRadialFlipYKey, radialFlipY_);
  prefs.putBool(kPrefsRadialCalibratedKey, true);
  prefs.end();

  Serial.print("Radial calibration saved ");
  Serial.print(radialAngleOffsetDeg_);
  Serial.print(" swap ");
  Serial.print(radialSwapAxes_);
  Serial.print(" flip ");
  Serial.print(radialFlipX_);
  Serial.print(",");
  Serial.println(radialFlipY_);
  return true;
}

bool AppController::radialCursor(
    float &cursorX,
    float &cursorY,
    float &rawAngleDeg,
    float &mappedAngleDeg) const {
  float rawX = 0.0f;
  float rawY = 0.0f;
  if (!radialRawVector(rawX, rawY)) {
    cursorX = 0.0f;
    cursorY = kRadialCursorRadius;
    rawAngleDeg = 270.0f;
    mappedAngleDeg = 270.0f;
    return false;
  }

  rawAngleDeg = normalizeDegrees(atan2f(-rawY, rawX) * RAD_TO_DEG);
  float mappedX = 0.0f;
  float mappedY = 0.0f;
  transformRadialVector(rawX, rawY, mappedX, mappedY);
  const float transformedAngleDeg = normalizeDegrees(atan2f(-mappedY, mappedX) * RAD_TO_DEG);
  mappedAngleDeg = normalizeDegrees(transformedAngleDeg + radialAngleOffsetDeg_);
  const float cursorRadians = mappedAngleDeg * DEG_TO_RAD;
  cursorX = cosf(cursorRadians) * kRadialCursorRadius;
  cursorY = -sinf(cursorRadians) * kRadialCursorRadius;
  return true;
}

bool AppController::radialRawVector(float &rawX, float &rawY) const {
  const ImuPose &pose = imu_.pose();
  if (!pose.valid) {
    rawX = 0.0f;
    rawY = 0.0f;
    return false;
  }

  const float roll = relativeDegrees(pose.rollDeg, cubeRollZeroDeg_);
  const float pitch = relativeDegrees(pose.pitchDeg, cubePitchZeroDeg_);
  const float yaw = relativeDegrees(pose.yawDeg, cubeYawZeroDeg_);
  rawX = (roll + yaw * kRadialYawMix) * kRadialCursorGain;
  rawY = (pitch - yaw * kRadialYawMix) * kRadialCursorGain;
  if (fabsf(rawX) + fabsf(rawY) < 1.0f) {
    rawY = kRadialCursorRadius;
  }
  return true;
}

void AppController::transformRadialVector(float rawX, float rawY, float &mappedX, float &mappedY) const {
  mappedX = radialSwapAxes_ ? rawY : rawX;
  mappedY = radialSwapAxes_ ? rawX : rawY;
  if (radialFlipX_) {
    mappedX = -mappedX;
  }
  if (radialFlipY_) {
    mappedY = -mappedY;
  }
}

RadialMenuItem AppController::radialItemForAngle(float angleDeg) const {
  const float normalized = normalizeDegrees(angleDeg);
  if (normalized >= 45.0f && normalized < 135.0f) {
    return RadialMenuItem::Info;
  }
  if (normalized >= 135.0f && normalized < 225.0f) {
    return RadialMenuItem::PreviousPet;
  }
  if (normalized >= 225.0f && normalized < 315.0f) {
    return RadialMenuItem::Cancel;
  }
  return RadialMenuItem::NextPet;
}

void AppController::renderRadialMenuFrame() {
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  float rawAngleDeg = 0.0f;
  float mappedAngleDeg = 0.0f;
  const bool ready = radialCursor(cursorX, cursorY, rawAngleDeg, mappedAngleDeg);

  RadialMenuModel model;
  model.selectedItem = radialSelectedItem_;
  model.cursorX = cursorX;
  model.cursorY = cursorY;
  model.imuReady = ready;
  model.calibratingHint = fabsf(radialSpinAccumulatedDeg_) > 180.0f;
  screen_.renderRadialMenu(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderRadialCalibrationFrame() {
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  float rawAngleDeg = 0.0f;
  float mappedAngleDeg = 0.0f;
  radialCursor(cursorX, cursorY, rawAngleDeg, mappedAngleDeg);

  RadialCalibrationModel model;
  model.targetItem = calibrationTargetItem_;
  model.completedCount = calibrationStep_;
  model.cursorX = cursorX;
  model.cursorY = cursorY;
  model.failed = radialCalibrationFailed_;
  screen_.renderRadialCalibration(model);
  lastHomeRenderMs_ = millis();
}

void AppController::enterRadialMenu(uint32_t now) {
  mode_ = AppMode::RadialMenu;
  radialSelectedItem_ = RadialMenuItem::Cancel;
  holdGestureConsumed_ = false;
  resetShortPressSequence();
  resetMotionBaseline();
  resetRadialSpinTracking();
  stopCubeThrow();
  recordActivity(now);
  renderRadialMenuFrame();
  Serial.println("Long press -> radial menu");
}

void AppController::updateRadialMenu(uint32_t now) {
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  float rawAngleDeg = 0.0f;
  float mappedAngleDeg = 0.0f;
  const bool ready = radialCursor(cursorX, cursorY, rawAngleDeg, mappedAngleDeg);
  if (ready) {
    radialSelectedItem_ = radialItemForAngle(mappedAngleDeg);
    updateRadialSpinTracking(rawAngleDeg);
    if (fabsf(radialSpinAccumulatedDeg_) >= kRadialCalibrationMinTurnDeg) {
      enterRadialCalibration(now);
      return;
    }
  }

  if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
    renderRadialMenuFrame();
  }
}

void AppController::completeRadialMenu(uint32_t now) {
  const RadialMenuItem selected = radialSelectedItem_;
  mode_ = AppMode::Normal;
  resetRadialSpinTracking();
  recordActivity(now);
  triggerRadialItem(selected, now);
}

void AppController::enterRadialCalibration(uint32_t now) {
  mode_ = AppMode::RadialCalibration;
  calibrationStep_ = 0;
  calibrationTargetItem_ = RadialMenuItem::Info;
  radialCalibrationFailed_ = false;
  for (uint8_t index = 0; index < 4; ++index) {
    calibrationRawX_[index] = 0.0f;
    calibrationRawY_[index] = 0.0f;
  }
  resetRadialSpinTracking();
  recordActivity(now);
  renderRadialCalibrationFrame();
  Serial.println("Radial calibration started");
}

void AppController::updateRadialCalibration(uint32_t now) {
  static constexpr RadialMenuItem kTargets[4] = {
      RadialMenuItem::Info,
      RadialMenuItem::PreviousPet,
      RadialMenuItem::Cancel,
      RadialMenuItem::NextPet,
  };

  calibrationTargetItem_ = kTargets[calibrationStep_];

  if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
    renderRadialCalibrationFrame();
  }
}

void AppController::confirmRadialCalibrationSample(uint32_t now) {
  static constexpr RadialMenuItem kTargets[4] = {
      RadialMenuItem::Info,
      RadialMenuItem::PreviousPet,
      RadialMenuItem::Cancel,
      RadialMenuItem::NextPet,
  };

  float rawX = 0.0f;
  float rawY = 0.0f;
  if (!radialRawVector(rawX, rawY)) {
    radialCalibrationFailed_ = true;
    renderRadialCalibrationFrame();
    Serial.println("Radial calibration sample skipped: imu pose invalid");
    return;
  }

  calibrationRawX_[calibrationStep_] = rawX;
  calibrationRawY_[calibrationStep_] = rawY;
  ++calibrationStep_;
  recordActivity(now);

  if (calibrationStep_ >= 4) {
    const bool saved = finishRadialCalibration();
    mode_ = AppMode::Normal;
    renderHomeText(saved ? "menu cal ok" : "menu cal fail");
    Serial.println(saved ? "Radial calibration complete" : "Radial calibration failed");
    return;
  }

  calibrationTargetItem_ = kTargets[calibrationStep_];
  radialCalibrationFailed_ = false;
  renderRadialCalibrationFrame();
  Serial.print("Radial calibration sample ");
  Serial.println(calibrationStep_);
}

bool AppController::finishRadialCalibration() {
  static constexpr float kTargetAngles[4] = {90.0f, 180.0f, 270.0f, 0.0f};
  float bestError = 100000.0f;
  bool bestSwap = false;
  bool bestFlipX = false;
  bool bestFlipY = false;
  float bestOffset = 0.0f;

  for (uint8_t swap = 0; swap < 2; ++swap) {
    for (uint8_t flipX = 0; flipX < 2; ++flipX) {
      for (uint8_t flipY = 0; flipY < 2; ++flipY) {
        float angles[4];
        float sinSum = 0.0f;
        float cosSum = 0.0f;
        for (uint8_t index = 0; index < 4; ++index) {
          float mappedX = swap ? calibrationRawY_[index] : calibrationRawX_[index];
          float mappedY = swap ? calibrationRawX_[index] : calibrationRawY_[index];
          if (flipX) {
            mappedX = -mappedX;
          }
          if (flipY) {
            mappedY = -mappedY;
          }
          angles[index] = normalizeDegrees(atan2f(-mappedY, mappedX) * RAD_TO_DEG);
          const float offset = normalizeDegrees(kTargetAngles[index] - angles[index]);
          sinSum += sinf(offset * DEG_TO_RAD);
          cosSum += cosf(offset * DEG_TO_RAD);
        }

        const float offsetDeg = normalizeDegrees(atan2f(sinSum, cosSum) * RAD_TO_DEG);
        float error = 0.0f;
        bool adjacencyOk = true;
        for (uint8_t index = 0; index < 4; ++index) {
          const float calibratedAngle = normalizeDegrees(angles[index] + offsetDeg);
          error += angleDistance(calibratedAngle, kTargetAngles[index]);

          const uint8_t next = (index + 1) % 4;
          const float nextAngle = normalizeDegrees(angles[next] + offsetDeg);
          const float distance = angleDistance(calibratedAngle, nextAngle);
          if (distance < kRadialAdjacentMinDeg || distance > kRadialAdjacentMaxDeg) {
            adjacencyOk = false;
          }
        }

        if (adjacencyOk && error < bestError) {
          bestError = error;
          bestSwap = swap;
          bestFlipX = flipX;
          bestFlipY = flipY;
          bestOffset = offsetDeg;
        }
      }
    }
  }

  if (bestError > 90.0f) {
    radialCalibrationFailed_ = true;
    Serial.print("Radial calibration failed error ");
    Serial.println(bestError);
    return false;
  }

  return saveRadialCalibration(bestSwap, bestFlipX, bestFlipY, bestOffset);
}

void AppController::resetRadialSpinTracking() {
  radialSpinTracking_ = false;
  radialSpinPreviousAngleDeg_ = 0.0f;
  radialSpinAccumulatedDeg_ = 0.0f;
}

void AppController::updateRadialSpinTracking(float rawAngleDeg) {
  if (!radialSpinTracking_) {
    radialSpinPreviousAngleDeg_ = rawAngleDeg;
    radialSpinTracking_ = true;
    return;
  }

  radialSpinAccumulatedDeg_ += shortestAngleDelta(radialSpinPreviousAngleDeg_, rawAngleDeg);
  radialSpinPreviousAngleDeg_ = rawAngleDeg;
}

void AppController::triggerRadialItem(RadialMenuItem item, uint32_t now) {
  resetShortPressSequence();
  resetMotionBaseline();
  switch (item) {
    case RadialMenuItem::Info:
      mode_ = AppMode::StatusView;
      pet_.wakeForLongPress();
      renderStatus();
      Serial.println("Radial menu -> status");
      return;
    case RadialMenuItem::PreviousPet:
      pet_.previousPet();
      stopCubeThrow();
      renderHomeText("previous");
      Serial.println("Radial menu -> previous pet");
      return;
    case RadialMenuItem::NextPet:
      pet_.advancePet();
      stopCubeThrow();
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
    resetMotionBaseline();
    renderHomeText(currentHomeHint());
    Serial.println("Short press -> exit status");
    return;
  }

  if (updateShortPressSequence(now)) {
    return;
  }

  centerCube();
  renderHomeText("centered");

  Serial.println("Short press -> center cube");
}

void AppController::handleLongPress() {
  if (mode_ == AppMode::ImuLocked) {
    return;
  }

  enterRadialMenu(millis());
}

void AppController::handleExtraLongPress() {
  if (mode_ == AppMode::ImuLocked) {
    return;
  }

  resetShortPressSequence();
  renderHomeText("hold menu");
  Serial.println("Extra long press ignored; use radial calibration");
}
