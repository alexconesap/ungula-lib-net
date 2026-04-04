// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once

/**
 * @file wifi_espnow.h
 * @brief Minimal WiFi initialization for ESP-NOW communication.
 *
 * Use this header for nodes that only need ESP-NOW (no web server, no AP).
 * It initializes the WiFi radio in STA mode — the minimum required for
 * ESP-NOW to function.
 *
 * Usage in your .ino:
 * @code
 *   #include <wifi/wifi_espnow.h>
 *
 *   void setup() {
 *     if (!ungula::wifi::wifi_espnow_init()) {
 *       // handle error
 *     }
 *     // ESP-NOW transport is now ready to use
 *   }
 * @endcode
 */

namespace ungula {
  namespace wifi {

    /// Initialize WiFi radio for ESP-NOW communication.
    /// Sets up WiFi in STA mode — the minimum required for ESP-NOW.
    /// No AP is started, no HTTP server, no web UI.
    /// @return true on success
    bool wifi_espnow_init();

  }  // namespace wifi
}  // namespace ungula
