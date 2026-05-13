#include "display/renderer.h"

#include <FastLED.h>

#include "app/app_state.h"
#include "app/message_manager.h"
#include "colors/colot_utils.h"
#include "config/constants.h"
#include "display/font.h"
#include "text/text_utils.h"
#include "text/utf8.h"

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
		drawGlyph(state, x, 0, codepoint, applyBrightnessToColor(state, colorForGlyph(state, glyphIndex, glyphCount)));
		glyphIndex++;
		x += 6;
	}

	FastLED.show();
	delay(100);

	state.scrollX--;
	advanceMessageScrollState(state, displayMessage);
}
