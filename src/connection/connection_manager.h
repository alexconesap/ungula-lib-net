// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

// Generic connection lifecycle manager for ESP-NOW client nodes.
// Handles boot-time pairing, heartbeat-based liveness detection,
// and resilient reconnection with configurable backoff.
//
// Project-specific probe messages are injected via a callback —
// this class has no dependency on any wire protocol.

#include <cstdint>

#include <comm/i_transport.h>
#include <comm/transport_types.h>
#include <pairing/pairing_client.h>
#include "connection_config.h"
#include "reconnect_messages.h"

namespace ungula {

  /// Connection phase determines timeout strategy
  enum class ConnectionPhase : uint8_t {
    BOOT_STORED_PAIRING = 0,  // Have stored MAC, trying to reach coordinator at boot
    BOOT_SCANNING = 1,        // No stored MAC or retries exhausted, scanning channels
    RUNTIME_CONNECTED = 2,    // Normal operation, monitoring heartbeats
    RUNTIME_RECONNECTING = 3,  // Lost connection, probing current channel
    RUNTIME_CHANNEL_SCAN = 4   // Probes exhausted, scanning all channels
  };

  /// Convert ConnectionPhase to string for logging
  inline const char* connectionPhaseToString(ConnectionPhase phase) {
    switch (phase) {
      case ConnectionPhase::BOOT_STORED_PAIRING:
        return "BOOT_STORED";
      case ConnectionPhase::BOOT_SCANNING:
        return "BOOT_SCANNING";
      case ConnectionPhase::RUNTIME_CONNECTED:
        return "RUNTIME_CONNECTED";
      case ConnectionPhase::RUNTIME_RECONNECTING:
        return "RUNTIME_RECONNECTING";
      case ConnectionPhase::RUNTIME_CHANNEL_SCAN:
        return "RUNTIME_CHANNEL_SCAN";
      default:
        return "UNKNOWN";
    }
  }

  /// Callback type for sending a probe heartbeat to the coordinator.
  /// The application provides this to build and send its own wire-format
  /// heartbeat message. Called with the coordinator MAC and channel.
  /// @param coordMac  MAC address of the paired coordinator
  /// @param channel   WiFi channel the coordinator is on
  /// @param ctx       Opaque context pointer (passed through from constructor)
  using ProbeCallback = void (*)(const comm::MacAddress& coordMac, uint8_t channel, void* ctx);

  /// Generic connection manager for ESP-NOW client nodes.
  /// Reusable across projects — timing is configured via ConnectionConfig,
  /// probe messages are injected via ProbeCallback.
  class ConnectionManager {
    public:
      /// @param transport  Transport layer for peer management
      /// @param pairing    Pairing client (manages scan/pair lifecycle)
      /// @param config     Timing and retry configuration
      /// @param probeCb    Callback to send a probe heartbeat (project-specific)
      /// @param probeCtx   Opaque context passed to probeCb (typically the app instance)
      ConnectionManager(comm::ITransport& transport, pairing::PairingClient& pairing,
                        const ConnectionConfig& config, ProbeCallback probeCb, void* probeCtx);

      /// Call once at boot after pairing client is set up
      void begin(uint32_t nowMs);

      /// Call every loop iteration
      void loop(uint32_t nowMs);

      /// Report a heartbeat received from coordinator
      void onHeartbeatReceived(uint32_t nowMs);

      /// Report any valid message received from coordinator
      void onMessageReceived(uint32_t nowMs);

      /// Handle a reconnect acknowledgment from the coordinator.
      /// Called by the transport receive handler when a ReconnectAck is detected.
      /// @return true if consumed
      bool onReconnectAck(const ReconnectAck& ack, uint32_t nowMs);

      /// Check if connection is healthy
      bool isConnected() const;

      /// Get current phase
      ConnectionPhase getPhase() const;

      /// Get the current WiFi channel from the transport
      uint8_t getChannel() const;

    private:
      comm::ITransport& transport_;
      pairing::PairingClient& pairing_;
      ConnectionConfig config_;
      ProbeCallback probeCb_;
      void* probeCtx_;

      ConnectionPhase phase_;
      uint32_t lastHeardMs_;
      uint32_t phaseStartMs_;
      uint32_t nextRetryMs_;
      uint8_t retryCount_;
      bool connected_;
      bool began_;

      void transitionTo(ConnectionPhase newPhase, uint32_t nowMs);
      void handleBootStoredPairing(uint32_t nowMs);
      void handleBootScanning(uint32_t nowMs);
      void handleRuntimeConnected(uint32_t nowMs);
      void handleRuntimeReconnecting(uint32_t nowMs);
      void handleRuntimeChannelScan(uint32_t nowMs);
      void sendProbe();
      void sendProbeOnChannel(uint8_t channel);
      void advanceScanChannel();
      uint32_t calculateBackoffDelay(uint32_t initialMs, float multiplier, uint32_t maxMs) const;

      // Channel scan state (used in RUNTIME_CHANNEL_SCAN)
      uint8_t channelScanIndex_;
  };

}  // namespace ungula
