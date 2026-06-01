#include "ui/RadialMenuController.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

#include "util/Math.h"

namespace {
constexpr float kRadialCursorGain = 3.0f;
constexpr float kRadialCursorRadius = 92.0f;
constexpr float kRadialYawMix = 0.35f;
constexpr float kRadialAdjacentMinDeg = 50.0f;
constexpr float kRadialAdjacentMaxDeg = 130.0f;
constexpr const char *kPrefsNamespace = "peek";
constexpr const char *kPrefsCalibratedKey = "radialCal";
constexpr const char *kPrefsOffsetKey = "radialOffset";
constexpr const char *kPrefsSwapKey = "radialSwap";
constexpr const char *kPrefsFlipXKey = "radialFlipX";
constexpr const char *kPrefsFlipYKey = "radialFlipY";
constexpr uint32_t kHomeFrameIntervalMs = 75;
}  // namespace

void RadialMenuController::loadCalibration() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) return;

  calibrated_ = prefs.getBool(kPrefsCalibratedKey, false);
  config_.angleOffsetDeg = prefs.getFloat(kPrefsOffsetKey, 0.0f);
  config_.swapAxes = prefs.getBool(kPrefsSwapKey, false);
  config_.flipX = prefs.getBool(kPrefsFlipXKey, false);
  config_.flipY = prefs.getBool(kPrefsFlipYKey, false);
  prefs.end();
}

bool RadialMenuController::saveCalibration(const RadialCalibrationConfig &config) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) return false;

  config_ = config;
  config_.angleOffsetDeg = normalizeDegrees(config_.angleOffsetDeg);
  calibrated_ = true;

  prefs.putFloat(kPrefsOffsetKey, config_.angleOffsetDeg);
  prefs.putBool(kPrefsSwapKey, config_.swapAxes);
  prefs.putBool(kPrefsFlipXKey, config_.flipX);
  prefs.putBool(kPrefsFlipYKey, config_.flipY);
  prefs.putBool(kPrefsCalibratedKey, true);
  prefs.end();
  return true;
}

RadialCursorResult RadialMenuController::computeCursor(
    const ImuPose &pose, float rollZero, float pitchZero, float yawZero) const {
  RadialCursorResult result;
  float rawX = 0.0f, rawY = 0.0f;
  result.valid = computeRawVector(pose, rollZero, pitchZero, yawZero, rawX, rawY);

  if (!result.valid) {
    result.cursorX = 0.0f;
    result.cursorY = kRadialCursorRadius;
    result.rawAngleDeg = 270.0f;
    result.mappedAngleDeg = 270.0f;
    return result;
  }

  result.rawAngleDeg = normalizeDegrees(atan2f(-rawY, rawX) * RAD_TO_DEG);
  float mappedX = 0.0f, mappedY = 0.0f;
  transformVector(rawX, rawY, mappedX, mappedY);
  float transformed = normalizeDegrees(atan2f(-mappedY, mappedX) * RAD_TO_DEG);
  result.mappedAngleDeg = normalizeDegrees(transformed + config_.angleOffsetDeg);
  const float cursorRadians = result.mappedAngleDeg * DEG_TO_RAD;
  result.cursorX = cosf(cursorRadians) * kRadialCursorRadius;
  result.cursorY = -sinf(cursorRadians) * kRadialCursorRadius;
  return result;
}

bool RadialMenuController::computeRawVector(
    const ImuPose &pose, float rollZero, float pitchZero,
    float yawZero, float &rawX, float &rawY) const {
  if (!pose.valid) {
    rawX = 0.0f;
    rawY = 0.0f;
    return false;
  }

  const float roll = relativeDegrees(pose.rollDeg, rollZero);
  const float pitch = relativeDegrees(pose.pitchDeg, pitchZero);
  const float yaw = relativeDegrees(pose.yawDeg, yawZero);
  rawX = (roll + yaw * kRadialYawMix) * kRadialCursorGain;
  rawY = (pitch - yaw * kRadialYawMix) * kRadialCursorGain;

  if (fabsf(rawX) + fabsf(rawY) < 1.0f) {
    rawY = kRadialCursorRadius;
  }
  return true;
}

RadialMenuItem RadialMenuController::itemForAngle(float angleDeg) const {
  const float normalized = normalizeDegrees(angleDeg);
  if (normalized >= 45.0f && normalized < 135.0f) return RadialMenuItem::Info;
  if (normalized >= 135.0f && normalized < 225.0f) return RadialMenuItem::PreviousPet;
  if (normalized >= 225.0f && normalized < 315.0f) return RadialMenuItem::Cancel;
  return RadialMenuItem::NextPet;
}

void RadialMenuController::enterMenu(uint32_t now) {
  calibrating_ = false;
  calibrationFailed_ = false;
  selectedItem_ = RadialMenuItem::Cancel;
  awaitingInitialRelease_ = true;
  resetSpinTracking();
}

void RadialMenuController::update(uint32_t now) {
  // selectedItem_ is updated externally via itemForAngle after computeCursor
}

RadialMenuItem RadialMenuController::completeMenu() {
  const RadialMenuItem selected = selectedItem_;
  calibrating_ = false;
  awaitingInitialRelease_ = false;
  resetSpinTracking();
  return selected;
}

