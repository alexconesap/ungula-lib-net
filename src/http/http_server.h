// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// ============================================================================
// HTTP + WebSocket server abstraction.
//
// Provides a portable HTTP server with REST route registration, query parameter
// parsing, PROGMEM response support, and optional WebSocket broadcast.
//
// Implementation: ESP-IDF httpd (selected at compile time via ESP_PLATFORM).
// No Arduino dependency.
//
// Usage:
//   ungula::http::HttpServer server;
//   server.start(80);
//   server.route(HTTP_GET, "/api/status", [](HttpRequest& req) {
//     req.sendJson(200, R"({"status":"ok"})");
//   });
//   server.enableWebSocket("/ws");
//   server.wsBroadcast(json, strlen(json));
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace ungula {
  namespace http {

    /// HTTP methods
    enum class Method : uint8_t { GET, POST, PUT, DELETE_ };

    /// Opaque request handle passed to route handlers.
    /// Wraps the platform-specific request object.
    class HttpRequest {
      public:
        /// Send a response with the given status code, content type, and body
        void send(int code, const char* content_type, const char* body);

        /// Send a response from PROGMEM data (flash-stored strings)
        void sendProgmem(int code, const char* content_type, const char* progmem_data);

        /// Convenience: send JSON response
        void sendJson(int code, const char* json);

        /// Check if a query parameter exists
        bool hasParam(const char* name) const;

        /// Get a query parameter value. Returns empty string if not found.
        const char* param(const char* name) const;

        /// Get the request URI
        const char* uri() const;

        /// Get the raw request body (for POST with JSON payload)
        const char* body() const;

        // Internal — set by the implementation
        void* impl_ = nullptr;

        // Parameter buffer (populated by implementation before calling handler)
        static constexpr int MAX_PARAMS = 10;
        struct Param {
            char name[24];
            char value[48];
        };
        Param params_[MAX_PARAMS] = {};
        int paramCount_ = 0;
        char uri_[96] = {};
        char body_[384] = {};
    };

    /// Route handler function type
    using RouteHandler = void (*)(HttpRequest& req);

    /// Maximum number of registered routes
    static constexpr int MAX_ROUTES = 40;

    /// HTTP + WebSocket server
    class HttpServer {
      public:
        /// Start the server on the given port
        bool start(uint16_t port = 80);

        /// Stop the server
        void stop();

        /// Register a route handler
        void route(Method method, const char* path, RouteHandler handler);

        /// Set handler for unmatched routes (404)
        void setNotFoundHandler(RouteHandler handler);

        /// Finalize server setup — call AFTER all routes and enableWebSocket().
        /// Registers the wildcard catch-all handlers that dispatch to the route table.
        void ready();

        /// Enable WebSocket support on the given path (e.g., "/ws")
        bool enableWebSocket(const char* path = "/ws");

        /// Broadcast a text message to all connected WebSocket clients
        int wsBroadcast(const char* data, size_t len);

        /// Send a WebSocket PING frame to all connected clients.
        /// Stale clients that fail to respond are removed.
        int wsPing();

        /// Number of connected WebSocket clients
        int wsClientCount() const;

        /// Check if the server is running
        bool isRunning() const;

        // --- Internal state (accessed by platform implementation) ---
        // Not part of the public API — do not use directly from application code.

        void* impl_ = nullptr;

        struct Route {
            Method method;
            const char* path;
            RouteHandler handler;
        };
        Route routes_[MAX_ROUTES] = {};
        int routeCount_ = 0;
        RouteHandler notFoundHandler_ = nullptr;

        static constexpr int MAX_WS_CLIENTS = 4;
        int wsClientFds_[MAX_WS_CLIENTS] = {};
        int wsClientCount_ = 0;
        bool wsEnabled_ = false;
    };

  }  // namespace http
}  // namespace ungula
