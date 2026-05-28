#include "app/AppController.h"

#include <Arduino.h>

AppController::AppController() : screen_(display_) {}

void AppController::begin() {
  Serial.begin(115200);

  if (!display_.begin()) {
    Serial.println("GC9A01 init failed");
    while (true) {
      delay(1000);
    }
  }

  touch_.begin(config_);
  const bool imuReady = imu_.begin();

  BootScreenModel bootModel;
  bootModel.title = "Peek";
  bootModel.message = imuReady ? "imu ok" : "imu missing";
  screen_.renderBoot(bootModel);
  delay(500);

  Serial.println("FSR402 test start");
  showText(0);
  lastTouchMs_ = millis();
}

void AppController::loop() {
  const uint32_t now = millis();
  imu_.update(now);

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
  renderHomeText(pet_.currentText(), pet_.isSleeping() ? "sleeping" : "tap / hold");
}

void AppController::renderHomeText(const char *text, const char *hintText) {
  HomeScreenModel model;
  model.primaryText = text;
  model.hintText = hintText;
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.wifiConnected = false;
  model.backendConnected = false;
  model.poseAlert = false;
  screen_.renderHome(model);
}

void AppController::renderStatus() {
  StatusScreenModel model;
  model.touchAnalog = touch_.lastValue();
  model.wifiRssi = 0;
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.backendConnected = false;
  model.imuReady = imu_.isReady();
  model.imuAddress = imu_.address();
  model.imuAccelZ = imu_.lastSample().accelZ;
  screen_.renderStatus(model);
}

void AppController::handleCompletedClick() {
  pet_.advanceAfterClick();
  renderHomeText(pet_.currentText(), "short press");

  Serial.print("Click -> ");
  Serial.println(pet_.currentText());
}

void AppController::handleLongPress() {
  pet_.wakeForLongPress();
  renderStatus();

  Serial.println("Long press -> status");
}
