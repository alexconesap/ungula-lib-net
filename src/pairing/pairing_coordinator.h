// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <comm/i_transport.h>
#include <connection/reconnect_messages.h>
#include <preferences/core/i_preferences.h>
#include "pairing_beacon.h"
#include "pairing_types.h"

namespace ungula {
  namespace pairing {

    /// Maximum number of paired clients
    static constexpr uint8_t MAX_PAIRED_CLIENTS = 2;

    /// Info about one paired client
    struct PairedClientInfo {
        comm::MacAddress mac;
        uint8_t deviceId;
        bool active;

        PairedClientInfo() : deviceId(0), active(false) {
          mac.clear();
        }
    };

    /// Event data for a newly paired client
    struct PairedClientEvent {
        comm::MacAddress mac;
        uint8_t deviceId;
    };

    /// Callback when a new client pairs
    using OnClientPairedCallback = void (*)(const PairedClientEvent& event);

    /// Coordinator-side pairing manager
    class PairingCoordinator {
      public:
        /// @param transport   Transport layer
        /// @param prefs       Preferences for persistence
        /// @param prefsNs     Namespace for pairing data
        PairingCoordinator(comm::ITransport& transport, IPreferences& prefs, const char* prefsNs);

        /// Load previously paired clients from preferences
        void loadPairedClients();

        /// Enable pairing mode (start broadcasting beacons)
        void enablePairing();

        /// Disable pairing mode
        void disablePairing();

        /// Check if pairing mode is active
        bool isPairingEnabled() const;

        /// Must be called from the main loop
        void loop(uint32_t nowMs);

        /// Handle a received message
        /// @return true if consumed as a pairing message
        bool handleReceived(const comm::MacAddress& srcMac, const uint8_t* data, uint16_t len);

        /// Register callback for when a client pairs
        void onClientPaired(OnClientPairedCallback cb);

        /// Get paired client info by index
        const PairedClientInfo* getPairedClient(uint8_t index) const;

        /// Check if a MAC is paired
        bool isPaired(const comm::MacAddress& mac) const;

        /// Get number of active paired clients
        uint8_t pairedClientCount() const;

        /// Unpair all clients
        void unpairAll();

      private:
        comm::ITransport& transport_;
        IPreferences& prefs_;
        const char* prefsNs_;

        bool pairingEnabled_;
        uint32_t lastBeaconMs_;
        PairedClientInfo clients_[MAX_PAIRED_CLIENTS];
        OnClientPairedCallback onPairedCb_;

        void broadcastBeacon();
        void handlePairingRequest(const comm::MacAddress& srcMac, const PairingRequest& req);
        void handleReconnectProbe(const comm::MacAddress& srcMac, const ReconnectProbe& probe);
        bool storePairedClient(const comm::MacAddress& mac, uint8_t deviceId);
    };

  }  // namespace pairing
}  // namespace ungula
