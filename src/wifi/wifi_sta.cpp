// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#ifdef ENABLE_WIFI_STA

#include "wifi_sta.h"

#include <WiFi.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <logger.h>
#include <time/time_control.h>

#include <cstring>

namespace ungula {
  namespace wifi {

    static char s_sta_ip[16] = "0.0.0.0";

    bool wifi_sta_connect(const WifiStaConfig& config) {
      if (config.ssid == nullptr || config.ssid[0] == '\0') {
        log_error("WiFi STA: no SSID provided");
        return false;
      }

      WiFi.begin(config.ssid, config.password);

      const uint32_t startMs = TimeControl::millis();
      while ((TimeControl::millis() - startMs) < config.connectTimeoutMs) {
        if (WiFi.status() == WL_CONNECTED) {
          IPAddress ipa = WiFi.localIP();
          snprintf(s_sta_ip, sizeof(s_sta_ip), "%d.%d.%d.%d", ipa[0], ipa[1], ipa[2], ipa[3]);
          return true;
        }
        TimeControl::delay(250);
      }

      log_error("WiFi STA: connection to '%s' timed out", config.ssid);
      WiFi.disconnect(false);
      return false;
    }

    void wifi_sta_disconnect() {
      WiFi.disconnect(false);
      std::strncpy(s_sta_ip, "0.0.0.0", sizeof(s_sta_ip));
    }

    bool wifi_sta_is_connected() {
      return WiFi.status() == WL_CONNECTED;
    }

    const char* wifi_sta_get_ip() {
      if (WiFi.status() == WL_CONNECTED) {
        IPAddress ipa = WiFi.localIP();
        snprintf(s_sta_ip, sizeof(s_sta_ip), "%d.%d.%d.%d", ipa[0], ipa[1], ipa[2], ipa[3]);
        return s_sta_ip;
      }
      return "0.0.0.0";
    }

    uint8_t wifi_sta_get_channel() {
      return static_cast<uint8_t>(WiFi.channel());
    }

    /// Check if an SSID matches any of the given prefixes.
    static bool matchesPrefix(const char* ssid, const char* const* prefixes, uint8_t prefixCount) {
      if (prefixes == nullptr || prefixCount == 0) {
        return true;
      }
      for (uint8_t i = 0; i < prefixCount; ++i) {
        if (prefixes[i] != nullptr &&
            std::strncmp(ssid, prefixes[i], std::strlen(prefixes[i])) == 0) {
          return true;
        }
      }
      return false;
    }

    uint8_t wifi_sta_scan(WifiScanResult* results, uint8_t maxResults, const char* const* prefixes,
                          uint8_t prefixCount) {
      if (results == nullptr || maxResults == 0) {
        return 0;
      }

      if (maxResults > WIFI_MAX_SCAN_RESULTS) {
        maxResults = WIFI_MAX_SCAN_RESULTS;
      }

      int16_t found = WiFi.scanNetworks();
      if (found < 0) {
        log_error("WiFi STA: scan failed (error %d)", found);
        WiFi.scanDelete();
        return 0;
      }

      uint8_t count = 0;
      for (int16_t i = 0; i < found && count < maxResults; ++i) {
        String ssidStr = WiFi.SSID(i);
        const char* ssid = ssidStr.c_str();

        if (ssid[0] == '\0') {
          continue;
        }

        if (!matchesPrefix(ssid, prefixes, prefixCount)) {
          continue;
        }

        std::strncpy(results[count].ssid, ssid, sizeof(results[count].ssid) - 1);
        results[count].ssid[sizeof(results[count].ssid) - 1] = '\0';
        results[count].rssi = static_cast<int8_t>(WiFi.RSSI(i));
        results[count].channel = static_cast<uint8_t>(WiFi.channel(i));
        results[count].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        ++count;
      }

      WiFi.scanDelete();
      return count;
    }

  }  // namespace wifi
}  // namespace ungula

#endif  // ENABLE_WIFI_STA
