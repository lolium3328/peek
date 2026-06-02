#pragma once

#include <stdint.h>

class Arduino_DataBus;
class Arduino_GFX;

#ifdef PEEK_HOST_PREVIEW
struct HostDisplayAccess;
#endif

enum class DisplayTextStyle {
  Small,
  Primary
};

class DisplayDriver {
public:
  DisplayDriver();

  bool begin();
  void setSleep(bool sleeping);
  void clear(uint16_t color);
  void drawTextCentered(const char *text);
  void drawTextCentered(const char *text, int16_t centerY, DisplayTextStyle style, uint16_t color);
  void drawText(const char *text, int16_t x, int16_t y, DisplayTextStyle style, uint16_t color);
  void drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color);
  void fillCircle(int16_t x, int16_t y, int16_t radius, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, uint16_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
  void drawRgb565Bitmap(int16_t x, int16_t y, const uint16_t *pixels, int16_t width, int16_t height);

private:
#ifdef PEEK_HOST_PREVIEW
  friend struct HostDisplayAccess;
#endif

  void applyTextStyle(DisplayTextStyle style, uint16_t color);

  Arduino_DataBus *bus_;
  Arduino_GFX *gfx_;
  bool sleeping_ = false;
};
