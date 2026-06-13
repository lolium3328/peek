#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

class File {
public:
  File() = default;
  explicit File(const char *path) : stream_(path, std::ios::binary) {}

  size_t read(uint8_t *buffer, size_t size) {
    if (!stream_) {
      return 0;
    }
    stream_.read(reinterpret_cast<char *>(buffer), static_cast<std::streamsize>(size));
    return static_cast<size_t>(stream_.gcount());
  }

  bool seek(uint32_t position) {
    if (!stream_) {
      return false;
    }
    stream_.seekg(static_cast<std::streamoff>(position), std::ios::beg);
    return bool(stream_);
  }

  void close() {
    stream_.close();
  }

  explicit operator bool() const {
    return bool(stream_);
  }

private:
  std::ifstream stream_;
};

class LittleFSClass {
public:
  File open(const char *path, const char *) {
    return File(path);
  }
};

inline LittleFSClass LittleFS;
