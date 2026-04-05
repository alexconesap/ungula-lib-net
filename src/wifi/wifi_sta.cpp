// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

// WiFi STA (station) — pure ESP-IDF, no Arduino dependency.
// Auto-reconnect logic modeled after Arduino's WiFi.begin() implementation:
// on disconnection, automatically retry unless it was a voluntary disconnect.

#include "wifi_sta.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <logger.h>
#include <time/time_control.h>

#include <cstring>

namespace ungula {
  namespace wifi {

    static char s_sta_ip[16] = "0.0.0.0";
    static bool s_sta_initialized = false;

    // Event group for blocking connect
    static EventGroupHandle_t s_wifi_event_group = nullptr;
    static const int CONNECTED_BIT = BIT0;
    static const int FAIL_BIT = BIT1;

    // Auto-reconnect state
    static bool s_connecting = false;          // true while a connect attempt is active
    static bool s_auto_reconnect = true;       // auto-retry on transient disconnections
    static bool s_first_connect = false;       // first attempt always gets one retry
    static bool s_voluntary_disconnect = false; // user called wifi_sta_disconnect()

    /// Check if the disconnect reason is transient (worth retrying).
    /// Mirrors Arduino's _is_staReconnectableReason().
    static bool isReconnectableReason(uint8_t reason) {
      switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:         // 2
        case WIFI_REASON_AUTH_LEAVE:          // 3
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: // 15
        case WIFI_REASON_BEACON_TIMEOUT:      // 200
        case WIFI_REASON_NO_AP_FOUND:         // 201
        case WIFI_REASON_AUTH_FAIL:           // 202
        case WIFI_REASON_ASSOC_FAIL:          // 203
        case WIFI_REASON_HANDSHAKE_TIMEOUT:   // 204
        case WIFI_REASON_CONNECTION_FAIL:     // 205
          return true;
        default:
          return false;
      }
    }

    // Persistent event handler — auto-reconnects on transient disconnections
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
      if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* evt = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        uint8_t reason = evt->reason;

        bool doReconnect = false;

        if (s_voluntary_disconnect) {
          // User-initiated disconnect — don't reconnect
          s_voluntary_disconnect = false;
        } else if (s_first_connect) {
          // First connection attempt — always retry once (like Arduino)
          s_first_connect = false;
          doReconnect = true;
        } else if (s_auto_reconnect && isReconnectableReason(reason)) {
          // Transient error — auto-reconnect
          doReconnect = true;
        }

