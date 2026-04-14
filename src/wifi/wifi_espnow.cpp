// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

// Minimal WiFi initialization for ESP-NOW — delegates to sta_init().

#include "wifi_espnow.h"
#include "wifi_sta.h"

namespace ungula {
  namespace wifi {

    bool espnow_init() {
      return sta_init();
    }

  }  // namespace wifi
}  // namespace ungula
