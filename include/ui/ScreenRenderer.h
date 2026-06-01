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
  void renderRadialMenu(const RadialMenuModel &model);
  void renderRadialCalibration(const RadialCalibrationModel &model);

private:
  enum class HomeContentKind {
    None,
    Cube,
    Animation,
    Text,
  };

  void resetHomeCache();
  void drawHomeChrome(const HomeScreenModel &model);
  bool updateHomeChrome(const HomeScreenModel &model);
  void renderHomeContent(const HomeScreenModel &model, bool force);
  HomeContentKind homeContentKind(const HomeScreenModel &model) const;
  bool textChanged(const char *cached, const char *current) const;
  void copyText(char *target, uint8_t targetSize, const char *source);
  void drawTopStatus(const HomeScreenModel &model);
  void clearPetArea();
  void drawPetCube(const HomeScreenModel &model);
  void drawPetCubeBuffered(const HomeScreenModel &model);
  void clearPetBuffer(uint16_t color);
  void putPetPixel(int16_t x, int16_t y, uint16_t color);
  void drawPetBufferLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  bool drawPetAnimation(const HomeScreenModel &model);
  void drawWeatherChip(int16_t x, const char *label, const char *weather);
  void drawConnectionDots(bool wifiConnected, bool backendConnected);
  void drawBottomHint(const char *hintText);
  void drawStatusPill(int16_t x, int16_t y, const char *text, uint16_t color);
  void drawTinyBattery(int16_t x, int16_t y, uint8_t percent, uint16_t color);
  void drawRadialSector(float centerDeg, uint16_t color);
  void drawRadialCursor(float cursorX, float cursorY, uint16_t color);
  const char *radialItemLabel(RadialMenuItem item) const;
  uint16_t radialItemColor(RadialMenuItem item, bool selected) const;

  DisplayDriver &display_;
  bool homeChromeDrawn_ = false;
  HomeContentKind lastHomeContentKind_ = HomeContentKind::None;
  uint8_t lastLocalBatteryPercent_ = 0;
  uint8_t lastPeerBatteryPercent_ = 0;
  bool lastWifiConnected_ = false;
  bool lastBackendConnected_ = false;
  bool lastPoseAlert_ = false;
  char lastPrimaryText_[32] = "";
  char lastHintText_[40] = "";
  char lastLocalWeather_[16] = "";
  char lastPeerWeather_[16] = "";
  char lastLocalLabel_[8] = "";
  char lastPeerLabel_[8] = "";
  char lastAnimationPath_[80] = "";
};
