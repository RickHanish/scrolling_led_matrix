#include <Arduino.h>
#include <time.h>

#include "app/app_state.h"
#include "app/message_manager.h"
#include "config/constants.h"
#include "config/secrets.h"
#include "display/matrix_display.h"
#include "display/renderer.h"
#include "network/web_server.h"
#include "network/wifi_manager.h"

AppState appState;

void setup() {
  Serial.begin(115200);

  initMatrix(appState);
  connectWiFi(appState, WIFI_SSID, WIFI_PASSWORD);

  configTzTime(NTP_TZ, NTP_SERVER_1, NTP_SERVER_2);
  Serial.println(getTimeString());

  setupWebServer(appState, API_KEY);
}

void loop() {
  pollWebServer(appState);
  updateMessageTimeout(appState);
  applyBrightness(appState);
  renderFrame(appState);
}
