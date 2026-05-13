#pragma once

#include <Arduino.h>

String preprocessMessage(String s);
size_t countGlyphs(const String& text);
int renderedWidth(const String& text);
