// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

// WiFi AP initialization — pure ESP-IDF, no Arduino dependency.

#include "wifi_ap.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <logger.h>
#include <time/time_control.h>

#include <cstring>

namespace ungula {
  namespace wifi {

    static bool s_ap_active = false;
    static bool s_netif_initialized = false;
    static esp_netif_t* s_ap_netif = nullptr;
    static esp_netif_t* s_sta_netif = nullptr;
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

      // Initialize TCP/IP and event loop (safe to call multiple times)
      if (!s_netif_initialized) {
        esp_netif_init();
        esp_event_loop_create_default();
        s_ap_netif = esp_netif_create_default_wifi_ap();
        s_sta_netif = esp_netif_create_default_wifi_sta();
        s_netif_initialized = true;
      }

      // Clean up any leftover WiFi state from a previous boot/session.
      // On soft reboot the radio hardware may retain state — disconnect, stop,
      // and deinit first to ensure a clean start. Errors are expected on first boot.
      esp_wifi_disconnect();
      esp_wifi_stop();
      esp_wifi_deinit();

      // Initialize WiFi with default config
      wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
      esp_err_t err = esp_wifi_init(&wifi_init_cfg);
      if (err != ESP_OK) {
        log_error("esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
      }

      // Disable persistent storage (we manage credentials ourselves)
      esp_wifi_set_storage(WIFI_STORAGE_RAM);

      // Set AP+STA mode
      err = esp_wifi_set_mode(WIFI_MODE_APSTA);
      if (err != ESP_OK) {
        log_error("esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        return false;
      }

      // Configure the AP
      wifi_config_t ap_cfg = {};
      std::strncpy(reinterpret_cast<char*>(ap_cfg.ap.ssid), config.ssid, sizeof(ap_cfg.ap.ssid) - 1);
      if (config.password != nullptr && config.password[0] != '\0') {
        std::strncpy(reinterpret_cast<char*>(ap_cfg.ap.password), config.password,
                     sizeof(ap_cfg.ap.password) - 1);
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
      } else {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
      }
      ap_cfg.ap.channel = static_cast<uint8_t>(config.channel);
      ap_cfg.ap.max_connection = config.maxConnections;

      err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
      if (err != ESP_OK) {
        log_error("esp_wifi_set_config(AP) failed: %s", esp_err_to_name(err));
        return false;
      }

      // Start WiFi
      err = esp_wifi_start();
      if (err != ESP_OK) {
        log_error("esp_wifi_start failed: %s", esp_err_to_name(err));
        return false;
      }

      // Small delay to let AP stabilize
      TimeControl::delay(100);

      // Read AP IP
      if (s_ap_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_ap_netif, &ip_info) == ESP_OK) {
          snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&ip_info.ip));
        }
      }

      // Verify AP is running
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
      if (s_sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
          snprintf(s_sta_ip_str, sizeof(s_sta_ip_str), IPSTR, IP2STR(&ip_info.ip));
          return s_sta_ip_str;
        }
      }
      return "0.0.0.0";
    }

    bool wifi_ap_sta_connected() {
      if (!s_sta_netif) {
        return false;
      }
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(s_sta_netif, &ip_info) != ESP_OK) {
        return false;
      }
      return ip_info.ip.addr != 0;
    }

    const char* wifi_ap_get_mac() {
      uint8_t mac[6];
      if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        snprintf(s_mac_str, sizeof(s_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
