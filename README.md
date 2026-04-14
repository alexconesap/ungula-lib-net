# UngulaNetLib

Networking library for ESP32 projects. WiFi AP management, HTTP+WebSocket server, HTTP client, and NTP time synchronisation — all built on ESP-IDF, no Arduino networking dependencies.

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

if (ap_init(config)) {
    log_info("AP ready at %s", ap_get_ip());  // "192.168.4.1"
}
```

| Function | Returns | Description |
| --- | --- | --- |
| `ap_init(config)` | `bool` | Initialize WiFi AP+STA mode |
| `ap_get_ip()` | `const char*` | AP IP address |
| `ap_is_active()` | `bool` | Whether AP is running |
| `ap_get_channel()` | `WifiChannel` | Effective channel in use |

## ESP-NOW Initialization

For nodes that only need ESP-NOW (no web server, no AP), use `espnow_init()` to bring up the WiFi radio in STA mode -- the minimum required for ESP-NOW to work.

```cpp
#include <wifi/wifi_espnow.h>

using namespace ungula::wifi;

void setup() {
    if (!espnow_init()) {
        // handle error
    }
    // ESP-NOW transport is now ready to use
}
```

| Function | Returns | Description |
| --- | --- | --- |
| `espnow_init()` | `bool` | Initialize WiFi in STA mode for ESP-NOW only |

No AP is started, no HTTP server, no web UI. This is the right choice for headless nodes that communicate exclusively via ESP-NOW.

## HTTP + WebSocket Server

*Requires `-DESP_PLATFORM`*

A unified HTTP and WebSocket server built on ESP-IDF `httpd`. One server, one port, both REST routes and WebSocket on the same instance. No Arduino WebServer dependency.

### Starting the server

```cpp
#include <http/http_server.h>

ungula::http::HttpServer server;

