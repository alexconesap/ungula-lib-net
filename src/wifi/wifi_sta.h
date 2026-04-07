// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

/**
 * @file wifi_sta.h
 * @brief WiFi STA (station) management — connect to external routers, scan networks
 *
 * Provides functions to connect the ESP32 STA interface to an external WiFi router,
 * disconnect, and scan for available networks with optional SSID prefix filtering.
 *
 * Prefix filtering allows scanning only for SSIDs that start with given strings,
 * useful for auto-discovery in multi-device deployments (e.g., "RACHEL_WIFI_").
 *
 * Note: On ESP32 in AP+STA mode, connecting to a router forces the WiFi channel
 * to match the router's channel. This affects AP and ESP-NOW on the same device.
 */

#pragma once

#include <cstdint>
#include <wifi/wifi_channel.h>

namespace ungula {
  namespace wifi {

    /// STA connection configuration
    struct WifiStaConfig {
        const char* ssid;
        const char* password;
        uint32_t connectTimeoutMs;

        WifiStaConfig() : ssid(nullptr), password(nullptr), connectTimeoutMs(15000) {}
    };

    /// Result of a single network found during scan
    struct WifiScanResult {
        char ssid[33];
        int8_t rssi;
        uint8_t channel;
        bool encrypted;
    };

    /// Maximum networks returned by a single scan
    static constexpr uint8_t WIFI_MAX_SCAN_RESULTS = 16;

    /// Initialize WiFi in STA-only mode (no AP).
    /// Call this instead of wifi_ap_init() for nodes that don't serve a web UI
    /// but still need ESP-NOW and optional STA connectivity.
    /// @return true on success
    bool wifi_sta_init();

    /// Connect STA interface to an external WiFi router.
    /// Blocks until connected and IP obtained, or timeout.
    /// @param config Connection parameters (SSID, password, timeout)
    /// @return true if connected with valid IP within timeout
    bool wifi_sta_connect(const WifiStaConfig& config);

    /// Disconnect STA from the external router.
    /// After disconnecting, the WiFi channel may revert to the AP's configured channel.
    void wifi_sta_disconnect();

    /// Check if STA is connected to an external router (has valid IP).
    /// @return true if connected
    bool wifi_sta_is_connected();

    /// Get the STA IP address as string.
    /// @return IP address string, or "0.0.0.0" if not connected
    const char* wifi_sta_get_ip();

    /// Get the current WiFi channel.
    /// @return WifiChannel (Ch1-Ch13), or ChAuto if not available
    WifiChannel wifi_sta_get_channel();

    /// Re-apply DNS servers and default route from the STA netif to the
    /// global lwIP resolver. Useful as a recovery step after DHCP lease
    /// renewal or when getaddrinfo() starts failing on a working STA link.
    /// Safe to call from any task; no-op if STA is not connected.
    void wifi_sta_refresh_dns();

    /// Scan for available WiFi networks.
    /// @param results Output array to fill with scan results
    /// @param maxResults Maximum number of results to return (capped at WIFI_MAX_SCAN_RESULTS)
    /// @param prefixes Optional array of SSID prefix strings for filtering (nullptr = no filter)
    /// @param prefixCount Number of prefix strings in the array
    /// @return Number of networks found (and stored in results)
    uint8_t wifi_sta_scan(WifiScanResult* results, uint8_t maxResults,
                          const char* const* prefixes = nullptr, uint8_t prefixCount = 0);

  }  // namespace wifi
}  // namespace ungula
