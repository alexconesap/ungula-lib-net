// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "pairing_coordinator.h"

#include <cstring>

#include <emblogx/logger.h>

namespace ungula {
  namespace pairing {

    PairingCoordinator::PairingCoordinator(comm::ITransport& transport, IPreferences& prefs,
                                           const char* prefsNs)
        : transport_(transport),
          prefs_(prefs),
          prefsNs_(prefsNs),
          pairingEnabled_(false),
          lastBeaconMs_(0),
          onPairedCb_(nullptr) {}

    void PairingCoordinator::loadPairedClients() {
      prefs_.begin(prefsNs_);

      for (uint8_t i = 0; i < MAX_PAIRED_CLIENTS; ++i) {
        char macKey[16];
        char idKey[16];
        snprintf(macKey, sizeof(macKey), "pair_mac_%d", i);
        snprintf(idKey, sizeof(idKey), "pair_id_%d", i);

        size_t read = prefs_.getBytes(macKey, clients_[i].mac.addr, comm::MacAddress::ADDR_LEN);
        if (read == comm::MacAddress::ADDR_LEN && !clients_[i].mac.isZero()) {
          clients_[i].deviceId = prefs_.getUInt8(idKey, 0);
          clients_[i].active = true;

          // Add as ESP-NOW peer
          transport_.addPeer(clients_[i].mac, 0);
        }
      }

      prefs_.end();
    }

    void PairingCoordinator::enablePairing() {
      pairingEnabled_ = true;
      lastBeaconMs_ = 0;
    }

    void PairingCoordinator::disablePairing() {
      pairingEnabled_ = false;
    }

    bool PairingCoordinator::isPairingEnabled() const {
      return pairingEnabled_;
    }

    void PairingCoordinator::loop(uint32_t nowMs) {
      if (!pairingEnabled_) {
        return;
      }

      if (nowMs - lastBeaconMs_ >= BEACON_INTERVAL_MS) {
        lastBeaconMs_ = nowMs;
        broadcastBeacon();
      }
    }

    bool PairingCoordinator::handleReceived(const comm::MacAddress& srcMac, const uint8_t* data,
                                            uint16_t len) {
      if (len < 8)
        return false;

      // Check for reconnect probe (always active, no pairing mode needed)
      if (memcmp(data, RECONNECT_MAGIC, 4) == 0) {
        auto* probe = reinterpret_cast<const ReconnectProbe*>(data);
        if (probe->isValid()) {
          handleReconnectProbe(srcMac, *probe);
          return true;
        }
      }

      // Check for pairing request (uses pairing magic)
      if (memcmp(data, PAIRING_MAGIC, 4) == 0) {
        auto* req = reinterpret_cast<const PairingRequest*>(data);
        if (req->isValid()) {
          handlePairingRequest(srcMac, *req);
          return true;
        }
      }

      return false;
    }

    void PairingCoordinator::onClientPaired(OnClientPairedCallback cb) {
      onPairedCb_ = cb;
    }

    const PairedClientInfo* PairingCoordinator::getPairedClient(uint8_t index) const {
      if (index >= MAX_PAIRED_CLIENTS)
        return nullptr;
      if (!clients_[index].active)
        return nullptr;
      return &clients_[index];
    }

    bool PairingCoordinator::isPaired(const comm::MacAddress& mac) const {
      for (uint8_t i = 0; i < MAX_PAIRED_CLIENTS; ++i) {
        if (clients_[i].active && clients_[i].mac == mac) {
          return true;
        }
      }
      return false;
    }

    uint8_t PairingCoordinator::pairedClientCount() const {
      uint8_t count = 0;
      for (uint8_t i = 0; i < MAX_PAIRED_CLIENTS; ++i) {
        if (clients_[i].active)
          ++count;
      }
      return count;
    }

