// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "pairing_client.h"

#include <cstring>

#include <logger.h>

namespace ungula {
  namespace pairing {

    PairingClient::PairingClient(comm::ITransport& transport, IPreferences& prefs,
                                 const char* prefsNs, uint8_t deviceId)
        : transport_(transport),
          prefs_(prefs),
          prefsNs_(prefsNs),
          deviceId_(deviceId),
          state_(PairingState::IDLE),
          scanChannels_(nullptr),
          scanChannelCount_(0),
          scanIndex_(0),
          currentScanChannel_(1),
          channelStartMs_(0),
          pairingStartMs_(0),
          pairedChannel_(0),
          onPairedCb_(nullptr) {
      coordinatorMac_.clear();
    }

    StoredPairing PairingClient::loadStoredPairing() {
      StoredPairing result;

      prefs_.begin(prefsNs_);

      uint8_t flag = prefs_.getUInt8(PREF_KEY_PAIRED_FLAG, 0);
      if (flag != 1) {
        prefs_.end();
        return result;
      }

      size_t read = prefs_.getBytes(PREF_KEY_PAIRED_MAC, result.coordinatorMac.addr,
                                    comm::MacAddress::ADDR_LEN);
      result.channel = prefs_.getUInt8(PREF_KEY_PAIRED_CHANNEL, 0);

      prefs_.end();

      // Validate channel: must be in the configured scan list (not just 1-13).
      // A stale channel from a previous scan could be outside the list.
      bool channelValid = false;
      if (result.channel > 0 && result.channel <= MAX_SCAN_CHANNELS) {
        if (scanChannels_ && scanChannelCount_ > 0) {
          for (uint8_t i = 0; i < scanChannelCount_; ++i) {
            if (scanChannels_[i] == result.channel) {
              channelValid = true;
              break;
            }
          }
        } else {
          channelValid = true;  // no scan list configured, accept 1-13
        }
      }

      if (read != comm::MacAddress::ADDR_LEN || result.coordinatorMac.isZero()) {
        log_warn("Stored pairing data invalid (read=%d, mac_zero=%d)", (int)read,
                 result.coordinatorMac.isZero() ? 1 : 0);
        return result;
      }

      // MAC is valid — always restore pairing identity
      coordinatorMac_ = result.coordinatorMac;
      state_ = PairingState::PAIRED;
      result.valid = true;

      if (channelValid) {
        // Channel is good — use it directly
        pairedChannel_ = result.channel;
        transport_.setChannel(result.channel);
        transport_.addPeer(result.coordinatorMac, 0);
      } else {
        // Channel is stale/invalid — keep the MAC, set channel to first scan channel.
        // ConnectionManager will fail the initial probes and fall through to
        // RUNTIME_CHANNEL_SCAN, which sends reconnect probes on each valid channel.
        log_warn("Stored channel %d not in scan list, will scan", result.channel);
        uint8_t fallbackCh = (scanChannels_ && scanChannelCount_ > 0) ? scanChannels_[0] : 1;
        pairedChannel_ = fallbackCh;
        result.channel = fallbackCh;
        transport_.setChannel(fallbackCh);
        transport_.addPeer(result.coordinatorMac, 0);
      }

      return result;
    }

    void PairingClient::setScanChannels(const uint8_t* channels, uint8_t count) {
      scanChannels_ = channels;
      scanChannelCount_ = count;
    }

    void PairingClient::startScanning() {
      state_ = PairingState::SCANNING;
      scanIndex_ = 0;
      if (scanChannels_ && scanChannelCount_ > 0) {
        currentScanChannel_ = scanChannels_[0];
      } else {
        currentScanChannel_ = 1;
      }
      channelStartMs_ = 0;
      pairingStartMs_ = 0;
      coordinatorMac_.clear();
      pairedChannel_ = 0;

      transport_.setChannel(currentScanChannel_);
    }

    void PairingClient::stopScanning() {
      if (state_ == PairingState::SCANNING) {
        state_ = PairingState::IDLE;
      }
    }

    bool PairingClient::isScanning() const {
      return state_ == PairingState::SCANNING;
    }

    bool PairingClient::isPaired() const {
      return state_ == PairingState::PAIRED;
    }

    PairingState PairingClient::getState() const {
      return state_;
    }

    void PairingClient::loop(uint32_t nowMs) {
      switch (state_) {
        case PairingState::SCANNING:
          if (channelStartMs_ == 0) {
            channelStartMs_ = nowMs;
          }
          if (nowMs - channelStartMs_ >= CHANNEL_SCAN_DWELL_MS) {
            advanceChannel(nowMs);
          }
          break;

        case PairingState::RESPONDING:
          // Waiting for confirmation
          if (nowMs - pairingStartMs_ >= PAIRING_TIMEOUT_MS) {
            log_warn("Pairing response timeout, restarting scan");
            startScanning();
          }
          break;

        default:
          break;
      }
    }

