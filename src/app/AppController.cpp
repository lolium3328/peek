#include "app/AppController.h"

#include <Arduino.h>

void AppController::begin() {
  Serial.begin(115200);

  if (!display_.begin()) {
    Serial.println("GC9A01 init failed");
    while (true) {
      delay(1000);
    }
  }

  touch_.begin(config_);

  Serial.println("FSR402 test start");
  showText(0);
  lastTouchMs_ = millis();
}

void AppController::loop() {
  const uint32_t now = millis();
  const TouchEvent event = touch_.update(now);
  if (!event.sampled) {
    return;
  }

  Serial.print("AO = ");
  Serial.println(event.analogValue);

  if (event.pressed) {
    lastTouchMs_ = now;
  }

  if (event.type == TouchEventType::LongPress) {
    lastTouchMs_ = now;
    handleLongPress();
  } else if (event.type == TouchEventType::ShortPress) {
    lastTouchMs_ = now;
    handleCompletedClick();
  }

  if (!pet_.isSleeping() && !touch_.isPressed() && (now - lastTouchMs_ >= config_.sleepTimeoutMs)) {
    showText(0);
    Serial.println("Sleep timeout -> zzz...");
  }
}

void AppController::showText(size_t index) {
  pet_.showText(index);
  display_.drawTextCentered(pet_.currentText());
}

void AppController::handleCompletedClick() {
  pet_.advanceAfterClick();
  display_.drawTextCentered(pet_.currentText());

  Serial.print("Click -> ");
  Serial.println(pet_.currentText());
}

void AppController::handleLongPress() {
  pet_.wakeForLongPress();
  display_.drawTextCentered(config_.longPressText);

  Serial.print("Long press -> ");
  Serial.println(config_.longPressText);
}
