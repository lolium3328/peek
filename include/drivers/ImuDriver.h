#pragma once

#include <stdint.h>

struct ImuSample {
  int16_t accelX = 0;
  int16_t accelY = 0;
  int16_t accelZ = 0;
  int16_t temperature = 0;
  int16_t gyroX = 0;
  int16_t gyroY = 0;
  int16_t gyroZ = 0;
  bool valid = false;
};

struct ImuPose {
  float rollDeg = 0.0f;
  float pitchDeg = 0.0f;
  float yawDeg = 0.0f;
  bool valid = false;
  bool calibrated = false;
};

class ImuDriver {
public:
  bool begin();
  void update(uint32_t now);

  bool isReady() const;
  uint8_t address() const;
  uint8_t whoAmI() const;
  const ImuSample &lastSample() const;
  const ImuPose &pose() const;

private:
  bool configureDevice();
  bool calibrateGyroBias();
  void resetPose();
  void updatePose(uint32_t now);
  void scanBus();
  bool probeAddress(uint8_t address);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t &value);
  bool readBytes(uint8_t reg, uint8_t *buffer, uint8_t length);
  bool readSample();

  bool ready_ = false;
  uint8_t address_ = 0;
  uint8_t whoAmI_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastPoseUpdateMs_ = 0;
  float gyroBiasX_ = 0.0f;
  float gyroBiasY_ = 0.0f;
  float gyroBiasZ_ = 0.0f;
  ImuSample lastSample_;
  ImuPose pose_;
};
