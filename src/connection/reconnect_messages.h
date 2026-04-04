// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once

#include <cstdint>
#include <cstring>

namespace ungula {

  /// Magic bytes for reconnect messages (different from pairing magic)
  static constexpr uint8_t RECONNECT_MAGIC[4] = {0xAC, 0xDC, 0x07, 0x77};

  /// Reconnect probe sent by a client (RBB) to a known coordinator (ICB)
  /// when trying to find it on a different channel after connection loss.
  struct __attribute__((packed)) ReconnectProbe {
      uint8_t magic[4];
      uint8_t deviceId;     // the RBB's device ID (so ICB knows who's asking)
      uint8_t reserved[3];

      void init(uint8_t devId) {
        memcpy(magic, RECONNECT_MAGIC, 4);
        deviceId = devId;
        reserved[0] = 0;
        reserved[1] = 0;
        reserved[2] = 0;
      }

      bool isValid() const {
        return memcmp(magic, RECONNECT_MAGIC, 4) == 0;
      }
  };

  /// Reconnect acknowledgment sent by coordinator (ICB) back to the client.
  /// Tells the client the current channel to use.
  struct __attribute__((packed)) ReconnectAck {
      uint8_t magic[4];
      uint8_t channel;      // the current operating channel
      uint8_t reserved[3];

      void init(uint8_t ch) {
        memcpy(magic, RECONNECT_MAGIC, 4);
        channel = ch;
        reserved[0] = 0;
        reserved[1] = 0;
        reserved[2] = 0;
      }

      bool isValid() const {
        return memcmp(magic, RECONNECT_MAGIC, 4) == 0;
      }
  };

  static_assert(sizeof(ReconnectProbe) == 8, "ReconnectProbe must be 8 bytes");
  static_assert(sizeof(ReconnectAck) == 8, "ReconnectAck must be 8 bytes");

}  // namespace ungula
