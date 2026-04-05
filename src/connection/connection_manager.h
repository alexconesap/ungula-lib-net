// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once

// Transport-agnostic connection lifecycle manager.
//
// Manages the connection state machine for a client node that communicates
// with a remote coordinator. All transport-specific logic (channel scanning,
// peer management, reconnect probes) is delegated to an ISessionProvider.
//
// The connection manager only knows about:
//   - heartbeats (has the coordinator spoken recently?)
//   - probes (ask the session provider to try reaching the coordinator)
//   - discovery (ask the session provider to find a new coordinator)
//   - state transitions based on timeouts and events

#include <cstdint>

#include "connection_config.h"
#include "i_session_provider.h"

namespace ungula {

  class ConnectionManager {
    public:
      /// @param session  Session provider (handles transport-specific discovery/reconnection)
      /// @param config   Timing, retry, and policy configuration
      ConnectionManager(ISessionProvider& session, const ConnectionConfig& config);

      /// Call once at boot after the session provider is ready
      void begin(uint32_t nowMs);

      /// Call every loop iteration
      void loop(uint32_t nowMs);

      /// Notify that a heartbeat was received from the coordinator
      void onHeartbeatReceived(uint32_t nowMs);

      /// Notify that any valid message was received from the coordinator
      void onMessageReceived(uint32_t nowMs);

      /// Notify that a reacquisition response was received (coordinator found
      /// on a new context). Called by the session provider or transport handler.
      void onReacquisitionSuccess(uint32_t nowMs);

      /// Check if the connection is healthy
      bool isConnected() const { return connected_; }

      /// Get the current state
      ConnMgrState getState() const { return state_; }

    private:
      ISessionProvider& session_;
      ConnectionConfig config_;

      ConnMgrState state_;
      uint32_t lastHeardMs_;
      uint32_t stateEnteredMs_;
      uint32_t nextProbeMs_;
      uint8_t probeCount_;
      bool connected_;
      bool began_;

      void transitionTo(ConnMgrState newState, uint32_t nowMs);
      void handleUnpairedDiscovery(uint32_t nowMs);
      void handlePairedConnected(uint32_t nowMs);
      void handlePairedDegraded(uint32_t nowMs);
      void handleReacquiringStatic(uint32_t nowMs);
      void handleReacquiringDynamic(uint32_t nowMs);

      /// Common handler for heartbeat/message received — restores connection
      void handleMessageFromCoordinator(uint32_t nowMs);
  };

}  // namespace ungula
