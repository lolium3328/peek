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

class ImuDriver {
public:
  bool begin();
  void update(uint32_t now);

  bool isReady() const;
  uint8_t address() const;
  uint8_t whoAmI() const;
  const ImuSample &lastSample() const;

private:
  void scanBus();
  bool probeAddress(uint8_t address);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t &value);
  bool readBytes(uint8_t reg, uint8_t *buffer, uint8_t length);
  bool readSample();
  void logSample() const;

  bool ready_ = false;
  uint8_t address_ = 0;
  uint8_t whoAmI_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastLogMs_ = 0;
  ImuSample lastSample_;
};
