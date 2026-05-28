#pragma once

#include <stdint.h>

enum class TouchEventType {
  None,
  ShortPress,
  LongPress,
  ExtraLongPress
};

struct TouchEvent {
  TouchEventType type = TouchEventType::None;
  bool sampled = false;
  bool pressed = false;
  int inputValue = 1;
  uint32_t at = 0;
};
