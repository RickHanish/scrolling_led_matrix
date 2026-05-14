#pragma once

#include <Arduino.h>

constexpr uint8_t DATA_PIN = 18;
constexpr uint8_t WIDTH = 32;
constexpr uint8_t HEIGHT = 8;
constexpr uint16_t NUM_LEDS = WIDTH * HEIGHT;

constexpr uint8_t DEFAULT_BRIGHTNESS_PCT = 50;
constexpr unsigned long CUSTOM_MESSAGE_MAX_AGE_MS = 48UL * 60UL * 60UL * 1000UL;

constexpr const char* NTP_TZ = "CST6CDT,M3.2.0/2,M11.1.0/2";
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.nist.gov";
