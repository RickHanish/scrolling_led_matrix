#include "display/matrix_display.h"

#include <FastLED.h>
#include <math.h>

#include "app/app_state.h"
#include "config/constants.h"

void initMatrix(AppState& state) {
	FastLED.addLeds<WS2812B, DATA_PIN, GRB>(state.leds, NUM_LEDS);
	FastLED.setBrightness(255);
	applyBrightness(state);
}

void applyBrightness(AppState& state) {
	uint8_t cappedBrightnessPct = state.currentBrightnessPct;
	if (state.renderBrightnessLimitPct < cappedBrightnessPct) {
		cappedBrightnessPct = state.renderBrightnessLimitPct;
	}

	state.currentBrightness = static_cast<uint8_t>(pow(cappedBrightnessPct / 100.0, 2.2) * 255);
	FastLED.setBrightness(state.currentBrightness);
}
