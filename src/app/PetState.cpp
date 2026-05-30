#include "app/PetState.h"

namespace {
const char *kPetTexts[] = {
    "",
    "Pet 2",
    "Pet 3",
};

constexpr size_t kPetCount = sizeof(kPetTexts) / sizeof(kPetTexts[0]);
constexpr size_t kCubePetIndex = 0;
constexpr size_t kPet2Index = 1;
} // namespace

void PetState::reset() {
  currentPetIndex_ = kCubePetIndex;
  sleeping_ = true;
}

void PetState::advancePet() {
  currentPetIndex_ = (currentPetIndex_ + 1) % kPetCount;
  sleeping_ = false;
}

void PetState::wakeForLongPress() {
  sleeping_ = false;
}

const char *PetState::currentPetText() const {
  return kPetTexts[currentPetIndex_];
}

bool PetState::isCubePet() const {
  return currentPetIndex_ == kCubePetIndex;
}

bool PetState::isPet2() const {
  return currentPetIndex_ == kPet2Index;
}

bool PetState::isSleeping() const {
  return sleeping_;
}
