// Test pattern: one white pixel scans left-to-right, top-to-bottom.
#include <Arduino.h>
#include <FastLED.h>

constexpr uint8_t DATA_PIN = 18;
constexpr uint8_t WIDTH = 32;
constexpr uint8_t HEIGHT = 8;
constexpr uint16_t NUM_LEDS = WIDTH * HEIGHT;

CRGB leds[NUM_LEDS];

static uint16_t XY(uint8_t x, uint8_t y) {
	uint8_t px = HEIGHT - 1 - y;
	uint8_t py = x;

	if (py % 2 == 0) {
		px = HEIGHT - px - 1;
	}

	return static_cast<uint16_t>(py) * HEIGHT + px;
}

void setup() {
	FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
	FastLED.setBrightness(64);
	FastLED.clear(true);
}

void loop() {
	static uint8_t x = 0;
	static uint8_t y = 0;

	FastLED.clear();
	leds[XY(x, y)] = CRGB::White;
	FastLED.show();
	delay(50);

	x++;
	if (x >= WIDTH) {
		x = 0;
		y++;
		if (y >= HEIGHT) {
			y = 0;
		}
	}
}
