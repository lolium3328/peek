#include "services/ProvisioningService.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>

namespace {
constexpr uint32_t kRestartDelayMs = 1200;

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t index = 0; index < value.length(); ++index) {
    const char c = value[index];
    if (c == '&') {
      escaped += "&amp;";
    } else if (c == '<') {
      escaped += "&lt;";
    } else if (c == '>') {
      escaped += "&gt;";
    } else if (c == '"') {
      escaped += "&quot;";
    } else {
      escaped += c;
    }
  }
  return escaped;
}

uint32_t boundedUInt(const String &value, uint32_t fallback, uint32_t minValue, uint32_t maxValue) {
  if (value.length() == 0) {
    return fallback;
  }

  const uint32_t next = static_cast<uint32_t>(value.toInt());
  if (next < minValue) {
    return minValue;
  }
  if (next > maxValue) {
    return maxValue;
  }
  return next;
}
}

void ProvisioningService::begin(DeviceConfig &config, ConfigStore &configStore) {
  config_ = &config;
  configStore_ = &configStore;

  if (config.wifiSsid.length() > 0) {
    active_ = false;
    return;
  }

  startPortal();
}

void ProvisioningService::loop(uint32_t now) {
  if (!active_) {
    return;
  }

  server_.handleClient();
  if (restartRequested_ && now >= restartAtMs_) {
    Serial.println("Provisioning saved, restarting");
    delay(50);
    ESP.restart();
  }
}

bool ProvisioningService::isActive() const {
  return active_;
}

const String &ProvisioningService::apSsid() const {
  return apSsid_;
}

const String &ProvisioningService::apPassword() const {
  return apPassword_;
}

void ProvisioningService::startPortal() {
  const uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", static_cast<uint16_t>(mac & 0xFFFF));
  apSsid_ = String("Peek-") + suffix;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid_.c_str(), apPassword_.c_str());

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleConfigJson(); });
  server_.on("/api/config", HTTP_POST, [this]() { handleSave(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  active_ = true;

  Serial.print("Provisioning AP started ssid=");
  Serial.print(apSsid_);
  Serial.print(" password=");
  Serial.print(apPassword_);
  Serial.print(" ip=");
  Serial.println(WiFi.softAPIP());
}

void ProvisioningService::handleRoot() {
  server_.send(200, "text/html; charset=utf-8", htmlPage());
}

void ProvisioningService::handleConfigJson() {
  server_.send(200, "application/json", jsonConfig());
}

void ProvisioningService::handleSave() {
  if (!config_ || !configStore_) {
    server_.send(500, "text/plain", "config unavailable");
    return;
  }

  DeviceConfig next = *config_;
  next.wifiSsid = server_.arg("wifiSsid");
  next.wifiPassword = server_.arg("wifiPassword");
  next.backendUrl = server_.arg("backendUrl");
  next.deviceId = server_.arg("deviceId");
  next.deviceToken = server_.arg("deviceToken");
  next.backendPollIntervalMs =
      boundedUInt(server_.arg("backendPollIntervalMs"), next.backendPollIntervalMs, 1000, 600000);

  next.wifiSsid.trim();
  next.backendUrl.trim();
  next.deviceId.trim();
  next.deviceToken.trim();
  if (next.deviceId.length() == 0) {
    next.deviceId = "peek-dev";
  }

  if (next.wifiSsid.length() == 0) {
    server_.send(400, "text/plain", "wifi ssid is required");
    return;
  }

  const bool saved = configStore_->save(next);
  if (!saved) {
    server_.send(500, "text/plain", "save failed");
    return;
  }

  *config_ = next;
  server_.send(200, "text/html; charset=utf-8",
               "<!doctype html><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
               "<body style=\"font-family:sans-serif;padding:24px\">Saved. Peek will restart.</body>");
  requestRestart();
}

void ProvisioningService::handleNotFound() {
  server_.sendHeader("Location", "/", true);
  server_.send(302, "text/plain", "");
}

void ProvisioningService::requestRestart() {
  restartRequested_ = true;
  restartAtMs_ = millis() + kRestartDelayMs;
}

String ProvisioningService::htmlPage() const {
  const String deviceId = config_ ? htmlEscape(config_->deviceId) : "peek-dev";
  const String backendUrl = config_ ? htmlEscape(config_->backendUrl) : "";
  const uint32_t pollMs = config_ ? config_->backendPollIntervalMs : 5000;

  String html;
  html.reserve(3600);
  html += F("<!doctype html><html><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>Peek Setup</title><style>"
            "body{margin:0;background:#eef2f4;color:#182025;font-family:system-ui,sans-serif}"
            "main{max-width:560px;margin:0 auto;padding:24px}"
            "form{display:grid;gap:14px;background:#fff;border:1px solid #d6e0e4;border-radius:8px;padding:18px}"
            "label{display:grid;gap:6px;font-weight:800;color:#65727a}"
            "input{min-height:42px;border:1px solid #d6e0e4;border-radius:8px;padding:0 11px;font:inherit}"
            "button{min-height:44px;border:0;border-radius:8px;background:#172126;color:#fff;font-weight:900;font:inherit}"
            "h1{margin:0 0 8px;font-size:28px}p{margin:0 0 18px;color:#65727a}"
            "</style></head><body><main><h1>Peek Setup</h1>"
            "<p>Connect Peek to Wi-Fi and the control server.</p>"
            "<form method=\"post\" action=\"/api/config\">");
  html += F("<label>Wi-Fi SSID<input name=\"wifiSsid\" required autocomplete=\"off\"></label>");
  html += F("<label>Wi-Fi Password<input name=\"wifiPassword\" type=\"password\" autocomplete=\"off\"></label>");
  html += F("<label>Backend URL<input name=\"backendUrl\" inputmode=\"url\" value=\"");
  html += backendUrl;
  html += F("\" placeholder=\"http://server:3001\"></label>");
  html += F("<label>Device ID<input name=\"deviceId\" value=\"");
  html += deviceId;
  html += F("\" autocomplete=\"off\"></label>");
  html += F("<label>Device Token<input name=\"deviceToken\" type=\"password\" autocomplete=\"off\"></label>");
  html += F("<label>Sync Interval ms<input name=\"backendPollIntervalMs\" type=\"number\" min=\"1000\" max=\"600000\" step=\"1000\" value=\"");
  html += String(pollMs);
  html += F("\"></label><button type=\"submit\">Save and Restart</button></form></main></body></html>");
  return html;
}

String ProvisioningService::jsonConfig() const {
  JsonDocument doc;
  if (config_) {
    doc["deviceId"] = config_->deviceId;
    doc["wifiSsid"] = config_->wifiSsid;
    doc["backendUrl"] = config_->backendUrl;
    doc["backendPollIntervalMs"] = config_->backendPollIntervalMs;
  }
  doc["apSsid"] = apSsid_;
  doc["apPassword"] = apPassword_;

  String json;
  serializeJson(doc, json);
  return json;
}