void setup() {
    ap_init(apConfig);

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

## Pairing (`pairing/`)

### Multi-Channel Pairing for ESP-NOW Networks

The pairing system lets a coordinator (e.g. a central controller) discover and pair with client nodes across multiple WiFi channels. Once paired, the MAC and channel are stored in NVS so they survive reboots.

**Coordinator side** (the device that accepts connections):

```cpp
#include "pairing/pairing_coordinator.h"

using namespace ungula;

PairingCoordinator pairing(transport, prefs, "pair_ns");

void setup() {
    pairing.loadPairedClients();

    // When a new node pairs with us
    pairing.onClientPaired([](const comm::MacAddress& mac, uint8_t deviceId) {
        log_info("Node %d paired", deviceId);
    });
}

void onUserPressedPairButton() {
    pairing.enablePairing();  // Starts broadcasting beacons
}

void loop() {
    pairing.loop(millis());
}

// In your receive callback:
void onMessage(const comm::MacAddress& src, const uint8_t* data, uint16_t len) {
    if (pairing.handleReceived(src, data, len)) return;  // consumed by pairing
    // ... handle application messages
}
```

**Client side** (the device that joins):

```cpp
#include "pairing/pairing_client.h"

using namespace ungula;

PairingClient pairing(transport, prefs, "pair_ns", MY_DEVICE_ID);

void setup() {
    uint8_t scanChannels[] = {1, 6, 11};
    pairing.setScanChannels(scanChannels, 3);

    pairing.onPaired([](const comm::MacAddress& mac, uint8_t ch) {
        log_info("Paired with coordinator on channel %d", ch);
    });

    auto stored = pairing.loadStoredPairing();
    if (!stored.valid) {
        pairing.startScanning();  // No stored pairing, start looking
    }
}

void loop() {
    pairing.loop(millis());
}
```

## Communication (`comm/`)

### Sending and Receiving Messages

`ITransport` is the transport interface. You code against it, and the actual implementation (ESP-NOW, or anything else) is injected at setup time. This means your application logic never depends on a specific radio or protocol.

The ESP-NOW implementation is `EspNowTransport`. Here is a complete example — a coordinator that broadcasts a heartbeat every second and prints anything it receives:

```cpp
#include <comm/esp_now_transport.h>
#include <comm/message_header.h>

using namespace ungula::comm;

EspNowTransport transport;

// This runs every time a message arrives
void onMessage(const MacAddress& src, const uint8_t* data, uint16_t len) {
    auto header = extractHeader(data, len);
    Serial.printf("Got message type %d from peer\n", header.messageType);
}

void setup() {
    transport.init();
    transport.setChannel(6);
    transport.onReceive(onMessage);
}

void loop() {
    // Build a heartbeat message
    uint8_t buf[sizeof(MessageHeader)];
    MessageHeader hdr = {};
    hdr.protocolVersion = 1;
    hdr.messageType = 0x01;  // your app-defined type
    memcpy(buf, &hdr, sizeof(hdr));

    // Broadcast to all peers on the channel
    transport.send(MacAddress::broadcast(), buf, sizeof(buf));
    delay(1000);
}
```

For unicast (sending to a specific device), you need to register the peer first:

```cpp
MacAddress peer = MacAddress::fromBytes(peerMacBytes);
transport.addPeer(peer, 6);  // channel 6

auto err = transport.send(peer, buf, len);
if (err != TransportError::OK) {
    Serial.println("Send failed");
}
```

### Writing Your Own Transport

If you need something other than ESP-NOW (BLE, LoRa, serial, a mock for testing), implement `ITransport`:

```cpp
class MyLoRaTransport : public ungula::comm::ITransport {
public:
    TransportError init() override { /* ... */ }
    TransportError send(const MacAddress& dst, const uint8_t* data, uint16_t len) override { /* ... */ }
    void onReceive(TransportReceiveCallback cb) override { receiveCb_ = cb; }
    // ... rest of the interface
};
```

Then pass it to your application code the same way. Nothing changes downstream.

### MessageHeader

Every message starts with an 8-byte header. Utility functions let you pull it apart:

```cpp
auto hdr = extractHeader(data, len);
const uint8_t* payload = extractPayload(data);
uint16_t payloadLen = payloadLength(len);

if (!isValidHeader(data, len)) {
    return;  // too short or corrupt
}
```

| Field | Type | Description |
| --- | --- | --- |
| `protocolVersion` | `uint8_t` | Protocol version |
| `messageType` | `uint8_t` | Application-defined type |
| `sourceDeviceId` | `uint8_t` | Sender device ID |
| `sequenceNumber` | `uint8_t` | Rolling sequence (0-255) |
| `flags` | `uint8_t` | Bit 0: requiresAck, Bit 1: isAck |
| `reserved[3]` | `uint8_t[]` | Must be zero |

## NTP Time Synchronisation

*Requires `-DESP_PLATFORM`*

Synchronise the device clock with an NTP server. Uses the ESP-IDF SNTP service under the hood — once synced, the standard POSIX `time()` returns real wall-clock time and the service keeps it updated in the background.

This replaces the Arduino `NTPClient` + `WiFiUdp` pattern with a zero-dependency ESP-IDF implementation.

### Basic usage

```cpp
#include <ntp/ntp_client.h>

using namespace ungula::ntp;

void setup() {
    sta_connect(staConfig);

    ntp_init();  // uses pool.ntp.org, UTC, 1 h re-sync
}

void loop() {
    if (ntp_is_synced()) {
        char buf[20];
        ntp_format_local(buf, sizeof(buf));
        log_info("Time: %s", buf);  // "2026-04-09 14:30:00"
    }
}
```

### Custom configuration

```cpp
NtpConfig cfg;
cfg.server           = "time.nist.gov";
cfg.fallbackServer   = "time.google.com";
cfg.utcOffsetSeconds = -5 * 3600;  // US Eastern (UTC-5)
cfg.syncIntervalSec  = 1800;       // re-sync every 30 min

ntp_init(cfg);
```

### API

| Function | Returns | Description |
| --- | --- | --- |
| `ntp_init(config)` | `void` | Start SNTP service. Safe to call more than once |
| `ntp_stop()` | `void` | Stop the SNTP service |
| `ntp_is_synced()` | `bool` | True once the clock has been set by NTP |
| `ntp_epoch()` | `time_t` | Current UTC epoch (0 if not synced) |
| `ntp_local_epoch()` | `time_t` | UTC epoch + configured offset (0 if not synced) |
| `ntp_format_local(buf, size)` | `size_t` | Format local time as `YYYY-MM-DD HH:MM:SS` |

WiFi STA must be connected before calling `ntp_init()` so the DNS resolver can reach the NTP server. On desktop hosts the functions are stubbed (always returns not synced).

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

| Library | Repo | Used for |
| ------- | ---- | -------- |
| UngulaGenericLib | [ungula-lib](https://github.com/alexconesap/ungula-lib.git) | `WifiChannel` enum, platform abstractions |
| embLogX | [emblogx](https://github.com/alexconesap/emblogx.git) | Logging via `log_error()` / `log_warn()` |

ESP-IDF component dependencies (part of the SDK, no extra components needed):

- **esp_wifi** — WiFi AP/STA
- **esp_http_client** — HTTP client
- **esp_http_server** — HTTP+WebSocket server
- **esp_sntp** — NTP time synchronisation

For local development, keep the libraries as siblings:

```text
your_workspace/
  lib/            <- UngulaGenericLib
  lib_emblogx/    <- embLogX
  lib_net/        <- this library
```

## License

MIT License — see LICENSE file
