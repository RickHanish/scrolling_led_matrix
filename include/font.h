#pragma once
#include <Arduino.h>
#include <array>
#include <cstdint>
#include <unordered_map>

extern std::unordered_map<uint32_t, std::array<uint8_t, 5>> font_map;

const uint8_t* get_glyph(uint32_t codepoint);
