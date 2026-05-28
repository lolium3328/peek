#pragma once

#include <stddef.h>
#include <stdint.h>

class FileSystem {
public:
  bool begin();

  bool isReady() const;
  size_t totalBytes() const;
  size_t usedBytes() const;

private:
  bool ready_ = false;
};
