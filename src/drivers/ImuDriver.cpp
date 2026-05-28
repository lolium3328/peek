#include "drivers/ImuDriver.h"

#include <Arduino.h>
#include <Wire.h>

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

int16_t readSigned16(const uint8_t *buffer, uint8_t offset) {
  return static_cast<int16_t>((static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1]);
}

bool isSupportedWhoAmI(uint8_t whoAmI) {
  return whoAmI == 0x68 || whoAmI == 0x70 || whoAmI == 0x71;
}
} // namespace

bool ImuDriver::begin() {
  ready_ = false;
  address_ = 0;
  whoAmI_ = 0;
  lastSample_ = ImuSample();
  lastSampleMs_ = 0;
  lastLogMs_ = 0;

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

  if (!writeRegister(kRegisterPowerManagement1, 0x00)) {
    Serial.println("IMU: wake failed");
    return false;
  }
  delay(100);

  writeRegister(kRegisterSampleRate, 0x04);
  writeRegister(kRegisterConfig, 0x03);
  writeRegister(kRegisterGyroConfig, 0x00);
  writeRegister(kRegisterAccelConfig, 0x00);

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

      writeRegister(kRegisterPowerManagement1, 0x00);
      delay(100);
      writeRegister(kRegisterSampleRate, 0x04);
      writeRegister(kRegisterConfig, 0x03);
      writeRegister(kRegisterGyroConfig, 0x00);
      writeRegister(kRegisterAccelConfig, 0x00);
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
  lastSampleMs_ = millis();
  return true;
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
  Serial.println(lastSample_.gyroZ);
}
