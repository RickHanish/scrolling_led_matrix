#include "text/utf8.h"

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
