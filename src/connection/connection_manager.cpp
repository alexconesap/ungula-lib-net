// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "connection_manager.h"

#include <cmath>
#include <logger.h>

namespace ungula {

  ConnectionManager::ConnectionManager(comm::ITransport& transport, pairing::PairingClient& pairing,
                                       const ConnectionConfig& config, ProbeCallback probeCb,
                                       void* probeCtx)
      : transport_(transport),
        pairing_(pairing),
        config_(config),
        probeCb_(probeCb),
        probeCtx_(probeCtx),
        phase_(ConnectionPhase::BOOT_STORED_PAIRING),
        lastHeardMs_(0),
        phaseStartMs_(0),
        nextRetryMs_(0),
        retryCount_(0),
        connected_(false),
        began_(false),
        channelScanIndex_(0) {}

  void ConnectionManager::begin(uint32_t nowMs) {
    began_ = true;

    if (pairing_.isPaired()) {
      transitionTo(ConnectionPhase::BOOT_STORED_PAIRING, nowMs);
      nextRetryMs_ = nowMs + config_.bootInitialWaitMs;
    } else {
      transitionTo(ConnectionPhase::BOOT_SCANNING, nowMs);
      pairing_.startScanning();
    }
  }

  void ConnectionManager::loop(uint32_t nowMs) {
    if (!began_) {
      return;
    }

    switch (phase_) {
      case ConnectionPhase::BOOT_STORED_PAIRING:
        handleBootStoredPairing(nowMs);
        break;
      case ConnectionPhase::BOOT_SCANNING:
        handleBootScanning(nowMs);
        break;
      case ConnectionPhase::RUNTIME_CONNECTED:
        handleRuntimeConnected(nowMs);
        break;
      case ConnectionPhase::RUNTIME_RECONNECTING:
        handleRuntimeReconnecting(nowMs);
        break;
      case ConnectionPhase::RUNTIME_CHANNEL_SCAN:
        handleRuntimeChannelScan(nowMs);
        break;
    }
  }

  void ConnectionManager::onHeartbeatReceived(uint32_t nowMs) {
    if (!pairing_.isPaired()) {
      return;
    }

    // During channel scan, ignore stale/queued messages — only ReconnectAck
    // (via onReconnectAck) should establish connection on the correct channel.
    if (phase_ == ConnectionPhase::RUNTIME_CHANNEL_SCAN) {
      return;
    }

    lastHeardMs_ = nowMs;

    if (!connected_) {
      connected_ = true;
      transitionTo(ConnectionPhase::RUNTIME_CONNECTED, nowMs);
    }
  }

  void ConnectionManager::onMessageReceived(uint32_t nowMs) {
    if (!pairing_.isPaired()) {
      return;
    }

    // During channel scan, ignore stale/queued messages
    if (phase_ == ConnectionPhase::RUNTIME_CHANNEL_SCAN) {
      return;
    }

    lastHeardMs_ = nowMs;

    if (!connected_) {
      connected_ = true;
      transitionTo(ConnectionPhase::RUNTIME_CONNECTED, nowMs);
    }
  }

  bool ConnectionManager::isConnected() const {
    return connected_;
  }

  ConnectionPhase ConnectionManager::getPhase() const {
    return phase_;
  }

  uint8_t ConnectionManager::getChannel() const {
    return transport_.getChannel();
  }

  // --- Private ---

  void ConnectionManager::transitionTo(ConnectionPhase newPhase, uint32_t nowMs) {
    phase_ = newPhase;
    phaseStartMs_ = nowMs;
    retryCount_ = 0;
  }

  void ConnectionManager::handleBootStoredPairing(uint32_t nowMs) {
    if (connected_) {
      return;
    }
    if (nowMs < nextRetryMs_) {
      return;
    }

    sendProbe();
    retryCount_++;

    uint32_t delay;
    if (retryCount_ >= config_.bootMaxRetries) {
      delay = config_.bootMaxDelayMs;
    } else {
      delay = calculateBackoffDelay(config_.bootInitialDelayMs, config_.bootBackoffMultiplier,
                                    config_.bootMaxDelayMs);
    }
    nextRetryMs_ = nowMs + delay;
  }

  void ConnectionManager::handleBootScanning(uint32_t nowMs) {
    pairing_.loop(nowMs);

    if (pairing_.isPaired()) {
      connected_ = true;
      transitionTo(ConnectionPhase::RUNTIME_CONNECTED, nowMs);
      lastHeardMs_ = nowMs;
    }
  }

