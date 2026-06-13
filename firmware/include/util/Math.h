#pragma once

#include <math.h>
#include <stdint.h>

inline float relativeDegrees(float value, float zero) {
  float degrees = value - zero;
  while (degrees > 180.0f) degrees -= 360.0f;
  while (degrees < -180.0f) degrees += 360.0f;
  return degrees;
}

inline float normalizeDegrees(float degrees) {
  while (degrees >= 360.0f) degrees -= 360.0f;
  while (degrees < 0.0f) degrees += 360.0f;
  return degrees;
}

inline float shortestAngleDelta(float fromDeg, float toDeg) {
  float delta = normalizeDegrees(toDeg) - normalizeDegrees(fromDeg);
  while (delta > 180.0f) delta -= 360.0f;
  while (delta < -180.0f) delta += 360.0f;
  return delta;
}

inline float angleDistance(float aDeg, float bDeg) {
  return fabsf(shortestAngleDelta(aDeg, bDeg));
}

inline float clampFloat(float value, float minimum, float maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

inline float moveFloatToward(float value, float target, float step) {
  if (value < target) return value + step > target ? target : value + step;
  if (value > target) return value - step < target ? target : value - step;
  return value;
}

inline float lerpFloat(float start, float end, float amount) {
  return start + (end - start) * amount;
}

inline float smoothStep(float value) {
  const float clamped = clampFloat(value, 0.0f, 1.0f);
  return clamped * clamped * (3.0f - 2.0f * clamped);
}
