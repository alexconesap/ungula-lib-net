// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// ============================================================================
// HTTP client abstraction for GET/POST requests.
//
// Implementation selected at compile time:
//   ESP_PLATFORM → ESP-IDF esp_http_client
//   otherwise    → libcurl (for desktop tests)
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ungula {
  namespace http {

    /// Result of an HTTP request
    struct HttpResult {
        bool success = false;
        int statusCode = 0;
        char body[1024] = {};  // response body (truncated if larger)
        size_t bodyLen = 0;

        /// Check if the response body contains a substring
        bool bodyContains(const char* needle) const {
          return bodyLen > 0 && strstr(body, needle) != nullptr;
        }
    };

    /// Send an HTTP GET request.
    HttpResult httpGet(const char* url, int timeout_ms = 10000);

    /// Send an HTTP POST request with a JSON body.
    HttpResult httpPost(const char* url, const char* json, size_t json_len, int timeout_ms = 10000);

  }  // namespace http
}  // namespace ungula
