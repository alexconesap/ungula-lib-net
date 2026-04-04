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

      log_info("WiFi STA: connecting to '%s' (timeout %lu ms)", config.ssid,
               static_cast<unsigned long>(config.connectTimeoutMs));

      // Initiate connection (does not block)
      WiFi.begin(config.ssid, config.password);

      // Poll for connection with timeout
      const uint32_t startMs = TimeControl::millis();
      while ((TimeControl::millis() - startMs) < config.connectTimeoutMs) {
        if (WiFi.status() == WL_CONNECTED) {
          IPAddress ip = WiFi.localIP();
          snprintf(s_sta_ip, sizeof(s_sta_ip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
          log_info("WiFi STA: connected, IP=%s, channel=%d", s_sta_ip, WiFi.channel());
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
      log_info("WiFi STA: disconnected");
    }

    bool wifi_sta_is_connected() {
      return WiFi.status() == WL_CONNECTED;
    }

    const char* wifi_sta_get_ip() {
      if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(s_sta_ip, sizeof(s_sta_ip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        return s_sta_ip;
      }
      return "0.0.0.0";
    }

    /// Check if an SSID matches any of the given prefixes.
    static bool matchesPrefix(const char* ssid, const char* const* prefixes, uint8_t prefixCount) {
      if (prefixes == nullptr || prefixCount == 0) {
        return true;  // no filter = accept all
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

      log_info("WiFi STA: scanning networks%s",
               (prefixes != nullptr && prefixCount > 0) ? " (with prefix filter)" : "");

      // Synchronous scan — use default dwell time per channel for reliable detection
      int16_t found = WiFi.scanNetworks();
      if (found < 0) {
        log_error("WiFi STA: scan failed (error %d)", found);
        WiFi.scanDelete();
        return 0;
      }

      log_info("WiFi STA: scan found %d networks total", found);

      uint8_t count = 0;
      for (int16_t i = 0; i < found && count < maxResults; ++i) {
        // Keep the String alive while we use its c_str() pointer
        String ssidStr = WiFi.SSID(i);
        const char* ssid = ssidStr.c_str();

        if (ssid[0] == '\0') {
          continue;  // skip hidden networks
        }

        if (!matchesPrefix(ssid, prefixes, prefixCount)) {
          continue;
        }

        std::strncpy(results[count].ssid, ssid, sizeof(results[count].ssid) - 1);
        results[count].ssid[sizeof(results[count].ssid) - 1] = '\0';
        results[count].rssi = static_cast<int8_t>(WiFi.RSSI(i));
        results[count].channel = static_cast<uint8_t>(WiFi.channel(i));
        results[count].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

        log_info("WiFi STA:   [%d] '%s' ch=%d rssi=%d %s", count, results[count].ssid,
                 results[count].channel, results[count].rssi,
                 results[count].encrypted ? "secured" : "open");
        ++count;
      }

      WiFi.scanDelete();

      log_info("WiFi STA: returning %d networks%s", count,
               (prefixes != nullptr && prefixCount > 0) ? " (after prefix filter)" : "");
      return count;
    }

  }  // namespace wifi
}  // namespace ungula

#endif  // ENABLE_WIFI_STA
