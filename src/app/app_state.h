#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <WebServer.h>
#include <vector>

#include "colors/color_modes.h"
#include "config/constants.h"

enum class RenderProgram : uint8_t {
	Message,
	Test,
	Design,
	Image
};

enum class MatrixTest : uint8_t {
	ScanWhitePixel,
	SolidFill,
	RainbowCycle
};

enum class MatrixDesign : uint8_t {
	Plasma,
	CheckerPulse,
	DiamondWave,
	RollingField
};

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
	uint8_t currentBrightness = 50; // Default to ~50/255 until applyBrightness calculates it
	uint8_t renderBrightnessLimitPct = 100;

	RenderProgram renderProgram = RenderProgram::Message;
	MatrixTest selectedTest = MatrixTest::ScanWhitePixel;
	MatrixDesign selectedDesign = MatrixDesign::Plasma;
	CRGB testSolidColor = CRGB(0, 160, 255);
	uint16_t animationTick = 0;
	uint16_t scanIndex = 0;
	std::vector<CRGB> imagePixels;
	uint16_t imageWidth = 0;
	uint16_t imageHeight = 0;
	int16_t imageScrollY = 0;
	int8_t imageScrollDirection = 1;

	bool customMessage = false;
	unsigned long customMessageStartedAt = 0;
	int defaultIndex = 0;
	String cachedWeather;
	String currentDefaultDisplay;
	String httpServerAddr;
};
