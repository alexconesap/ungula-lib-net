// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

// Minimal WiFi initialization for ESP-NOW — delegates to wifi_sta_init().

#include "wifi_espnow.h"
#include "wifi_sta.h"

namespace ungula {
  namespace wifi {

    bool wifi_espnow_init() {
      return wifi_sta_init();
    }

  }  // namespace wifi
}  // namespace ungula
