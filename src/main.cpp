#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
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

// String message =
//   "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789 !\"#$%&'()*+,-./:;<=>?@" +
//   String("\u2764") +     // heart
//   String("\U0001F642") + // smiley face
//   String("\U0001F641") + // sad face
//   String("\U0001F31E") + // sunshine
//   String("\U0001F44D") + // thumbs up
//   String("\u2601") +     // cloud
//   String("\U0001F4A7");        // rain drop
String message =
  "Hello <3 :) :(" +
  String("\u2600\uFE0F") + // sunshine
  String("\u2764") +       // heart
  String("\U0001F642") +   // smiley face
  String("\U0001F641") +   // sad face
  String("\U0001F31E") +   // sunshine
  String("\U0001F44D") +   // thumbs up
  String("\u2601") +       // cloud
  String("\U0001F4A7");    // rain drop
// String message = "Hello world";
CRGB currentColor = CRGB::White;
uint8_t currentBrightnessPct = 15;
uint8_t currentBrightness;


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

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}


void handleRoot() {
  // long rgb = strtol(c.substring(1).c_str(), nullptr, 16);
  // currentColor.r = (rgb >> 16) & 0xFF;
  // currentColor.g = (rgb >> 8) & 0xFF;
  // currentColor.b = rgb & 0xFF;
  char currentColorHex[8];
  sprintf(currentColorHex, "#%02X%02X%02X", currentColor.r, currentColor.g, currentColor.b);

  String html =
    "<meta charset='UTF-8'>"
    "<h2>LED Sign</h2>"
    "<form action='/set' method='POST'>"

    "Message:<br>"
    "<input name='msg' value='" + message + "'><br><br>"

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
  if (!server.hasArg("msg") || !server.hasArg("key")) {
    server.send(400, "text/plain", "Missing args");
    return;
  }

  if (server.arg("key") != API_KEY) {
    server.send(403, "text/plain", "Forbidden");
    return;
  }

  message = preprocessMessage(server.arg("msg"));

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

  FastLED.addLeds<WS2812B, 18, GRB>(leds, NUM_LEDS);
  currentBrightness = pow(currentBrightnessPct / 100.0, 2.2) * 255;
  FastLED.setBrightness(currentBrightness);

  connect_WiFi();

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

  currentBrightness = pow(currentBrightnessPct / 100.0, 2.2) * 255;
  FastLED.setBrightness(currentBrightness);

  FastLED.clear();

  int x = scroll_x;
  size_t index = 0;
  uint32_t codepoint;

  while (decodeUtf8(message, index, codepoint)) {
    if (codepoint == 0xFE0F) { // variation selector-16 (emoji style), skip it
      continue;
    }
    draw_glyph(x, 0, codepoint, currentColor);
    x += 6;
  }

  FastLED.show();
  delay(100);

  scroll_x--;

  if (scroll_x < -renderedWidth(message)) {
    scroll_x = WIDTH;
  }
}
