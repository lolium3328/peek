#pragma once

#include <stdint.h>

#include "drivers/ImuDriver.h"
#include "ui/ScreenTypes.h"

struct RadialCalibrationConfig {
  bool swapAxes = false;
  bool flipX = false;
  bool flipY = false;
  float angleOffsetDeg = 0.0f;
};

struct RadialCursorResult {
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  float rawAngleDeg = 0.0f;
  float mappedAngleDeg = 0.0f;
  bool valid = false;
};

class RadialMenuController {
public:
  void loadCalibration();
  bool saveCalibration(const RadialCalibrationConfig &config);

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

  void startCalibration(uint32_t now);
  bool confirmSample(uint32_t now, const ImuPose &pose,
                     float rollZero, float pitchZero, float yawZero);
  void updateCalibration(uint32_t now);
  bool finishCalibration();
  bool isCalibrating() const { return calibrating_; }
  bool isCalibrated() const { return calibrated_; }
  bool calibrationFailed() const { return calibrationFailed_; }
  uint8_t calibrationStep() const { return calibrationStep_; }
  RadialMenuItem calibrationTarget() const { return calibrationTarget_; }

  void resetSpinTracking();
  void updateSpinTracking(float rawAngleDeg);
  float spinAccumulatedDeg() const { return spinAccumulatedDeg_; }

  const RadialCalibrationConfig &config() const { return config_; }
  bool awaitingInitialRelease() const { return awaitingInitialRelease_; }
  void clearAwaitingInitialRelease() { awaitingInitialRelease_ = false; }

private:
  void transformVector(float rawX, float rawY,
                       float &mappedX, float &mappedY) const;

  bool calibrated_ = false;
  bool calibrating_ = false;
  bool calibrationFailed_ = false;
  bool awaitingInitialRelease_ = false;
  bool spinTracking_ = false;
  uint8_t calibrationStep_ = 0;
  RadialMenuItem selectedItem_ = RadialMenuItem::Cancel;
  RadialMenuItem calibrationTarget_ = RadialMenuItem::Info;
  float spinPreviousAngleDeg_ = 0.0f;
  float spinAccumulatedDeg_ = 0.0f;
  float calibrationRawX_[4] = {};
  float calibrationRawY_[4] = {};
  RadialCalibrationConfig config_;
};
