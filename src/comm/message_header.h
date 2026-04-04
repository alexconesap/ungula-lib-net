// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cstdint>

namespace ungula {
  namespace comm {

    /// Fixed-size message header prepended to every transmitted message.
    /// Total size: 8 bytes.
    struct __attribute__((packed)) MessageHeader {
        uint8_t protocolVersion;  // Protocol version for compatibility
        uint8_t messageType;      // Application-defined message type
        uint8_t sourceDeviceId;   // Sender device ID (application-defined)
        uint8_t sequenceNumber;   // Rolling sequence number (0-255)
        uint8_t flags;            // Bit flags: bit0=requiresAck, bit1=isAck
        uint8_t reserved[3];      // Reserved for future use (must be zero)
    };

    static_assert(sizeof(MessageHeader) == 8, "MessageHeader must be exactly 8 bytes");

    /// Check if a received buffer starts with a valid header
    inline bool isValidHeader(const uint8_t* data, uint16_t len, uint8_t expectedVersion) {
      if (len < sizeof(MessageHeader)) {
        return false;
      }
      const auto* hdr = reinterpret_cast<const MessageHeader*>(data);
      return hdr->protocolVersion == expectedVersion;
    }

    /// Extract header from raw buffer
    inline const MessageHeader* extractHeader(const uint8_t* data, uint16_t len) {
      if (len < sizeof(MessageHeader)) {
        return nullptr;
      }
      return reinterpret_cast<const MessageHeader*>(data);
    }

    /// Extract payload pointer (data after header)
    inline const uint8_t* extractPayload(const uint8_t* data, uint16_t len) {
      if (len <= sizeof(MessageHeader)) {
        return nullptr;
      }
      return data + sizeof(MessageHeader);
    }

    /// Get payload length
    inline uint16_t payloadLength(uint16_t totalLen) {
      if (totalLen <= sizeof(MessageHeader)) {
        return 0;
      }
      return totalLen - sizeof(MessageHeader);
    }

  }  // namespace comm
}  // namespace ungula
