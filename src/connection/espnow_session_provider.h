// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once

// ESP-NOW session provider — implements ISessionProvider for ESP-NOW transport.
// Handles channel-based discovery (via PairingClient), heartbeat probing,
// and channel-scanning reacquisition (via ReconnectProbe/Ack).

#include "i_session_provider.h"
#include "reconnect_messages.h"

#include <comm/i_transport.h>
#include <pairing/pairing_client.h>

namespace ungula {

  /// Callback for sending an application-level probe (heartbeat).
  /// The session provider handles WHERE to send; this callback handles WHAT.
  using ProbeCallback = void (*)(const comm::MacAddress& coordMac, void* ctx);

  class EspNowSessionProvider : public ISessionProvider {
    public:
      /// @param transport  ESP-NOW transport
      /// @param pairing    Pairing client (for discovery and stored coordinator info)
      /// @param probeCb    App callback that sends a heartbeat probe to the coordinator
      /// @param probeCtx   Opaque context for the probe callback
      EspNowSessionProvider(comm::ITransport& transport, pairing::PairingClient& pairing,
                            ProbeCallback probeCb, void* probeCtx);

      // -- ISessionProvider --

      bool hasPairing() const override;
      void startDiscovery() override;
      void loopDiscovery(uint32_t nowMs) override;
      bool isDiscoveryComplete() const override;
      void sendProbe() override;
      void startReacquisition() override;
      bool loopReacquisition(uint32_t nowMs) override;
      bool isReacquisitionComplete() const override;
      void resetReacquisition() override;

      /// Handle a ReconnectAck received from the coordinator.
      /// Call this from the transport receive handler.
      /// @return true if consumed
      bool onReconnectAck(const ReconnectAck& ack);

    private:
      comm::ITransport& transport_;
      pairing::PairingClient& pairing_;
      ProbeCallback probeCb_;
      void* probeCtx_;

      // Reacquisition state
      bool reacquiring_ = false;
      bool reacquisitionDone_ = false;
      uint8_t scanIndex_ = 0;

      void sendReconnectProbeOnChannel(uint8_t channel);
  };

}  // namespace ungula
