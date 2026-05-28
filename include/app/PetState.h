#pragma once

#include <stddef.h>

class PetState {
public:
  void showText(size_t index);
  void advanceAfterClick();
  void wakeForLongPress();

  const char *currentText() const;
  bool isSleeping() const;

private:
  size_t currentTextIndex_ = 0;
  bool sleeping_ = true;
};
