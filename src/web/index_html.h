#pragma once

#include <Arduino.h>

struct AppState;

String buildIndexHtml(const AppState& state, const char* apiKey);
