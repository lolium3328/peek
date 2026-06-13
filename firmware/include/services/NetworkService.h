#pragma once

#include <WString.h>
#include <stdint.h>

#include "config/DeviceConfig.h"

class NetworkService {
public:
  void begin(const DeviceConfig &config);
  void loop(uint32_t now);

  bool isEnabled() const;
  bool isConnected() const;
  int32_t rssi() const;
  String ipAddress() const;

private:
  void startConnect(uint32_t now);

  const DeviceConfig *config_ = nullptr;
  bool enabled_ = false;
  uint32_t lastConnectAttemptMs_ = 0;
};
