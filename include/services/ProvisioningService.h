#pragma once

#include <WebServer.h>
#include <WString.h>
#include <stdint.h>

#include "config/DeviceConfig.h"
#include "services/ConfigStore.h"

class ProvisioningService {
public:
  void begin(DeviceConfig &config, ConfigStore &configStore);
  void loop(uint32_t now);

  bool isActive() const;
  const String &apSsid() const;
  const String &apPassword() const;

private:
  void startPortal();
  void handleRoot();
  void handleConfigJson();
  void handleSave();
  void handleNotFound();
  void requestRestart();
  String htmlPage() const;
  String jsonConfig() const;

  DeviceConfig *config_ = nullptr;
  ConfigStore *configStore_ = nullptr;
  WebServer server_{80};
  bool active_ = false;
  bool restartRequested_ = false;
  uint32_t restartAtMs_ = 0;
  String apSsid_;
  String apPassword_ = "peeksetup";
};
