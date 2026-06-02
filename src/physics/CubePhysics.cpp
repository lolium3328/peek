#include "physics/CubePhysics.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

#include "util/Math.h"

namespace {
constexpr uint32_t kThrowCooldownMs = 900;
constexpr uint32_t kThrowMinSettleMs = 1400;
constexpr uint32_t kThrowLogIntervalMs = 200;
constexpr uint32_t kCubeScaleRecoverDurationMs = 2000;
constexpr int32_t kThrowAccelDeltaThreshold = 7200;
constexpr float kCubeNormalScale = 32.0f;
constexpr float kCubeThrownScale = 18.0f;
constexpr float kCubeScaleShrinkPixelsPerSecond = 24.0f;
constexpr float kCubeScaleGrowPixelsPerSecond = 7.0f;
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
constexpr uint32_t kHomeFrameIntervalMs = 75;
constexpr const char *kPrefsNamespace = "peek";
constexpr const char *kPrefsCalibratedKey = "screenCal";
constexpr const char *kPrefsRollKey = "screenRoll";
constexpr const char *kPrefsPitchKey = "screenPitch";
constexpr const char *kPrefsYawKey = "screenYaw";
}  // namespace

void CubePhysics::update(uint32_t now) {
  if (!cubeThrown_) return;

  float dt = static_cast<float>(now - lastThrowUpdateMs_) / 1000.0f;
  if (dt <= 0.0f) return;
  lastThrowUpdateMs_ = now;
  if (dt > 0.12f) dt = static_cast<float>(kHomeFrameIntervalMs) / 1000.0f;
  const float frameScale = dt / (static_cast<float>(kHomeFrameIntervalMs) / 1000.0f);
  updateScale(now);

  if (cubeScaleRecovering_) {
    updateRecovery(now, dt, frameScale);
    return;
  }

  velocityX_ += -cubeOffsetX_ * kThrowSpring * dt;
  velocityY_ += -cubeOffsetY_ * kThrowSpring * dt;
  cubeOffsetX_ += velocityX_ * dt;
  cubeOffsetY_ += velocityY_ * dt;

  const float distance = sqrtf(cubeOffsetX_ * cubeOffsetX_ + cubeOffsetY_ * cubeOffsetY_);
  if (distance > kThrowCircleRadius) {
    const float normalX = cubeOffsetX_ / distance;
    const float normalY = cubeOffsetY_ / distance;
    cubeOffsetX_ = normalX * kThrowCircleRadius;
    cubeOffsetY_ = normalY * kThrowCircleRadius;
    const float normalVelocity = velocityX_ * normalX + velocityY_ * normalY;
    if (normalVelocity > 0.0f) {
      velocityX_ -= (1.0f + kThrowBounceDamping) * normalVelocity * normalX;
      velocityY_ -= (1.0f + kThrowBounceDamping) * normalVelocity * normalY;
    }
  }

  const float velocityDamping = powf(kThrowVelocityDamping, frameScale);
  velocityX_ *= velocityDamping;
  velocityY_ *= velocityDamping;
  const float speed = sqrtf(velocityX_ * velocityX_ + velocityY_ * velocityY_);

  spinRollVelocity_ += -spinRollDeg_ * kThrowSpinSpring * dt;
  spinPitchVelocity_ += -spinPitchDeg_ * kThrowSpinSpring * dt;
  spinYawVelocity_ += -spinYawDeg_ * kThrowSpinSpring * dt;
  spinRollDeg_ += spinRollVelocity_ * dt;
  spinPitchDeg_ += spinPitchVelocity_ * dt;
  spinYawDeg_ += spinYawVelocity_ * dt;
  const float spinDamping = powf(kThrowSpinDamping, frameScale);
  spinRollVelocity_ *= spinDamping;
  spinPitchVelocity_ *= spinDamping;
  spinYawVelocity_ *= spinDamping;

  const float spinAmount = fabsf(spinRollDeg_) + fabsf(spinPitchDeg_) + fabsf(spinYawDeg_);
  const bool readyForFinalScaleRecover = distance < kCubeScaleRecoverOffsetThreshold
                                         && speed < kCubeScaleRecoverVelocityThreshold
                                         && spinAmount < kCubeScaleRecoverSpinThreshold;
  if (!cubeScaleRecovering_
      && readyForFinalScaleRecover
      && now - lastThrowStartMs_ >= kThrowMinSettleMs) {
    startRecovery(now);
  }
}

void CubePhysics::applyToModel(HomeScreenModel &model, const ImuPose &pose,
                                float rollZero, float pitchZero,
                                float yawZero) const {
  if (!pose.valid) {
    model.cubeVisible = false;
    return;
  }

  model.cubeVisible = true;
  model.cubeRollDeg = relativeDegrees(pose.rollDeg, rollZero) + spinRollDeg_;
  model.cubePitchDeg = relativeDegrees(pose.pitchDeg, pitchZero) + spinPitchDeg_;
  model.cubeYawDeg = relativeDegrees(pose.yawDeg, yawZero) + spinYawDeg_;
  model.cubeOffsetX = cubeOffsetX_;
  model.cubeOffsetY = cubeOffsetY_;
  model.cubeScale = cubeRenderScale_;
}

