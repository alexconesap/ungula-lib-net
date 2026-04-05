// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#include "connection_manager.h"

#include <logger.h>

namespace ungula {

  ConnectionManager::ConnectionManager(ISessionProvider& session, const ConnectionConfig& config)
      : session_(session),
        config_(config),
        state_(ConnMgrState::UNPAIRED_DISCOVERY),
        lastHeardMs_(0),
        stateEnteredMs_(0),
        nextProbeMs_(0),
        probeCount_(0),
        connected_(false),
        began_(false) {}

  void ConnectionManager::begin(uint32_t nowMs) {
    began_ = true;

    if (session_.hasPairing()) {
      // Have stored pairing — wait for coordinator to come online
      transitionTo(ConnMgrState::PAIRED_DEGRADED, nowMs);
      // Boot grace period: don't probe immediately, let coordinator boot
      nextProbeMs_ = nowMs + config_.bootGracePeriodMs;
    } else {
      // No pairing — start discovery
      transitionTo(ConnMgrState::UNPAIRED_DISCOVERY, nowMs);
      session_.startDiscovery();
    }
  }

  void ConnectionManager::loop(uint32_t nowMs) {
    if (!began_) return;

    switch (state_) {
      case ConnMgrState::UNPAIRED_DISCOVERY:
        handleUnpairedDiscovery(nowMs);
        break;
      case ConnMgrState::PAIRED_CONNECTED:
        handlePairedConnected(nowMs);
        break;
      case ConnMgrState::PAIRED_DEGRADED:
        handlePairedDegraded(nowMs);
        break;
      case ConnMgrState::REACQUIRING_STATIC:
        handleReacquiringStatic(nowMs);
        break;
      case ConnMgrState::REACQUIRING_DYNAMIC:
        handleReacquiringDynamic(nowMs);
        break;
    }
  }

  void ConnectionManager::onHeartbeatReceived(uint32_t nowMs) {
    if (!session_.hasPairing()) return;

    // During broad reacquisition, ignore stale messages — only
    // onReacquisitionSuccess (verified coordinator response) is accepted
    if (state_ == ConnMgrState::REACQUIRING_DYNAMIC) return;

    handleMessageFromCoordinator(nowMs);
  }

  void ConnectionManager::onMessageReceived(uint32_t nowMs) {
    if (!session_.hasPairing()) return;
    if (state_ == ConnMgrState::REACQUIRING_DYNAMIC) return;

    handleMessageFromCoordinator(nowMs);
  }

  void ConnectionManager::onReacquisitionSuccess(uint32_t nowMs) {
    lastHeardMs_ = nowMs;
    connected_ = true;
    session_.resetReacquisition();
    transitionTo(ConnMgrState::PAIRED_CONNECTED, nowMs);
  }

  // --- Private ---

  void ConnectionManager::handleMessageFromCoordinator(uint32_t nowMs) {
    lastHeardMs_ = nowMs;

    if (!connected_) {
      connected_ = true;
      session_.resetReacquisition();
      transitionTo(ConnMgrState::PAIRED_CONNECTED, nowMs);
    }
  }

  void ConnectionManager::transitionTo(ConnMgrState newState, uint32_t nowMs) {
    state_ = newState;
    stateEnteredMs_ = nowMs;
    probeCount_ = 0;
  }

  void ConnectionManager::handleUnpairedDiscovery(uint32_t nowMs) {
    session_.loopDiscovery(nowMs);

    if (session_.isDiscoveryComplete()) {
      connected_ = true;
      lastHeardMs_ = nowMs;
      transitionTo(ConnMgrState::PAIRED_CONNECTED, nowMs);
    }
  }

  void ConnectionManager::handlePairedConnected(uint32_t nowMs) {
    if (lastHeardMs_ > 0 && (nowMs - lastHeardMs_) > config_.heartbeatTimeoutMs) {
      connected_ = false;
      log_warn("ConnMgr: heartbeat timeout (%lums)", config_.heartbeatTimeoutMs);
      transitionTo(ConnMgrState::PAIRED_DEGRADED, nowMs);
    }
  }

  void ConnectionManager::handlePairedDegraded(uint32_t nowMs) {
    if (connected_) {
      transitionTo(ConnMgrState::PAIRED_CONNECTED, nowMs);
      return;
    }

    if ((nowMs - stateEnteredMs_) < config_.degradedGracePeriodMs) {
      return;
    }

    log_warn("ConnMgr: degraded grace expired, starting recovery");
    transitionTo(ConnMgrState::REACQUIRING_STATIC, nowMs);
    nextProbeMs_ = nowMs;
  }

  void ConnectionManager::handleReacquiringStatic(uint32_t nowMs) {
    if (connected_) {
      transitionTo(ConnMgrState::PAIRED_CONNECTED, nowMs);
      return;
    }
    if (nowMs < nextProbeMs_) return;

    session_.sendProbe();
    probeCount_++;
    nextProbeMs_ = nowMs + config_.staticProbeIntervalMs;

    if (config_.policy == ConnectionPolicy::DYNAMIC &&
        probeCount_ >= config_.staticMaxProbes) {
      log_warn("ConnMgr: static probes exhausted (%d), broad reacquisition", probeCount_);
      transitionTo(ConnMgrState::REACQUIRING_DYNAMIC, nowMs);
      session_.startReacquisition();
      nextProbeMs_ = nowMs;
      return;
    }
  }

  void ConnectionManager::handleReacquiringDynamic(uint32_t nowMs) {
    if (connected_) {
      transitionTo(ConnMgrState::PAIRED_CONNECTED, nowMs);
      return;
    }
    if (nowMs < nextProbeMs_) return;

    session_.loopReacquisition(nowMs);
    nextProbeMs_ = nowMs + config_.dynamicProbeIntervalMs;

    if (session_.isReacquisitionComplete()) {
      connected_ = true;
      lastHeardMs_ = nowMs;
      session_.resetReacquisition();
      transitionTo(ConnMgrState::PAIRED_CONNECTED, nowMs);
    }
  }

}  // namespace ungula
