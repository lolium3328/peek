#include "app/AppController.h"

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>

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

  if (!display_.begin()) {
    Serial.println("GC9A01 init failed");
    while (true) {
      delay(1000);
    }
  }

  touch_.begin(config_);
  const bool imuReady = imu_.begin();

  BootScreenModel bootModel;
  bootModel.title = "Peek";
  bootModel.message = imuReady ? "imu ok" : "imu missing";
  screen_.renderBoot(bootModel);
  delay(500);

  Serial.println("Button input start");
  showText(0);
  lastTouchMs_ = millis();
}

void AppController::loop() {
  const uint32_t now = millis();
  imu_.update(now);
  detectCubeThrow(now);
  updateCubeThrow(now);
  updateCubeScale(now);

  if (!statusVisible_ && (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs)) {
    renderHomeFrame();
  }

  const TouchEvent event = touch_.update(now);
  if (!event.sampled) {
    return;
  }

  Serial.print("Button = ");
  Serial.println(event.pressed ? "down" : "up");

  if (event.pressed) {
    lastTouchMs_ = now;
  }

  if (event.type == TouchEventType::LongPress) {
    lastTouchMs_ = now;
    handleLongPress();
  } else if (event.type == TouchEventType::ShortPress) {
    lastTouchMs_ = now;
    handleCompletedClick();
  }

  if (!pet_.isSleeping() && !touch_.isPressed() && (now - lastTouchMs_ >= config_.sleepTimeoutMs)) {
    showText(0);
    Serial.println("Sleep timeout -> zzz...");
  }
}

void AppController::showText(size_t index) {
  pet_.showText(index);
  statusVisible_ = false;
  renderHomeText(pet_.currentText(), pet_.isSleeping() ? "sleeping" : "tap / hold");
}

void AppController::renderHomeText(const char *text, const char *hintText) {
  const ImuPose &pose = imu_.pose();
  HomeScreenModel model;
  model.primaryText = pose.valid ? text : "imu?";
  model.hintText = hintText;
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.wifiConnected = false;
  model.backendConnected = false;
  model.poseAlert = false;
  applyCubeMotion(model, pose);
  screen_.renderHome(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderHomeFrame() {
  const ImuPose &pose = imu_.pose();
  HomeScreenModel model;
  model.primaryText = pose.valid ? pet_.currentText() : "imu?";
  model.hintText = pet_.isSleeping() ? "sleeping" : "tap / hold";
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.wifiConnected = false;
  model.backendConnected = false;
  applyCubeMotion(model, pose);
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
  model.wifiRssi = 0;
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.backendConnected = false;
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

void AppController::detectCubeThrow(uint32_t now) {
  if (statusVisible_ || !imu_.lastSample().valid) {
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
  model.cubeVisible = pose.valid;
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
  renderHomeText(pet_.currentText(), "centered");

  Serial.println("Short press -> center cube");
}

void AppController::handleLongPress() {
  statusVisible_ = true;
  pet_.wakeForLongPress();
  renderStatus();

  Serial.println("Long press -> status");
}
