#include "display/scene_renderer.h"

#include <FastLED.h>

#include "app/app_state.h"
#include "config/constants.h"

static uint16_t XY(uint8_t x, uint8_t y) {
	uint8_t px = HEIGHT - 1 - y;
	uint8_t py = x;

	if (py % 2 == 0) {
		px = HEIGHT - px - 1;
	}

	return py * HEIGHT + px;
}

static void setPixel(AppState& state, int x, int y, const CRGB& color) {
	if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
		state.leds[XY(static_cast<uint8_t>(x), static_cast<uint8_t>(y))] = color;
	}
}

static void drawBird(AppState& state, int x, int y, const CRGB& color) {
	setPixel(state, x, y, color);
	setPixel(state, x + 1, y + 1, color);
	setPixel(state, x + 2, y, color);
}

static void drawCloud(AppState& state, int x, int y) {
	setPixel(state, x, y, CRGB(240, 246, 255));
	setPixel(state, x + 1, y, CRGB(240, 246, 255));
	setPixel(state, x + 2, y, CRGB(240, 246, 255));
	setPixel(state, x + 1, y - 1, CRGB(255, 255, 255));
	setPixel(state, x + 3, y - 1, CRGB(255, 255, 255));
	setPixel(state, x + 2, y - 2, CRGB(255, 255, 255));
}

static void drawFencePost(AppState& state, int x, int topY) {
	setPixel(state, x, topY, CRGB(92, 58, 22));
	setPixel(state, x, topY + 1, CRGB(92, 58, 22));
	setPixel(state, x, topY + 2, CRGB(92, 58, 22));
	setPixel(state, x - 1, topY + 1, CRGB(112, 72, 28));
	setPixel(state, x + 1, topY + 1, CRGB(112, 72, 28));
}

static void drawTractor(AppState& state, int x, int y) {
	setPixel(state, x, y, CRGB(220, 40, 30));
	setPixel(state, x + 1, y, CRGB(220, 40, 30));
	setPixel(state, x + 2, y, CRGB(170, 24, 20));
	setPixel(state, x, y - 1, CRGB(220, 40, 30));
	setPixel(state, x + 1, y - 1, CRGB(255, 190, 70));
	setPixel(state, x + 2, y - 1, CRGB(170, 24, 20));
	setPixel(state, x + 3, y - 1, CRGB(255, 190, 70));
	setPixel(state, x + 1, y + 1, CRGB(28, 28, 28));
	setPixel(state, x + 3, y + 1, CRGB(28, 28, 28));
	setPixel(state, x + 4, y + 1, CRGB(28, 28, 28));
}

static void renderRollingField(AppState& state) {
	for (uint8_t x = 0; x < WIDTH; x++) {
		uint8_t horizon = 3 + static_cast<uint8_t>(sin8(static_cast<uint8_t>(x * 9)) / 96);
		for (uint8_t y = 0; y < HEIGHT; y++) {
			CRGB color;
			if (y < horizon) {
				color = CRGB(94, 180, 255);
			} else if (y == horizon) {
				color = CRGB(242, 225, 92);
			} else if (y < HEIGHT - 2) {
				color = CRGB(56, 158, 66);
			} else {
				color = CRGB(110, 78, 34);
			}
			state.leds[XY(x, y)] = color;
		}

		if (x % 5 == 0 && horizon + 1 < HEIGHT) {
			setPixel(state, x, horizon + 1, CRGB(58, 138, 56));
		}
		if (x % 7 == 0 && horizon + 2 < HEIGHT) {
			setPixel(state, x, horizon + 2, CRGB(46, 126, 48));
		}
	}

	drawCloud(state, 2 + static_cast<int>((state.animationTick / 8) % 5), 1);
	drawCloud(state, 14 + static_cast<int>((state.animationTick / 10) % 4), 2);
	drawCloud(state, 23 + static_cast<int>((state.animationTick / 12) % 3), 1);

	for (int x = 2; x < WIDTH - 2; x += 4) {
		uint8_t horizon = 3 + static_cast<uint8_t>(sin8(static_cast<uint8_t>(x * 9)) / 96);
		drawFencePost(state, x, static_cast<int>(horizon) + 1);
	}

	for (int x = 0; x < 6; x++) {
		for (int y = 0; y < 4; y++) {
			setPixel(state, 21 + x, 2 + y, CRGB(120, 62, 18));
		}
	}
	for (int x = 0; x < 7; x++) {
		setPixel(state, 20 + x, 1, CRGB(150, 80, 28));
		if (x <= 3) {
			setPixel(state, 21 + x, 0, CRGB(150, 80, 28));
		} else {
			setPixel(state, 20 + x, 0, CRGB(150, 80, 28));
		}
	}
	for (int y = 2; y < 6; y++) {
		setPixel(state, 23, y, CRGB::Black);
	}
	setPixel(state, 25, 3, CRGB::Black);
	uint16_t birdPhase = state.animationTick % 180;
	if (birdPhase < 36) {
		int x = static_cast<int>(WIDTH + 2 - birdPhase);
		int y = 1 + static_cast<int>((birdPhase % 3) / 2);
		drawBird(state, x, y, CRGB(35, 35, 45));
	}
	if (birdPhase >= 88 && birdPhase < 124) {
		int x = static_cast<int>(-4 + (birdPhase - 88));
		int y = 1 + static_cast<int>((birdPhase % 5) / 3);
		drawBird(state, x, y, CRGB(35, 35, 45));
	}
	if (birdPhase >= 132 && birdPhase < 156) {
		int x = static_cast<int>(WIDTH + 8 - (birdPhase - 132) * 2);
		int y = 1 + static_cast<int>(((birdPhase - 132) % 4) / 2);
		drawBird(state, x, y, CRGB(35, 35, 45));
	}

	int tractorPhase = static_cast<int>(state.animationTick % 120);
	if (tractorPhase < 34) {
		int x = -6 + tractorPhase / 2;
		int y = HEIGHT - 4;
		drawTractor(state, x, y);
	} else if (tractorPhase >= 60 && tractorPhase < 94) {
		int x = WIDTH + 2 - (tractorPhase - 60) / 2;
		int y = HEIGHT - 4;
		drawTractor(state, x, y);
	}

	uint16_t phase = state.animationTick % 160;
	if (phase < 50) {
		int x = static_cast<int>(31 - phase / 5);
		int y = 6;
		setPixel(state, x, y, CRGB(200, 120, 60));
		setPixel(state, x - 1, y, CRGB(180, 100, 40));
	} else if (phase >= 80 && phase < 120) {
		int x = static_cast<int>(phase / 4 - 20);
		setPixel(state, x, 6, CRGB(130, 90, 50));
		setPixel(state, x + 1, 5, CRGB(130, 90, 50));
	}
}

