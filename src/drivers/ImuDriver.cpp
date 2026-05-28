#include "drivers/ImuDriver.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "Pins.h"

namespace {
constexpr uint8_t kAddressLow = 0x68;
constexpr uint8_t kAddressHigh = 0x69;
constexpr uint8_t kRegisterSampleRate = 0x19;
constexpr uint8_t kRegisterConfig = 0x1A;
constexpr uint8_t kRegisterGyroConfig = 0x1B;
constexpr uint8_t kRegisterAccelConfig = 0x1C;
constexpr uint8_t kRegisterAccelXHigh = 0x3B;
constexpr uint8_t kRegisterPowerManagement1 = 0x6B;
constexpr uint8_t kRegisterWhoAmI = 0x75;
constexpr uint32_t kSampleIntervalMs = 50;
constexpr uint32_t kLogIntervalMs = 500;
constexpr uint32_t kRetryIntervalMs = 2000;
constexpr uint8_t kGyroCalibrationSamples = 80;
constexpr float kGyroSensitivity = 131.0f;
constexpr float kComplementaryAlpha = 0.96f;

int16_t readSigned16(const uint8_t *buffer, uint8_t offset) {
  return static_cast<int16_t>((static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1]);
}

bool isSupportedWhoAmI(uint8_t whoAmI) {
  return whoAmI == 0x68 || whoAmI == 0x70 || whoAmI == 0x71;
}

float wrapDegrees(float degrees) {
  while (degrees > 180.0f) {
    degrees -= 360.0f;
  }
  while (degrees < -180.0f) {
    degrees += 360.0f;
  }
  return degrees;
}
} // namespace

bool ImuDriver::begin() {
  ready_ = false;
  address_ = 0;
  whoAmI_ = 0;
  lastSample_ = ImuSample();
  pose_ = ImuPose();
  lastSampleMs_ = 0;
  lastLogMs_ = 0;
  lastPoseUpdateMs_ = 0;
  gyroBiasX_ = 0.0f;
  gyroBiasY_ = 0.0f;
  gyroBiasZ_ = 0.0f;

  Serial.print("IMU: begin SDA=");
  Serial.print(Pins::IMU_SDA);
  Serial.print(" SCL=");
  Serial.print(Pins::IMU_SCL);
  Serial.print(" INT=");
  Serial.println(Pins::IMU_INT);

  Wire.begin(Pins::IMU_SDA, Pins::IMU_SCL);
  Wire.setClock(400000);
  pinMode(Pins::IMU_INT, INPUT);

  scanBus();

  if (probeAddress(kAddressLow)) {
    address_ = kAddressLow;
  } else if (probeAddress(kAddressHigh)) {
    address_ = kAddressHigh;
  } else {
    Serial.println("IMU: missing, expected 0x68 or 0x69");
    return false;
  }

  if (!readRegister(kRegisterWhoAmI, whoAmI_)) {
    Serial.println("IMU: WHO_AM_I read failed");
    return false;
  }

  Serial.print("IMU: found address 0x");
  Serial.print(address_, HEX);
  Serial.print(" whoami 0x");
  Serial.println(whoAmI_, HEX);

  if (!isSupportedWhoAmI(whoAmI_)) {
    Serial.println("IMU: unsupported WHO_AM_I");
    return false;
  }

  if (!configureDevice()) {
    return false;
  }

  ready_ = readSample();
  Serial.println(ready_ ? "IMU: ready" : "IMU: first sample failed");
  return ready_;
}

