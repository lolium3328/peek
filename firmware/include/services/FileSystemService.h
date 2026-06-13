#pragma once

#include <WString.h>

#include "storage/FileSystem.h"

class FileSystemService {
public:
  void begin();
  void loop();

private:
  void processLine(const String &line);
  void handleRead(const String &path);
  void handleWrite(const String &path, const String &base64Data);
  void handleStat();
  void respond(const String &msg);
  String base64Encode(const String &data);
  String base64Decode(const String &input);

  String buffer_;
  FileSystem *fs_ = nullptr;
};
