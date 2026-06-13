#include "drivers/TouchSensor.h"

#include <Arduino.h>

#include "Pins.h"

void TouchSensor::begin(const DeviceConfig &config) {
  config_ = config;
  pinMode(Pins::BUTTON, INPUT_PULLUP);
}

TouchEvent TouchSensor::update(uint32_t now) {
  TouchEvent event;
  event.at = now;

  if (now - lastSampleMs_ < config_.touchSampleIntervalMs) {
    return event;
  }

  lastSampleMs_ = now;
  const bool pressedNow = digitalRead(Pins::BUTTON) == LOW;
  lastValue_ = pressedNow ? 0 : 1;
  event.sampled = true;
  event.inputValue = lastValue_;

  if (pressedNow) {
    if (!pressInProgress_) {
      pressInProgress_ = true;
      longPressTriggered_ = false;
      extraLongPressTriggered_ = false;
      pressStartMs_ = now;
    }

    event.pressed = true;
    if (!extraLongPressTriggered_ && (now - pressStartMs_ >= config_.extraLongPressMs)) {
      extraLongPressTriggered_ = true;
      event.type = TouchEventType::ExtraLongPress;
    } else if (!longPressTriggered_ && (now - pressStartMs_ >= config_.longPressMs)) {
      longPressTriggered_ = true;
      event.type = TouchEventType::LongPress;
    }
    return event;
  }

  if (pressInProgress_) {
    pressInProgress_ = false;
    const bool wasLongPress = longPressTriggered_ || extraLongPressTriggered_;
    longPressTriggered_ = false;
    extraLongPressTriggered_ = false;

    if (!wasLongPress) {
      event.type = TouchEventType::ShortPress;
    }
  }

  event.pressed = pressInProgress_;
  return event;
}

bool TouchSensor::isPressed() const {
  return pressInProgress_;
}

int TouchSensor::lastValue() const {
  return lastValue_;
}