    bool PairingClient::handleReceived(const comm::MacAddress& srcMac, const uint8_t* data,
                                       uint16_t len, uint32_t nowMs) {
      if (len < 4)
        return false;

      // Check magic bytes
      if (memcmp(data, PAIRING_MAGIC, 4) != 0) {
        return false;
      }

      if (state_ == PairingState::SCANNING && len >= sizeof(PairingBeacon)) {
        auto* beacon = reinterpret_cast<const PairingBeacon*>(data);
        if (beacon->isValid()) {
          handleBeacon(srcMac, *beacon, nowMs);
          return true;
        }
      }

      if (state_ == PairingState::RESPONDING && len >= sizeof(PairingConfirm)) {
        auto* confirm = reinterpret_cast<const PairingConfirm*>(data);
        if (confirm->isValid()) {
          handleConfirm(srcMac, *confirm);
          return true;
        }
      }

      return false;
    }

    void PairingClient::onPaired(OnPairedCallback cb) {
      onPairedCb_ = cb;
    }

    const comm::MacAddress& PairingClient::getCoordinatorMac() const {
      return coordinatorMac_;
    }

    uint8_t PairingClient::getPairedChannel() const {
      return pairedChannel_;
    }

    void PairingClient::setPairedChannel(uint8_t channel) {
      // Validate: only accept channels 1-13
      if (channel == 0 || channel > MAX_SCAN_CHANNELS) {
        log_warn("setPairedChannel: invalid channel %d, ignoring", channel);
        return;
      }
      pairedChannel_ = channel;
      prefs_.begin(prefsNs_);
      prefs_.putUInt8(PREF_KEY_PAIRED_CHANNEL, channel);
      prefs_.end();
    }

    void PairingClient::clearPairing() {
      prefs_.begin(prefsNs_);
      prefs_.remove(PREF_KEY_PAIRED_MAC);
      prefs_.remove(PREF_KEY_PAIRED_CHANNEL);
      prefs_.remove(PREF_KEY_PAIRED_FLAG);
      prefs_.end();

      coordinatorMac_.clear();
      pairedChannel_ = 0;
      state_ = PairingState::IDLE;
    }

    void PairingClient::advanceChannel(uint32_t nowMs) {
      if (scanChannels_ && scanChannelCount_ > 0) {
        scanIndex_++;
        if (scanIndex_ >= scanChannelCount_) {
          scanIndex_ = 0;
        }
        currentScanChannel_ = scanChannels_[scanIndex_];
      } else {
        currentScanChannel_++;
        if (currentScanChannel_ > MAX_SCAN_CHANNELS) {
          currentScanChannel_ = 1;
        }
      }

      transport_.setChannel(currentScanChannel_);
      channelStartMs_ = nowMs;
    }

    void PairingClient::sendPairingRequest(const comm::MacAddress& coordMac) {
      // Add coordinator as peer for sending
      transport_.addPeer(coordMac, 0);

      PairingRequest req;
      req.init(deviceId_);

      transport_.send(coordMac, reinterpret_cast<const uint8_t*>(&req), sizeof(req));
    }

    void PairingClient::handleBeacon(const comm::MacAddress& srcMac, const PairingBeacon& beacon,
                                     uint32_t nowMs) {
      coordinatorMac_ = srcMac;
      pairedChannel_ = beacon.channel;
      state_ = PairingState::RESPONDING;
      pairingStartMs_ = nowMs;

      sendPairingRequest(srcMac);
    }

    void PairingClient::handleConfirm(const comm::MacAddress& srcMac,
                                      const PairingConfirm& confirm) {
      if (confirm.accepted) {
        coordinatorMac_ = srcMac;
        pairedChannel_ = confirm.channel;
        state_ = PairingState::PAIRED;

        // Set to confirmed channel
        transport_.setChannel(pairedChannel_);

        // Persist pairing
        storePairing();

        if (onPairedCb_) {
          onPairedCb_(coordinatorMac_, pairedChannel_);
        }
      } else {
        log_warn("Pairing rejected, restarting scan");
        startScanning();
      }
    }

    void PairingClient::storePairing() {
      prefs_.begin(prefsNs_);
      prefs_.putBytes(PREF_KEY_PAIRED_MAC, coordinatorMac_.addr, comm::MacAddress::ADDR_LEN);
      prefs_.putUInt8(PREF_KEY_PAIRED_CHANNEL, pairedChannel_);
      prefs_.putUInt8(PREF_KEY_PAIRED_FLAG, 1);
      prefs_.end();
    }

  }  // namespace pairing
}  // namespace ungula
