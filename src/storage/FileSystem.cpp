#include "storage/FileSystem.h"

#include <Arduino.h>
#include <LittleFS.h>

bool FileSystem::begin() {
  ready_ = LittleFS.begin(true);
  if (!ready_) {
    return false;
  }

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
