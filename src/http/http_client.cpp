// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// HTTP client — platform-specific implementations.

#include "http_client.h"

// =============================================================================
// ESP-IDF implementation
// =============================================================================
#if defined(ESP_PLATFORM) && defined(ENABLE_HTTP_SERVER)

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>

static const char* TAG = "http_client";

namespace ungula {
  namespace http {

    // Collect response body into HttpResult
    static esp_err_t on_data(esp_http_client_event_t* evt) {
      if (evt->event_id == HTTP_EVENT_ON_DATA) {
        auto* result = static_cast<HttpResult*>(evt->user_data);
        size_t space = sizeof(result->body) - result->bodyLen - 1;
        if (space > 0) {
          size_t copy = evt->data_len < space ? evt->data_len : space;
          memcpy(result->body + result->bodyLen, evt->data, copy);
          result->bodyLen += copy;
          result->body[result->bodyLen] = '\0';
        }
      }
      return ESP_OK;
    }

    HttpResult httpGet(const char* url, int timeout_ms) {
      HttpResult result;

      esp_http_client_config_t config = {};
      config.url = url;
      config.method = HTTP_METHOD_GET;
      config.timeout_ms = timeout_ms;
      config.event_handler = on_data;
      config.user_data = &result;
      config.crt_bundle_attach = esp_crt_bundle_attach;

      esp_http_client_handle_t client = esp_http_client_init(&config);
      if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to init HTTP client for %s", url);
        return result;
      }

      esp_err_t err = esp_http_client_perform(client);
      if (err == ESP_OK) {
        result.statusCode = esp_http_client_get_status_code(client);
        result.success = (result.statusCode >= 200 && result.statusCode < 300);
      } else {
        ESP_LOGW(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
      }

      esp_http_client_cleanup(client);
      return result;
    }

    HttpResult httpPost(const char* url, const char* json, size_t json_len, int timeout_ms) {
      HttpResult result;

      esp_http_client_config_t config = {};
      config.url = url;
      config.method = HTTP_METHOD_POST;
      config.timeout_ms = timeout_ms;
      config.event_handler = on_data;
      config.user_data = &result;
      config.crt_bundle_attach = esp_crt_bundle_attach;

      esp_http_client_handle_t client = esp_http_client_init(&config);
      if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to init HTTP client for %s", url);
        return result;
      }

      esp_http_client_set_header(client, "Content-Type", "application/json");
      esp_http_client_set_post_field(client, json, static_cast<int>(json_len));

      esp_err_t err = esp_http_client_perform(client);
      if (err == ESP_OK) {
        result.statusCode = esp_http_client_get_status_code(client);
        result.success = (result.statusCode >= 200 && result.statusCode < 300);
      } else {
        ESP_LOGW(TAG, "POST to %s failed: %s", url, esp_err_to_name(err));
      }

      esp_http_client_cleanup(client);
      return result;
    }

  }  // namespace http
}  // namespace ungula

// =============================================================================
// Desktop implementation (libcurl) — for testing only
// =============================================================================
#elif !defined(ESP_PLATFORM)

#include <curl/curl.h>
#include <cstdio>

namespace ungula {
  namespace http {

    struct CurlWriteCtx {
        HttpResult* result;
    };

    static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
      auto* ctx = static_cast<CurlWriteCtx*>(userdata);
      size_t total = size * nmemb;
      size_t space = sizeof(ctx->result->body) - ctx->result->bodyLen - 1;
      if (space > 0) {
        size_t copy = total < space ? total : space;
        memcpy(ctx->result->body + ctx->result->bodyLen, ptr, copy);
        ctx->result->bodyLen += copy;
        ctx->result->body[ctx->result->bodyLen] = '\0';
      }
      return total;
    }

    HttpResult httpGet(const char* url, int timeout_ms) {
      HttpResult result;
      CURL* curl = curl_easy_init();
      if (!curl)
        return result;

      CurlWriteCtx ctx = {&result};
      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

      CURLcode res = curl_easy_perform(curl);
      if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        result.statusCode = static_cast<int>(code);
        result.success = (code >= 200 && code < 300);
      }

      curl_easy_cleanup(curl);
      return result;
    }

    HttpResult httpPost(const char* url, const char* json, size_t json_len, int timeout_ms) {
      HttpResult result;
      CURL* curl = curl_easy_init();
      if (!curl)
        return result;

      CurlWriteCtx ctx = {&result};
      struct curl_slist* headers = nullptr;
      headers = curl_slist_append(headers, "Content-Type: application/json");

      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_len));
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

      CURLcode res = curl_easy_perform(curl);
      if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        result.statusCode = static_cast<int>(code);
        result.success = (code >= 200 && code < 300);
      }

      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return result;
    }

  }  // namespace http
}  // namespace ungula

#endif  // ESP_PLATFORM