void ImuDriver::update(uint32_t now) {
  if (!ready_) {
    if (now - lastLogMs_ >= kRetryIntervalMs) {
      lastLogMs_ = now;
      Serial.println("IMU: offline, retrying");
      scanBus();

      if (probeAddress(kAddressLow)) {
        address_ = kAddressLow;
      } else if (probeAddress(kAddressHigh)) {
        address_ = kAddressHigh;
      } else {
        Serial.println("IMU: still missing, expected 0x68 or 0x69");
        return;
      }

      if (!readRegister(kRegisterWhoAmI, whoAmI_)) {
        Serial.println("IMU: retry WHO_AM_I read failed");
        return;
      }

      Serial.print("IMU: retry found address 0x");
      Serial.print(address_, HEX);
      Serial.print(" whoami 0x");
      Serial.println(whoAmI_, HEX);

      if (!isSupportedWhoAmI(whoAmI_)) {
        Serial.println("IMU: retry unsupported WHO_AM_I");
        return;
      }

      if (!configureDevice()) {
        return;
      }
      ready_ = readSample();
      Serial.println(ready_ ? "IMU: retry ready" : "IMU: retry sample failed");
    }
    return;
  }

  if (now - lastSampleMs_ >= kSampleIntervalMs) {
    if (!readSample()) {
      ready_ = false;
      Serial.println("IMU: read failed, marked offline");
      return;
    }
  }

  if (now - lastLogMs_ >= kLogIntervalMs) {
    lastLogMs_ = now;
    logSample();
  }
}

bool ImuDriver::isReady() const {
  return ready_;
}

uint8_t ImuDriver::address() const {
  return address_;
}

uint8_t ImuDriver::whoAmI() const {
  return whoAmI_;
}

const ImuSample &ImuDriver::lastSample() const {
  return lastSample_;
}

const ImuPose &ImuDriver::pose() const {
  return pose_;
}

bool ImuDriver::configureDevice() {
  if (!writeRegister(kRegisterPowerManagement1, 0x00)) {
    Serial.println("IMU: wake failed");
    return false;
  }
  delay(100);

  writeRegister(kRegisterSampleRate, 0x04);
  writeRegister(kRegisterConfig, 0x03);
  writeRegister(kRegisterGyroConfig, 0x00);
  writeRegister(kRegisterAccelConfig, 0x00);
  resetPose();
  return calibrateGyroBias();
}

bool ImuDriver::calibrateGyroBias() {
  int32_t gyroXSum = 0;
  int32_t gyroYSum = 0;
  int32_t gyroZSum = 0;
  uint8_t samples = 0;
  uint8_t buffer[14];

  Serial.println("IMU: keep still for gyro calibration");
  for (uint8_t index = 0; index < kGyroCalibrationSamples; ++index) {
    if (readBytes(kRegisterAccelXHigh, buffer, sizeof(buffer))) {
      gyroXSum += readSigned16(buffer, 8);
      gyroYSum += readSigned16(buffer, 10);
      gyroZSum += readSigned16(buffer, 12);
      ++samples;
    }
    delay(5);
  }

  if (samples == 0) {
    Serial.println("IMU: gyro calibration failed");
    return false;
  }

  gyroBiasX_ = static_cast<float>(gyroXSum) / samples;
  gyroBiasY_ = static_cast<float>(gyroYSum) / samples;
  gyroBiasZ_ = static_cast<float>(gyroZSum) / samples;
  pose_.calibrated = true;

  Serial.print("IMU: gyro bias ");
  Serial.print(gyroBiasX_);
  Serial.print(",");
  Serial.print(gyroBiasY_);
  Serial.print(",");
  Serial.println(gyroBiasZ_);
  return true;
}

void ImuDriver::resetPose() {
  pose_ = ImuPose();
  lastPoseUpdateMs_ = 0;
}

void ImuDriver::scanBus() {
  Serial.println("IMU: I2C scan start");

  bool foundAny = false;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      foundAny = true;
      Serial.print("IMU: I2C device 0x");
      Serial.println(address, HEX);
    }
  }

  if (!foundAny) {
    Serial.println("IMU: I2C scan found no devices");
  }
}

bool ImuDriver::probeAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool ImuDriver::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address_);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool ImuDriver::readRegister(uint8_t reg, uint8_t &value) {
  return readBytes(reg, &value, 1);
}

