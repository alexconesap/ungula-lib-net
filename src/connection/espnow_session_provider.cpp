// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#include "espnow_session_provider.h"

namespace ungula {

  EspNowSessionProvider::EspNowSessionProvider(comm::ITransport& transport,
                                               pairing::PairingClient& pairing,
                                               ProbeCallback probeCb, void* probeCtx)
      : transport_(transport),
        pairing_(pairing),
        probeCb_(probeCb),
        probeCtx_(probeCtx) {}

  bool EspNowSessionProvider::hasPairing() const {
    return pairing_.isPaired();
  }

  void EspNowSessionProvider::startDiscovery() {
    pairing_.startScanning();
  }

  void EspNowSessionProvider::loopDiscovery(uint32_t nowMs) {
    pairing_.loop(nowMs);
  }

  bool EspNowSessionProvider::isDiscoveryComplete() const {
    return pairing_.isPaired();
  }

  void EspNowSessionProvider::sendProbe() {
    if (!pairing_.isPaired() || probeCb_ == nullptr) return;

    auto coordMac = pairing_.getCoordinatorMac();
    transport_.addPeer(coordMac, 0);
    probeCb_(coordMac, probeCtx_);
  }

  void EspNowSessionProvider::startReacquisition() {
    reacquiring_ = true;
    reacquisitionDone_ = false;
    scanIndex_ = 0;
  }

  bool EspNowSessionProvider::loopReacquisition(uint32_t nowMs) {
    if (!reacquiring_ || !pairing_.isPaired()) return false;

    const uint8_t* channels = pairing_.getScanChannels();
    uint8_t channelCount = pairing_.getScanChannelCount();
    uint8_t channel;

    if (channels && channelCount > 0) {
      channel = channels[scanIndex_ % channelCount];
    } else {
      channel = (scanIndex_ % pairing::MAX_SCAN_CHANNELS) + 1;
    }

    sendReconnectProbeOnChannel(channel);
    scanIndex_++;
    return true;
  }

  bool EspNowSessionProvider::isReacquisitionComplete() const {
    return reacquisitionDone_;
  }

  void EspNowSessionProvider::resetReacquisition() {
    reacquiring_ = false;
    reacquisitionDone_ = false;
    scanIndex_ = 0;
  }

  bool EspNowSessionProvider::onReconnectAck(const ReconnectAck& ack) {
    if (!reacquiring_) return false;

    // Coordinator responded — switch to its channel and update pairing
    transport_.setChannel(ack.channel);
    pairing_.setPairedChannel(ack.channel);

    auto coordMac = pairing_.getCoordinatorMac();
    transport_.addPeer(coordMac, 0);

    reacquisitionDone_ = true;
    return true;
  }

  void EspNowSessionProvider::sendReconnectProbeOnChannel(uint8_t channel) {
    auto coordMac = pairing_.getCoordinatorMac();

    transport_.setChannel(channel);
    transport_.addPeer(coordMac, 0);

    ReconnectProbe probe;
    probe.init(pairing_.getDeviceId());
    transport_.send(coordMac, reinterpret_cast<const uint8_t*>(&probe), sizeof(probe));
  }

}  // namespace ungula
