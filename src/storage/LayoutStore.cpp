#include "storage/LayoutStore.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace {
constexpr const char *kConfigDir = "/config";
constexpr const char *kLayoutPath = "/config/layout.json";
constexpr const char *kDefaultLayoutJson =
    "{\"version\":1,\"revision\":0,\"updatedAt\":0,\"screen\":{\"width\":240,\"height\":240,"
    "\"shape\":\"circle\"},\"components\":[{\"id\":\"pet\",\"type\":\"cube\",\"label\":\"Pet\","
    "\"x\":120,\"y\":116,\"scale\":1},{\"id\":\"batteryA\",\"type\":\"arc\",\"label\":\"A\","
    "\"x\":120,\"y\":120,\"radius\":106,\"startAngle\":136,\"endAngle\":224,"
    "\"binding\":\"battery.local\"},{\"id\":\"batteryB\",\"type\":\"arc\",\"label\":\"B\","
    "\"x\":120,\"y\":120,\"radius\":106,\"startAngle\":-44,\"endAngle\":44,"
    "\"binding\":\"battery.peer\"}]}";
}

bool LayoutStore::begin(const FileSystem &fileSystem) {
  ready_ = fileSystem.isReady();
  if (!ready_) {
    layoutJson_ = kDefaultLayoutJson;
    return false;
  }

  LittleFS.mkdir(kConfigDir);
  if (!ensureDefaultLayout()) {
    layoutJson_ = kDefaultLayoutJson;
    return false;
  }

  return loadLayout();
}

const String &LayoutStore::layoutJson() const {
  return layoutJson_;
}

bool LayoutStore::saveLayoutJson(const String &json) {
  if (json.length() == 0) {
    return false;
  }

  layoutJson_ = json;
  if (!ready_) {
    return false;
  }

  File file = LittleFS.open(kLayoutPath, "w");
  if (!file) {
    Serial.println("Layout save failed: open");
    return false;
  }

  file.print(json);
  file.close();
  Serial.println("Layout saved to LittleFS");
  return true;
}

bool LayoutStore::hasLayout() const {
  return layoutJson_.length() > 0;
}

bool LayoutStore::ensureDefaultLayout() {
  if (LittleFS.exists(kLayoutPath)) {
    return true;
  }

  File file = LittleFS.open(kLayoutPath, "w");
  if (!file) {
    Serial.println("Default layout create failed");
    return false;
  }
  file.print(kDefaultLayoutJson);
  file.close();
  return true;
}

bool LayoutStore::loadLayout() {
  File file = LittleFS.open(kLayoutPath, "r");
  if (!file) {
    Serial.println("Layout load failed: open");
    return false;
  }

  layoutJson_ = file.readString();
  file.close();
  if (layoutJson_.length() == 0) {
    layoutJson_ = kDefaultLayoutJson;
  }

  Serial.print("Layout loaded bytes ");
  Serial.println(layoutJson_.length());
  return true;
}
