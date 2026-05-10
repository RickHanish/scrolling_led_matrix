#include <Arduino.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <time.h>
#include "wifi_info.h"
#include "font.h"

#define DATA_PIN 18

#define WIDTH 32
#define HEIGHT 8
#define NUM_LEDS (WIDTH * HEIGHT)

#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

int scroll_x = WIDTH;

WebServer server(80);

String message;

CRGB currentColor = CRGB::White;
uint8_t currentBrightnessPct = 15;
uint8_t currentBrightness;

// Default message cycling state
bool customMessage = false; // true when user explicitly set a message
unsigned long customMessageStartedAt = 0;
const unsigned long customMessageMaxAgeMs = 48UL * 60UL * 60UL * 1000UL;
int defaultIndex = 0; // 0=welcome,1=time/date,2=weather,3=IP address
String cachedWeather = "";
String currentDefaultDisplay = ""; // built when a default message begins
String httpServerAddr = "";

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String("Time N/A");
  }

  char monthBuf[16];
  strftime(monthBuf, sizeof(monthBuf), "%b", &timeinfo);

  int hour12 = timeinfo.tm_hour % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }

  char buf[64];
  snprintf(
    buf,
    sizeof(buf),
    "%s %d, %d:%02d %s",
    monthBuf,
    timeinfo.tm_mday,
    hour12,
    timeinfo.tm_min,
    (timeinfo.tm_hour < 12) ? "AM" : "PM"
  );

  return String(buf);
}


String getWeatherString() {
  HTTPClient http;
  // Request textual condition + temperature to avoid emoji glyphs
  const char* url = "http://wttr.in/Houston?format=j1";
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      return String("Weather N/A");
    }

    String condition = doc["current_condition"][0]["weatherDesc"][0]["value"].as<String>();
    int tempFInt = doc["current_condition"][0]["temp_F"].as<int>();
    String tempFStr = doc["current_condition"][0]["temp_F"].as<String>();
    int maxRainChance = 0;
    JsonArray hourly = doc["weather"][0]["hourly"];
    for (JsonObject hour : hourly) {
      int rainChance = hour["chanceofrain"].as<int>();
      if (rainChance > maxRainChance) {
        maxRainChance = rainChance;
      }
    }
    String emoji;
    String lowerCondition = condition;
    lowerCondition.toLowerCase();

    if (lowerCondition.indexOf("thunder") >= 0 ||
        lowerCondition.indexOf("storm") >= 0 ||
        lowerCondition.indexOf("lightning") >= 0) {
      emoji = String("\U0001F329"); // storm cloud
    } else if (lowerCondition.indexOf("snow") >= 0 ||
               lowerCondition.indexOf("sleet") >= 0 ||
               lowerCondition.indexOf("hail") >= 0 ||
               lowerCondition.indexOf("blizzard") >= 0 ||
               tempFInt < 32) {
      emoji = String("\u2744"); // snowflake
    } else if (lowerCondition.indexOf("fog") >= 0 ||
               lowerCondition.indexOf("mist") >= 0 ||
               lowerCondition.indexOf("haze") >= 0 ||
               lowerCondition.indexOf("smoke") >= 0) {
      emoji = String("\U0001F32B"); // fog / haze
    } else if (lowerCondition.indexOf("partly") >= 0 ||
               lowerCondition.indexOf("sun and cloud") >= 0 ||
               lowerCondition.indexOf("sunny intervals") >= 0) {
      emoji = String("\u26C5"); // partly sunny
    } else if (lowerCondition.indexOf("rain") >= 0 ||
               lowerCondition.indexOf("drizzle") >= 0 ||
               lowerCondition.indexOf("shower") >= 0) {
      emoji = String("\U0001F4A7"); // raindrop
    } else if (lowerCondition.indexOf("cloud") >= 0 ||
               lowerCondition.indexOf("overcast") >= 0) {
      emoji = String("\u2601"); // cloud
    }
    else {
      emoji = String("\u2600\uFE0F"); // sunshine
    }

    return condition + " " + emoji + ", " + tempFStr + "°F, " + String(maxRainChance) + "% rain";
  }
  http.end();
  return String("Weather N/A");
}


