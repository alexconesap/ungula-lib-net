// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cstdint>
#include <cstring>

#include <comm/transport_types.h>
#include "pairing_types.h"

namespace ungula {
  namespace pairing {

    /// Beacon broadcast by the coordinator to announce its presence
    struct __attribute__((packed)) PairingBeacon {
        uint8_t magic[4];
        uint8_t role;
        uint8_t channel;
        uint8_t reserved[2];

        void init(uint8_t ch) {
          memcpy(magic, PAIRING_MAGIC, 4);
          role = PAIRING_ROLE_COORDINATOR;
          channel = ch;
          reserved[0] = 0;
          reserved[1] = 0;
        }

        bool isValid() const {
          return memcmp(magic, PAIRING_MAGIC, 4) == 0 && role == PAIRING_ROLE_COORDINATOR;
        }
    };

    /// Request sent by client to coordinator after receiving beacon
    struct __attribute__((packed)) PairingRequest {
        uint8_t magic[4];
        uint8_t role;
        uint8_t deviceId;
        uint8_t reserved[2];

        void init(uint8_t devId) {
          memcpy(magic, PAIRING_MAGIC, 4);
          role = PAIRING_ROLE_CLIENT;
          deviceId = devId;
          reserved[0] = 0;
          reserved[1] = 0;
        }

        bool isValid() const {
          return memcmp(magic, PAIRING_MAGIC, 4) == 0 && role == PAIRING_ROLE_CLIENT;
        }
    };

    /// Confirmation sent by coordinator to client
    struct __attribute__((packed)) PairingConfirm {
        uint8_t magic[4];
        uint8_t role;
        uint8_t accepted;
        uint8_t channel;
        uint8_t reserved;

        void init(bool accept, uint8_t ch) {
          memcpy(magic, PAIRING_MAGIC, 4);
          role = PAIRING_ROLE_COORDINATOR;
          accepted = accept ? 1 : 0;
          channel = ch;
          reserved = 0;
        }

        bool isValid() const {
          return memcmp(magic, PAIRING_MAGIC, 4) == 0 && role == PAIRING_ROLE_COORDINATOR;
        }
    };

    static_assert(sizeof(PairingBeacon) == 8, "PairingBeacon must be 8 bytes");
    static_assert(sizeof(PairingRequest) == 8, "PairingRequest must be 8 bytes");
    static_assert(sizeof(PairingConfirm) == 8, "PairingConfirm must be 8 bytes");

  }  // namespace pairing
}  // namespace ungula
