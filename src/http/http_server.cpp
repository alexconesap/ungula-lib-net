// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// HTTP + WebSocket server implementation using ESP-IDF httpd.

#if defined(ESP_PLATFORM)

#include "http_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <unistd.h>

#include <cstring>

static const char* TAG = "http";

namespace ungula {
  namespace http {

    // --- Forward declarations for friend access ---
    void httpd_dispatch(void* server_ptr, void* req_ptr);
    void httpd_ws_connect(void* server_ptr, int fd_val);
    void httpd_ws_disconnect(void* server_ptr, int fd_val);

    // Global pointer to the server instance (single instance per system)
    static HttpServer* s_instance = nullptr;

    // --- HttpRequest implementation ---

    void HttpRequest::send(int code, const char* content_type, const char* body_text) {
      auto* req = static_cast<httpd_req_t*>(impl_);
      httpd_resp_set_status(req, code == 200   ? "200 OK"
                                 : code == 404 ? "404 Not Found"
                                 : code == 400 ? "400 Bad Request"
                                 : code == 500 ? "500 Internal Server Error"
                                               : "200 OK");
      httpd_resp_set_type(req, content_type);
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      httpd_resp_send(req, body_text, body_text ? strlen(body_text) : 0);
    }

    void HttpRequest::sendProgmem(int code, const char* content_type, const char* progmem_data) {
      // On ESP32, PROGMEM is just regular flash-mapped memory — same as send()
      send(code, content_type, progmem_data);
    }

    void HttpRequest::sendJson(int code, const char* json) {
      send(code, "application/json", json);
    }

    bool HttpRequest::hasParam(const char* name) const {
      for (int idx = 0; idx < paramCount_; ++idx) {
        if (strcmp(params_[idx].name, name) == 0)
          return true;
      }
      return false;
    }

    const char* HttpRequest::param(const char* name) const {
      for (int idx = 0; idx < paramCount_; ++idx) {
        if (strcmp(params_[idx].name, name) == 0)
          return params_[idx].value;
      }
      return "";
    }

    const char* HttpRequest::uri() const {
      return uri_;
    }

    const char* HttpRequest::body() const {
      return body_;
    }

    // --- Parse query string into HttpRequest params ---

    static void parseQueryParams(httpd_req_t* req, HttpRequest& out) {
      out.paramCount_ = 0;
      size_t query_len = httpd_req_get_url_query_len(req);
      if (query_len == 0)
        return;

      char query[256];
      if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
        return;

      // Parse key=value pairs separated by &
      char* saveptr = nullptr;
      char* token = strtok_r(query, "&", &saveptr);
      while (token && out.paramCount_ < HttpRequest::MAX_PARAMS) {
        char* eq_sign = strchr(token, '=');
        if (eq_sign) {
          *eq_sign = '\0';
          strncpy(out.params_[out.paramCount_].name, token, sizeof(out.params_[0].name) - 1);
          strncpy(out.params_[out.paramCount_].value, eq_sign + 1,
                  sizeof(out.params_[0].value) - 1);
          out.paramCount_++;
        }
        token = strtok_r(nullptr, "&", &saveptr);
      }
    }

    // --- Read POST body ---

    static void readBody(httpd_req_t* req, HttpRequest& out) {
      out.body_[0] = '\0';
      int content_len = req->content_len;
      if (content_len <= 0)
        return;
      if (content_len >= static_cast<int>(sizeof(out.body_))) {
        content_len = sizeof(out.body_) - 1;
      }
      int received = httpd_req_recv(req, out.body_, content_len);
      if (received > 0) {
        out.body_[received] = '\0';
      }
    }

    // --- Generic httpd handler that dispatches to registered routes ---

