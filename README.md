# UngulaNetLib

Networking library for ESP32 projects. WiFi AP management, HTTP+WebSocket server, and HTTP client — all built on ESP-IDF, no Arduino networking dependencies.

The library compiles all components when `ESP_PLATFORM` is defined. The host project controls what it uses through its own `#include` directives and project-level guards — the library does not impose feature flags.

## Compile flags

| Flag | What it enables | Who needs it |
| --- | --- | --- |
| `ESP_PLATFORM` | ESP-IDF implementations (WiFi, httpd, esp_http_client) | All ESP32 nodes |
| `CONFIG_HTTPD_STACK` | httpd task stack size in bytes (default 8192) | Override if handlers need more stack |

Example build flags:

```text
-DESP_PLATFORM
```

## WiFi AP

Sets up the ESP32 in AP+STA mode so you can host a local network and still use ESP-NOW at the same time.

```cpp
#include <wifi/wifi_ap.h>

using namespace ungula::wifi;

WifiApConfig config;
config.ssid           = "MyDevice";
config.password       = "secret123";
config.channel        = WifiChannel::Ch6;
config.maxConnections = 4;

if (wifi_ap_init(config)) {
    log_info("AP ready at %s", wifi_ap_get_ip());  // "192.168.4.1"
}
```

| Function | Returns | Description |
| --- | --- | --- |
| `wifi_ap_init(config)` | `bool` | Initialize WiFi AP+STA mode |
| `wifi_ap_get_ip()` | `const char*` | AP IP address |
| `wifi_ap_is_active()` | `bool` | Whether AP is running |
| `wifi_ap_get_channel()` | `WifiChannel` | Effective channel in use |

## ESP-NOW Initialization

For nodes that only need ESP-NOW (no web server, no AP), use `wifi_espnow_init()` to bring up the WiFi radio in STA mode -- the minimum required for ESP-NOW to work.

```cpp
#include <wifi/wifi_espnow.h>

using namespace ungula::wifi;

void setup() {
    if (!wifi_espnow_init()) {
        // handle error
    }
    // ESP-NOW transport is now ready to use
}
```

| Function | Returns | Description |
| --- | --- | --- |
| `wifi_espnow_init()` | `bool` | Initialize WiFi in STA mode for ESP-NOW only |

No AP is started, no HTTP server, no web UI. This is the right choice for headless nodes that communicate exclusively via ESP-NOW.

## HTTP + WebSocket Server

*Requires `-DESP_PLATFORM`*

A unified HTTP and WebSocket server built on ESP-IDF `httpd`. One server, one port, both REST routes and WebSocket on the same instance. No Arduino WebServer dependency.

### Starting the server

```cpp
#include <http/http_server.h>

ungula::http::HttpServer server;

void setup() {
    wifi_ap_init(apConfig);

    server.start(80);
    server.enableWebSocket("/ws");
}
```

### Registering routes

Routes are plain function pointers. The server dispatches incoming requests to the matching handler based on method + path.

```cpp
using Req = ungula::http::HttpRequest;
using Method = ungula::http::Method;

static void handleStatus(Req& req) {
    req.sendJson(200, R"({"status":"ok","uptime":12345})");
}

static void handleReboot(Req& req) {
    req.sendJson(200, R"({"status":"rebooting"})");
    requestReboot();
}

static void handleUpdateSetting(Req& req) {
    if (req.hasParam("temp")) {
        int temp = atoi(req.param("temp"));
        setTemperature(temp);
    }
    req.sendJson(200, R"({"status":"ok"})");
}

static void handlePostCommand(Req& req) {
    // POST body is available via req.body()
    const char* json_body = req.body();
    processCommand(json_body);
    req.sendJson(200, R"({"status":"ok"})");
}

void registerRoutes(ungula::http::HttpServer& server) {
    server.route(Method::GET, "/api/status", handleStatus);
    server.route(Method::POST, "/api/reboot", handleReboot);
    server.route(Method::PUT, "/api/settings", handleUpdateSetting);
    server.route(Method::POST, "/api/command", handlePostCommand);
    server.setNotFoundHandler([](Req& req) {
        req.send(404, "text/plain", "Not found");
    });
}
```

### Serving static content from PROGMEM

Web portal HTML, CSS, and JS can be stored in flash and served directly:

