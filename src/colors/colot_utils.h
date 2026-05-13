#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <cstddef>
#include "colors/color_modes.h"

struct AppState;

bool parseHexColor(const String& value, CRGB& outColor);
String colorToHex(const CRGB& color);
ColorMode parseColorMode(const String& modeValue);
String colorModeToString(ColorMode mode);
String serializePerLetterColors(const AppState& state);
void parsePerLetterColors(AppState& state, const String& csv);
CRGB colorForGlyph(const AppState& state, size_t glyphIndex, size_t glyphCount);