bool CubePhysics::detectThrow(uint32_t now, const ImuSample &sample,
                               bool touchPressed) {
  if (touchPressed) return false;

  int32_t dx = 0, dy = 0, dz = 0;
  if (!readMotionDelta(sample, dx, dy, dz)) return false;

  const int32_t motion = labs(dx) + labs(dy) + labs(dz);
  if (motion < kThrowAccelDeltaThreshold || now - lastThrowStartMs_ < kThrowCooldownMs) {
    return false;
  }

  startThrow(now, dx, dy, dz);
  return true;
}

bool CubePhysics::detectHeldShake(uint32_t now, const ImuSample &sample,
                                   bool touchPressed) {
  if (!touchPressed || cubeThrown_) return false;

  int32_t dx = 0, dy = 0, dz = 0;
  if (!readMotionDelta(sample, dx, dy, dz)) return false;

  const int32_t motion = labs(dx) + labs(dy) + labs(dz);
  if (motion < kThrowAccelDeltaThreshold || now - lastPetGestureMs_ < kThrowCooldownMs) {
    return false;
  }

  lastPetGestureMs_ = now;
  return true;
}

void CubePhysics::stopThrow() {
  cubeThrown_ = false;
  cubeScaleRecovering_ = false;
  scaleRecoverStartMs_ = 0;
  cubeOffsetX_ = 0.0f;
  cubeOffsetY_ = 0.0f;
  velocityX_ = 0.0f;
  velocityY_ = 0.0f;
  spinRollDeg_ = 0.0f;
  spinPitchDeg_ = 0.0f;
  spinYawDeg_ = 0.0f;
  spinRollVelocity_ = 0.0f;
  spinPitchVelocity_ = 0.0f;
  spinYawVelocity_ = 0.0f;
  cubeRenderScale_ = kCubeNormalScale;
  lastThrowLogMs_ = 0;
}

void CubePhysics::centerPose(float rollDeg, float pitchDeg, float yawDeg) {
  stopThrow();
  hasMotionBaseline_ = false;
}

void CubePhysics::loadZeroCalibration(float &rollZero, float &pitchZero,
                                       float &yawZero) const {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) return;

  const bool calibrated = prefs.getBool(kPrefsCalibratedKey, false);
  if (calibrated) {
    rollZero = prefs.getFloat(kPrefsRollKey, 0.0f);
    pitchZero = prefs.getFloat(kPrefsPitchKey, 0.0f);
    yawZero = prefs.getFloat(kPrefsYawKey, 0.0f);
  }
  prefs.end();
}

bool CubePhysics::saveZeroCalibration(const ImuPose &pose,
                                       float &rollZero, float &pitchZero,
                                       float &yawZero) {
  if (!pose.valid) return false;

  rollZero = pose.rollDeg;
  pitchZero = pose.pitchDeg;
  yawZero = pose.yawDeg;
  stopThrow();
  hasMotionBaseline_ = false;

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) return false;
  prefs.putFloat(kPrefsRollKey, rollZero);
  prefs.putFloat(kPrefsPitchKey, pitchZero);
  prefs.putFloat(kPrefsYawKey, yawZero);
  prefs.putBool(kPrefsCalibratedKey, true);
  prefs.end();
  return true;
}

bool CubePhysics::readMotionDelta(const ImuSample &sample,
                                   int32_t &dx, int32_t &dy, int32_t &dz) {
  if (!sample.valid) return false;

  if (!hasMotionBaseline_) {
    prevAccelX_ = sample.accelX;
    prevAccelY_ = sample.accelY;
    prevAccelZ_ = sample.accelZ;
    hasMotionBaseline_ = true;
    return false;
  }

  dx = static_cast<int32_t>(sample.accelX) - prevAccelX_;
  dy = static_cast<int32_t>(sample.accelY) - prevAccelY_;
  dz = static_cast<int32_t>(sample.accelZ) - prevAccelZ_;
  prevAccelX_ = sample.accelX;
  prevAccelY_ = sample.accelY;
  prevAccelZ_ = sample.accelZ;
  return true;
}

