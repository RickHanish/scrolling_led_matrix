#pragma once

#include <Arduino.h>

bool decodeUtf8(const String& text, size_t& index, uint32_t& codepoint);
