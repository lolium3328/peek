#pragma once

#include <stdint.h>

#include "drivers/ImuDriver.h"
#include "ui/ScreenTypes.h"

class CubePhysics {
public:
  void update(uint32_t now);
  void applyToModel(HomeScreenModel &model, const ImuPose &pose,
                    float rollZero, float pitchZero, float yawZero) const;

  bool detectThrow(uint32_t now, const ImuSample &sample,
                   bool touchPressed);
  bool detectHeldShake(uint32_t now, const ImuSample &sample,
                       bool touchPressed);

  void stopThrow();
  void centerPose(float rollDeg, float pitchDeg, float yawDeg);

  float renderScale() const { return cubeRenderScale_; }
  bool isThrown() const { return cubeThrown_; }
  bool isRecovering() const { return cubeScaleRecovering_; }

  void loadZeroCalibration(float &rollZero, float &pitchZero,
                           float &yawZero) const;
  bool saveZeroCalibration(const ImuPose &pose, float &rollZero,
                           float &pitchZero, float &yawZero);

private:
  bool readMotionDelta(const ImuSample &sample,
                       int32_t &dx, int32_t &dy, int32_t &dz);
  void startThrow(uint32_t now, int32_t dx, int32_t dy, int32_t dz);
  void updateScale(uint32_t now);
  void startRecovery(uint32_t now);
  void updateRecovery(uint32_t now, float dt, float frameScale);

  bool hasMotionBaseline_ = false;
  int16_t prevAccelX_ = 0, prevAccelY_ = 0, prevAccelZ_ = 0;

  bool cubeThrown_ = false;
  bool cubeScaleRecovering_ = false;
  uint32_t lastThrowStartMs_ = 0;
  uint32_t lastThrowUpdateMs_ = 0;
  uint32_t lastScaleUpdateMs_ = 0;
  uint32_t lastThrowLogMs_ = 0;
  uint32_t lastPetGestureMs_ = 0;
  uint32_t scaleRecoverStartMs_ = 0;
  float cubeOffsetX_ = 0.0f;
  float cubeOffsetY_ = 0.0f;
  float cubeRenderScale_ = 32.0f;
  float recoverScaleStart_ = 32.0f;
  float velocityX_ = 0.0f;
  float velocityY_ = 0.0f;
  float spinRollDeg_ = 0.0f;
  float spinPitchDeg_ = 0.0f;
  float spinYawDeg_ = 0.0f;
  float spinRollVelocity_ = 0.0f;
  float spinPitchVelocity_ = 0.0f;
  float spinYawVelocity_ = 0.0f;
};
