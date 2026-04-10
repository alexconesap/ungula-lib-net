// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#include "ntp_client.h"

#include <cstring>

// =============================================================================
// ESP-IDF implementation — uses the built-in SNTP service
// =============================================================================
#if defined(ESP_PLATFORM)

#include <esp_sntp.h>
#include <sys/time.h>

static int32_t s_utcOffset = 0;
static bool s_initialised = false;

namespace ungula {
  namespace ntp {

    void ntp_init(const NtpConfig& config) {
      if (s_initialised) {
        return;
      }

      s_utcOffset = config.utcOffsetSeconds;

      esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
      esp_sntp_setservername(0, config.server);
      if (config.fallbackServer) {
        esp_sntp_setservername(1, config.fallbackServer);
      }
      esp_sntp_set_sync_interval(config.syncIntervalSec * 1000);
      esp_sntp_init();

      s_initialised = true;
    }

    void ntp_stop() {
      if (!s_initialised) {
        return;
      }
      esp_sntp_stop();
      s_initialised = false;
    }

    bool ntp_is_synced() {
      if (!s_initialised) {
        return false;
      }
      // After the first successful sync the system time jumps from the
      // boot epoch (Jan 1970) to a value well past the year 2020.
      time_t now = 0;
      time(&now);
      return now > 1577836800;  // 2020-01-01 00:00:00 UTC
    }

    time_t ntp_epoch() {
      time_t now = 0;
      time(&now);
      if (now < 1577836800) {
        return 0;
      }
      return now;
    }

    time_t ntp_local_epoch() {
      time_t utc = ntp_epoch();
      if (utc == 0) {
        return 0;
      }
      return utc + s_utcOffset;
    }

    size_t ntp_format_local(char* buf, size_t bufSize) {
      if (bufSize < 20) {
        return 0;
      }
      time_t local = ntp_local_epoch();
      if (local == 0) {
        return 0;
      }
      struct tm timeinfo;
      gmtime_r(&local, &timeinfo);
      return strftime(buf, bufSize, "%Y-%m-%d %H:%M:%S", &timeinfo);
    }

  }  // namespace ntp
}  // namespace ungula

// =============================================================================
// Desktop stub — no real NTP, always unsynchronised
// =============================================================================
#else

static int32_t s_utcOffset = 0;

namespace ungula {
  namespace ntp {

    void ntp_init(const NtpConfig& config) {
      s_utcOffset = config.utcOffsetSeconds;
    }

    void ntp_stop() {}

    bool ntp_is_synced() {
      return false;
    }

    time_t ntp_epoch() {
      return 0;
    }

    time_t ntp_local_epoch() {
      return 0;
    }

    size_t ntp_format_local(char* /*buf*/, size_t /*bufSize*/) {
      return 0;
    }

  }  // namespace ntp
}  // namespace ungula

#endif  // ESP_PLATFORM
