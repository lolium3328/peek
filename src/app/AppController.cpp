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

  BootScreenModel bootModel;
  bootModel.title = "Peek";
  bootModel.message = provisioning_.isActive() ? "setup ap" : (imuReady ? "imu ok" : "imu missing");
  screen_.renderBoot(bootModel);
  delay(500);

  Serial.println("Button input start");
  resetPet();
  lastTouchMs_ = millis();
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
    statusVisible_ = false;
    holdGestureConsumed_ = false;
    resetMotionBaseline();
    if (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs) {
      renderHomeFrame();
    }
    return;
  }

  if (!provisioningActive) {
    backend_.loop(now, network_, imu_.pose(), imu_.isReady());
  }
  updateCubeThrow(now);
  updateCubeScale(now);

  if (!statusVisible_ && (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs)) {
    renderHomeFrame();
  }

  const TouchEvent event = touch_.update(now);
  const bool releasedNow = event.sampled && !event.pressed;
  if (event.sampled) {
    Serial.print("Button = ");
    Serial.println(event.pressed ? "down" : "up");

    if (event.pressed) {
      lastTouchMs_ = now;
      resetMotionBaseline();
    }
  }

  detectHeldPetGesture(now);
  detectCubeThrow(now);

  if (event.sampled && !holdGestureConsumed_) {
    if (event.type == TouchEventType::ExtraLongPress) {
      lastTouchMs_ = now;
      handleExtraLongPress();
    } else if (event.type == TouchEventType::LongPress) {
      lastTouchMs_ = now;
      handleLongPress();
    } else if (event.type == TouchEventType::ShortPress) {
      lastTouchMs_ = now;
      handleCompletedClick();
    }
  }

  if (releasedNow) {
    holdGestureConsumed_ = false;
    resetMotionBaseline();
  }

  if (!pet_.isSleeping() && !touch_.isPressed() && (now - lastTouchMs_ >= config_.sleepTimeoutMs)) {
    resetPet();
    Serial.println("Sleep timeout -> cube pet");
  }
}

void AppController::resetPet() {
  pet_.reset();
  statusVisible_ = false;
  renderHomeText(pet_.isSleeping() ? "sleeping" : "hold + shake");
}

