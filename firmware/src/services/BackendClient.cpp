#include "services/BackendClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace {
constexpr const char *kFirmwareVersion = "0.1.0";
constexpr uint32_t kConnectedGraceMs = 15000;
constexpr size_t kAssetReserveBytes = 128 * 1024;

String trimTrailingSlash(const String &value) {
  String result = value;
  result.trim();
  while (result.endsWith("/")) {
    result.remove(result.length() - 1);
  }
  return result;
}

bool writeHttpResponseToFile(HTTPClient &http, int status, const String &localPath, size_t expectedSize) {
  if (status < 200 || status >= 300) {
    http.end();
    return false;
  }

  const String tempPath = localPath + ".tmp";
  File file = LittleFS.open(tempPath, "w");
  if (!file) {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[512];
  size_t written = 0;
  while (http.connected()) {
    const int available = stream->available();
    if (available <= 0) {
      if (written >= expectedSize && expectedSize > 0) {
        break;
      }
      delay(1);
      continue;
    }

    const int readSize = stream->readBytes(buffer, min(available, static_cast<int>(sizeof(buffer))));
    if (readSize <= 0) {
      break;
    }
    file.write(buffer, readSize);
    written += readSize;
  }
  file.close();
  http.end();

  if (expectedSize > 0 && written != expectedSize) {
    LittleFS.remove(tempPath);
    return false;
  }

  LittleFS.remove(localPath);
  if (!LittleFS.rename(tempPath, localPath)) {
    LittleFS.remove(tempPath);
    return false;
  }

  return true;
}
} // namespace

void BackendClient::begin(const DeviceConfig &config, AssetStore &assetStore) {
  config_ = &config;
  assetStore_ = &assetStore;
  enabled_ = config.backendUrl.length() > 0;

  if (!enabled_) {
    return;
  }
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
  String body;

  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    began = http.begin(client, url);
    if (began) {
      http.addHeader("Content-Type", "application/json");
      status = http.POST(payload);
      if (status >= 200 && status < 300) {
        body = http.getString();
      }
    }
  } else {
    WiFiClient client;
    began = http.begin(client, url);
    if (began) {
      http.addHeader("Content-Type", "application/json");
      status = http.POST(payload);
      if (status >= 200 && status < 300) {
        body = http.getString();
      }
    }
  }

  lastHttpStatus_ = status;
  if (!began) {
    return;
  }

  if (status < 200 || status >= 300) {
    http.end();
    return;
  }

  http.end();

  if (!applySyncResponse(body)) {
    return;
  }

  lastSyncSuccessMs_ = now;
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
  JsonObject storage = status["storage"].to<JsonObject>();
  storage["totalBytes"] = assetStore_ ? LittleFS.totalBytes() : 0;
  storage["usedBytes"] = assetStore_ ? LittleFS.usedBytes() : 0;
  storage["freeBytes"] = assetStore_ && LittleFS.totalBytes() >= LittleFS.usedBytes()
                             ? LittleFS.totalBytes() - LittleFS.usedBytes()
                             : 0;

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
    return false;
  }

  JsonObject data = doc["data"];
  if (data.isNull()) {
    return false;
  }

  if (!data["assets"].isNull() && assetStore_) {
    String assetsJson;
    serializeJson(data["assets"], assetsJson);
    if (assetsJson.length() > 0 && assetsJson != assetStore_->manifestJson()) {
      assetStore_->saveManifestJson(assetsJson);
    }
    syncPet2Asset(data["assets"].as<JsonObject>());
  }

  return true;
}

void BackendClient::syncPet2Asset(JsonObject assets) {
  if (!assetStore_) {
    return;
  }

  const char *pet2AssetId = assets["pet2AssetId"] | "";
  if (!pet2AssetId || pet2AssetId[0] == '\0') {
    return;
  }

  JsonArray assetList = assets["assets"].as<JsonArray>();
  for (JsonObject asset : assetList) {
    const char *assetId = asset["id"] | "";
    if (String(assetId) != pet2AssetId) {
      continue;
    }

    const char *devicePath = asset["devicePath"] | "";
    const char *deviceFormat = asset["deviceFormat"] | "";
    const size_t encodedSize = asset["encodedSize"] | 0;
    if (!devicePath || devicePath[0] == '\0' || String(deviceFormat) != "pka-rgb565-rle") {
      return;
    }

    const String id(assetId);
    if (assetStore_->hasAnimationFile(id, encodedSize)) {
      assetStore_->markPet2AnimationDownloaded(id);
      return;
    }

    if (!assetStore_->canStoreAsset(encodedSize, kAssetReserveBytes)) {
      return;
    }

    const String url = endpoint(devicePath);
    const String localPath = assetStore_->localAnimationPath(id);
    if (downloadAssetFile(url, localPath, encodedSize)) {
      assetStore_->markPet2AnimationDownloaded(id);
    }
    return;
  }
}

bool BackendClient::downloadAssetFile(const String &url, const String &localPath, size_t expectedSize) {
  HTTPClient http;
  int status = -1;
  bool began = false;

  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    began = http.begin(client, url);
    if (began) {
      status = http.GET();
      return writeHttpResponseToFile(http, status, localPath, expectedSize);
    }
  } else {
    WiFiClient client;
    began = http.begin(client, url);
    if (began) {
      status = http.GET();
      return writeHttpResponseToFile(http, status, localPath, expectedSize);
    }
  }

  http.end();
  return false;
}
