#include "app/AppController.h"

#include <Arduino.h>

namespace {
constexpr uint32_t kHomeFrameIntervalMs = 75;
}

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

  Serial.println("Button input start");
  showText(0);
  lastTouchMs_ = millis();
}

void AppController::loop() {
  const uint32_t now = millis();
  imu_.update(now);

  if (!statusVisible_ && (now - lastHomeRenderMs_ >= kHomeFrameIntervalMs)) {
    renderHomeFrame();
  }

  const TouchEvent event = touch_.update(now);
  if (!event.sampled) {
    return;
  }

  Serial.print("Button = ");
  Serial.println(event.pressed ? "down" : "up");

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
  statusVisible_ = false;
  renderHomeText(pet_.currentText(), pet_.isSleeping() ? "sleeping" : "tap / hold");
}

void AppController::renderHomeText(const char *text, const char *hintText) {
  const ImuPose &pose = imu_.pose();
  HomeScreenModel model;
  model.primaryText = pose.valid ? text : "imu?";
  model.hintText = hintText;
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.wifiConnected = false;
  model.backendConnected = false;
  model.poseAlert = false;
  model.cubeVisible = pose.valid;
  model.cubeRollDeg = pose.rollDeg;
  model.cubePitchDeg = pose.pitchDeg;
  model.cubeYawDeg = pose.yawDeg;
  screen_.renderHome(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderHomeFrame() {
  const ImuPose &pose = imu_.pose();
  HomeScreenModel model;
  model.primaryText = pose.valid ? pet_.currentText() : "imu?";
  model.cubeVisible = pose.valid;
  model.cubeRollDeg = pose.rollDeg;
  model.cubePitchDeg = pose.pitchDeg;
  model.cubeYawDeg = pose.yawDeg;
  screen_.renderHomeFrame(model);
  lastHomeRenderMs_ = millis();
}

void AppController::renderStatus() {
  const ImuPose &pose = imu_.pose();
  StatusScreenModel model;
  model.buttonPressed = touch_.isPressed();
  model.wifiRssi = 0;
  model.localBatteryPercent = 92;
  model.peerBatteryPercent = 79;
  model.backendConnected = false;
  model.imuReady = imu_.isReady();
  model.imuAddress = imu_.address();
  model.imuAccelZ = imu_.lastSample().accelZ;
  model.imuRollDeg = pose.rollDeg;
  model.imuPitchDeg = pose.pitchDeg;
  screen_.renderStatus(model);
}

void AppController::handleCompletedClick() {
  statusVisible_ = false;
  pet_.advanceAfterClick();
  renderHomeText(pet_.currentText(), "short press");

  Serial.print("Click -> ");
  Serial.println(pet_.currentText());
}

void AppController::handleLongPress() {
  statusVisible_ = true;
  pet_.wakeForLongPress();
  renderStatus();

  Serial.println("Long press -> status");
}
