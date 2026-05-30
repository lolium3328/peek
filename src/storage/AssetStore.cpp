#include "storage/AssetStore.h"

#include <Arduino.h>
#include <ArduinoJson.h>
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
  refreshPet2AnimationPath();
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

bool AssetStore::hasPet2Animation() const {
  return pet2AnimationPath_.length() > 0;
}

const String &AssetStore::pet2AnimationPath() const {
  return pet2AnimationPath_;
}

String AssetStore::localAnimationPath(const String &assetId) const {
  return String(kAssetsDir) + "/" + assetId + ".pka";
}

bool AssetStore::hasAnimationFile(const String &assetId, size_t expectedSize) const {
  if (!ready_ || assetId.length() == 0) {
    return false;
  }

  File file = LittleFS.open(localAnimationPath(assetId), "r");
  if (!file) {
    return false;
  }
  const bool matches = expectedSize == 0 || file.size() == expectedSize;
  file.close();
  return matches;
}

bool AssetStore::canStoreAsset(size_t encodedSize, size_t reserveBytes) const {
  if (!ready_) {
    return false;
  }
  const size_t total = LittleFS.totalBytes();
  const size_t used = LittleFS.usedBytes();
  if (total <= used + reserveBytes) {
    return false;
  }
  return encodedSize <= total - used - reserveBytes;
}

bool AssetStore::markPet2AnimationDownloaded(const String &assetId) {
  if (assetId.length() == 0 || !hasAnimationFile(assetId, 0)) {
    return false;
  }

  pet2AssetId_ = assetId;
  pet2AnimationPath_ = localAnimationPath(assetId);
  return true;
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
  refreshPet2AnimationPath();

  Serial.print("Asset manifest loaded bytes ");
  Serial.println(manifestJson_.length());
  return true;
}

void AssetStore::refreshPet2AnimationPath() {
  pet2AssetId_ = "";
  pet2AnimationPath_ = "";

  JsonDocument doc;
  if (deserializeJson(doc, manifestJson_)) {
    return;
  }

  const char *assetId = doc["pet2AssetId"] | "";
  if (!assetId || assetId[0] == '\0') {
    return;
  }

  const String candidateId(assetId);
  if (!hasAnimationFile(candidateId, 0)) {
    return;
  }

  pet2AssetId_ = candidateId;
  pet2AnimationPath_ = localAnimationPath(candidateId);
}
