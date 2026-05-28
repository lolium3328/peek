#pragma once

#include <stdint.h>

class Arduino_DataBus;
class Arduino_GFX;

class DisplayDriver {
public:
  DisplayDriver();

  bool begin();
  void drawTextCentered(const char *text);
  void setBatteryBars(uint8_t leftPercent, uint8_t rightPercent);

private:
  void drawBatteryBars();
  void drawBatteryArc(bool leftSide, uint8_t percent);
  void drawArcSegment(int16_t startDeg, int16_t sweepDeg, uint16_t color, uint8_t thickness);

  Arduino_DataBus *bus_;
  Arduino_GFX *gfx_;
  uint8_t leftBatteryPercent_ = 92;
  uint8_t rightBatteryPercent_ = 79;
};
