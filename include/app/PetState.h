#pragma once

#include <stddef.h>

class PetState {
public:
  void reset();
  void advancePet();
  void wakeForLongPress();

  const char *currentPetText() const;
  bool isCubePet() const;
  bool isSleeping() const;

private:
  size_t currentPetIndex_ = 0;
  bool sleeping_ = true;
};
