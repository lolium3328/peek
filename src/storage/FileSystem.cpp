#include "storage/FileSystem.h"

#include <Arduino.h>
#include <LittleFS.h>

bool FileSystem::begin() {
  ready_ = LittleFS.begin(true);
  if (!ready_) {
    Serial.println("LittleFS mount failed");
    return false;
  }

  Serial.print("LittleFS ready used/total ");
  Serial.print(LittleFS.usedBytes());
  Serial.print("/");
  Serial.println(LittleFS.totalBytes());
  return true;
}

bool FileSystem::isReady() const {
  return ready_;
}

size_t FileSystem::totalBytes() const {
  return ready_ ? LittleFS.totalBytes() : 0;
}

size_t FileSystem::usedBytes() const {
  return ready_ ? LittleFS.usedBytes() : 0;
}
