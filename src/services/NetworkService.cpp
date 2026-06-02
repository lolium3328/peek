#include "services/NetworkService.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wpa2.h>

namespace {
constexpr uint32_t kReconnectIntervalMs = 10000;
}

void NetworkService::begin(const DeviceConfig &config) {
  config_ = &config;
  enabled_ = config.wifiSsid.length() > 0;

  if (!enabled_) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  startConnect(millis());
}

void NetworkService::loop(uint32_t now) {
  if (!enabled_ || isConnected()) {
    return;
  }

  if (now - lastConnectAttemptMs_ >= kReconnectIntervalMs) {
    startConnect(now);
  }
}

bool NetworkService::isEnabled() const {
  return enabled_;
}

bool NetworkService::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

int32_t NetworkService::rssi() const {
  return isConnected() ? WiFi.RSSI() : 0;
}

String NetworkService::ipAddress() const {
  return isConnected() ? WiFi.localIP().toString() : String();
}

void NetworkService::startConnect(uint32_t now) {
  if (!config_ || !enabled_) {
    return;
  }

  lastConnectAttemptMs_ = now;

  if (config_->wifiUsername.length() > 0) {

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    esp_wifi_sta_wpa2_ent_set_identity(
        reinterpret_cast<const uint8_t *>(config_->wifiUsername.c_str()),
        config_->wifiUsername.length());
    esp_wifi_sta_wpa2_ent_set_username(
        reinterpret_cast<const uint8_t *>(config_->wifiUsername.c_str()),
        config_->wifiUsername.length());
    esp_wifi_sta_wpa2_ent_set_password(
        reinterpret_cast<const uint8_t *>(config_->wifiPassword.c_str()),
        config_->wifiPassword.length());

    esp_wifi_sta_wpa2_ent_enable();
    WiFi.begin(config_->wifiSsid.c_str());
  } else {
    // WPA2-Personal (PSK)
    WiFi.begin(config_->wifiSsid.c_str(), config_->wifiPassword.c_str());
  }
}
