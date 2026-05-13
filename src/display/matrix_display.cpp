#include "display/matrix_display.h"

#include <FastLED.h>
#include <math.h>

#include "app/app_state.h"
#include "config/constants.h"

void initMatrix(AppState& state) {
	FastLED.addLeds<WS2812B, DATA_PIN, GRB>(state.leds, NUM_LEDS);
	applyBrightness(state);
}

void applyBrightness(AppState& state) {
	state.currentBrightness = static_cast<uint8_t>(pow(state.currentBrightnessPct / 100.0, 2.2) * 255);
	FastLED.setBrightness(state.currentBrightness);
}
