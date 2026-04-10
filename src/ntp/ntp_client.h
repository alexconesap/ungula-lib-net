// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

// NTP time synchronisation.
//
// On ESP32 this uses the ESP-IDF SNTP service which sets the POSIX system
// clock automatically. Once synced, standard time() / gettimeofday() return
// real wall-clock time and the SNTP service keeps it updated in the
// background.
//
// The API is intentionally minimal: init, check, and read.
//
// Gated by ESP_PLATFORM — on desktop hosts the header is still includable
// but the functions are stubbed (always returns epoch 0 / not synced).

#include <cstdint>
#include <ctime>

namespace ungula {
  namespace ntp {

    /// NTP server configuration.
    struct NtpConfig {
        const char* server = "pool.ntp.org";
        const char* fallbackServer = "time.google.com";
        int32_t utcOffsetSeconds = 0;     // timezone offset from UTC
        uint32_t syncIntervalSec = 3600;  // re-sync interval (default 1 h)
    };

    /// Start the SNTP service. Safe to call more than once (subsequent
    /// calls are ignored). WiFi STA must be connected before calling this
    /// so the DNS resolver can reach the NTP server.
    void ntp_init(const NtpConfig& config = NtpConfig{});

    /// Stop the SNTP service and release resources.
    void ntp_stop();

    /// Returns true once the system clock has been set by NTP at least once.
    bool ntp_is_synced();

    /// Current UTC epoch (seconds since 1970-01-01 00:00:00).
    /// Returns 0 if the clock has not been set yet.
    time_t ntp_epoch();

    /// Current epoch with the configured UTC offset applied.
    time_t ntp_local_epoch();

    /// Format the current local time into buf as "YYYY-MM-DD HH:MM:SS".
    /// Returns the number of characters written (excluding the null
    /// terminator), or 0 if the clock is not synced or buf is too small.
    /// buf must be at least 20 bytes.
    size_t ntp_format_local(char* buf, size_t bufSize);

  }  // namespace ntp
}  // namespace ungula
