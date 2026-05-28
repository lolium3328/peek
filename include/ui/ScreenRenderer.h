#pragma once

#include "drivers/DisplayDriver.h"
#include "ui/ScreenTypes.h"

class ScreenRenderer {
public:
  explicit ScreenRenderer(DisplayDriver &display);

  void renderBoot(const BootScreenModel &model);
  void renderHome(const HomeScreenModel &model);
  void renderHomeFrame(const HomeScreenModel &model);
  void renderStatus(const StatusScreenModel &model);

private:
  void drawTopStatus(const HomeScreenModel &model);
  void clearPetArea();
  void drawPetCube(const HomeScreenModel &model);
  void drawWeatherChip(int16_t x, const char *label, const char *weather);
  void drawConnectionDots(bool wifiConnected, bool backendConnected);
  void drawBottomHint(const char *hintText);
  void drawStatusPill(int16_t x, int16_t y, const char *text, uint16_t color);
  void drawTinyBattery(int16_t x, int16_t y, uint8_t percent, uint16_t color);

  DisplayDriver &display_;
};