    static esp_err_t generic_handler(httpd_req_t* req) {
      if (!s_instance)
        return ESP_FAIL;

      // Convert httpd method to our Method enum
      Method method;
      switch (req->method) {
        case HTTP_GET:
          method = Method::GET;
          break;
        case HTTP_POST:
          method = Method::POST;
          break;
        case HTTP_PUT:
          method = Method::PUT;
          break;
        case HTTP_DELETE:
          method = Method::DELETE_;
          break;
        default:
          return ESP_FAIL;
      }

      // Find matching route
      // The URI from httpd_req_t includes query string — strip it for matching
      char path[128];
      const char* query_start = strchr(req->uri, '?');
      if (query_start) {
        size_t path_len = query_start - req->uri;
        if (path_len >= sizeof(path))
          path_len = sizeof(path) - 1;
        memcpy(path, req->uri, path_len);
        path[path_len] = '\0';
      } else {
        strncpy(path, req->uri, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
      }

      RouteHandler handler = nullptr;
      for (int idx = 0; idx < s_instance->routeCount_; ++idx) {
        if (s_instance->routes_[idx].method == method &&
            strcmp(s_instance->routes_[idx].path, path) == 0) {
          handler = s_instance->routes_[idx].handler;
          break;
        }
      }

      if (!handler) {
        if (s_instance->notFoundHandler_) {
          handler = s_instance->notFoundHandler_;
        } else {
          httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
          return ESP_OK;
        }
      }

      // Build HttpRequest
      HttpRequest http_req;
      http_req.impl_ = req;
      strncpy(http_req.uri_, path, sizeof(http_req.uri_) - 1);
      parseQueryParams(req, http_req);
      if (method == Method::POST || method == Method::PUT) {
        readBody(req, http_req);
      }

      handler(http_req);
      return ESP_OK;
    }

    // --- WebSocket handler ---

    static esp_err_t ws_handler(httpd_req_t* req) {
      if (!s_instance)
        return ESP_FAIL;

      if (req->method == HTTP_GET) {
        // New WS connection
        httpd_ws_connect(s_instance, httpd_req_to_sockfd(req));
        return ESP_OK;
      }

      // Receive frame — drain it (broadcast-only server)
      httpd_ws_frame_t ws_pkt;
      memset(&ws_pkt, 0, sizeof(ws_pkt));
      ws_pkt.type = HTTPD_WS_TYPE_TEXT;

      esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
      if (ret != ESP_OK)
        return ret;

      if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        httpd_ws_disconnect(s_instance, httpd_req_to_sockfd(req));
        return ESP_OK;
      }

      if (ws_pkt.len > 0) {
        uint8_t buf[128];
        ws_pkt.payload = buf;
        size_t to_read = ws_pkt.len < sizeof(buf) ? ws_pkt.len : sizeof(buf);
        httpd_ws_recv_frame(req, &ws_pkt, to_read);
      }

      return ESP_OK;
    }

    static void on_close(httpd_handle_t hd, int sockfd) {
      if (s_instance) {
        httpd_ws_disconnect(s_instance, sockfd);
      }
      close(sockfd);
    }

    // --- Friend functions ---

    void httpd_ws_connect(void* server_ptr, int fd_val) {
      auto* srv = static_cast<HttpServer*>(server_ptr);
      if (srv->wsClientCount_ >= HttpServer::MAX_WS_CLIENTS)
        return;
      for (int idx = 0; idx < srv->wsClientCount_; ++idx) {
        if (srv->wsClientFds_[idx] == fd_val)
          return;
      }
      srv->wsClientFds_[srv->wsClientCount_++] = fd_val;
      ESP_LOGI(TAG, "WS client connected: fd=%d (total=%d)", fd_val, srv->wsClientCount_);
    }

    void httpd_ws_disconnect(void* server_ptr, int fd_val) {
      auto* srv = static_cast<HttpServer*>(server_ptr);
      for (int idx = 0; idx < srv->wsClientCount_; ++idx) {
        if (srv->wsClientFds_[idx] == fd_val) {
          for (int jdx = idx; jdx < srv->wsClientCount_ - 1; ++jdx) {
            srv->wsClientFds_[jdx] = srv->wsClientFds_[jdx + 1];
          }
          --srv->wsClientCount_;
          ESP_LOGI(TAG, "WS client disconnected: fd=%d (total=%d)", fd_val, srv->wsClientCount_);
          return;
        }
      }
    }

    // --- HttpServer implementation ---

    // Default httpd task stack size. Override with -DCONFIG_HTTPD_STACK=12288
#ifndef CONFIG_HTTPD_STACK
#define CONFIG_HTTPD_STACK 8192
#endif

    bool HttpServer::start(uint16_t port) {
      if (impl_ != nullptr)
        return true;

      httpd_config_t config = HTTPD_DEFAULT_CONFIG();
      config.server_port = port;
      config.stack_size = CONFIG_HTTPD_STACK;
      config.max_open_sockets = MAX_WS_CLIENTS + 4;
      config.max_uri_handlers = MAX_ROUTES + 2;
      config.close_fn = on_close;
      config.uri_match_fn = httpd_uri_match_wildcard;

      httpd_handle_t server = nullptr;
      esp_err_t ret = httpd_start(&server, &config);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server on port %d: %s", port, esp_err_to_name(ret));
        return false;
      }

      impl_ = server;
      s_instance = this;

      // NOTE: wildcard handlers are registered in ready() after WebSocket
      // and other specific-path handlers, so they don't shadow them.

      ESP_LOGI(TAG, "HTTP server started on port %d", port);
      return true;
    }