  void ConnectionManager::handleRuntimeConnected(uint32_t nowMs) {
    if (lastHeardMs_ > 0 && (nowMs - lastHeardMs_) > config_.heartbeatTimeoutMs) {
      connected_ = false;
      log_warn("ConnMgr: heartbeat timeout (%lums)", config_.heartbeatTimeoutMs);
      transitionTo(ConnectionPhase::RUNTIME_RECONNECTING, nowMs);
      nextRetryMs_ = nowMs + config_.runtimeInitialDelayMs;
    }
  }

  void ConnectionManager::handleRuntimeReconnecting(uint32_t nowMs) {
    if (connected_) {
      return;
    }
    if (nowMs < nextRetryMs_) {
      return;
    }

    // Send a heartbeat probe on the stored channel. The coordinator may
    // just be temporarily offline (rebooting) — same channel, just not
    // responding yet.
    sendProbe();
    retryCount_++;

    if (retryCount_ >= config_.runtimeMaxRetries) {
      // Heartbeat probes exhausted. Switch to reconnect probes that cycle
      // through ALL channels (including the stored one). The coordinator
      // always responds to reconnect probes from known MACs.
      log_warn("ConnMgr: %d probes failed, starting channel scan", retryCount_);
      channelScanIndex_ = 0;
      transitionTo(ConnectionPhase::RUNTIME_CHANNEL_SCAN, nowMs);
      nextRetryMs_ = nowMs;
      return;
    }

    uint32_t delay = calculateBackoffDelay(config_.runtimeInitialDelayMs,
                                           config_.runtimeBackoffMultiplier,
                                           config_.runtimeMaxDelayMs);
    nextRetryMs_ = nowMs + delay;
  }

  void ConnectionManager::handleRuntimeChannelScan(uint32_t nowMs) {
    if (connected_) {
      return;
    }
    if (nowMs < nextRetryMs_) {
      return;
    }

    // Cycle through ALL channels (including the stored one) and send
    // reconnect probes. The stored channel is tried first and most
    // frequently since it's the most likely one (coordinator just rebooted).
    const uint8_t* channels = pairing_.getScanChannels();
    uint8_t channelCount = pairing_.getScanChannelCount();
    uint8_t channel;

    if (channels && channelCount > 0) {
      channel = channels[channelScanIndex_ % channelCount];
    } else {
      channel = (channelScanIndex_ % pairing::MAX_SCAN_CHANNELS) + 1;
    }

    sendProbeOnChannel(channel);
    retryCount_++;
    channelScanIndex_++;

    // 500ms dwell per channel to allow the coordinator to respond
    nextRetryMs_ = nowMs + 500;
  }

  bool ConnectionManager::onReconnectAck(const ReconnectAck& ack, uint32_t nowMs) {
    if (phase_ != ConnectionPhase::RUNTIME_CHANNEL_SCAN) {
      return false;
    }

    transport_.setChannel(ack.channel);
    pairing_.setPairedChannel(ack.channel);

    auto coordMac = pairing_.getCoordinatorMac();
    transport_.addPeer(coordMac, 0);

    connected_ = true;
    lastHeardMs_ = nowMs;
    transitionTo(ConnectionPhase::RUNTIME_CONNECTED, nowMs);
    return true;
  }

  void ConnectionManager::sendProbeOnChannel(uint8_t channel) {
    if (!pairing_.isPaired()) {
      return;
    }

    auto coordMac = pairing_.getCoordinatorMac();
    transport_.setChannel(channel);
    transport_.addPeer(coordMac, 0);

    ReconnectProbe probe;
    probe.init(pairing_.getDeviceId());
    transport_.send(coordMac, reinterpret_cast<const uint8_t*>(&probe), sizeof(probe));
  }

  void ConnectionManager::advanceScanChannel() {
    channelScanIndex_++;
  }

  void ConnectionManager::sendProbe() {
    if (!pairing_.isPaired() || probeCb_ == nullptr) {
      return;
    }

    auto coordMac = pairing_.getCoordinatorMac();
    uint8_t channel = pairing_.getPairedChannel();
    transport_.addPeer(coordMac, 0);

    probeCb_(coordMac, channel, probeCtx_);
  }

  uint32_t ConnectionManager::calculateBackoffDelay(uint32_t initialMs, float multiplier,
                                                    uint32_t maxMs) const {
    uint32_t delay = initialMs;
    for (uint8_t i = 0; i < retryCount_; ++i) {
      delay = static_cast<uint32_t>(std::lround(delay * multiplier));
      if (delay > maxMs) {
        delay = maxMs;
        break;
      }
    }
    return delay;
  }

}  // namespace ungula
