#include "services/ConfigStore.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {
constexpr const char *kNamespace = "peekcfg";
constexpr const char *kKeyConfigured = "configured";
constexpr const char *kKeyDeviceId = "deviceId";
constexpr const char *kKeyDeviceToken = "deviceToken";
constexpr const char *kKeyWifiSsid = "wifiSsid";
constexpr const char *kKeyWifiPassword = "wifiPassword";
constexpr const char *kKeyWifiUsername = "wifiUsername";
constexpr const char *kKeyBackendUrl = "backendUrl";
constexpr const char *kKeyBackendPollMs = "backendPollMs";
constexpr const char *kKeyTouchSampleMs = "touchSampleMs";
constexpr const char *kKeyLongPressMs = "longPressMs";
constexpr const char *kKeyExtraLongPressMs = "extraLongMs";
constexpr const char *kKeySleepTimeoutMs = "sleepMs";
}

bool ConfigStore::begin() {
  Preferences prefs;
  ready_ = prefs.begin(kNamespace, false);
  if (!ready_) {
    return false;
  }
  prefs.end();
  return true;
}

DeviceConfig ConfigStore::load() const {
  DeviceConfig config = defaultDeviceConfig();
  if (!ready_) {
    return config;
  }

  config.deviceId = getStringValue(kKeyDeviceId, config.deviceId);
  config.deviceToken = getStringValue(kKeyDeviceToken, config.deviceToken);
  config.wifiSsid = getStringValue(kKeyWifiSsid, config.wifiSsid);
  config.wifiPassword = getStringValue(kKeyWifiPassword, config.wifiPassword);
  config.wifiUsername = getStringValue(kKeyWifiUsername, config.wifiUsername);
  config.backendUrl = getStringValue(kKeyBackendUrl, config.backendUrl);
  config.backendPollIntervalMs = getUIntValue(kKeyBackendPollMs, config.backendPollIntervalMs);
  config.touchSampleIntervalMs = getUIntValue(kKeyTouchSampleMs, config.touchSampleIntervalMs);
  config.longPressMs = getUIntValue(kKeyLongPressMs, config.longPressMs);
  config.extraLongPressMs = getUIntValue(kKeyExtraLongPressMs, config.extraLongPressMs);
  config.sleepTimeoutMs = getUIntValue(kKeySleepTimeoutMs, config.sleepTimeoutMs);

  return config;
}

bool ConfigStore::save(const DeviceConfig &config) {
  if (!ready_) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  prefs.putBool(kKeyConfigured, true);
  prefs.putString(kKeyDeviceId, config.deviceId);
  prefs.putString(kKeyDeviceToken, config.deviceToken);
  prefs.putString(kKeyWifiSsid, config.wifiSsid);
  prefs.putString(kKeyWifiPassword, config.wifiPassword);
  prefs.putString(kKeyWifiUsername, config.wifiUsername);
  prefs.putString(kKeyBackendUrl, config.backendUrl);
  prefs.putUInt(kKeyBackendPollMs, config.backendPollIntervalMs);
  prefs.putUInt(kKeyTouchSampleMs, config.touchSampleIntervalMs);
  prefs.putUInt(kKeyLongPressMs, config.longPressMs);
  prefs.putUInt(kKeyExtraLongPressMs, config.extraLongPressMs);
  prefs.putUInt(kKeySleepTimeoutMs, config.sleepTimeoutMs);
  prefs.end();

  return true;
}

bool ConfigStore::reset() {
  if (!ready_) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  const bool cleared = prefs.clear();
  prefs.end();
  return cleared;
}

String ConfigStore::getStringValue(const char *key, const String &fallback) const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return fallback;
  }
  const String value = prefs.getString(key, fallback);
  prefs.end();
  return value;
}

uint32_t ConfigStore::getUIntValue(const char *key, uint32_t fallback) const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return fallback;
  }
  const uint32_t value = prefs.getUInt(key, fallback);
  prefs.end();
  return value;
}