    void HttpServer::stop() {
      if (impl_ != nullptr) {
        httpd_stop(static_cast<httpd_handle_t>(impl_));
        impl_ = nullptr;
        s_instance = nullptr;
        wsClientCount_ = 0;
        ESP_LOGI(TAG, "HTTP server stopped");
      }
    }

    void HttpServer::route(Method method, const char* path, RouteHandler handler) {
      if (routeCount_ >= MAX_ROUTES) {
        ESP_LOGE(TAG, "Max routes reached, cannot register %s", path);
        return;
      }
      routes_[routeCount_++] = {method, path, handler};
    }

    void HttpServer::setNotFoundHandler(RouteHandler handler) {
      notFoundHandler_ = handler;
    }

    void HttpServer::ready() {
      auto* server = static_cast<httpd_handle_t>(impl_);
      if (server == nullptr)
        return;

      // Register wildcard catch-all handlers LAST so specific paths
      // (like WebSocket /ws) are matched first by httpd.
      httpd_uri_t get_uri = {};
      get_uri.uri = "/*";
      get_uri.method = HTTP_GET;
      get_uri.handler = generic_handler;

      httpd_uri_t post_uri = {};
      post_uri.uri = "/*";
      post_uri.method = HTTP_POST;
      post_uri.handler = generic_handler;

      httpd_uri_t put_uri = {};
      put_uri.uri = "/*";
      put_uri.method = HTTP_PUT;
      put_uri.handler = generic_handler;

      httpd_register_uri_handler(server, &get_uri);
      httpd_register_uri_handler(server, &post_uri);
      httpd_register_uri_handler(server, &put_uri);

      ESP_LOGI(TAG, "HTTP server ready (%d routes)", routeCount_);
    }

    bool HttpServer::enableWebSocket(const char* path) {
      auto* server = static_cast<httpd_handle_t>(impl_);
      if (server == nullptr) {
        ESP_LOGE(TAG, "Cannot enable WS — server not started");
        return false;
      }

      httpd_uri_t ws_uri = {
          .uri = path,
          .method = HTTP_GET,
          .handler = ws_handler,
          .user_ctx = nullptr,
          .is_websocket = true,
          .handle_ws_control_frames = false,
          .supported_subprotocol = nullptr,
      };

      esp_err_t ret = httpd_register_uri_handler(server, &ws_uri);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WS handler on %s: %s", path, esp_err_to_name(ret));
        return false;
      }

      wsEnabled_ = true;
      ESP_LOGI(TAG, "WebSocket enabled on %s", path);
      return true;
    }

    int HttpServer::wsBroadcast(const char* data, size_t len) {
      auto* server = static_cast<httpd_handle_t>(impl_);
      if (server == nullptr || wsClientCount_ == 0)
        return 0;

      httpd_ws_frame_t ws_pkt;
      memset(&ws_pkt, 0, sizeof(ws_pkt));
      ws_pkt.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(data));
      ws_pkt.len = len;
      ws_pkt.type = HTTPD_WS_TYPE_TEXT;

      int sent = 0;
      for (int idx = wsClientCount_ - 1; idx >= 0; --idx) {
        esp_err_t ret = httpd_ws_send_frame_async(server, wsClientFds_[idx], &ws_pkt);
        if (ret == ESP_OK) {
          ++sent;
        } else {
          ESP_LOGW(TAG, "WS send failed to fd=%d, removing", wsClientFds_[idx]);
          httpd_ws_disconnect(this, wsClientFds_[idx]);
        }
      }
      return sent;
    }

    int HttpServer::wsPing() {
      auto* server = static_cast<httpd_handle_t>(impl_);
      if (server == nullptr || wsClientCount_ == 0)
        return 0;

      httpd_ws_frame_t ws_pkt;
      memset(&ws_pkt, 0, sizeof(ws_pkt));
      ws_pkt.type = HTTPD_WS_TYPE_PING;
      ws_pkt.payload = nullptr;
      ws_pkt.len = 0;

      int sent = 0;
      for (int idx = wsClientCount_ - 1; idx >= 0; --idx) {
        esp_err_t ret = httpd_ws_send_frame_async(server, wsClientFds_[idx], &ws_pkt);
        if (ret == ESP_OK) {
          ++sent;
        } else {
          ESP_LOGW(TAG, "WS ping failed to fd=%d, removing", wsClientFds_[idx]);
          httpd_ws_disconnect(this, wsClientFds_[idx]);
        }
      }
      return sent;
    }

    int HttpServer::wsClientCount() const {
      return wsClientCount_;
    }

    bool HttpServer::isRunning() const {
      return impl_ != nullptr;
    }

  }  // namespace http
}  // namespace ungula

#endif  // ESP_PLATFORM
