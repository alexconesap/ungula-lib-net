// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <esp_now.h>

#include "i_transport.h"

namespace ungula {
  namespace comm {

    /// ESP-NOW transport implementation
    class EspNowTransport : public ITransport {
      public:
        EspNowTransport();
        ~EspNowTransport() override;

        TransportError init() override;
        TransportError send(const MacAddress& dst, const uint8_t* data, uint16_t len) override;
        void onReceive(TransportReceiveCallback callback) override;
        void onSendComplete(TransportSendCallback callback) override;
        const MacAddress& getOwnMac() const override;
        TransportError setChannel(uint8_t channel) override;
        uint8_t getChannel() const override;
        TransportError addPeer(const MacAddress& mac, uint8_t channel) override;
        TransportError removePeer(const MacAddress& mac) override;
        bool hasPeer(const MacAddress& mac) const override;

      private:
        bool initialized_;
        MacAddress ownMac_;

        // ESP-NOW requires static C callbacks
        static void onDataRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len);
        static void onDataSentCb(const uint8_t* mac, esp_now_send_status_t status);
    };

  }  // namespace comm
}  // namespace ungula
