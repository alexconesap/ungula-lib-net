// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include "transport_types.h"

namespace ungula {
  namespace comm {

    /// Abstract transport interface.
    /// All communication between nodes goes through this interface.
    class ITransport {
      public:
        virtual ~ITransport() = default;

        /// Initialize the transport layer
        virtual TransportError init() = 0;

        /// Send data to a specific MAC address
        /// @param dst   Destination MAC (broadcast = ff:ff:ff:ff:ff:ff)
        /// @param data  Data buffer
        /// @param len   Length (must be <= TRANSPORT_MAX_PAYLOAD)
        virtual TransportError send(const MacAddress& dst, const uint8_t* data, uint16_t len) = 0;

        /// Register callback for incoming data
        virtual void onReceive(TransportReceiveCallback cb) = 0;

        /// Register callback for send completion
        virtual void onSendComplete(TransportSendCallback cb) = 0;

        /// Get this device's own MAC address
        virtual const MacAddress& getOwnMac() const = 0;

        /// Set the WiFi channel (for ESP-NOW channel alignment)
        /// @param channel  Channel number 1-13 (0 = auto)
        virtual TransportError setChannel(uint8_t channel) = 0;

        /// Get the current channel
        virtual uint8_t getChannel() const = 0;

        /// Add a peer (required by ESP-NOW before unicast send)
        virtual TransportError addPeer(const MacAddress& mac, uint8_t channel = 0) = 0;

        /// Remove a peer
        virtual TransportError removePeer(const MacAddress& mac) = 0;

        /// Check if a peer is registered
        virtual bool hasPeer(const MacAddress& mac) const = 0;
    };

  }  // namespace comm
}  // namespace ungula
