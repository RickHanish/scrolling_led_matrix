#include "network/wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app/app_state.h"

void connectWiFi(AppState& state, const char* ssid, const char* password) {
	WiFi.begin(ssid, password);

	Serial.print("Opening HTTP server");

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("\nConnected!");
	Serial.print("IP: ");
	Serial.println(WiFi.localIP());

	state.httpServerAddr = "IP: " + WiFi.localIP().toString();
}
