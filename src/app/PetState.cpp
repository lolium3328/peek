#include "app/PetState.h"

namespace {
const char *kDisplayTexts[] = {
    "zzz...",
    "boring",
    "pet me",
    "hmm...",
    "oops",
    "hehe",
    "lonely",
    "sleepy",
    "love",
    "again!",
    "one more",
    "miss u",
    "play?",
    "ok",
    "yay",
    "fun",
    "pet me",
    "wow"};

constexpr size_t kDisplayTextCount = sizeof(kDisplayTexts) / sizeof(kDisplayTexts[0]);
constexpr size_t kFirstActiveTextIndex = 1;
} // namespace

void PetState::showText(size_t index) {
  currentTextIndex_ = index % kDisplayTextCount;
  sleeping_ = (currentTextIndex_ == 0);
}

void PetState::advanceAfterClick() {
  size_t nextIndex = currentTextIndex_ + 1;
  if (nextIndex >= kDisplayTextCount) {
    nextIndex = kFirstActiveTextIndex;
  }
  showText(nextIndex);
}

void PetState::wakeForLongPress() {
  sleeping_ = false;
}

const char *PetState::currentText() const {
  return kDisplayTexts[currentTextIndex_];
}

bool PetState::isSleeping() const {
  return sleeping_;
}
