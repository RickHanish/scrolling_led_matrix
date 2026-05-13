#pragma once

struct AppState;

void setupWebServer(AppState& state, const char* apiKey);
void pollWebServer(AppState& state);