static void renderImageWindow(AppState& state) {
	if (state.imagePixels.empty() || state.imageWidth == 0 || state.imageHeight == 0) {
		fill_solid(state.leds, NUM_LEDS, CRGB::Black);
		return;
	}

	int windowHeight = HEIGHT;
	int maxOffset = static_cast<int>(state.imageHeight) - windowHeight;
	if (maxOffset < 0) {
		maxOffset = 0;
	}

	if (state.animationTick % 5 == 0 && maxOffset > 0) {
		state.imageScrollY += state.imageScrollDirection;
		if (state.imageScrollY >= maxOffset) {
			state.imageScrollY = static_cast<int16_t>(maxOffset);
			state.imageScrollDirection = -1;
		} else if (state.imageScrollY <= 0) {
			state.imageScrollY = 0;
			state.imageScrollDirection = 1;
		}
	}

	for (uint8_t y = 0; y < HEIGHT; y++) {
		for (uint8_t x = 0; x < WIDTH; x++) {
			int srcY = y + state.imageScrollY;
			if (srcY >= 0 && srcY < state.imageHeight) {
				state.leds[XY(x, y)] = state.imagePixels[static_cast<size_t>(srcY) * state.imageWidth + x];
			} else {
				state.leds[XY(x, y)] = CRGB::Black;
			}
		}
	}
}

void renderTestScene(AppState& state) {
	if (state.selectedTest == MatrixTest::ScanWhitePixel) {
		uint16_t index = state.scanIndex % NUM_LEDS;
		uint8_t x = static_cast<uint8_t>(index % WIDTH);
		uint8_t y = static_cast<uint8_t>(index / WIDTH);
		state.leds[XY(x, y)] = CRGB::White;
		state.scanIndex = static_cast<uint16_t>((state.scanIndex + 1) % NUM_LEDS);
		return;
	}

	if (state.selectedTest == MatrixTest::SolidFill) {
		fill_solid(state.leds, NUM_LEDS, state.testSolidColor);
		return;
	}

	for (uint8_t y = 0; y < HEIGHT; y++) {
		for (uint8_t x = 0; x < WIDTH; x++) {
			uint8_t hue = static_cast<uint8_t>(state.animationTick * 2 + x * 6 + y * 10);
			state.leds[XY(x, y)] = CHSV(hue, 255, 255);
		}
	}
}

void renderDesignScene(AppState& state) {
	if (state.selectedDesign == MatrixDesign::Plasma) {
		for (uint8_t y = 0; y < HEIGHT; y++) {
			for (uint8_t x = 0; x < WIDTH; x++) {
				uint8_t p1 = sin8(static_cast<uint8_t>(x * 10 + state.animationTick * 3));
				uint8_t p2 = sin8(static_cast<uint8_t>(y * 18 + state.animationTick * 2));
				uint8_t hue = static_cast<uint8_t>((p1 / 2) + (p2 / 2));
				state.leds[XY(x, y)] = CHSV(hue, 230, 255);
			}
		}
		return;
	}

	if (state.selectedDesign == MatrixDesign::CheckerPulse) {
		uint8_t pulse = sin8(static_cast<uint8_t>(state.animationTick * 4));
		for (uint8_t y = 0; y < HEIGHT; y++) {
			for (uint8_t x = 0; x < WIDTH; x++) {
				bool checker = ((x + y) % 2) == 0;
				uint8_t hue = checker ? static_cast<uint8_t>(state.animationTick + 20)
				                      : static_cast<uint8_t>(state.animationTick + 140);
				state.leds[XY(x, y)] = CHSV(hue, 220, pulse);
			}
		}
		return;
	}

	if (state.selectedDesign == MatrixDesign::DiamondWave) {
		for (uint8_t y = 0; y < HEIGHT; y++) {
			for (uint8_t x = 0; x < WIDTH; x++) {
				int16_t dx = static_cast<int16_t>(x) - static_cast<int16_t>(WIDTH / 2);
				int16_t dy = static_cast<int16_t>(y) - static_cast<int16_t>(HEIGHT / 2);
				uint8_t diamond = static_cast<uint8_t>((abs(dx) + abs(dy)) * 20);
				uint8_t hue = static_cast<uint8_t>(diamond + state.animationTick * 3);
				state.leds[XY(x, y)] = CHSV(hue, 255, 255);
			}
		}
		return;
	}

	renderRollingField(state);
}

void renderImageScene(AppState& state) {
	renderImageWindow(state);
}
