// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

/**
 * @file wifi_config.cpp
 * @brief WiFi STA credentials persistence — NVS load/save/clear with CRC32.
 */

#ifdef ENABLE_WIFI_STA

#include "wifi_config.h"

#include <logger.h>
#include <util/crc32.h>

#include <cstring>

namespace ungula {
  namespace wifi {

    struct WifiConfigBlob {
        WifiConfig config;
        uint32_t crc;
    };

    WifiConfigStore::WifiConfigStore(ungula::IPreferences& prefs, const char* nvsNamespace)
        : prefs_(prefs), ns_(nvsNamespace) {}

    WifiConfig WifiConfigStore::load() {
      WifiConfig config = WifiConfig::createDefault();

      if (!prefs_.begin(ns_)) {
        return config;
      }

      WifiConfigBlob blob;
      size_t read = prefs_.getBytes(NVS_KEY, reinterpret_cast<uint8_t*>(&blob), sizeof(blob));
      prefs_.end();

      if (read != sizeof(blob)) {
        log_info("No WiFi config in NVS (%s), using defaults", ns_);
        return config;
      }

      uint32_t expected = crc32(reinterpret_cast<const uint8_t*>(&blob.config), sizeof(WifiConfig));
      if (blob.crc != expected) {
        log_warn("WiFi config CRC mismatch (%s), using defaults", ns_);
        return config;
      }

      config = blob.config;
      config.ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
      config.password[WIFI_PASS_MAX_LEN - 1] = '\0';

      log_debug("WiFi config loaded (%s): enabled=%d, ssid='%s'", ns_, config.enabled ? 1 : 0,
                config.ssid);
      return config;
    }

    void WifiConfigStore::save(const WifiConfig& config) {
      WifiConfigBlob blob;
      blob.config = config;
      blob.crc = crc32(reinterpret_cast<const uint8_t*>(&blob.config), sizeof(WifiConfig));

      if (!prefs_.begin(ns_)) {
        log_error("Failed to open NVS namespace '%s' for WiFi config", ns_);
        return;
      }
      if (!prefs_.putBytes(NVS_KEY, reinterpret_cast<const uint8_t*>(&blob), sizeof(blob))) {
        log_error("Failed to write WiFi config to NVS (%s)", ns_);
      }
      prefs_.end();

      log_info("WiFi config saved (%s): enabled=%d, ssid='%s'", ns_, config.enabled ? 1 : 0,
               config.ssid);
    }

    void WifiConfigStore::clear() {
      if (!prefs_.begin(ns_)) {
        log_error("Failed to open NVS namespace '%s' for WiFi config clear", ns_);
        return;
      }
      prefs_.remove(NVS_KEY);
      prefs_.end();

      log_info("WiFi config cleared (%s)", ns_);
    }

  }  // namespace wifi
}  // namespace ungula

#endif  // ENABLE_WIFI_STA
