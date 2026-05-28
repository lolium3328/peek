#pragma once

class Arduino_DataBus;
class Arduino_GFX;

class DisplayDriver {
public:
  DisplayDriver();

  bool begin();
  void drawTextCentered(const char *text);

private:
  Arduino_DataBus *bus_;
  Arduino_GFX *gfx_;
};
