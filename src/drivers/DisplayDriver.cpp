#include "drivers/DisplayDriver.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "Pins.h"
#include "../../front/magicalmond_ogyg820pt7b.h"

DisplayDriver::DisplayDriver()
    : bus_(new Arduino_ESP32SPI(
          Pins::TFT_DC,
          Pins::TFT_CS,
          Pins::TFT_SCK,
          Pins::TFT_MOSI,
          GFX_NOT_DEFINED)),
      gfx_(new Arduino_GC9A01(
          bus_,
          Pins::TFT_RST,
          0,
          true,
          240,
          240)) {}

bool DisplayDriver::begin() {
  return gfx_->begin();
}

void DisplayDriver::drawTextCentered(const char *text) {
  gfx_->fillScreen(BLACK);
  gfx_->setFont(&magicalmond_ogyg820pt7b);
  gfx_->setTextColor(WHITE);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  gfx_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  const int16_t x = (240 - static_cast<int16_t>(w)) / 2 - x1;
  const int16_t y = (240 - static_cast<int16_t>(h)) / 2 - y1;
  gfx_->setCursor(x, y);
  gfx_->println(text);
}
