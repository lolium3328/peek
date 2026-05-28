#pragma once

#include <stdint.h>

enum class TouchEventType {
  None,
  ShortPress,
  LongPress
};

struct TouchEvent {
  TouchEventType type = TouchEventType::None;
  bool sampled = false;
  bool pressed = false;
  int analogValue = 0;
  uint32_t at = 0;
};
