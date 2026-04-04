// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cstdint>

namespace ungula {

  /// Configuration for ConnectionManager timing and retry behavior.
  /// All fields have sensible defaults — construct with ConnectionConfig{}
  /// for standard ESP-NOW reconnection behavior, or override per-project.
  struct ConnectionConfig {
      // Boot-time backoff when trying to reach a stored coordinator
      uint32_t bootInitialDelayMs = 500;
      uint32_t bootMaxDelayMs = 8000;
      uint8_t bootMaxRetries = 5;
      float bootBackoffMultiplier = 2.0F;
      uint32_t bootInitialWaitMs = 3000;  // grace period for coordinator boot

      // Runtime reconnection after heartbeat loss
      uint32_t runtimeInitialDelayMs = 100;
      uint32_t runtimeMaxDelayMs = 2000;
      uint8_t runtimeMaxRetries = 3;
      float runtimeBackoffMultiplier = 2.0F;

      // Heartbeat liveness
      uint32_t heartbeatTimeoutMs = 2000;
  };

}  // namespace ungula