```cpp
#include <pgmspace.h>

static const char MY_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><body><h1>Hello</h1></body></html>
)rawhtml";

static void handlePortal(Req& req) {
    req.sendProgmem(200, "text/html", MY_HTML);
}

server.route(Method::GET, "/", handlePortal);
```

### WebSocket broadcast

Push real-time updates to all connected browser clients:

```cpp
server.enableWebSocket("/ws");

// Later, when something changes:
const char* json = R"({"temp":350,"mode":"ready"})";
int clients_sent = server.wsBroadcast(json, strlen(json));
```

The WebSocket is broadcast-only — the server ignores incoming messages from clients. This is by design: commands go through REST POST endpoints, status updates push through WebSocket.

### HttpRequest API

| Method | Description |
| --- | --- |
| `req.send(code, contentType, body)` | Send a response |
| `req.sendProgmem(code, contentType, data)` | Send from flash (PROGMEM) |
| `req.sendJson(code, json)` | Convenience: send JSON response |
| `req.hasParam("name")` | Check if query parameter exists |
| `req.param("name")` | Get query parameter value |
| `req.body()` | Get POST/PUT request body |
| `req.uri()` | Get the request path |

### Server configuration

The httpd task stack defaults to 8192 bytes (`CONFIG_HTTPD_STACK`). If your handlers build large JSON on the stack, you can increase it via build flags:

```text
-DCONFIG_HTTPD_STACK=12288
```

Max 40 routes, 4 WebSocket clients.

## HTTP Client

*Requires `-DESP_PLATFORM`*

Simple GET and POST requests for pushing data to cloud APIs or fetching remote resources.

On ESP32, uses ESP-IDF `esp_http_client`. On desktop (for testing), uses libcurl.

### GET request

```cpp
#include <http/http_client.h>

auto result = ungula::http::httpGet("https://api.example.com/health");
if (result.success) {
    log_info("Server responded %d: %s", result.statusCode, result.body);
}
```

### POST request (JSON)

```cpp
const char* json = R"({"device":"node-1","temp":350,"status":"ready"})";
auto result = ungula::http::httpPost(
    "https://api.example.com/status",
    json, strlen(json)
);

if (result.success) {
    log_info("Status pushed OK (%d)", result.statusCode);
} else {
    log_warn("Push failed: status=%d", result.statusCode);
}
```

### Timeout control

Both `httpGet` and `httpPost` accept an optional timeout in milliseconds (default 10 seconds):

```cpp
// 3-second timeout for a health check
auto result = ungula::http::httpGet("https://api.example.com/ping", 3000);
```

### HttpResult

| Field | Type | Description |
| --- | --- | --- |
| `success` | `bool` | True if HTTP status 2xx |
| `statusCode` | `int` | HTTP response code (200, 404, 500, etc.) |
| `body` | `char[1024]` | Response body (truncated if larger) |
| `bodyLen` | `size_t` | Actual bytes received (up to buffer size) |
| `bodyContains(str)` | `bool` | Check if body contains a substring |

The body buffer is 1024 bytes. Responses larger than that are silently truncated — no crash, no allocation. This is intentional for embedded use where you typically only need a short JSON response or a status check.

## Testing

The HTTP client has a test suite that runs on desktop (macOS/Linux) using libcurl against real endpoints.

### Prerequisites

- CMake 3.16+
- C++17 compiler
- libcurl development headers (`brew install curl` on macOS)
- Internet access (tests hit external APIs)

### Run the tests

```shell
cd tests
./1_build.sh     # configure cmake (only needed once)
./2_run.sh       # build and run all tests
```

### What's tested

16 tests against Postman Echo and httpbin.org:

| Category | Tests |
| --- | --- |
| GET requests | 200 response, query params echoed, headers endpoint |
| POST requests | 200 response, body echoed back |
| Status codes | 200, 404, 500 — success flag matches |
| Timeouts | 1s delay completes, 5s delay with 2s timeout fails |
| Chunked responses | Streaming endpoint received |
| Large responses | 1MB truncated gracefully, 1MB streaming truncated, 500B fits exactly |
| Edge cases | Invalid domain fails, empty POST body |

## Dependencies

Requires [UngulaGenericLib](https://github.com/alexconesap/ungula-lib.git) (`lib/`) for the logger and `WifiChannel` enum.

```text
your_project/
  lib/          <- UngulaGenericLib
  lib_net/      <- this library
  src/
    main.ino
```

## License

MIT License — see LICENSE file
