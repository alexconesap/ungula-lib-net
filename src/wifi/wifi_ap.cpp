// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "wifi_ap.h"

#include <WiFi.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <logger.h>
#include <time/time_control.h>

namespace ungula {
  namespace wifi {

    static bool s_ap_active = false;
    static char s_ip_str[16] = "0.0.0.0";
    static char s_sta_ip_str[16] = "0.0.0.0";
    static char s_mac_str[18] = "00:00:00:00:00:00";
    static WifiChannel s_channel = WifiChannel::ChAuto;
    static WifiChannel read_effective_wifi_channel();

    bool wifi_ap_init(const WifiApConfig& config) {
      if (s_ap_active) {
        log_warn("WiFi AP already initialized");
        return true;
      }

      // Disable ESP32's internal WiFi auto-connect (we manage our own credentials)
      WiFi.persistent(false);
      WiFi.setAutoReconnect(false);

      // Set WiFi mode to AP+STA (allows both AP and ESP-NOW on STA interface)
      WiFi.mode(WIFI_AP_STA);

      // Small delay to let WiFi mode stabilize
      TimeControl::delay(100);

      // Configure and start the soft AP (use simpler 3-param version like old code)
      WiFi.softAP(config.ssid, config.password, static_cast<int>(config.channel));

      // Small delay for AP to start
      TimeControl::delay(100);

      // Get and store IP address
      IPAddress ip_add = WiFi.softAPIP();
      snprintf(s_ip_str, sizeof(s_ip_str), "%d.%d.%d.%d", ip_add[0], ip_add[1], ip_add[2],
               ip_add[3]);

      // Check if AP is actually running
      wifi_mode_t mode;
      esp_wifi_get_mode(&mode);
      if (!(mode & WIFI_MODE_AP)) {
        log_error("WiFi AP failed to start");
        return false;
      }

      // Store channel
      s_channel = read_effective_wifi_channel();
      if (s_channel == WifiChannel::ChAuto) {
        log_error("WiFi channel readback failed");
        return false;
      }

      s_ap_active = true;
      return true;
    }

    const char* wifi_ap_get_ip() {
      return s_ip_str;
    }

    const char* wifi_ap_get_sta_ip() {
      // Read current STA IP from ESP-IDF netif (updates on connect/disconnect)
      esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      if (sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
          snprintf(s_sta_ip_str, sizeof(s_sta_ip_str), IPSTR, IP2STR(&ip_info.ip));
          return s_sta_ip_str;
        }
      }
      return "0.0.0.0";
    }

    bool wifi_ap_sta_connected() {
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

    const char* wifi_ap_get_mac() {
      uint8_t mac[6];
      if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        snprintf(s_mac_str, sizeof(s_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
                 mac[2], mac[3], mac[4], mac[5]);
      }
      return s_mac_str;
    }

    bool wifi_ap_is_active() {
      return s_ap_active;
    }

    WifiChannel wifi_ap_get_channel() {
      return s_channel;
    }

    static WifiChannel read_effective_wifi_channel() {
      uint8_t primary = 0;
      wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
      esp_err_t err = esp_wifi_get_channel(&primary, &second);
      if (err != ESP_OK) {
        log_error("esp_wifi_get_channel failed: %s", esp_err_to_name(err));
        return WifiChannel::ChAuto;
      }
      return static_cast<WifiChannel>(primary);
    }

  }  // namespace wifi
}  // namespace ungula
