#include "services/BackendClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace {
constexpr const char *kFirmwareVersion = "0.1.0";
constexpr uint32_t kConnectedGraceMs = 15000;

String trimTrailingSlash(const String &value) {
  String result = value;
  result.trim();
  while (result.endsWith("/")) {
    result.remove(result.length() - 1);
  }
  return result;
}
} // namespace

void BackendClient::begin(const DeviceConfig &config, LayoutStore &layoutStore, AssetStore &assetStore) {
  config_ = &config;
  layoutStore_ = &layoutStore;
  assetStore_ = &assetStore;
  enabled_ = config.backendUrl.length() > 0;

  if (!enabled_) {
    Serial.println("Backend sync disabled: empty backendUrl");
    return;
  }

  Serial.print("Backend sync target ");
  Serial.println(trimTrailingSlash(config.backendUrl));
}

void BackendClient::loop(
    uint32_t now,
    const NetworkService &network,
    const ImuPose &pose,
    bool imuReady) {
  if (!enabled_ || !network.isConnected()) {
    return;
  }

  const uint32_t interval = config_->backendPollIntervalMs > 0 ? config_->backendPollIntervalMs : 5000;
  if (lastSyncAttemptMs_ != 0 && now - lastSyncAttemptMs_ < interval) {
    return;
  }

  syncNow(now, network, pose, imuReady);
}

bool BackendClient::isEnabled() const {
  return enabled_;
}

bool BackendClient::isConnected(uint32_t now) const {
  return enabled_ && lastSyncSuccessMs_ != 0 && now - lastSyncSuccessMs_ <= kConnectedGraceMs;
}

int BackendClient::lastHttpStatus() const {
  return lastHttpStatus_;
}

void BackendClient::syncNow(
    uint32_t now,
    const NetworkService &network,
    const ImuPose &pose,
    bool imuReady) {
  lastSyncAttemptMs_ = now;
  const String url = endpoint("/api/device/sync");
  const String payload = buildSyncPayload(network, pose, imuReady);

  HTTPClient http;
  int status = -1;
  bool began = false;

  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    began = http.begin(client, url);
    if (began) {
      http.addHeader("Content-Type", "application/json");
      status = http.POST(payload);
    }
  } else {
    WiFiClient client;
    began = http.begin(client, url);
    if (began) {
      http.addHeader("Content-Type", "application/json");
      status = http.POST(payload);
    }
  }

  lastHttpStatus_ = status;
  if (!began) {
    Serial.println("Backend sync failed: begin");
    return;
  }

  if (status < 200 || status >= 300) {
    Serial.print("Backend sync http ");
    Serial.println(status);
    http.end();
    return;
  }

  const String body = http.getString();
  http.end();

  if (!applySyncResponse(body)) {
    Serial.println("Backend sync response ignored");
    return;
  }

  lastSyncSuccessMs_ = now;
  Serial.println("Backend sync ok");
}

String BackendClient::endpoint(const char *path) const {
  return trimTrailingSlash(config_->backendUrl) + path;
}

String BackendClient::buildSyncPayload(
    const NetworkService &network,
    const ImuPose &pose,
    bool imuReady) const {
  JsonDocument doc;
  doc["deviceId"] = config_->deviceId;
  doc["token"] = config_->deviceToken;
  doc["layoutCached"] = layoutStore_ && layoutStore_->hasLayout();
  doc["assetsCached"] = assetStore_ && assetStore_->hasManifest();

  JsonObject status = doc["status"].to<JsonObject>();
  status["connected"] = true;
  status["firmwareVersion"] = kFirmwareVersion;
  status["ipAddress"] = network.ipAddress();
  status["wifiRssi"] = network.rssi();
  status["batteryPercent"] = nullptr;
  status["charging"] = nullptr;
  status["state"] = "online";
  status["touchAnalog"] = nullptr;
  status["lastEvent"] = "sync";
  status["motorActive"] = false;

  JsonObject imu = status["imu"].to<JsonObject>();
  if (pose.valid) {
    imu["pitch"] = pose.pitchDeg;
    imu["roll"] = pose.rollDeg;
    imu["yaw"] = pose.yawDeg;
  } else {
    imu["pitch"] = nullptr;
    imu["roll"] = nullptr;
    imu["yaw"] = nullptr;
  }
  status["imuReady"] = imuReady;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool BackendClient::applySyncResponse(const String &body) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    Serial.print("Backend sync json error ");
    Serial.println(error.c_str());
    return false;
  }

  JsonObject data = doc["data"];
  if (data.isNull()) {
    return false;
  }

  if (!data["layout"].isNull() && layoutStore_) {
    String layoutJson;
    serializeJson(data["layout"], layoutJson);
    if (layoutJson.length() > 0 && layoutJson != layoutStore_->layoutJson()) {
      layoutStore_->saveLayoutJson(layoutJson);
    }
  }

  if (!data["assets"].isNull() && assetStore_) {
    String assetsJson;
    serializeJson(data["assets"], assetsJson);
    if (assetsJson.length() > 0 && assetsJson != assetStore_->manifestJson()) {
      assetStore_->saveManifestJson(assetsJson);
    }
  }

  return true;
}