void RadialMenuController::startCalibration(uint32_t now) {
  calibrating_ = true;
  calibrationFailed_ = false;
  awaitingInitialRelease_ = false;
  calibrationStep_ = 0;
  calibrationTarget_ = RadialMenuItem::Info;
  for (uint8_t i = 0; i < 4; ++i) {
    calibrationRawX_[i] = 0.0f;
    calibrationRawY_[i] = 0.0f;
  }
  resetSpinTracking();
}

bool RadialMenuController::confirmSample(
    uint32_t now, const ImuPose &pose, float rollZero,
    float pitchZero, float yawZero) {
  static constexpr RadialMenuItem kTargets[4] = {
      RadialMenuItem::Info,
      RadialMenuItem::PreviousPet,
      RadialMenuItem::Cancel,
      RadialMenuItem::NextPet,
  };

  float rawX = 0.0f, rawY = 0.0f;
  if (!computeRawVector(pose, rollZero, pitchZero, yawZero, rawX, rawY)) {
    calibrationFailed_ = true;
    return false;
  }

  calibrationRawX_[calibrationStep_] = rawX;
  calibrationRawY_[calibrationStep_] = rawY;
  ++calibrationStep_;

  if (calibrationStep_ >= 4) {
    const bool saved = finishCalibration();
    calibrating_ = false;
    return saved;
  }

  calibrationTarget_ = kTargets[calibrationStep_];
  calibrationFailed_ = false;
  return true;
}

void RadialMenuController::updateCalibration(uint32_t now) {
  static constexpr RadialMenuItem kTargets[4] = {
      RadialMenuItem::Info,
      RadialMenuItem::PreviousPet,
      RadialMenuItem::Cancel,
      RadialMenuItem::NextPet,
  };
  calibrationTarget_ = kTargets[calibrationStep_];
}

bool RadialMenuController::finishCalibration() {
  static constexpr float kTargetAngles[4] = {90.0f, 180.0f, 270.0f, 0.0f};

  float bestError = 100000.0f;
  RadialCalibrationConfig best;

  for (uint8_t swap = 0; swap < 2; ++swap) {
    for (uint8_t flipX = 0; flipX < 2; ++flipX) {
      for (uint8_t flipY = 0; flipY < 2; ++flipY) {
        float angles[4];
        float sinSum = 0.0f, cosSum = 0.0f;
        for (uint8_t i = 0; i < 4; ++i) {
          float mx = swap ? calibrationRawY_[i] : calibrationRawX_[i];
          float my = swap ? calibrationRawX_[i] : calibrationRawY_[i];
          if (flipX) mx = -mx;
          if (flipY) my = -my;
          angles[i] = normalizeDegrees(atan2f(-my, mx) * RAD_TO_DEG);
          const float offset = normalizeDegrees(kTargetAngles[i] - angles[i]);
          sinSum += sinf(offset * DEG_TO_RAD);
          cosSum += cosf(offset * DEG_TO_RAD);
        }

        const float offsetDeg = normalizeDegrees(atan2f(sinSum, cosSum) * RAD_TO_DEG);
        float error = 0.0f;
        bool adjacencyOk = true;
        for (uint8_t i = 0; i < 4; ++i) {
          const float calibratedAngle = normalizeDegrees(angles[i] + offsetDeg);
          error += angleDistance(calibratedAngle, kTargetAngles[i]);
          const uint8_t next = (i + 1) % 4;
          const float dist = angleDistance(
              normalizeDegrees(angles[i] + offsetDeg),
              normalizeDegrees(angles[next] + offsetDeg));
          if (dist < kRadialAdjacentMinDeg || dist > kRadialAdjacentMaxDeg) {
            adjacencyOk = false;
          }
        }

        if (adjacencyOk && error < bestError) {
          bestError = error;
          best.swapAxes = swap;
          best.flipX = flipX;
          best.flipY = flipY;
          best.angleOffsetDeg = offsetDeg;
        }
      }
    }
  }

  if (bestError > 90.0f) {
    calibrationFailed_ = true;
    return false;
  }

  return saveCalibration(best);
}

void RadialMenuController::resetSpinTracking() {
  spinTracking_ = false;
  spinPreviousAngleDeg_ = 0.0f;
  spinAccumulatedDeg_ = 0.0f;
}

void RadialMenuController::updateSpinTracking(float rawAngleDeg) {
  if (!spinTracking_) {
    spinPreviousAngleDeg_ = rawAngleDeg;
    spinTracking_ = true;
    return;
  }
  spinAccumulatedDeg_ += shortestAngleDelta(spinPreviousAngleDeg_, rawAngleDeg);
  spinPreviousAngleDeg_ = rawAngleDeg;
}

void RadialMenuController::transformVector(float rawX, float rawY,
                                            float &mappedX,
                                            float &mappedY) const {
  mappedX = config_.swapAxes ? rawY : rawX;
  mappedY = config_.swapAxes ? rawX : rawY;
  if (config_.flipX) mappedX = -mappedX;
  if (config_.flipY) mappedY = -mappedY;
}
