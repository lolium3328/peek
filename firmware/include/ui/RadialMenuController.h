#pragma once

#include <stdint.h>

#include "drivers/ImuDriver.h"
#include "ui/ScreenTypes.h"

struct RadialCursorResult {
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  float rawAngleDeg = 0.0f;
  float mappedAngleDeg = 0.0f;
  bool valid = false;
};

class RadialMenuController {
public:
  RadialCursorResult computeCursor(const ImuPose &pose,
                                   float rollZero, float pitchZero,
                                   float yawZero) const;

  bool computeRawVector(const ImuPose &pose,
                        float rollZero, float pitchZero,
                        float yawZero, float &rawX, float &rawY) const;

  RadialMenuItem itemForAngle(float angleDeg) const;

  void enterMenu(uint32_t now);
  void update(uint32_t now);
  RadialMenuItem completeMenu();

  bool awaitingInitialRelease() const { return awaitingInitialRelease_; }
  void clearAwaitingInitialRelease() { awaitingInitialRelease_ = false; }

private:
  bool awaitingInitialRelease_ = false;
  RadialMenuItem selectedItem_ = RadialMenuItem::Cancel;
};
