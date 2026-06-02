#include "ui/RadialMenuController.h"

#include <Arduino.h>
#include <math.h>

#include "util/Math.h"

namespace {
constexpr float kRadialCursorGain = 3.0f;
constexpr float kRadialCursorRadius = 92.0f;
constexpr float kRadialYawMix = 0.35f;
}  // namespace

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
  result.mappedAngleDeg = result.rawAngleDeg;
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
  selectedItem_ = RadialMenuItem::Cancel;
  awaitingInitialRelease_ = true;
}

void RadialMenuController::update(uint32_t now) {
  // selectedItem_ is updated externally via itemForAngle after computeCursor
}

RadialMenuItem RadialMenuController::completeMenu() {
  const RadialMenuItem selected = selectedItem_;
  awaitingInitialRelease_ = false;
  return selected;
}