void CubePhysics::startThrow(uint32_t now, int32_t dx, int32_t dy, int32_t dz) {
  cubeThrown_ = true;
  cubeScaleRecovering_ = false;
  scaleRecoverStartMs_ = 0;
  lastThrowStartMs_ = now;
  lastThrowUpdateMs_ = now;
  lastThrowLogMs_ = now;

  float launchX = static_cast<float>(dx) + static_cast<float>(dz) * kThrowZProjection;
  float launchY = static_cast<float>(dy) - static_cast<float>(dz) * kThrowZProjection;
  velocityX_ = clampFloat(launchX * kThrowVelocityScale, -560.0f, 560.0f);
  velocityY_ = clampFloat(launchY * kThrowVelocityScale, -560.0f, 560.0f);

  const float speed = sqrtf(velocityX_ * velocityX_ + velocityY_ * velocityY_);
  if (speed < kThrowMinLaunchVelocity) {
    const float motion = static_cast<float>(labs(dx) + labs(dy) + labs(dz));
    const float fallbackSpeed = clampFloat(
        motion * kThrowVelocityScale * 0.75f, kThrowMinLaunchVelocity, 560.0f);
    float dirX = launchX;
    float dirY = launchY;
    float dirLen = sqrtf(dirX * dirX + dirY * dirY);
    if (dirLen < 1.0f) {
      dirX = dz >= 0 ? 1.0f : -1.0f;
      dirY = -0.75f;
      dirLen = sqrtf(dirX * dirX + dirY * dirY);
    }
    velocityX_ = dirX / dirLen * fallbackSpeed;
    velocityY_ = dirY / dirLen * fallbackSpeed;
  }

  spinRollVelocity_ = clampFloat(static_cast<float>(dy) * kThrowSpinScale, -520.0f, 520.0f);
  spinPitchVelocity_ = clampFloat(static_cast<float>(-dx) * kThrowSpinScale, -520.0f, 520.0f);
  spinYawVelocity_ = clampFloat(static_cast<float>(dz) * kThrowSpinScale, -520.0f, 520.0f);
}

void CubePhysics::updateScale(uint32_t now) {
  if (lastScaleUpdateMs_ == 0) {
    lastScaleUpdateMs_ = now;
    return;
  }

  float dt = static_cast<float>(now - lastScaleUpdateMs_) / 1000.0f;
  if (dt <= 0.0f) return;
  lastScaleUpdateMs_ = now;
  if (dt > 0.12f) dt = static_cast<float>(kHomeFrameIntervalMs) / 1000.0f;

  if (cubeScaleRecovering_) {
    const float progress = smoothStep(
        static_cast<float>(now - scaleRecoverStartMs_) / kCubeScaleRecoverDurationMs);
    cubeRenderScale_ = lerpFloat(recoverScaleStart_, kCubeNormalScale, progress);
    return;
  }

  const float target = cubeThrown_ ? kCubeThrownScale : kCubeNormalScale;
  const float speed = target < cubeRenderScale_
                          ? kCubeScaleShrinkPixelsPerSecond
                          : kCubeScaleGrowPixelsPerSecond;
  cubeRenderScale_ = moveFloatToward(cubeRenderScale_, target, speed * dt);
}

void CubePhysics::startRecovery(uint32_t now) {
  cubeScaleRecovering_ = true;
  scaleRecoverStartMs_ = now;
  recoverScaleStart_ = cubeRenderScale_;
}

void CubePhysics::updateRecovery(uint32_t now, float dt, float frameScale) {
  velocityX_ += -cubeOffsetX_ * kCubeRecoveryOffsetSpring * dt;
  velocityY_ += -cubeOffsetY_ * kCubeRecoveryOffsetSpring * dt;
  cubeOffsetX_ += velocityX_ * dt;
  cubeOffsetY_ += velocityY_ * dt;
  const float velDamping = powf(kCubeRecoveryVelocityDamping, frameScale);
  velocityX_ *= velDamping;
  velocityY_ *= velDamping;

  spinRollVelocity_ += -spinRollDeg_ * kCubeRecoverySpinSpring * dt;
  spinPitchVelocity_ += -spinPitchDeg_ * kCubeRecoverySpinSpring * dt;
  spinYawVelocity_ += -spinYawDeg_ * kCubeRecoverySpinSpring * dt;
  spinRollDeg_ += spinRollVelocity_ * dt;
  spinPitchDeg_ += spinPitchVelocity_ * dt;
  spinYawDeg_ += spinYawVelocity_ * dt;
  const float spinDamping = powf(kCubeRecoverySpinDamping, frameScale);
  spinRollVelocity_ *= spinDamping;
  spinPitchVelocity_ *= spinDamping;
  spinYawVelocity_ *= spinDamping;

  const float linearProgress =
      static_cast<float>(now - scaleRecoverStartMs_) / kCubeScaleRecoverDurationMs;
  if (linearProgress < 1.0f) return;

  cubeThrown_ = false;
  cubeScaleRecovering_ = false;
  cubeOffsetX_ = 0.0f;
  cubeOffsetY_ = 0.0f;
  velocityX_ = 0.0f;
  velocityY_ = 0.0f;
  spinRollDeg_ = 0.0f;
  spinPitchDeg_ = 0.0f;
  spinYawDeg_ = 0.0f;
  cubeRenderScale_ = kCubeNormalScale;
  spinRollVelocity_ = 0.0f;
  spinPitchVelocity_ = 0.0f;
  spinYawVelocity_ = 0.0f;
  lastThrowLogMs_ = 0;
  scaleRecoverStartMs_ = 0;
}
