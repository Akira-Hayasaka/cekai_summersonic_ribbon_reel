#pragma once

#include "FontFace.h"
#include "TypoTypes.h"

#include <memory>
#include <string>
#include <vector>

class TextRunSegmenter {
public:
	// Split utf8 text by '\n' into lines, then split each line into TextRun
	// segments by font coverage. Returns lines[lineIdx][runIdx].
	std::vector<std::vector<TextRun>> segment(
		const std::string &utf8,
		const std::shared_ptr<FontFace> &primaryFont,
		const std::vector<std::shared_ptr<FontFace>> &fallbacks,
		const ShapeOptions &baseOptions) const;

private:
	// Decode the next UTF-8 codepoint starting at bytePos; advances bytePos.
	// Returns the Unicode codepoint, or 0xFFFD on invalid sequence.
	static uint32_t decodeUtf8Codepoint(const std::string &str, size_t &bytePos);

	// Return the first font in [primary, fallbacks...] that has a glyph for cp.
	// Falls back to primary if none found.
	static std::shared_ptr<FontFace> findFont(
		uint32_t cp,
		const std::shared_ptr<FontFace> &primary,
		const std::vector<std::shared_ptr<FontFace>> &fallbacks);
};
