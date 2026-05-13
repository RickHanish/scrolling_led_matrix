#include "display/renderer.h"

#include <FastLED.h>

#include "app/app_state.h"
#include "app/message_manager.h"
#include "colors/color_utils.h"
#include "config/constants.h"
#include "display/font.h"
#include "text/text_utils.h"
#include "text/utf8.h"

static void renderMessageFrame(AppState& state);
static void renderTestFrame(AppState& state);
static void renderDesignFrame(AppState& state);

static uint16_t XY(uint8_t x, uint8_t y) {
	uint8_t px = HEIGHT - 1 - y;
	uint8_t py = x;

	if (py % 2 == 0) {
		px = HEIGHT - px - 1;
	}

	return py * 8 + px;
}

static void drawGlyph(AppState& state, int xOffset, int yOffset, uint32_t codepoint, CRGB color) {
	const uint8_t* bitmap = get_glyph(codepoint);
	if (bitmap == NULL) {
		return;
	}

	for (int col = 0; col < 5; col++) {
		uint8_t line = bitmap[col];

		for (int row = 0; row < 7; row++) {
			if (line & (1 << row)) {
				int x = xOffset + col;
				int y = yOffset + row;

				if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
					state.leds[XY(x, y)] = color;
				}
			}
		}
	}
}

void renderFrame(AppState& state) {
	FastLED.clear();

	if (state.renderProgram == RenderProgram::Test) {
		renderTestFrame(state);
	} else if (state.renderProgram == RenderProgram::Design) {
		renderDesignFrame(state);
	} else {
		renderMessageFrame(state);
	}

	FastLED.show();
	delay(100);
	state.animationTick++;
}

static void renderMessageFrame(AppState& state) {

	String displayMessage = getDisplayMessage(state);
	int x = state.scrollX;
	size_t index = 0;
	uint32_t codepoint;
	size_t glyphIndex = 0;
	size_t glyphCount = countGlyphs(displayMessage);

	while (decodeUtf8(displayMessage, index, codepoint)) {
		if (codepoint == 0xFE0F) {
			continue;
		}
		drawGlyph(state, x, 0, codepoint, colorForGlyph(state, glyphIndex, glyphCount));
		glyphIndex++;
		x += 6;
	}

	state.scrollX--;
	advanceMessageScrollState(state, displayMessage);
}

static void renderTestFrame(AppState& state) {
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

static void renderDesignFrame(AppState& state) {
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

	for (uint8_t y = 0; y < HEIGHT; y++) {
		for (uint8_t x = 0; x < WIDTH; x++) {
			int16_t dx = static_cast<int16_t>(x) - static_cast<int16_t>(WIDTH / 2);
			int16_t dy = static_cast<int16_t>(y) - static_cast<int16_t>(HEIGHT / 2);
			uint8_t diamond = static_cast<uint8_t>((abs(dx) + abs(dy)) * 20);
			uint8_t hue = static_cast<uint8_t>(diamond + state.animationTick * 3);
			state.leds[XY(x, y)] = CHSV(hue, 255, 255);
		}
	}
}
