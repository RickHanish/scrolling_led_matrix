#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <WebServer.h>
#include <vector>

#include "colors/color_modes.h"
#include "config/constants.h"

struct AppState {
	CRGB leds[NUM_LEDS];

	int scrollX = WIDTH;
	WebServer server{80};

	String message;

	CRGB currentColor = CRGB::White;
	CRGB gradientStartColor = CRGB::Red;
	CRGB gradientEndColor = CRGB::Blue;
	std::vector<CRGB> perLetterColors;
	ColorMode colorMode = ColorMode::Solid;

	uint8_t currentBrightnessPct = DEFAULT_BRIGHTNESS_PCT;
	uint8_t currentBrightness = 0;

	bool customMessage = false;
	unsigned long customMessageStartedAt = 0;
	int defaultIndex = 0;
	String cachedWeather;
	String currentDefaultDisplay;
	String httpServerAddr;
};
