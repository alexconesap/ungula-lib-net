// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once
#ifndef __cplusplus
#error UngulaNetLib requires a C++ compiler
#endif

// Ungula Networking Library
// WiFi AP, HTTP+WebSocket server, HTTP client

#include <ungula_lib.h>

// WiFi AP
#include "wifi/wifi_ap.h"

// WiFi STA (station mode)
#ifdef ENABLE_WIFI_STA
#include "wifi/wifi_config.h"
#include "wifi/wifi_sta.h"
#endif

// HTTP + WebSocket server (only for nodes that serve a web UI)
#ifdef ENABLE_HTTP_SERVER
#include "http/http_client.h"
#include "http/http_server.h"
#endif
