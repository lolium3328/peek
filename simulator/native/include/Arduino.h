#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef PROGMEM
#define PROGMEM
#endif

struct GFXglyph {
  uint16_t bitmapOffset;
  uint8_t width;
  uint8_t height;
  uint8_t xAdvance;
  int8_t xOffset;
  int8_t yOffset;
};

struct GFXfont {
  uint8_t *bitmap;
  GFXglyph *glyph;
  uint8_t first;
  uint8_t last;
  uint8_t yAdvance;
};

constexpr float DEG_TO_RAD = 0.017453292519943295769f;

inline uint32_t millis() {
  return 0;
}
