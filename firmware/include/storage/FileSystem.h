#pragma once

#include <stddef.h>
#include <stdint.h>

#include <WString.h>

class FileSystem {
public:
  bool begin();

  bool isReady() const;
  size_t totalBytes() const;
  size_t usedBytes() const;
  bool readFile(const String &path, String &out);
  bool writeFile(const String &path, const String &data);

private:
  bool ready_ = false;
};
