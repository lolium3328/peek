#include "services/FileSystemService.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace {

const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64CharIndex(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

String encodeBase64(const String &input) {
  if (input.length() == 0) return "";

  const size_t inputLen = input.length();
  const size_t outputLen = 4 * ((inputLen + 2) / 3);
  String result;
  result.reserve(outputLen);

  for (size_t i = 0; i < inputLen; i += 3) {
    const uint8_t b0 = static_cast<uint8_t>(input[i]);
    const uint8_t b1 = (i + 1 < inputLen) ? static_cast<uint8_t>(input[i + 1]) : 0;
    const uint8_t b2 = (i + 2 < inputLen) ? static_cast<uint8_t>(input[i + 2]) : 0;

    result += kBase64Chars[b0 >> 2];
    result += kBase64Chars[((b0 & 0x03) << 4) | (b1 >> 4)];
    result += (i + 1 < inputLen) ? kBase64Chars[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
    result += (i + 2 < inputLen) ? kBase64Chars[b2 & 0x3F] : '=';
  }

  return result;
}

String decodeBase64(const String &input) {
  const size_t inputLen = input.length();
  if (inputLen == 0) return "";

  const size_t outputLen = 3 * (inputLen / 4);
  String result;
  result.reserve(outputLen);

  for (size_t i = 0; i + 3 < inputLen; i += 4) {
    const int c0 = base64CharIndex(input[i]);
    const int c1 = base64CharIndex(input[i + 1]);
    const int c2 = base64CharIndex(input[i + 2]);
    const int c3 = base64CharIndex(input[i + 3]);

    if (c0 < 0 || c1 < 0) break;

    result += static_cast<char>((c0 << 2) | (c1 >> 4));

    if (c2 >= 0 && input[i + 2] != '=') {
      result += static_cast<char>(((c1 & 0x0F) << 4) | (c2 >> 2));
    }

    if (c3 >= 0 && input[i + 3] != '=') {
      result += static_cast<char>(((c2 & 0x03) << 6) | c3);
    }
  }

  return result;
}

} // namespace

void FileSystemService::begin() {
  Serial.setTimeout(100);
}

void FileSystemService::loop() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n') {
      processLine(buffer_);
      buffer_ = "";
    } else if (c != '\r') {
      buffer_ += c;
    }
  }
}

void FileSystemService::processLine(const String &line) {
  if (line.startsWith(">WRITE ")) {
    const int pathStart = 7;
    const int spaceIdx = line.indexOf(' ', pathStart);
    if (spaceIdx < 0) {
      respond("ERR invalid WRITE format");
      return;
    }
    const String path = line.substring(pathStart, spaceIdx);
    const String base64Data = line.substring(spaceIdx + 1);
    handleWrite(path, base64Data);
  } else if (line.startsWith(">READ ")) {
    const String path = line.substring(6);
    handleRead(path);
  } else if (line == ">STAT") {
    handleStat();
  }
}

void FileSystemService::handleRead(const String &path) {
  if (path.length() == 0) {
    respond("ERR path required");
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    respond("ERR file not found");
    return;
  }

  String content = file.readString();
  file.close();

  const String encoded = encodeBase64(content);
  respond("OK " + encoded);
}

void FileSystemService::handleWrite(const String &path, const String &base64Data) {
  if (path.length() == 0) {
    respond("ERR path required");
    return;
  }

  const String data = decodeBase64(base64Data);

  File file = LittleFS.open(path, "w");
  if (!file) {
    respond("ERR open failed");
    return;
  }

  const size_t written = file.print(data);
  file.close();

  if (written == data.length()) {
    respond("OK");
  } else {
    respond("ERR write failed");
  }
}

void FileSystemService::handleStat() {
  const size_t total = LittleFS.totalBytes();
  const size_t used = LittleFS.usedBytes();
  const size_t free = total >= used ? total - used : 0;

  String resp = "OK total=";
  resp += total;
  resp += " used=";
  resp += used;
  resp += " free=";
  resp += free;
  respond(resp);
}

void FileSystemService::respond(const String &msg) {
  Serial.print(">");
  Serial.println(msg);
}
