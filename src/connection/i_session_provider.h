// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once

#include <cstdint>

namespace ungula {

  /// Abstract session provider — manages discovery, probing, and reacquisition
  /// for a connection manager. The implementation handles transport-specific
  /// details (channels, peers, pairing protocols) so the connection manager
  /// remains transport-agnostic.
  class ISessionProvider {
    public:
      virtual ~ISessionProvider() = default;

      // -- Query --

      /// Does this node have a stored pairing (knows who its coordinator is)?
      virtual bool hasPairing() const = 0;

      // -- Discovery (unpaired → find and pair with coordinator) --

      /// Start the discovery/pairing process (scan for coordinator)
      virtual void startDiscovery() = 0;

      /// Drive the discovery process (call every loop iteration while discovering)
      virtual void loopDiscovery(uint32_t nowMs) = 0;

      /// Has discovery/pairing completed successfully?
      virtual bool isDiscoveryComplete() const = 0;

      // -- Probing (send a keepalive probe on the current session context) --

      /// Send a probe/heartbeat to the coordinator on the current context.
      /// Used during REACQUIRING_STATIC to check if the coordinator is back.
      virtual void sendProbe() = 0;

      // -- Reacquisition (paired but lost connection) --

      /// Start directed reacquisition — try to reach coordinator using known
      /// session context plus neighboring contexts. Does NOT do a full scan.
      virtual void startReacquisition() = 0;

      /// Drive the reacquisition process (call every loop iteration)
      /// @return true if a reacquisition probe was sent this iteration
      virtual bool loopReacquisition(uint32_t nowMs) = 0;

      /// Has the coordinator responded to a reacquisition attempt?
      virtual bool isReacquisitionComplete() const = 0;

      /// Reset reacquisition state (called when transitioning out of reacquisition)
      virtual void resetReacquisition() = 0;
  };

}  // namespace ungula
