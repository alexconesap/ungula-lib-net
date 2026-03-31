// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

/**
 * @file wifi_config.h
 * @brief WiFi STA credentials persistence (NVS)
 *
 * Stores and retrieves WiFi SSID, password, and enable flag using the
 * IPreferences abstraction. This is a storage/preferences concern, not part
 * of the transport or communications layer.
 *
 * Each project provides its own NVS namespace to avoid collisions:
 *   WifiConfigStore store(prefs, "my_wifi");
 *   auto cfg = store.load();
 *
 * It is internally using a single NVS key ("wifi_cfg") to store a binary blob containing the
 * WifiConfig struct plus a CRC32 checksum.
 *
 * Data is CRC32-protected so corrupted NVS blobs fall back to defaults.
 */

#pragma once
#ifdef ENABLE_WIFI_STA

#include <preferences/core/i_preferences.h>

#include <cstdint>

namespace ungula {
  namespace wifi {

    static constexpr uint8_t WIFI_SSID_MAX_LEN = 33;  // 32 chars + null
    static constexpr uint8_t WIFI_PASS_MAX_LEN = 65;  // 64 chars + null

    /// WiFi STA configuration: SSID, password, and enable flag.
    struct WifiConfig {
        bool enabled;
        char ssid[WIFI_SSID_MAX_LEN];
        char password[WIFI_PASS_MAX_LEN];

        static WifiConfig createDefault() {
          WifiConfig c;
          c.enabled = false;
          c.ssid[0] = '\0';
          c.password[0] = '\0';
          return c;
        }

        /// True if an SSID has been configured (non-empty).
        bool hasCredentials() const {
          return ssid[0] != '\0';
        }
    };

    /// Handles loading, saving, and clearing WiFi STA credentials in NVS.
    /// The NVS namespace is set per instance, so different projects can use
    /// different namespaces without code changes.
    class WifiConfigStore {
      public:
        /// @param prefs Platform preferences (NVS) implementation
        /// @param nvsNamespace NVS namespace for this project (e.g., "icb_wifi", "rachel_wifi")
        WifiConfigStore(ungula::IPreferences& prefs, const char* nvsNamespace);

        /// Load config from NVS. Returns defaults if not found or CRC mismatch.
        WifiConfig load();

        /// Save config to NVS with CRC32 protection.
        void save(const WifiConfig& config);

        /// Clear config from NVS (resets to defaults on next load).
        void clear();

      private:
        ungula::IPreferences& prefs_;
        const char* ns_;
        static constexpr const char* NVS_KEY = "wifi_cfg";
    };

  }  // namespace wifi
}  // namespace ungula

#endif  // ENABLE_WIFI_STA
