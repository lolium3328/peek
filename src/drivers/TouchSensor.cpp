#include "drivers/TouchSensor.h"

#include <Arduino.h>

#include "Pins.h"

void TouchSensor::begin(const DeviceConfig &config) {
  config_ = config;
  pinMode(Pins::FSR_AO, INPUT);
  analogReadResolution(12);
}

TouchEvent TouchSensor::update(uint32_t now) {
  TouchEvent event;
  event.at = now;

  if (now - lastSampleMs_ < config_.touchSampleIntervalMs) {
    return event;
  }

  lastSampleMs_ = now;
  lastValue_ = analogRead(Pins::FSR_AO);
  event.sampled = true;
  event.analogValue = lastValue_;

  if (lastValue_ <= config_.touchPressThreshold) {
    if (!pressInProgress_) {
      pressInProgress_ = true;
      longPressTriggered_ = false;
      pressStartMs_ = now;
    }

    event.pressed = true;
    if (!longPressTriggered_ && (now - pressStartMs_ >= config_.longPressMs)) {
      longPressTriggered_ = true;
      event.type = TouchEventType::LongPress;
    }
    return event;
  }

  if (pressInProgress_ && lastValue_ >= config_.touchIdleThreshold) {
    pressInProgress_ = false;
    const bool wasLongPress = longPressTriggered_;
    longPressTriggered_ = false;

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
