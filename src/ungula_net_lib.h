// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once
#ifndef __cplusplus
#error UngulaNetLib requires a C++ compiler
#endif

// Ungula Networking Library
// Communication, pairing, connection management, WiFi, HTTP

#include <ungula_lib.h>

// Communication (transport interfaces, ESP-NOW, message header)
#include "comm/i_transport.h"
#include "comm/message_header.h"
#include "comm/transport_types.h"

// WiFi types
#include "wifi/wifi_channel.h"

// WiFi AP
#include "wifi/wifi_ap.h"

// WiFi STA (station mode — connect to routers, scan, ESP-NOW init)
#include "wifi/wifi_config.h"
#include "wifi/wifi_sta.h"

// Pairing types
#include "pairing/pairing_types.h"

// HTTP server and client
#include "http/http_client.h"
#include "http/http_server.h"