void AppController::renderHomeText(const char *hintText) {
  const ImuPose &pose = imu_.pose();
  HomeScreenModel model;
  model.primaryText = provisioning_.isActive()
                          ? "setup"
                          : (pet_.isCubePet() ? (pose.valid ? "" : "imu?") : pet_.currentPetText());
  model.hintText = provisioning_.isActive() ? provisioning_.apSsid().c_str() : hintText;
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.wifiConnected = network_.isConnected();
  model.backendConnected = !provisioning_.isActive() && backend_.isConnected(millis());
  model.poseAlert = false;
  if (!provisioning_.isActive()) {
    applyCubeMotion(model, pose);
  }
  if (pet_.isPet2() && assetStore_.hasPet2Animation()) {
    model.petAnimationVisible = true;
    model.petAnimationPath = assetStore_.pet2AnimationPath().c_str();
  }
  screen_.renderHome(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderHomeFrame() {
  const ImuPose &pose = imu_.pose();
  HomeScreenModel model;
  model.primaryText = provisioning_.isActive()
                          ? "setup"
                          : (pet_.isCubePet() ? (pose.valid ? "" : "imu?") : pet_.currentPetText());
  model.hintText = provisioning_.isActive()
                       ? provisioning_.apSsid().c_str()
                       : (touch_.isPressed() ? "gesture" : (pet_.isSleeping() ? "sleeping" : "hold + shake"));
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.wifiConnected = network_.isConnected();
  model.backendConnected = !provisioning_.isActive() && backend_.isConnected(millis());
  if (!provisioning_.isActive()) {
    applyCubeMotion(model, pose);
  }
  if (pet_.isPet2() && assetStore_.hasPet2Animation()) {
    model.petAnimationVisible = true;
    model.petAnimationPath = assetStore_.pet2AnimationPath().c_str();
  }
  if (cubeThrown_) {
    screen_.renderHome(model);
  } else {
    screen_.renderHomeFrame(model);
  }
  lastHomeRenderMs_ = millis();
}

void AppController::renderStatus() {
  const ImuPose &pose = imu_.pose();
  StatusScreenModel model;
  model.buttonPressed = touch_.isPressed();
  model.wifiRssi = static_cast<int8_t>(network_.rssi());
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.backendConnected = !provisioning_.isActive() && backend_.isConnected(millis());
  model.imuReady = imu_.isReady();
  model.imuAddress = imu_.address();
  model.imuAccelZ = imu_.lastSample().accelZ;
  model.imuRollDeg = pose.rollDeg;
  model.imuPitchDeg = pose.pitchDeg;
  screen_.renderStatus(model);
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
  if (holdGestureConsumed_ || !touch_.isPressed() || !imu_.lastSample().valid) {
    return;
  }

  const ImuSample &sample = imu_.lastSample();
  if (!hasMotionBaseline_) {
    previousAccelX_ = sample.accelX;
    previousAccelY_ = sample.accelY;
    previousAccelZ_ = sample.accelZ;
    hasMotionBaseline_ = true;
    return;
  }

  const int32_t accelDeltaX = static_cast<int32_t>(sample.accelX) - previousAccelX_;
  const int32_t accelDeltaY = static_cast<int32_t>(sample.accelY) - previousAccelY_;
  const int32_t accelDeltaZ = static_cast<int32_t>(sample.accelZ) - previousAccelZ_;
  previousAccelX_ = sample.accelX;
  previousAccelY_ = sample.accelY;
  previousAccelZ_ = sample.accelZ;

  const int32_t motion = labs(accelDeltaX) + labs(accelDeltaY) + labs(accelDeltaZ);
  if (motion < kThrowAccelDeltaThreshold || now - lastPetGestureMs_ < kThrowCooldownMs) {
    return;
  }

  pet_.advancePet();
  holdGestureConsumed_ = true;
  statusVisible_ = false;
  lastTouchMs_ = now;
  lastPetGestureMs_ = now;
  stopCubeThrow();
  renderHomeText("switched");

  Serial.print("Hold shake -> pet ");
  Serial.print(pet_.isCubePet() ? "cube" : pet_.currentPetText());
  Serial.print(" motion ");
  Serial.println(motion);
}

void AppController::detectCubeThrow(uint32_t now) {
  if (touch_.isPressed() || statusVisible_ || !pet_.isCubePet() || !imu_.lastSample().valid) {
    return;
  }

  const ImuSample &sample = imu_.lastSample();
  if (!hasMotionBaseline_) {
    previousAccelX_ = sample.accelX;
    previousAccelY_ = sample.accelY;
    previousAccelZ_ = sample.accelZ;
    hasMotionBaseline_ = true;
    return;
  }

  const int32_t accelDeltaX = static_cast<int32_t>(sample.accelX) - previousAccelX_;
  const int32_t accelDeltaY = static_cast<int32_t>(sample.accelY) - previousAccelY_;
  const int32_t accelDeltaZ = static_cast<int32_t>(sample.accelZ) - previousAccelZ_;
  previousAccelX_ = sample.accelX;
  previousAccelY_ = sample.accelY;
  previousAccelZ_ = sample.accelZ;

  const int32_t motion = labs(accelDeltaX) + labs(accelDeltaY) + labs(accelDeltaZ);
  if (motion < kThrowAccelDeltaThreshold || now - lastCubeThrowStartMs_ < kThrowCooldownMs) {
    return;
  }

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

void AppController::handleCompletedClick() {
  statusVisible_ = false;
  centerCube();
  renderHomeText("centered");

  Serial.println("Short press -> center cube");
}

void AppController::handleLongPress() {
  statusVisible_ = true;
  pet_.wakeForLongPress();
  renderStatus();

  Serial.println("Long press -> status");
}

void AppController::handleExtraLongPress() {
  statusVisible_ = false;
  const bool saved = saveScreenCalibration();
  renderHomeText(saved ? "cal saved" : "cal failed");

  Serial.println(saved ? "Extra long press -> save calibration"
                       : "Extra long press -> calibration failed");
}
