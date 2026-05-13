#include "text/text_utils.h"

#include "text/utf8.h"

String preprocessMessage(String s) {
	// Order matters: replace longer sequences first.
	s.replace("<3", String("\u2764"));
	s.replace(":)", String("\U0001F642"));
	s.replace(":(", String("\U0001F641"));

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
