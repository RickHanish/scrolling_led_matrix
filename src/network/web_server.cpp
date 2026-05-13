#include "network/web_server.h"

#include <Arduino.h>

#include "app/app_state.h"
#include "colors/color_utils.h"
#include "config/constants.h"
#include "text/text_utils.h"
#include "web/index_html.h"

static AppState* gState = nullptr;
static const char* gApiKey = nullptr;

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

	String action = server.hasArg("action") ? server.arg("action") : String("");

	if (action == "use_default" || server.hasArg("use_default")) {
		gState->customMessage = false;
		gState->customMessageStartedAt = 0;
		gState->defaultIndex = 0;
		gState->currentDefaultDisplay = "";
		gState->scrollX = WIDTH;
	} else if (server.hasArg("msg")) {
		gState->message = preprocessMessage(server.arg("msg"));
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

	server.send(200, "text/plain", "Message sent successfully! Updating LED panel...");
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
