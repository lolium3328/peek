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

bool FileSystem::readFile(const String &path, String &out) {
  if (!ready_) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  out = file.readString();
  file.close();
  return true;
}

bool FileSystem::writeFile(const String &path, const String &data) {
  if (!ready_) {
    return false;
  }

  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }

  const size_t written = file.print(data);
  file.close();
  return written == data.length();
}