bool decodeUtf8(const String& text, size_t& index, uint32_t& codepoint) {
  if (index >= text.length()) {
    return false;
  }

  const uint8_t first = static_cast<uint8_t>(text[index]);

  if (first < 0x80) {
    codepoint = first;
    index += 1;
    return true;
  }

  auto getContinuation = [&](size_t offset, uint8_t& value) -> bool {
    if (index + offset >= text.length()) {
      return false;
    }
    value = static_cast<uint8_t>(text[index + offset]);
    return (value & 0xC0) == 0x80;
  };

  uint8_t b1;
  uint8_t b2;
  uint8_t b3;

  if ((first & 0xE0) == 0xC0) {
    if (!getContinuation(1, b1)) {
      index += 1;
      codepoint = 0xFFFD;
      return true;
    }
    codepoint = ((first & 0x1F) << 6) | (b1 & 0x3F);
    index += 2;
    return true;
  }

  if ((first & 0xF0) == 0xE0) {
    if (!getContinuation(1, b1) || !getContinuation(2, b2)) {
      index += 1;
      codepoint = 0xFFFD;
      return true;
    }
    codepoint = ((first & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
    index += 3;
    return true;
  }

  if ((first & 0xF8) == 0xF0) {
    if (!getContinuation(1, b1) || !getContinuation(2, b2) || !getContinuation(3, b3)) {
      index += 1;
      codepoint = 0xFFFD;
      return true;
    }
    codepoint = ((first & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
    index += 4;
    return true;
  }

  index += 1;
  codepoint = 0xFFFD;
  return true;
}


String preprocessMessage(String s) {
  // Order matters: replace longer sequences first
  s.replace("<3",  String("\u2764"));     // heart
  s.replace(":)",  String("\U0001F642")); // smile
  s.replace(":(",  String("\U0001F641")); // frown

  return s;
}


size_t countGlyphs(const String& text) {
  size_t index = 0;
  size_t glyphCount = 0;
  uint32_t codepoint;

  while (decodeUtf8(text, index, codepoint)) {
    glyphCount++;
  }

  return glyphCount;
}


int renderedWidth(const String& text) {
  return static_cast<int>(countGlyphs(text) * 6);
}


void connect_WiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Opening HTTP server");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Cache server address for default-message display.
  httpServerAddr = "IP: " + WiFi.localIP().toString();
}


void handleRoot() {
  char currentColorHex[8];
  sprintf(currentColorHex, "#%02X%02X%02X", currentColor.r, currentColor.g, currentColor.b);

  String msgValue = customMessage ? message : String("");

  String html =
    "<meta charset='UTF-8'>"
    "<h2>LED Sign</h2>"
    "<form action='/set' method='POST'>"

    "Message:<br>"
    "<input name='msg' value='" + msgValue + "'><br><br>"

    "<input type='checkbox' name='use_default' value='1' " + String(!customMessage ? "checked" : "") + "> Use default messages<br><br>"

    "Color:<br>"
    "<input type='color' name='color' value='" + String(currentColorHex) + "'><br><br>"

    "Brightness:<br>"
    "<input type='number' name='brightness' min='0' max='100' value='" + String(currentBrightnessPct) + "'>%<br><br>"

    "<input type='hidden' name='key' value='" + String(API_KEY) + "'>"

    "<input type='submit'>"
    "</form>";

  server.send(200, "text/html", html);
}


void handleSet() {
  if (!server.hasArg("key")) {
    server.send(400, "text/plain", "Missing key");
    return;
  }

  if (server.arg("key") != API_KEY) {
    server.send(403, "text/plain", "Forbidden");
    return;
  }

  // If user checked 'use_default' then switch to default cycling
  if (server.hasArg("use_default")) {
    customMessage = false;
    customMessageStartedAt = 0;
    // reset defaults so the next default message starts fresh
    defaultIndex = 0;
    currentDefaultDisplay = "";
    scroll_x = WIDTH;
  } else {
    if (server.hasArg("msg")) {
      message = preprocessMessage(server.arg("msg"));
      customMessage = true;
      customMessageStartedAt = millis();
      // start showing the new custom message from the right
      scroll_x = WIDTH;
    }
  }

  if (server.hasArg("color")) {
    String c = server.arg("color"); // e.g. "#ff00aa"

    if (c.length() == 7 && c[0] == '#') {
      long rgb = strtol(c.substring(1).c_str(), nullptr, 16);

      currentColor.r = (rgb >> 16) & 0xFF;
      currentColor.g = (rgb >> 8) & 0xFF;
      currentColor.b = rgb & 0xFF;
    }
  }

  if (server.hasArg("brightness")) {
    String bStr = server.arg("brightness");
    bool valid = true;
    for (char c : bStr) {
      if (!isDigit(c)) valid = false;
    }
    if (valid) {
      int b = bStr.toInt();
      if (b < 0) b = 0;
      if (b > 100) b = 100;
      
      currentBrightnessPct = b;
    }
  }

  server.send(200, "text/plain", "Updating LED panel...");
}


uint16_t XY(uint8_t x, uint8_t y) {
  // Map physical position of pixel
  uint8_t px = HEIGHT - 1 - y;  // 0–7
  uint8_t py = x;  // 0–31

  // Apply serpentine on physical layout (width = 8)
  if (py % 2 == 0) {
    px = HEIGHT - px - 1;
  }

  return py * 8 + px;
}


void draw_glyph(int x_offset, int y_offset, uint32_t codepoint, CRGB color) {
  const uint8_t* bitmap = get_glyph(codepoint);
  if (bitmap == NULL) return;

  for (int col = 0; col < 5; col++) {
    uint8_t line = bitmap[col];

    for (int row = 0; row < 7; row++) {
      if (line & (1 << row)) {
        int x = x_offset + col;
        int y = y_offset + row;

        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
          leds[XY(x, y)] = color;
        }
      }
    }
  }
}


void setup() {
  Serial.begin(115200);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  currentBrightness = pow(currentBrightnessPct / 100.0, 2.2) * 255;
  FastLED.setBrightness(currentBrightness);

  connect_WiFi();

  // Initialize local timezone-aware NTP after Wi-Fi is connected.
  configTzTime("CST6CDT,M3.2.0/2,M11.1.0/2", "pool.ntp.org", "time.nist.gov");
  Serial.println(getTimeString());

  server.on("/", handleRoot);
  server.on("/set", HTTP_ANY, handleSet);

  server.begin();
  Serial.println("HTTP server started");

  server.onNotFound([]() {
    Serial.print("404: ");
    Serial.println(server.uri());
  });
}


void loop() {
  server.handleClient();

  if (customMessage && customMessageStartedAt != 0) {
    unsigned long customAge = millis() - customMessageStartedAt;
    if (customAge >= customMessageMaxAgeMs) {
      customMessage = false;
      customMessageStartedAt = 0;
      defaultIndex = 0;
      currentDefaultDisplay = "";
      scroll_x = WIDTH;
    }
  }

  currentBrightness = pow(currentBrightnessPct / 100.0, 2.2) * 255;
  FastLED.setBrightness(currentBrightness);

  FastLED.clear();

  // Choose display message (custom or rotating defaults)
  String displayMessage;
  if (customMessage && message.length() > 0) {
    displayMessage = message;
  } else {
    // Build current default display only when needed so we fetch weather sparingly
    if (currentDefaultDisplay.length() == 0) {
      if (defaultIndex == 0) {
        currentDefaultDisplay = String("Welcome!");
      }
      else if (defaultIndex == 1) {
        currentDefaultDisplay = getTimeString();
      }
      else if (defaultIndex == 2) {
        cachedWeather = getWeatherString();
        currentDefaultDisplay = cachedWeather;
      }
      else if (defaultIndex == 3) {
        currentDefaultDisplay = httpServerAddr.length() > 0 ? httpServerAddr : String("IP N/A");
      }
    }
    displayMessage = currentDefaultDisplay;
  }

  int x = scroll_x;
  size_t index = 0;
  uint32_t codepoint;

  while (decodeUtf8(displayMessage, index, codepoint)) {
    if (codepoint == 0xFE0F) { // variation selector-16 (emoji style), skip it
      continue;
    }
    draw_glyph(x, 0, codepoint, currentColor);
    x += 6;
  }

  FastLED.show();
  delay(100);

  scroll_x--;

  if (scroll_x < -renderedWidth(displayMessage)) {
    // advance to next default message and force rebuild on next frame
    currentDefaultDisplay = "";
    defaultIndex = (defaultIndex + 1) % 4;
    scroll_x = WIDTH;
  }
}
