#include "drivers/MotorDriver.h"

#include <Arduino.h>
#include <Wire.h>

#include "Pins.h"

namespace {
constexpr uint8_t kAddress = 0x5A;
constexpr uint8_t kRegisterMode = 0x01;
constexpr uint8_t kRegisterRealtimePlayback = 0x02;
constexpr uint8_t kRegisterLibrary = 0x03;
constexpr uint8_t kRegisterWaveform0 = 0x04;
constexpr uint8_t kRegisterGo = 0x0C;
constexpr uint8_t kRegisterFeedbackControl = 0x1A;

constexpr uint8_t kModeInternalTrigger = 0x00;
constexpr uint8_t kModeRealtimePlayback = 0x05;
constexpr uint8_t kLibraryLra = 0x06;
constexpr uint8_t kFeedbackLraMode = 0x80;
} // namespace

bool MotorDriver::begin() {
  ready_ = false;

  Wire.begin(Pins::IMU_SDA, Pins::IMU_SCL);
  Wire.setClock(400000);

  uint8_t feedback = 0;
  if (!readRegister(kRegisterFeedbackControl, feedback)) {
    return false;
  }

  if (!writeRegister(kRegisterMode, kModeInternalTrigger)) {
    return false;
  }
  if (!writeRegister(kRegisterFeedbackControl, feedback | kFeedbackLraMode)) {
    return false;
  }
  if (!writeRegister(kRegisterLibrary, kLibraryLra)) {
    return false;
  }

  ready_ = true;
  return true;
}

bool MotorDriver::isReady() const {
  return ready_;
}

void MotorDriver::play(Effect effect) {
  play(static_cast<uint8_t>(effect));
}

void MotorDriver::play(uint8_t effect) {
  if (!ready_ || effect == 0) {
    return;
  }

  writeRegister(kRegisterMode, kModeInternalTrigger);
  writeRegister(kRegisterWaveform0, effect);
  writeRegister(kRegisterWaveform0 + 1, 0);
  writeRegister(kRegisterGo, 1);
}

void MotorDriver::vibrate(uint8_t strength, uint32_t durationMs) {
  if (!ready_ || strength == 0 || durationMs == 0) {
    return;
  }

  if (strength > 127) {
    strength = 127;
  }

  writeRegister(kRegisterMode, kModeRealtimePlayback);
  writeRegister(kRegisterRealtimePlayback, strength);
  delay(durationMs);
  writeRegister(kRegisterRealtimePlayback, 0);
  writeRegister(kRegisterMode, kModeInternalTrigger);
}

void MotorDriver::setRealtimeStrength(uint8_t strength) {
  if (!ready_) {
    return;
  }

  if (strength > 127) {
    strength = 127;
  }

  writeRegister(kRegisterMode, kModeRealtimePlayback);
  writeRegister(kRegisterRealtimePlayback, strength);
}

void MotorDriver::stop() {
  if (!ready_) {
    return;
  }

  writeRegister(kRegisterRealtimePlayback, 0);
  writeRegister(kRegisterMode, kModeInternalTrigger);
}

bool MotorDriver::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool MotorDriver::readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(kAddress, static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}
