#pragma once

#include <WString.h>

#include "storage/FileSystem.h"

class AssetStore {
public:
  bool begin(const FileSystem &fileSystem);

  const String &manifestJson() const;
  bool saveManifestJson(const String &json);
  bool hasManifest() const;
  bool hasPet2Animation() const;
  const String &pet2AnimationPath() const;
  String localAnimationPath(const String &assetId) const;
  bool hasAnimationFile(const String &assetId, size_t expectedSize) const;
  bool canStoreAsset(size_t encodedSize, size_t reserveBytes) const;
  bool markPet2AnimationDownloaded(const String &assetId);

private:
  bool ensureDefaultManifest();
  bool loadManifest();
  void refreshPet2AnimationPath();

  bool ready_ = false;
  String manifestJson_;
  String pet2AssetId_;
  String pet2AnimationPath_;
};
