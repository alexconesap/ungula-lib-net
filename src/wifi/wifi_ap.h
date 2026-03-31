// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <wifi/wifi_channel.h>

#include <cstdint>

namespace ungula {
  namespace wifi {

    /// WiFi AP configuration
    struct WifiApConfig {
        const char* ssid;
        const char* password;
        WifiChannel channel;
        uint8_t maxConnections;

        WifiApConfig()
            : ssid(nullptr), password(nullptr), channel(WifiChannel::ChAuto), maxConnections(4) {}
    };

    /// Initialize WiFi in AP+STA mode
    /// @param config AP configuration
    /// @return true on success
    bool wifi_ap_init(const WifiApConfig& config = WifiApConfig());

    /// Get the AP IP address as string
    /// @return IP address string (e.g., "192.168.4.1")
    const char* wifi_ap_get_ip();

    /// Get the STA IP address as string (when connected to external WiFi)
    /// @return IP address string, or "0.0.0.0" if not connected
    const char* wifi_ap_get_sta_ip();

    /// Check if the STA interface is connected to an external WiFi network
    /// @return true if STA is connected
    bool wifi_ap_sta_connected();

    /// Get the device MAC address as string
    /// @return MAC address string (e.g., "AA:BB:CC:DD:EE:FF")
    const char* wifi_ap_get_mac();

    /// Check if AP is active
    /// @return true if AP is running
    bool wifi_ap_is_active();

    /// Get the current WiFi channel
    /// @return channel number
    WifiChannel wifi_ap_get_channel();

  }  // namespace wifi
}  // namespace ungula
