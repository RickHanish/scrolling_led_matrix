#pragma once

#include <Arduino.h>

struct AppState;

String getTimeString();
void updateMessageTimeout(AppState& state);
String getDisplayMessage(AppState& state);
void advanceMessageScrollState(AppState& state, const String& displayMessage);