        if (doReconnect) {
          esp_wifi_connect();  // non-blocking retry
        } else {
          // No more retries — signal failure to the blocking caller
          if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, FAIL_BIT);
          }
        }
      } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (s_wifi_event_group) {
          xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        }
      }
    }

    static void ensure_event_handler() {
      if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
      }
      // Register handlers only once — they persist across connect/disconnect cycles
      static bool s_registered = false;
      if (!s_registered) {
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler, nullptr);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr);
        s_registered = true;
      }
    }

    bool wifi_sta_init() {
      if (s_sta_initialized) {
        return true;
      }

      esp_netif_init();
      esp_event_loop_create_default();
      esp_netif_create_default_wifi_sta();

      // Clean up leftover state from a previous boot
      esp_wifi_disconnect();
      esp_wifi_stop();
      esp_wifi_deinit();

      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
      esp_err_t err = esp_wifi_init(&cfg);
      if (err != ESP_OK) {
        log_error("esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
      }

      esp_wifi_set_storage(WIFI_STORAGE_RAM);

      err = esp_wifi_set_mode(WIFI_MODE_STA);
      if (err != ESP_OK) {
        log_error("esp_wifi_set_mode(STA) failed: %s", esp_err_to_name(err));
        return false;
      }

      err = esp_wifi_start();
      if (err != ESP_OK) {
        log_error("esp_wifi_start failed: %s", esp_err_to_name(err));
        return false;
      }

      // Register event handlers early — they must be active before any connect
      ensure_event_handler();

      s_sta_initialized = true;
      return true;
    }

    bool wifi_sta_connect(const WifiStaConfig& config) {
      if (config.ssid == nullptr || config.ssid[0] == '\0') {
        log_error("WiFi STA: no SSID provided");
        return false;
      }

      ensure_event_handler();

      // Configure STA credentials
      wifi_config_t sta_cfg = {};
      std::strncpy(reinterpret_cast<char*>(sta_cfg.sta.ssid), config.ssid,
                   sizeof(sta_cfg.sta.ssid) - 1);
      if (config.password) {
        std::strncpy(reinterpret_cast<char*>(sta_cfg.sta.password), config.password,
                     sizeof(sta_cfg.sta.password) - 1);
      }

      esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
      if (err != ESP_OK) {
        log_error("esp_wifi_set_config(STA) failed: %s", esp_err_to_name(err));
        return false;
      }

      // Set up auto-reconnect state (like Arduino's first_connect flag)
      s_first_connect = true;
      s_voluntary_disconnect = false;
      s_connecting = true;

      // Clear stale event bits and connect
      xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT | FAIL_BIT);
      err = esp_wifi_connect();
      if (err != ESP_OK) {
        log_error("esp_wifi_connect failed: %s", esp_err_to_name(err));
        s_connecting = false;
        return false;
      }

      // Block until connected or all retries exhausted (event handler auto-retries)
      EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                              CONNECTED_BIT | FAIL_BIT,
                                              pdTRUE, pdFALSE,
                                              pdMS_TO_TICKS(config.connectTimeoutMs));

      s_connecting = false;

      if (bits & CONNECTED_BIT) {
        esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta_netif) {
          esp_netif_ip_info_t ip_info;
          if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&ip_info.ip));
          }
        }
        return true;
      }

      log_error("WiFi STA: connection to '%s' timed out", config.ssid);
      esp_wifi_disconnect();
      return false;
    }

    void wifi_sta_disconnect() {
      s_voluntary_disconnect = true;  // prevent auto-reconnect
      esp_wifi_disconnect();
      std::strncpy(s_sta_ip, "0.0.0.0", sizeof(s_sta_ip));
    }

    bool wifi_sta_is_connected() {
      esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      if (!sta_netif) {
        return false;
      }
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK) {
        return false;
      }
      return ip_info.ip.addr != 0;
    }

    const char* wifi_sta_get_ip() {
      esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      if (sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
          snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&ip_info.ip));
          return s_sta_ip;
        }
      }
      return "0.0.0.0";
    }

    WifiChannel wifi_sta_get_channel() {
      uint8_t primary = 0;
      wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
      esp_wifi_get_channel(&primary, &second);
      return static_cast<WifiChannel>(primary);
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

      wifi_scan_config_t scan_cfg = {};
      scan_cfg.show_hidden = false;
      scan_cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
      scan_cfg.scan_time.active.min = 120;
      scan_cfg.scan_time.active.max = 300;

      esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
      if (err != ESP_OK) {
        log_error("WiFi STA: scan failed: %s", esp_err_to_name(err));
        return 0;
      }

      uint16_t ap_count = 0;
      esp_wifi_scan_get_ap_num(&ap_count);
      if (ap_count == 0) {
        esp_wifi_clear_ap_list();
        return 0;
      }

      uint16_t fetch_count = (ap_count > WIFI_MAX_SCAN_RESULTS) ? WIFI_MAX_SCAN_RESULTS : ap_count;
      wifi_ap_record_t ap_records[WIFI_MAX_SCAN_RESULTS];
      esp_wifi_scan_get_ap_records(&fetch_count, ap_records);
      esp_wifi_clear_ap_list();

      uint8_t count = 0;
      for (uint16_t i = 0; i < fetch_count && count < maxResults; ++i) {
        const char* ssid = reinterpret_cast<const char*>(ap_records[i].ssid);

        if (ssid[0] == '\0') {
          continue;
        }

        if (!matchesPrefix(ssid, prefixes, prefixCount)) {
          continue;
        }

        std::strncpy(results[count].ssid, ssid, sizeof(results[count].ssid) - 1);
        results[count].ssid[sizeof(results[count].ssid) - 1] = '\0';
        results[count].rssi = ap_records[i].rssi;
        results[count].channel = ap_records[i].primary;
        results[count].encrypted = (ap_records[i].authmode != WIFI_AUTH_OPEN);
        ++count;
      }

      return count;
    }

  }  // namespace wifi
}  // namespace ungula
