#pragma once

#include <WString.h>

#include "storage/FileSystem.h"

class LayoutStore {
public:
  bool begin(const FileSystem &fileSystem);

  const String &layoutJson() const;
  bool saveLayoutJson(const String &json);
  bool hasLayout() const;

private:
  bool ensureDefaultLayout();
  bool loadLayout();

  bool ready_ = false;
  String layoutJson_;
};
