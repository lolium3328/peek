#pragma once

#include <WString.h>

#include "storage/FileSystem.h"

class AssetStore {
public:
  bool begin(const FileSystem &fileSystem);

  const String &manifestJson() const;
  bool saveManifestJson(const String &json);
  bool hasManifest() const;

private:
  bool ensureDefaultManifest();
  bool loadManifest();

  bool ready_ = false;
  String manifestJson_;
};