    void PairingCoordinator::unpairAll() {
      prefs_.begin(prefsNs_);

      for (uint8_t i = 0; i < MAX_PAIRED_CLIENTS; ++i) {
        if (clients_[i].active) {
          transport_.removePeer(clients_[i].mac);
        }

        char macKey[16];
        char idKey[16];
        snprintf(macKey, sizeof(macKey), "pair_mac_%d", i);
        snprintf(idKey, sizeof(idKey), "pair_id_%d", i);
        if (prefs_.hasKey(macKey))
          prefs_.remove(macKey);
        if (prefs_.hasKey(idKey))
          prefs_.remove(idKey);

        clients_[i] = PairedClientInfo();
      }

      prefs_.end();
    }

    void PairingCoordinator::broadcastBeacon() {
      PairingBeacon beacon;
      beacon.init(transport_.getChannel());

      // Add broadcast peer if not already present
      comm::MacAddress bcast = comm::MacAddress::broadcast();
      transport_.addPeer(bcast, 0);

      transport_.send(bcast, reinterpret_cast<const uint8_t*>(&beacon), sizeof(beacon));
    }

    void PairingCoordinator::handlePairingRequest(const comm::MacAddress& srcMac,
                                                  const PairingRequest& req) {
      // Store and confirm
      bool stored = storePairedClient(srcMac, req.deviceId);

      // Add as peer for sending confirmation
      transport_.addPeer(srcMac, 0);

      // Send confirmation
      PairingConfirm confirm;
      confirm.init(stored, transport_.getChannel());
      transport_.send(srcMac, reinterpret_cast<const uint8_t*>(&confirm), sizeof(confirm));

      if (stored && onPairedCb_) {
        onPairedCb_(PairedClientEvent{.mac = srcMac, .deviceId = req.deviceId});
      }
    }

    bool PairingCoordinator::storePairedClient(const comm::MacAddress& mac, uint8_t deviceId) {
      // Check if already paired (update)
      for (uint8_t i = 0; i < MAX_PAIRED_CLIENTS; ++i) {
        if (clients_[i].active && clients_[i].mac == mac) {
          clients_[i].deviceId = deviceId;
          return true;
        }
      }

      // Find empty slot
      int8_t slot = -1;
      for (uint8_t i = 0; i < MAX_PAIRED_CLIENTS; ++i) {
        if (!clients_[i].active) {
          slot = i;
          break;
        }
      }

      // No empty slot: replace existing slot with same deviceId (re-pairing)
      if (slot < 0) {
        for (uint8_t i = 0; i < MAX_PAIRED_CLIENTS; ++i) {
          if (clients_[i].active && clients_[i].deviceId == deviceId) {
            transport_.removePeer(clients_[i].mac);
            slot = i;
            break;
          }
        }
      }

      if (slot < 0) {
        log_warn("No pairing slot available");
        return false;
      }

      clients_[slot].mac = mac;
      clients_[slot].deviceId = deviceId;
      clients_[slot].active = true;

      // Persist
      prefs_.begin(prefsNs_);

      char macKey[16];
      char idKey[16];
      snprintf(macKey, sizeof(macKey), "pair_mac_%d", slot);
      snprintf(idKey, sizeof(idKey), "pair_id_%d", slot);
      prefs_.putBytes(macKey, mac.addr, comm::MacAddress::ADDR_LEN);
      prefs_.putUInt8(idKey, deviceId);

      prefs_.end();

      return true;
    }

    void PairingCoordinator::handleReconnectProbe(const comm::MacAddress& srcMac,
                                                   const ReconnectProbe& probe) {
      // Only respond to known/paired MAC addresses — reject unknown devices
      if (!isPaired(srcMac)) {
        log_warn("Reconnect probe from unknown MAC %02X:%02X:%02X:%02X:%02X:%02X, ignoring",
                 srcMac.addr[0], srcMac.addr[1], srcMac.addr[2],
                 srcMac.addr[3], srcMac.addr[4], srcMac.addr[5]);
        return;
      }

      uint8_t currentChannel = transport_.getChannel();

      // Register peer on the channel the probe arrived on so we can reply
      transport_.addPeer(srcMac, 0);

      // Send acknowledgment with our current channel
      ReconnectAck ack;
      ack.init(currentChannel);
      transport_.send(srcMac, reinterpret_cast<const uint8_t*>(&ack), sizeof(ack));
    }

  }  // namespace pairing
}  // namespace ungula
