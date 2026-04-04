// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <cstdint>
#include <cstring>

namespace ungula {
  namespace comm {

    /// MAC address wrapper - 6-byte hardware address (POD type for packed structs)
    /// Usage:
    /// uint8_t raw1[6] = {0x10,0x20,0x30,0x40,0x50,0x60};
    /// uint8_t raw2[6] = {0x10,0x20,0x30,0x40,0x50,0x61};
    /// MacAddress m1 = {};
    /// MacAddress m2 = MacAddress::fromBytes(raw1);
    /// MacAddress m3 = MacAddress::fromBytes(raw1);
    /// MacAddress m4 = MacAddress::fromBytes(raw2);
    /// MacAddress m5 = MacAddress::broadcast();
    /// m5.clear();
    /// assert(m1.isZero());
    /// assert(m2 == m3);
    /// assert(m2 != m4);
    /// assert(m5.isBroadcast());
    ///
    /// static void printMac(const MacAddress& mac) {
    ///     for (int i = 0; i < MacAddress::ADDR_LEN; ++i) {
    ///         printf("%02X", mac.addr[i]);
    ///         if (i < MacAddress::ADDR_LEN - 1) printf(":");
    ///     }
    ///     printf("\n");
    /// }
    struct MacAddress {
        static constexpr uint8_t ADDR_LEN = 6;
        uint8_t addr[ADDR_LEN] = {};

        bool operator==(const MacAddress& other) const noexcept {
          return memcmp(addr, other.addr, ADDR_LEN) == 0;
        }

        bool operator!=(const MacAddress& other) const noexcept {
          return !(*this == other);
        }

        bool isZero() const noexcept {
          static const uint8_t zero[ADDR_LEN] = {0};
          return memcmp(addr, zero, ADDR_LEN) == 0;
        }

        bool isBroadcast() const noexcept {
          static const uint8_t brc[ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
          return memcmp(addr, brc, ADDR_LEN) == 0;
        }

        void clear() {
          memset(addr, 0, ADDR_LEN);
        }

        void copyFrom(const uint8_t* src) {
          if (src != nullptr) {
            memcpy(addr, src, ADDR_LEN);
          } else {
            clear();
          }
        }

        static MacAddress fromBytes(const uint8_t* src) {
          MacAddress mac = {};
          if (src != nullptr) {
            memcpy(mac.addr, src, ADDR_LEN);
          }
          return mac;
        }

        static constexpr MacAddress broadcast() noexcept {
          return MacAddress{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
        }
    };

    /// Transport operation result
    enum class TransportError : uint8_t {
      OK = 0,
      SEND_FAILED = 1,
      NOT_INITIALIZED = 2,
      PEER_NOT_FOUND = 3,
      BUFFER_FULL = 4,
      INVALID_ARGUMENT = 5,
      TIMEOUT = 6
    };

    /// Maximum payload size for a single transport message
    /// ESP-NOW limit is 250 bytes; keep margin
    static constexpr uint16_t TRANSPORT_MAX_PAYLOAD = 240;

    /// Callback for received data
    /// @param srcMac   Sender MAC address
    /// @param data     Pointer to received data (valid only during callback)
    /// @param len      Length of received data in bytes
    using TransportReceiveCallback = void (*)(const MacAddress& srcMac, const uint8_t* data,
                                              uint16_t len);

    /// Callback for send completion
    /// @param dstMac   Destination MAC address
    /// @param success  True if acknowledged
    using TransportSendCallback = void (*)(const MacAddress& dstMac, bool success);

  }  // namespace comm
}  // namespace ungula
