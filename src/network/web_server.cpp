#include "network/web_server.h"

#include <Arduino.h>

#include "app/app_state.h"
#include "colors/color_utils.h"
#include "config/constants.h"
#include "text/text_utils.h"
#include "web/index_html.h"

static AppState* gState = nullptr;
static const char* gApiKey = nullptr;

static RenderProgram parseRenderProgram(const String& value) {
	if (value == "test") {
		return RenderProgram::Test;
	}
	if (value == "design") {
		return RenderProgram::Design;
	}
	return RenderProgram::Message;
}

static MatrixTest parseMatrixTest(const String& value) {
	if (value == "solid_fill") {
		return MatrixTest::SolidFill;
	}
	if (value == "rainbow_cycle") {
		return MatrixTest::RainbowCycle;
	}
	return MatrixTest::ScanWhitePixel;
}

static MatrixDesign parseMatrixDesign(const String& value) {
	if (value == "checker_pulse") {
		return MatrixDesign::CheckerPulse;
	}
	if (value == "diamond_wave") {
		return MatrixDesign::DiamondWave;
	}
	if (value == "rolling_field") {
		return MatrixDesign::RollingField;
	}
	return MatrixDesign::Plasma;
}

static bool parsePackedHexColor3(const String& token, CRGB& outColor) {
	if (token.length() == 6) {
		return parseHexColor(String("#") + token, outColor);
	}

	if (token.length() != 3) {
		return false;
	}

	int r = strtol(token.substring(0, 1).c_str(), nullptr, 16);
	int g = strtol(token.substring(1, 2).c_str(), nullptr, 16);
	int b = strtol(token.substring(2, 3).c_str(), nullptr, 16);
	outColor.r = static_cast<uint8_t>((r << 4) | r);
	outColor.g = static_cast<uint8_t>((g << 4) | g);
	outColor.b = static_cast<uint8_t>((b << 4) | b);
	return true;
}

static bool parseImagePixels(const String& packedPixels, uint16_t width, uint16_t height, std::vector<CRGB>& outPixels) {
	outPixels.clear();
	if (width == 0 || height == 0) {
		return false;
	}

	outPixels.reserve(static_cast<size_t>(width) * height);
	size_t start = 0;
	while (start <= packedPixels.length()) {
		int commaIndex = packedPixels.indexOf(',', start);
		String token;
		if (commaIndex < 0) {
			token = packedPixels.substring(start);
			start = packedPixels.length() + 1;
		} else {
			token = packedPixels.substring(start, commaIndex);
			start = static_cast<size_t>(commaIndex) + 1;
		}

		token.trim();
		if (token.length() == 0) {
			continue;
		}

		CRGB color;
		if (!parsePackedHexColor3(token, color)) {
			return false;
		}
		outPixels.push_back(color);
	}

	return outPixels.size() == static_cast<size_t>(width) * height;
}

static void setBrightnessLimitForProgram(AppState& state, RenderProgram program, MatrixTest testSelection) {
	if (program == RenderProgram::Message) {
		state.renderBrightnessLimitPct = 100;
		return;
	}

	if (program == RenderProgram::Test) {
		state.renderBrightnessLimitPct = (testSelection == MatrixTest::ScanWhitePixel) ? 100 : 50;
		return;
	}

	state.renderBrightnessLimitPct = 50;
}

static void handleRoot() {
	if (gState == nullptr || gApiKey == nullptr) {
		return;
	}

	gState->server.send(200, "text/html", buildIndexHtml(*gState, gApiKey));
}

