// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <cstdint>

namespace ungula {
  namespace pairing {

    /// Pairing state machine states
    enum class PairingState : uint8_t {
      IDLE = 0,
      SCANNING = 1,
      BROADCASTING = 2,
      RESPONDING = 3,
      CONFIRMING = 4,
      PAIRED = 5,
      FAILED = 6
    };

    /// Pairing beacon magic bytes for identification
    static constexpr uint8_t PAIRING_MAGIC[4] = {0xD1, 0x5C, 0x0A, 0x11};

    /// Time to listen on each channel during scanning (ms)
    static constexpr uint16_t CHANNEL_SCAN_DWELL_MS = 200;

    /// Maximum number of WiFi channels to scan
    static constexpr uint8_t MAX_SCAN_CHANNELS = 13;

    /// Pairing beacon broadcast interval (ms)
    static constexpr uint16_t BEACON_INTERVAL_MS = 100;

    /// Pairing response timeout (ms)
    static constexpr uint32_t PAIRING_TIMEOUT_MS = 10000;

    /// Preference keys for persisted pairing data
    static constexpr const char* PREF_KEY_PAIRED_MAC = "pair_mac";
    static constexpr const char* PREF_KEY_PAIRED_CHANNEL = "pair_ch";
    static constexpr const char* PREF_KEY_PAIRED_FLAG = "pair_ok";

    /// Pairing role constants
    static constexpr uint8_t PAIRING_ROLE_COORDINATOR = 1;
    static constexpr uint8_t PAIRING_ROLE_CLIENT = 2;

    /// Convert PairingState to string for logging
    inline const char* pairingStateToString(PairingState state) {
      switch (state) {
        case PairingState::IDLE:
          return "IDLE";
        case PairingState::SCANNING:
          return "SCANNING";
        case PairingState::BROADCASTING:
          return "BROADCASTING";
        case PairingState::RESPONDING:
          return "RESPONDING";
        case PairingState::CONFIRMING:
          return "CONFIRMING";
        case PairingState::PAIRED:
          return "PAIRED";
        case PairingState::FAILED:
          return "FAILED";
        default:
          return "UNKNOWN";
      }
    }

  }  // namespace pairing
}  // namespace ungula
