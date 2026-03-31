// Real-world HTTP client tests using Postman Echo.
// Requires internet access. Uses libcurl on desktop.

#include <gtest/gtest.h>
#include <http/http_client.h>

using namespace ungula::http;

// =============================================================================
// GET requests
// =============================================================================

TEST(HttpClient, GetReturns200) {
  auto result = httpGet("https://postman-echo.com/get");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
  EXPECT_GT(result.bodyLen, 0);
}

TEST(HttpClient, GetResponseContainsUrl) {
  auto result = httpGet("https://postman-echo.com/get?hello=world");
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.bodyContains("hello"));
  EXPECT_TRUE(result.bodyContains("world"));
}

TEST(HttpClient, GetHeadersEndpoint) {
  auto result = httpGet("https://postman-echo.com/headers");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
  // Response contains the headers we sent
  EXPECT_TRUE(result.bodyContains("host"));
}

// =============================================================================
// POST requests
// =============================================================================

TEST(HttpClient, PostReturns200) {
  const char* json = R"({"key":"value"})";
  auto result = httpPost("https://postman-echo.com/post", json, strlen(json));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
  EXPECT_GT(result.bodyLen, 0);
}

TEST(HttpClient, PostBodyIsEchoed) {
  const char* json = R"({"sensor":"temp","value":42})";
  auto result = httpPost("https://postman-echo.com/post", json, strlen(json));
  EXPECT_TRUE(result.success);
  // Postman Echo echoes the body back in the "data" field
  EXPECT_TRUE(result.bodyContains("sensor"));
  EXPECT_TRUE(result.bodyContains("temp"));
}

// =============================================================================
// Status codes
// =============================================================================

TEST(HttpClient, Status200) {
  auto result = httpGet("https://postman-echo.com/status/200");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
}

TEST(HttpClient, Status404) {
  auto result = httpGet("https://postman-echo.com/status/404");
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.statusCode, 404);
}

TEST(HttpClient, Status500) {
  auto result = httpGet("https://postman-echo.com/status/500");
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.statusCode, 500);
}

// =============================================================================
// Timeouts and delays
// =============================================================================

TEST(HttpClient, DelayedResponseCompletes) {
  // 1-second delay — should complete within default 10s timeout
  auto result = httpGet("https://postman-echo.com/delay/1");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
}

TEST(HttpClient, TimeoutOnSlowResponse) {
  // 5-second delay with 2-second timeout — should fail
  auto result = httpGet("https://postman-echo.com/delay/5", 2000);
  EXPECT_FALSE(result.success);
}

// =============================================================================
// Chunked / streaming responses
// =============================================================================

TEST(HttpClient, ChunkedResponseReceived) {
  auto result = httpGet("https://postman-echo.com/stream/3");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
  EXPECT_GT(result.bodyLen, 0);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(HttpClient, InvalidUrlFails) {
  auto result = httpGet("https://this-domain-does-not-exist-12345.invalid/test", 3000);
  EXPECT_FALSE(result.success);
}

TEST(HttpClient, EmptyPostBody) {
  auto result = httpPost("https://postman-echo.com/post", "", 0);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
}

// =============================================================================
// Large responses — verify truncation and no crashes
// =============================================================================

TEST(HttpClient, LargeResponseTruncatedGracefully) {
  // httpbin returns exactly 1MB of random bytes
  // Our HttpResult body buffer is 1024 bytes — must truncate without crashing
  auto result = httpGet("https://httpbin.org/bytes/1000000");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
  // Body should be filled to capacity (buffer size - 1 for null terminator)
  EXPECT_GE(result.bodyLen, 1000);
}

TEST(HttpClient, LargeStreamingResponseTruncated) {
  // httpbin streams 1MB in chunks — same truncation behavior expected
  auto result = httpGet("https://httpbin.org/stream-bytes/1000000");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
  EXPECT_GE(result.bodyLen, 1000);
}

TEST(HttpClient, ModerateSizeResponseFitsInBuffer) {
  // 500 bytes fits within our 1024-byte buffer
  auto result = httpGet("https://httpbin.org/bytes/500");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.statusCode, 200);
  EXPECT_EQ(result.bodyLen, 500);
}