static void handleSet() {
	if (gState == nullptr || gApiKey == nullptr) {
		return;
	}

	WebServer& server = gState->server;

	if (!server.hasArg("key")) {
		server.send(400, "text/plain", "Missing key");
		return;
	}

	if (server.arg("key") != gApiKey) {
		server.send(403, "text/plain", "Forbidden");
		return;
	}

	if (server.hasArg("mode")) {
		gState->colorMode = parseColorMode(server.arg("mode"));
	}

	if (server.hasArg("solid_color")) {
		parseHexColor(server.arg("solid_color"), gState->currentColor);
	}

	if (server.hasArg("gradient_start")) {
		parseHexColor(server.arg("gradient_start"), gState->gradientStartColor);
	}

	if (server.hasArg("gradient_end")) {
		parseHexColor(server.arg("gradient_end"), gState->gradientEndColor);
	}

	if (server.hasArg("letter_colors")) {
		parsePerLetterColors(*gState, server.arg("letter_colors"));
	}

	if (server.hasArg("program")) {
		gState->renderProgram = parseRenderProgram(server.arg("program"));
	}

	if (server.hasArg("test_name")) {
		gState->selectedTest = parseMatrixTest(server.arg("test_name"));
	}

	if (server.hasArg("design_name")) {
		gState->selectedDesign = parseMatrixDesign(server.arg("design_name"));
	}

	if (server.hasArg("test_color")) {
		parseHexColor(server.arg("test_color"), gState->testSolidColor);
	}

	if (server.hasArg("image_width") && server.hasArg("image_height") && server.hasArg("image_pixels")) {
		uint16_t width = static_cast<uint16_t>(server.arg("image_width").toInt());
		uint16_t height = static_cast<uint16_t>(server.arg("image_height").toInt());
		std::vector<CRGB> pixels;
		if (!parseImagePixels(server.arg("image_pixels"), width, height, pixels)) {
			server.send(400, "text/plain", "Invalid image data");
			return;
		}
		gState->imageWidth = width;
		gState->imageHeight = height;
		gState->imagePixels = pixels;
		gState->imageScrollY = 0;
		gState->imageScrollDirection = 1;
		gState->renderProgram = RenderProgram::Image;
	}

	String action = server.hasArg("action") ? server.arg("action") : String("");

	if (action == "use_default" || server.hasArg("use_default")) {
		gState->renderProgram = RenderProgram::Message;
		setBrightnessLimitForProgram(*gState, gState->renderProgram, gState->selectedTest);
		gState->customMessage = false;
		gState->customMessageStartedAt = 0;
		gState->defaultIndex = 0;
		gState->currentDefaultDisplay = "";
		gState->scrollX = WIDTH;
	} else if (action == "run_test") {
		gState->renderProgram = RenderProgram::Test;
		setBrightnessLimitForProgram(*gState, gState->renderProgram, gState->selectedTest);
		gState->scanIndex = 0;
	} else if (action == "run_design") {
		gState->renderProgram = RenderProgram::Design;
		setBrightnessLimitForProgram(*gState, gState->renderProgram, gState->selectedTest);
	} else if (action == "run_image") {
		gState->renderProgram = RenderProgram::Image;
		setBrightnessLimitForProgram(*gState, gState->renderProgram, gState->selectedTest);
	} else if (server.hasArg("msg")) {
		gState->message = preprocessMessage(server.arg("msg"));
		gState->renderProgram = RenderProgram::Message;
		setBrightnessLimitForProgram(*gState, gState->renderProgram, gState->selectedTest);
		gState->customMessage = true;
		gState->customMessageStartedAt = millis();
		gState->scrollX = WIDTH;
	}

	if (server.hasArg("color")) {
		parseHexColor(server.arg("color"), gState->currentColor);
	}

	if (server.hasArg("brightness")) {
		String bStr = server.arg("brightness");
		bool valid = true;
		for (char c : bStr) {
			if (!isDigit(c)) {
				valid = false;
			}
		}
		if (valid) {
			int b = bStr.toInt();
			if (b < 0) {
				b = 0;
			}
			if (b > 100) {
				b = 100;
			}
			gState->currentBrightnessPct = static_cast<uint8_t>(b);
		}
	}

	setBrightnessLimitForProgram(*gState, gState->renderProgram, gState->selectedTest);

	server.send(200, "text/plain", "Settings updated. Matrix mode changed.");
}

void setupWebServer(AppState& state, const char* apiKey) {
	gState = &state;
	gApiKey = apiKey;

	state.server.on("/", handleRoot);
	state.server.on("/favicon.ico", HTTP_GET, []() {
		if (gState != nullptr) {
			gState->server.send(204, "text/plain", "");
		}
	});
	state.server.on("/set", HTTP_ANY, handleSet);
	state.server.begin();

	Serial.println("HTTP server started");
	state.server.onNotFound([]() {
		Serial.print("404: ");
		Serial.println(gState->server.uri());
	});
}

void pollWebServer(AppState& state) {
	state.server.handleClient();
}
