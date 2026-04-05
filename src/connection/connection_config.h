// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once

#include <cstdint>

namespace ungula {

  /// Connection policy — determines how aggressively the node searches
  /// for the coordinator after losing connection.
  enum class ConnectionPolicy : uint8_t {
    /// STATIC: the coordinator is not expected to change transport context.
    /// On loss, retry on last known context. Escalate to broad reacquisition
    /// only after a long threshold (coordinator may just be rebooting).
    STATIC = 0,

    /// DYNAMIC: the coordinator may migrate to a different transport context
    /// at runtime. After a short retry on the current context, escalate to
    /// broad reacquisition (scan all contexts).
    DYNAMIC = 1,
  };

  /// Connection state — explicit FSM states for the connection lifecycle.
  enum class ConnMgrState : uint8_t {
    UNPAIRED_DISCOVERY = 0,    // No pairing — scanning for coordinator
    PAIRED_CONNECTED = 1,      // Healthy connection, monitoring heartbeat
    PAIRED_DEGRADED = 2,       // Heartbeat lost, grace period before acting
    REACQUIRING_STATIC = 3,    // Probing on last known context
    REACQUIRING_DYNAMIC = 4,   // Broad reacquisition across all contexts
  };

  /// Convert ConnMgrState to string for logging
  inline const char* connMgrStateToString(ConnMgrState state) {
    switch (state) {
      case ConnMgrState::UNPAIRED_DISCOVERY:
        return "UNPAIRED_DISCOVERY";
      case ConnMgrState::PAIRED_CONNECTED:
        return "PAIRED_CONNECTED";
      case ConnMgrState::PAIRED_DEGRADED:
        return "PAIRED_DEGRADED";
      case ConnMgrState::REACQUIRING_STATIC:
        return "REACQUIRING_STATIC";
      case ConnMgrState::REACQUIRING_DYNAMIC:
        return "REACQUIRING_DYNAMIC";
      default:
        return "UNKNOWN";
    }
  }

  /// Configuration for ConnectionManager timing and retry behavior.
  struct ConnectionConfig {
      /// Connection policy (STATIC or DYNAMIC)
      ConnectionPolicy policy = ConnectionPolicy::DYNAMIC;

      /// Heartbeat timeout — no messages for this long = degraded
      uint32_t heartbeatTimeoutMs = 2000;

      /// Grace period in PAIRED_DEGRADED before starting recovery.
      /// Covers brief glitches without triggering full reconnection.
      uint32_t degradedGracePeriodMs = 500;

      /// REACQUIRING_STATIC: probe interval on last known context
      uint32_t staticProbeIntervalMs = 1000;

      /// How many static probes before escalating to DYNAMIC.
      /// Only applies when policy == DYNAMIC.
      uint8_t staticMaxProbes = 5;

      /// REACQUIRING_DYNAMIC: interval between broad reacquisition probes
      uint32_t dynamicProbeIntervalMs = 500;

      /// Boot-time grace period before first probe (let coordinator boot)
      uint32_t bootGracePeriodMs = 3000;
  };

}  // namespace ungula