bool ImuDriver::readBytes(uint8_t reg, uint8_t *buffer, uint8_t length) {
  Wire.beginTransmission(address_);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t readLength = Wire.requestFrom(address_, length);
  if (readLength != length) {
    return false;
  }

  for (uint8_t index = 0; index < length; ++index) {
    buffer[index] = Wire.read();
  }
  return true;
}

bool ImuDriver::readSample() {
  uint8_t buffer[14];
  if (!readBytes(kRegisterAccelXHigh, buffer, sizeof(buffer))) {
    lastSample_.valid = false;
    return false;
  }

  lastSample_.accelX = readSigned16(buffer, 0);
  lastSample_.accelY = readSigned16(buffer, 2);
  lastSample_.accelZ = readSigned16(buffer, 4);
  lastSample_.temperature = readSigned16(buffer, 6);
  lastSample_.gyroX = readSigned16(buffer, 8);
  lastSample_.gyroY = readSigned16(buffer, 10);
  lastSample_.gyroZ = readSigned16(buffer, 12);
  lastSample_.valid = true;
  const uint32_t now = millis();
  updatePose(now);
  lastSampleMs_ = now;
  return true;
}

void ImuDriver::updatePose(uint32_t now) {
  if (!lastSample_.valid) {
    return;
  }

  const float accelX = static_cast<float>(lastSample_.accelX);
  const float accelY = static_cast<float>(lastSample_.accelY);
  const float accelZ = static_cast<float>(lastSample_.accelZ);
  const float accelRollDeg = atan2f(accelY, accelZ) * RAD_TO_DEG;
  const float accelPitchDeg = atan2f(-accelX, sqrtf(accelY * accelY + accelZ * accelZ)) * RAD_TO_DEG;

  if (!pose_.valid || lastPoseUpdateMs_ == 0) {
    pose_.rollDeg = accelRollDeg;
    pose_.pitchDeg = accelPitchDeg;
    pose_.yawDeg = 0.0f;
    pose_.valid = true;
    lastPoseUpdateMs_ = now;
    return;
  }

  float dt = static_cast<float>(now - lastPoseUpdateMs_) / 1000.0f;
  lastPoseUpdateMs_ = now;
  if (dt <= 0.0f || dt > 0.2f) {
    dt = static_cast<float>(kSampleIntervalMs) / 1000.0f;
  }

  const float gyroRollRate = (static_cast<float>(lastSample_.gyroX) - gyroBiasX_) / kGyroSensitivity;
  const float gyroPitchRate = (static_cast<float>(lastSample_.gyroY) - gyroBiasY_) / kGyroSensitivity;
  const float gyroYawRate = (static_cast<float>(lastSample_.gyroZ) - gyroBiasZ_) / kGyroSensitivity;

  pose_.rollDeg = kComplementaryAlpha * (pose_.rollDeg + gyroRollRate * dt)
                  + (1.0f - kComplementaryAlpha) * accelRollDeg;
  pose_.pitchDeg = kComplementaryAlpha * (pose_.pitchDeg + gyroPitchRate * dt)
                   + (1.0f - kComplementaryAlpha) * accelPitchDeg;
  pose_.yawDeg = wrapDegrees((pose_.yawDeg + gyroYawRate * dt) * 0.999f);
  pose_.valid = true;
}

void ImuDriver::logSample() const {
  if (!lastSample_.valid) {
    return;
  }

  Serial.print("IMU: acc ");
  Serial.print(lastSample_.accelX);
  Serial.print(",");
  Serial.print(lastSample_.accelY);
  Serial.print(",");
  Serial.print(lastSample_.accelZ);
  Serial.print(" gyro ");
  Serial.print(lastSample_.gyroX);
  Serial.print(",");
  Serial.print(lastSample_.gyroY);
  Serial.print(",");
  Serial.print(lastSample_.gyroZ);
  Serial.print(" pose ");
  Serial.print(pose_.rollDeg);
  Serial.print(",");
  Serial.print(pose_.pitchDeg);
  Serial.print(",");
  Serial.println(pose_.yawDeg);
}
