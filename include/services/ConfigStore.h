#pragma once

#include <stdint.h>

#include "config/DeviceConfig.h"

class ConfigStore {
public:
  bool begin();

  DeviceConfig load() const;
  bool save(const DeviceConfig &config);
  bool reset();

private:
  String getStringValue(const char *key, const String &fallback) const;
  uint32_t getUIntValue(const char *key, uint32_t fallback) const;

  bool ready_ = false;
};
