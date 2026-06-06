#pragma once

#include <stdint.h>

class MotorDriver {
public:
  enum class Effect : uint8_t {
    StrongClick = 1,
    SoftClick = 10,
    DoubleClick = 14,
    Tick = 47,
  };

  bool begin();
  bool isReady() const;
  void play(Effect effect);
  void play(uint8_t effect);

private:
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t &value);

  bool ready_ = false;
};
