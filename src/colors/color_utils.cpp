#include "colors/color_utils.h"

#include "app/app_state.h"

bool parseHexColor(const String& value, CRGB& outColor) {
	if (value.length() != 7 || value[0] != '#') {
		return false;
	}

	long rgb = strtol(value.substring(1).c_str(), nullptr, 16);
	outColor.r = (rgb >> 16) & 0xFF;
	outColor.g = (rgb >> 8) & 0xFF;
	outColor.b = rgb & 0xFF;
	return true;
}

String colorToHex(const CRGB& color) {
	char buf[8];
	snprintf(buf, sizeof(buf), "#%02X%02X%02X", color.r, color.g, color.b);
	return String(buf);
}

ColorMode parseColorMode(const String& modeValue) {
	if (modeValue == "gradient") {
		return ColorMode::Gradient;
	}
	if (modeValue == "per_letter") {
		return ColorMode::PerLetter;
	}
	return ColorMode::Solid;
}

String colorModeToString(ColorMode mode) {
	if (mode == ColorMode::Gradient) {
		return String("gradient");
	}
	if (mode == ColorMode::PerLetter) {
		return String("per_letter");
	}
	return String("solid");
}

String serializePerLetterColors(const AppState& state) {
	String csv;
	for (size_t i = 0; i < state.perLetterColors.size(); i++) {
		if (i > 0) {
			csv += ",";
		}
		csv += colorToHex(state.perLetterColors[i]);
	}
	return csv;
}

void parsePerLetterColors(AppState& state, const String& csv) {
	state.perLetterColors.clear();
	size_t start = 0;

	while (start <= csv.length()) {
		int commaIndex = csv.indexOf(',', start);
		String token;
		if (commaIndex < 0) {
			token = csv.substring(start);
			start = csv.length() + 1;
		} else {
			token = csv.substring(start, commaIndex);
			start = static_cast<size_t>(commaIndex) + 1;
		}

		token.trim();
		if (token.length() == 0) {
			continue;
		}

		CRGB color;
		if (parseHexColor(token, color)) {
			state.perLetterColors.push_back(color);
		}
	}
}

static CRGB blendGradientColor(const CRGB& start, const CRGB& end, size_t index, size_t count) {
	if (count <= 1) {
		return start;
	}

	int32_t denom = static_cast<int32_t>(count - 1);
	int32_t i = static_cast<int32_t>(index);

	CRGB out;
	out.r = static_cast<uint8_t>(start.r + ((static_cast<int32_t>(end.r) - start.r) * i) / denom);
	out.g = static_cast<uint8_t>(start.g + ((static_cast<int32_t>(end.g) - start.g) * i) / denom);
	out.b = static_cast<uint8_t>(start.b + ((static_cast<int32_t>(end.b) - start.b) * i) / denom);
	return out;
}

CRGB colorForGlyph(const AppState& state, size_t glyphIndex, size_t glyphCount) {
	if (state.colorMode == ColorMode::Gradient) {
		return blendGradientColor(state.gradientStartColor, state.gradientEndColor, glyphIndex, glyphCount);
	}

	if (state.colorMode == ColorMode::PerLetter && glyphIndex < state.perLetterColors.size()) {
		return state.perLetterColors[glyphIndex];
	}

	return state.currentColor;
}
