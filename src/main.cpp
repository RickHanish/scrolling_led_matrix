#include <Arduino.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <time.h>
#include <vector>
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
CRGB gradientStartColor = CRGB::Red;
CRGB gradientEndColor = CRGB::Blue;
std::vector<CRGB> perLetterColors;

enum class ColorMode : uint8_t {
  Solid,
  Gradient,
  PerLetter
};

ColorMode currentColorMode = ColorMode::Solid;

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


String escapeForJs(const String& input) {
  String out;
  out.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    char ch = input[i];
    if (ch == '\\') {
      out += "\\\\";
    } else if (ch == '"') {
      out += "\\\"";
    } else if (ch == '\n') {
      out += "\\n";
    } else if (ch == '\r') {
      out += "\\r";
    } else {
      out += ch;
    }
  }

  return out;
}


String serializePerLetterColors() {
  String csv = "";
  for (size_t i = 0; i < perLetterColors.size(); i++) {
    if (i > 0) {
      csv += ",";
    }
    csv += colorToHex(perLetterColors[i]);
  }
  return csv;
}


void parsePerLetterColors(const String& csv) {
  perLetterColors.clear();
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
      perLetterColors.push_back(color);
    }
  }
}


CRGB blendGradientColor(const CRGB& start, const CRGB& end, size_t index, size_t count) {
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


CRGB colorForGlyph(size_t glyphIndex, size_t glyphCount) {
  if (currentColorMode == ColorMode::Gradient) {
    return blendGradientColor(gradientStartColor, gradientEndColor, glyphIndex, glyphCount);
  }

  if (currentColorMode == ColorMode::PerLetter) {
    if (glyphIndex < perLetterColors.size()) {
      return perLetterColors[glyphIndex];
    }
  }

  return currentColor;
}

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
    if (codepoint == 0xFE0F) {
      continue;
    }
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
  String msgValue = customMessage ? message : String("");

  String html = R"HTML(
<!doctype html>
<html lang='en'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>LED Matrix Control</title>
  <style>
    :root {
      --bg-0: #0e1a2b;
      --bg-1: #142845;
      --card: #f7fbff;
      --ink: #11213a;
      --muted: #5a6c88;
      --accent: #0f8cff;
      --accent-2: #23d6b5;
      --danger: #d64545;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Trebuchet MS", "Segoe UI", sans-serif;
      color: var(--ink);
      min-height: 100vh;
      background:
        radial-gradient(1200px 800px at -10% -20%, #3c6ea5 0%, rgba(60,110,165,0) 45%),
        radial-gradient(1200px 900px at 120% 10%, #3fd8c2 0%, rgba(63,216,194,0) 40%),
        linear-gradient(135deg, var(--bg-0), var(--bg-1));
      display: grid;
      place-items: center;
      padding: 18px;
    }
    .panel {
      width: min(980px, 100%);
      background: linear-gradient(180deg, #ffffff, #f3f9ff);
      border-radius: 18px;
      box-shadow: 0 20px 48px rgba(0, 0, 0, 0.25);
      padding: 18px;
      display: grid;
      gap: 14px;
      animation: fadeIn 300ms ease-out;
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(8px); }
      to { opacity: 1; transform: translateY(0); }
    }
    h1 {
      margin: 2px 0 0;
      font-size: 1.35rem;
      letter-spacing: 0.3px;
    }
    .sub {
      margin: 0;
      color: var(--muted);
      font-size: 0.95rem;
    }
    .grid {
      display: grid;
      grid-template-columns: 1.3fr 1fr;
      gap: 12px;
    }
    .card {
      border: 1px solid #dce8f5;
      border-radius: 14px;
      padding: 12px;
      background: #ffffff;
    }
    label {
      font-weight: 600;
      font-size: 0.92rem;
      display: block;
      margin-bottom: 6px;
    }
    input[type='text'],
    input[type='number'],
    select {
      width: 100%;
      border: 1px solid #bfd2e6;
      border-radius: 10px;
      padding: 10px 12px;
      font-size: 0.95rem;
      color: var(--ink);
      background: #fff;
    }
    input[type='color'] {
      width: 100%;
      border: 1px solid #bfd2e6;
      border-radius: 10px;
      min-height: 44px;
      background: #fff;
      padding: 4px;
    }
    .row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    .actions {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
    }
    button {
      border: 0;
      border-radius: 11px;
      padding: 10px 14px;
      font-size: 0.95rem;
      cursor: pointer;
      transition: transform 120ms ease, box-shadow 120ms ease;
    }
    button:hover {
      transform: translateY(-1px);
      box-shadow: 0 8px 18px rgba(9, 39, 78, 0.16);
    }
    .btn-primary {
      background: linear-gradient(90deg, var(--accent), #2a7bff);
      color: #fff;
      font-weight: 700;
    }
    .btn-secondary {
      background: linear-gradient(90deg, #f0f4fa, #e9f6ff);
      color: #1b3557;
      border: 1px solid #c8d9ec;
      font-weight: 700;
    }
    .preview {
      min-height: 66px;
      border-radius: 12px;
      border: 1px dashed #bfd2e6;
      background: repeating-linear-gradient(
        -45deg,
        rgba(12, 109, 210, 0.06),
        rgba(12, 109, 210, 0.06) 8px,
        rgba(12, 109, 210, 0.02) 8px,
        rgba(12, 109, 210, 0.02) 16px
      );
      display: flex;
      align-items: center;
      gap: 1px;
      font-size: 1.25rem;
      font-weight: 700;
      letter-spacing: 1px;
      padding: 10px;
      overflow-x: auto;
      white-space: nowrap;
    }
    .letter-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(68px, 1fr));
      gap: 8px;
      max-height: 240px;
      overflow-y: auto;
      padding-right: 2px;
    }
    .swatch {
      border: 1px solid #c7d8ea;
      border-radius: 10px;
      padding: 6px;
      background: #f9fcff;
      display: grid;
      gap: 5px;
    }
    .swatch code {
      font-size: 0.72rem;
      color: var(--muted);
      text-align: center;
    }
    .status {
      font-size: 0.9rem;
      color: var(--muted);
      min-height: 1.2em;
    }
    .status.ok { color: #14873a; }
    .status.err { color: var(--danger); }
    @media (max-width: 900px) {
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main class='panel'>
    <div>
      <h1>LED Matrix Controller</h1>
      <p class='sub'>Send a custom message, switch to defaults, and design text colors with solid, gradient, or per-letter styles.</p>
    </div>

    <div class='grid'>
      <section class='card'>
        <label for='msg'>Custom Message</label>
        <input id='msg' type='text' maxlength='220' placeholder='Type your message'>

        <div style='height:10px'></div>

        <label for='mode'>Color Mode</label>
        <select id='mode'>
          <option value='solid'>Solid color</option>
          <option value='gradient'>Gradient across message</option>
          <option value='per_letter'>Color per letter</option>
        </select>

        <div style='height:10px'></div>

        <div id='solidWrap'>
          <label for='solidColor'>Solid Color</label>
          <input id='solidColor' type='color'>
        </div>

        <div id='gradientWrap' class='row' style='display:none'>
          <div>
            <label for='gradientStart'>Gradient Start</label>
            <input id='gradientStart' type='color'>
          </div>
          <div>
            <label for='gradientEnd'>Gradient End</label>
            <input id='gradientEnd' type='color'>
          </div>
        </div>

        <div style='height:10px'></div>

        <label for='brightness'>Brightness (%)</label>
        <input id='brightness' type='number' min='0' max='100'>

        <div style='height:12px'></div>
        <div class='actions'>
          <button id='sendCustom' class='btn-primary' type='button'>Submit Custom Message</button>
          <button id='useDefault' class='btn-secondary' type='button'>Use Default Messages</button>
        </div>
        <p id='status' class='status'></p>
      </section>

      <section class='card'>
        <label>Preview</label>
        <div id='preview' class='preview'>Preview</div>

        <div style='height:10px'></div>
        <div id='letterWrap' style='display:none'>
          <label>Per-Letter Colors</label>
          <div id='letterGrid' class='letter-grid'></div>
        </div>
      </section>
    </div>
  </main>

  <script>
    const state = {
      key: "__API_KEY__",
      message: "__MESSAGE__",
      mode: "__MODE__",
      solid: "__SOLID__",
      gradientStart: "__GRADIENT_START__",
      gradientEnd: "__GRADIENT_END__",
      brightness: __BRIGHTNESS__,
      letterColorsCsv: "__LETTER_COLORS__"
    };

    const msgInput = document.getElementById('msg');
    const modeInput = document.getElementById('mode');
    const solidColorInput = document.getElementById('solidColor');
    const gradientStartInput = document.getElementById('gradientStart');
    const gradientEndInput = document.getElementById('gradientEnd');
    const brightnessInput = document.getElementById('brightness');
    const preview = document.getElementById('preview');
    const letterGrid = document.getElementById('letterGrid');
    const letterWrap = document.getElementById('letterWrap');
    const solidWrap = document.getElementById('solidWrap');
    const gradientWrap = document.getElementById('gradientWrap');
    const statusEl = document.getElementById('status');

    let letterColors = (state.letterColorsCsv || '').split(',').map(v => v.trim()).filter(Boolean);

    function glyphsFromMessage(message) {
      return Array.from(message || '').filter(ch => ch !== '\uFE0F');
    }

    function clampColor(hex) {
      return /^#[0-9a-fA-F]{6}$/.test(hex) ? hex.toUpperCase() : state.solid;
    }

    function ensureLetterColors() {
      const glyphs = glyphsFromMessage(msgInput.value);
      const fallback = clampColor(solidColorInput.value || state.solid);
      const next = [];
      for (let i = 0; i < glyphs.length; i++) {
        next.push(clampColor(letterColors[i] || fallback));
      }
      letterColors = next;
    }

    function renderLetterPickers() {
      ensureLetterColors();
      const glyphs = glyphsFromMessage(msgInput.value);
      letterGrid.innerHTML = '';

      glyphs.forEach((glyph, i) => {
        const wrapper = document.createElement('div');
        wrapper.className = 'swatch';

        const code = document.createElement('code');
        const shown = glyph === ' ' ? 'space' : glyph;
        code.textContent = shown;

        const picker = document.createElement('input');
        picker.type = 'color';
        picker.value = letterColors[i];
        picker.addEventListener('input', () => {
          letterColors[i] = picker.value.toUpperCase();
          renderPreview();
        });

        wrapper.appendChild(code);
        wrapper.appendChild(picker);
        letterGrid.appendChild(wrapper);
      });
    }

    function hexToRgb(hex) {
      const h = (hex || '#FFFFFF').replace('#', '');
      return {
        r: parseInt(h.substring(0, 2), 16),
        g: parseInt(h.substring(2, 4), 16),
        b: parseInt(h.substring(4, 6), 16)
      };
    }

    function rgbToHex(c) {
      const two = (v) => v.toString(16).padStart(2, '0');
      return `#${two(c.r)}${two(c.g)}${two(c.b)}`.toUpperCase();
    }

    function gradientColor(startHex, endHex, index, count) {
      if (count <= 1) {
        return clampColor(startHex);
      }
      const a = hexToRgb(startHex);
      const b = hexToRgb(endHex);
      const t = index / (count - 1);
      return rgbToHex({
        r: Math.round(a.r + (b.r - a.r) * t),
        g: Math.round(a.g + (b.g - a.g) * t),
        b: Math.round(a.b + (b.b - a.b) * t)
      });
    }

    function colorForIndex(index, total) {
      if (modeInput.value === 'gradient') {
        return gradientColor(gradientStartInput.value, gradientEndInput.value, index, total);
      }
      if (modeInput.value === 'per_letter') {
        return clampColor(letterColors[index] || solidColorInput.value);
      }
      return clampColor(solidColorInput.value);
    }

    function renderPreview() {
      const glyphs = glyphsFromMessage(msgInput.value);
      preview.innerHTML = '';

      if (glyphs.length === 0) {
        preview.textContent = 'Preview';
        return;
      }

      glyphs.forEach((glyph, i) => {
        const span = document.createElement('span');
        span.textContent = glyph;
        span.style.color = colorForIndex(i, glyphs.length);
        preview.appendChild(span);
      });
    }

    function updateModeUI() {
      const mode = modeInput.value;
      solidWrap.style.display = mode === 'solid' ? 'block' : 'none';
      gradientWrap.style.display = mode === 'gradient' ? 'grid' : 'none';
      letterWrap.style.display = mode === 'per_letter' ? 'block' : 'none';
      if (mode === 'per_letter') {
        renderLetterPickers();
      }
      renderPreview();
    }

    function setStatus(text, kind) {
      statusEl.textContent = text;
      statusEl.className = 'status ' + (kind || '');
    }

    async function sendForm(action) {
      const brightness = Math.max(0, Math.min(100, Number(brightnessInput.value || 0)));
      const params = new URLSearchParams();
      params.set('key', state.key);
      params.set('action', action);
      params.set('brightness', String(brightness));
      params.set('mode', modeInput.value);
      params.set('solid_color', solidColorInput.value.toUpperCase());
      params.set('gradient_start', gradientStartInput.value.toUpperCase());
      params.set('gradient_end', gradientEndInput.value.toUpperCase());
      if (action === 'custom') {
        params.set('msg', msgInput.value);
        ensureLetterColors();
        params.set('letter_colors', letterColors.join(','));
      }

      try {
        setStatus('Sending...', '');
        const res = await fetch('/set', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
          body: params.toString()
        });
        const text = await res.text();
        if (!res.ok) {
          throw new Error(text || ('Request failed: ' + res.status));
        }
        setStatus(text || 'Updated', 'ok');
      } catch (error) {
        setStatus(error.message || 'Failed to send request', 'err');
      }
    }

    msgInput.value = state.message;
    modeInput.value = state.mode;
    solidColorInput.value = clampColor(state.solid);
    gradientStartInput.value = clampColor(state.gradientStart);
    gradientEndInput.value = clampColor(state.gradientEnd);
    brightnessInput.value = String(state.brightness);

    msgInput.addEventListener('input', () => {
      if (modeInput.value === 'per_letter') {
        renderLetterPickers();
      }
      renderPreview();
    });
    modeInput.addEventListener('change', updateModeUI);
    solidColorInput.addEventListener('input', () => {
      if (modeInput.value === 'per_letter') {
        ensureLetterColors();
      }
      renderPreview();
    });
    gradientStartInput.addEventListener('input', renderPreview);
    gradientEndInput.addEventListener('input', renderPreview);

    document.getElementById('sendCustom').addEventListener('click', () => sendForm('custom'));
    document.getElementById('useDefault').addEventListener('click', () => sendForm('use_default'));

    updateModeUI();
  </script>
</body>
</html>
)HTML";

  html.replace("__API_KEY__", escapeForJs(String(API_KEY)));
  html.replace("__MESSAGE__", escapeForJs(msgValue));
  html.replace("__MODE__", colorModeToString(currentColorMode));
  html.replace("__SOLID__", colorToHex(currentColor));
  html.replace("__GRADIENT_START__", colorToHex(gradientStartColor));
  html.replace("__GRADIENT_END__", colorToHex(gradientEndColor));
  html.replace("__BRIGHTNESS__", String(currentBrightnessPct));
  html.replace("__LETTER_COLORS__", escapeForJs(serializePerLetterColors()));

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

  if (server.hasArg("mode")) {
    currentColorMode = parseColorMode(server.arg("mode"));
  }

  if (server.hasArg("solid_color")) {
    parseHexColor(server.arg("solid_color"), currentColor);
  }

  if (server.hasArg("gradient_start")) {
    parseHexColor(server.arg("gradient_start"), gradientStartColor);
  }

  if (server.hasArg("gradient_end")) {
    parseHexColor(server.arg("gradient_end"), gradientEndColor);
  }

  if (server.hasArg("letter_colors")) {
    parsePerLetterColors(server.arg("letter_colors"));
  }

  String action = server.hasArg("action") ? server.arg("action") : String("");

  // If user checked 'use_default' then switch to default cycling
  if (action == "use_default" || server.hasArg("use_default")) {
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

  // Legacy color field support for older clients.
  if (server.hasArg("color")) {
    parseHexColor(server.arg("color"), currentColor);
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
  size_t glyphIndex = 0;
  size_t glyphCount = countGlyphs(displayMessage);

  while (decodeUtf8(displayMessage, index, codepoint)) {
    if (codepoint == 0xFE0F) { // variation selector-16 (emoji style), skip it
      continue;
    }
    draw_glyph(x, 0, codepoint, colorForGlyph(glyphIndex, glyphCount));
    glyphIndex++;
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
