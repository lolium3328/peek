#pragma once

#include <stdint.h>

class Arduino_DataBus;
class Arduino_GFX;

enum class DisplayTextStyle {
  Small,
  Primary
};

class DisplayDriver {
public:
  DisplayDriver();

  bool begin();
  void clear(uint16_t color);
  void drawTextCentered(const char *text);
  void drawTextCentered(const char *text, int16_t centerY, DisplayTextStyle style, uint16_t color);
  void drawText(const char *text, int16_t x, int16_t y, DisplayTextStyle style, uint16_t color);
  void setBatteryBars(uint8_t leftPercent, uint8_t rightPercent);
  void drawBatteryBars();
  void drawBatteryBars(uint8_t leftPercent, uint8_t rightPercent);
  void drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color);
  void fillCircle(int16_t x, int16_t y, int16_t radius, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, uint16_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
  void drawRgb565Bitmap(int16_t x, int16_t y, const uint16_t *pixels, int16_t width, int16_t height);

private:
  void applyTextStyle(DisplayTextStyle style, uint16_t color);
  void drawBatteryArc(bool leftSide, uint8_t percent);
  void drawArcSegment(int16_t startDeg, int16_t sweepDeg, uint16_t color, uint8_t thickness);
  void drawArcLine(int16_t startDeg, int16_t sweepDeg, int16_t radius, uint16_t color);

  Arduino_DataBus *bus_;
  Arduino_GFX *gfx_;
  uint8_t leftBatteryPercent_ = 92;
  uint8_t rightBatteryPercent_ = 79;
};
