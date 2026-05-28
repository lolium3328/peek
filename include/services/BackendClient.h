#pragma once

#include <WString.h>
#include <stdint.h>

#include "config/DeviceConfig.h"
#include "drivers/ImuDriver.h"
#include "services/NetworkService.h"
#include "storage/AssetStore.h"
#include "storage/LayoutStore.h"

class BackendClient {
public:
  void begin(const DeviceConfig &config, LayoutStore &layoutStore, AssetStore &assetStore);
  void loop(uint32_t now, const NetworkService &network, const ImuPose &pose, bool imuReady);

  bool isEnabled() const;
  bool isConnected(uint32_t now) const;
  int lastHttpStatus() const;

private:
  void syncNow(uint32_t now, const NetworkService &network, const ImuPose &pose, bool imuReady);
  String endpoint(const char *path) const;
  String buildSyncPayload(const NetworkService &network, const ImuPose &pose, bool imuReady) const;
  bool applySyncResponse(const String &body);

  const DeviceConfig *config_ = nullptr;
  LayoutStore *layoutStore_ = nullptr;
  AssetStore *assetStore_ = nullptr;
  bool enabled_ = false;
  uint32_t lastSyncAttemptMs_ = 0;
  uint32_t lastSyncSuccessMs_ = 0;
  int lastHttpStatus_ = 0;
};
