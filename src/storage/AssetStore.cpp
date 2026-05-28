#include "storage/AssetStore.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace {
constexpr const char *kAssetsDir = "/assets";
constexpr const char *kManifestPath = "/assets/manifest.json";
constexpr const char *kDefaultManifestJson =
    "{\"version\":1,\"revision\":0,\"assets\":[]}";
}

bool AssetStore::begin(const FileSystem &fileSystem) {
  ready_ = fileSystem.isReady();
  if (!ready_) {
    manifestJson_ = kDefaultManifestJson;
    return false;
  }

  LittleFS.mkdir(kAssetsDir);
  if (!ensureDefaultManifest()) {
    manifestJson_ = kDefaultManifestJson;
    return false;
  }

  return loadManifest();
}

const String &AssetStore::manifestJson() const {
  return manifestJson_;
}

bool AssetStore::saveManifestJson(const String &json) {
  if (json.length() == 0) {
    return false;
  }

  manifestJson_ = json;
  if (!ready_) {
    return false;
  }

  File file = LittleFS.open(kManifestPath, "w");
  if (!file) {
    Serial.println("Asset manifest save failed: open");
    return false;
  }

  file.print(json);
  file.close();
  Serial.println("Asset manifest saved to LittleFS");
  return true;
}

bool AssetStore::hasManifest() const {
  return manifestJson_.length() > 0;
}

bool AssetStore::ensureDefaultManifest() {
  if (LittleFS.exists(kManifestPath)) {
    return true;
  }

  File file = LittleFS.open(kManifestPath, "w");
  if (!file) {
    Serial.println("Default asset manifest create failed");
    return false;
  }
  file.print(kDefaultManifestJson);
  file.close();
  return true;
}

bool AssetStore::loadManifest() {
  File file = LittleFS.open(kManifestPath, "r");
  if (!file) {
    Serial.println("Asset manifest load failed: open");
    return false;
  }

  manifestJson_ = file.readString();
  file.close();
  if (manifestJson_.length() == 0) {
    manifestJson_ = kDefaultManifestJson;
  }

  Serial.print("Asset manifest loaded bytes ");
  Serial.println(manifestJson_.length());
  return true;
}
