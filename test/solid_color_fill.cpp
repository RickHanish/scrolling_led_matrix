// Test pattern: fill the entire matrix with one configurable color.
#include <Arduino.h>
#include <FastLED.h>

constexpr uint8_t DATA_PIN = 18;
constexpr uint8_t WIDTH = 32;
constexpr uint8_t HEIGHT = 8;
constexpr uint16_t NUM_LEDS = WIDTH * HEIGHT;

CRGB leds[NUM_LEDS];

// Change this value to any color you want to test.
constexpr CRGB TEST_COLOR = CRGB(0, 160, 255);

void setup() {
	FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
	FastLED.setBrightness(96);
	fill_solid(leds, NUM_LEDS, TEST_COLOR);
	FastLED.show();
}

void loop() {
	// Keep showing the same color frame.
	delay(250);
}
