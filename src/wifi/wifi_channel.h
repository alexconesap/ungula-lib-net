// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cstdint>

namespace ungula {
  namespace wifi {

    /// WiFi channels used for SoftAP + ESP-NOW communication
    enum class WifiChannel : uint8_t {
      ChAuto = 0,  // When Internet connection is required AP+STA (APSTA), the AP
                   // channel is set automatically by the router
      // Channels 1, 6, 11 and 13 are the non-overlapping channels for 2.4 GHz WiFi
      Ch1 = 1,
      Ch2 = 2,
      Ch3 = 3,
      Ch4 = 4,
      Ch5 = 5,
      Ch6 = 6,
      Ch7 = 7,
      Ch8 = 8,
      Ch9 = 9,
      Ch10 = 10,
      Ch11 = 11,
      Ch12 = 12,
      Ch13 = 13,
    };

  }  // namespace wifi
}  // namespace ungula
