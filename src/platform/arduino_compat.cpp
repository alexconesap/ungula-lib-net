// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

// The Arduino ESP32's precompiled liblwip.a expects this symbol. It is normally
// provided by Arduino's WiFi library, but `lib_net` uses ESP-IDF WiFi directly. This stub goes away
// when we stop compiling with Arduino CLI.

// When NOT using the Arduino stack, the lwip_hook_ip6_input function is not defined by lwIP, which
// causes linker errors:
// "esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/lwip/lwip/src/core/ipv6/ip6.c
// undefined reference to `lwip_hook_ip6_input'"

#ifdef ARDUINO_ARCH_ESP32

extern "C" {
struct pbuf;
struct netif;
}

extern "C" __attribute__((weak)) int lwip_hook_ip6_input(struct pbuf* p, struct netif* inp) {
  (void)p;
  (void)inp;
  return 0;
}

#endif