// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "esp_now_transport.h"

#include <esp_now.h>
#include <esp_wifi.h>

#include <emblogx/logger.h>

namespace ungula {
  namespace comm {

    // Static callback storage
    static TransportReceiveCallback s_recvCb = nullptr;
    static TransportSendCallback s_sendCb = nullptr;

    EspNowTransport::EspNowTransport() : initialized_(false) {
      ownMac_.clear();
    }

    EspNowTransport::~EspNowTransport() {
      if (initialized_) {
        esp_now_deinit();
        initialized_ = false;
      }
    }

    TransportError EspNowTransport::init() {
      if (initialized_) {
        return TransportError::OK;
      }

      // Read own MAC address (STA interface)
      uint8_t mac[MacAddress::ADDR_LEN];
      esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
      if (err != ESP_OK) {
        log_error("esp_wifi_get_mac() failed: %s", esp_err_to_name(err));
        return TransportError::NOT_INITIALIZED;
      }
      ownMac_.copyFrom(mac);

      err = esp_now_init();
      if (err != ESP_OK) {
        log_error("esp_now_init() failed: %s", esp_err_to_name(err));
        return TransportError::NOT_INITIALIZED;
      }

      // Register callbacks
      err = esp_now_register_recv_cb(onDataRecvCb);
      if (err != ESP_OK) {
        log_error("esp_now_register_recv_cb() failed: %s", esp_err_to_name(err));
        esp_now_deinit();
        return TransportError::NOT_INITIALIZED;
      }

      err = esp_now_register_send_cb(onDataSentCb);
      if (err != ESP_OK) {
        log_error("esp_now_register_send_cb() failed: %s", esp_err_to_name(err));
        esp_now_deinit();
        return TransportError::NOT_INITIALIZED;
      }

      initialized_ = true;
      return TransportError::OK;
    }

    TransportError EspNowTransport::send(const MacAddress& dst, const uint8_t* data, uint16_t len) {
      if (!initialized_) {
        return TransportError::NOT_INITIALIZED;
      }
      if ((data == nullptr && len > 0) || len > TRANSPORT_MAX_PAYLOAD) {
        return TransportError::INVALID_ARGUMENT;
      }

      const esp_err_t err = esp_now_send(dst.addr, data, len);
      return (err == ESP_OK) ? TransportError::OK : TransportError::SEND_FAILED;
    }

    void EspNowTransport::onReceive(TransportReceiveCallback callback) {
      s_recvCb = callback;
    }

    void EspNowTransport::onSendComplete(TransportSendCallback callback) {
      s_sendCb = callback;
    }

    const MacAddress& EspNowTransport::getOwnMac() const {
      return ownMac_;
    }

    TransportError EspNowTransport::setChannel(uint8_t channel) {
      if (channel == 0 || channel > 13) {
        return TransportError::INVALID_ARGUMENT;
      }

      esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
      if (err != ESP_OK) {
        log_error("esp_wifi_set_channel(%d) failed: %s", channel, esp_err_to_name(err));
        return TransportError::SEND_FAILED;
      }
      return TransportError::OK;
    }

    uint8_t EspNowTransport::getChannel() const {
      uint8_t primary = 0;
      wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
      esp_wifi_get_channel(&primary, &secondary);
      return primary;
    }

    TransportError EspNowTransport::addPeer(const MacAddress& mac, uint8_t channel) {
      if (!initialized_) {
        return TransportError::NOT_INITIALIZED;
      }

      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, mac.addr, MacAddress::ADDR_LEN);
      peerInfo.channel = channel;
      peerInfo.encrypt = false;

      // Update existing peer if channel changed, or add new
      if (esp_now_is_peer_exist(mac.addr)) {
        esp_now_peer_info_t existing = {};
        if (esp_now_get_peer(mac.addr, &existing) == ESP_OK && existing.channel == channel) {
          return TransportError::OK;  // already on the right channel
        }
        esp_err_t err = esp_now_mod_peer(&peerInfo);
        if (err != ESP_OK) {
          log_error("esp_now_mod_peer failed: %s", esp_err_to_name(err));
          return TransportError::SEND_FAILED;
        }
        return TransportError::OK;
      }

      esp_err_t err = esp_now_add_peer(&peerInfo);
      if (err != ESP_OK) {
        log_error("esp_now_add_peer failed: %s", esp_err_to_name(err));
        return TransportError::SEND_FAILED;
      }
      return TransportError::OK;
    }

    TransportError EspNowTransport::removePeer(const MacAddress& mac) {
      if (!initialized_) {
        return TransportError::NOT_INITIALIZED;
      }

      if (!esp_now_is_peer_exist(mac.addr)) {
        return TransportError::OK;
      }

      esp_err_t err = esp_now_del_peer(mac.addr);
      if (err != ESP_OK) {
        return TransportError::SEND_FAILED;
      }
      return TransportError::OK;
    }

    bool EspNowTransport::hasPeer(const MacAddress& mac) const {
      if (!initialized_) {
        return false;
      }
      return esp_now_is_peer_exist(mac.addr);
    }

    // Static C callback: data received (ESP-IDF v5.1+ signature)
    void EspNowTransport::onDataRecvCb(const esp_now_recv_info_t* info, const uint8_t* data,
                                       int len) {
      if (s_recvCb != nullptr && info != nullptr && data != nullptr && len > 0) {
        const MacAddress srcMac = MacAddress::fromBytes(info->src_addr);
        s_recvCb(srcMac, data, static_cast<uint16_t>(len));
      }
    }

    // Static C callback: data sent
    void EspNowTransport::onDataSentCb(const uint8_t* mac, esp_now_send_status_t status) {
      if (s_sendCb != nullptr && mac != nullptr) {
        const MacAddress dstMac = MacAddress::fromBytes(mac);
        s_sendCb(dstMac, status == ESP_NOW_SEND_SUCCESS);
      }
    }

  }  // namespace comm
}  // namespace ungula
