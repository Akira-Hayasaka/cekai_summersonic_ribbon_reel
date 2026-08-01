#include "TextRunSegmenter.h"

// ---------------------------------------------------------------------------
// UTF-8 decoder
// ---------------------------------------------------------------------------
uint32_t TextRunSegmenter::decodeUtf8Codepoint(const std::string &str, size_t &pos) {
	if (pos >= str.size()) {
		return 0;
	}

	const unsigned char c0 = static_cast<unsigned char>(str[pos]);

	// 1-byte sequence (ASCII)
	if (c0 < 0x80) {
		++pos;
		return c0;
	}

	// Continuation byte where a leading byte is expected — skip it
	if (c0 < 0xC0) {
		++pos;
		return 0xFFFD;
	}

	int extraBytes = 0;
	uint32_t codepoint = 0;

	if (c0 < 0xE0) {
		extraBytes = 1;
		codepoint  = c0 & 0x1F;
	} else if (c0 < 0xF0) {
		extraBytes = 2;
		codepoint  = c0 & 0x0F;
	} else if (c0 < 0xF8) {
		extraBytes = 3;
		codepoint  = c0 & 0x07;
	} else {
		++pos;
		return 0xFFFD;
	}

	++pos;

	for (int i = 0; i < extraBytes; ++i) {
		if (pos >= str.size()) {
			return 0xFFFD;
		}
		const unsigned char cb = static_cast<unsigned char>(str[pos]);
		if ((cb & 0xC0) != 0x80) {
			return 0xFFFD;
		}
		codepoint = (codepoint << 6) | (cb & 0x3F);
		++pos;
	}

	return codepoint;
}

// ---------------------------------------------------------------------------
// Font finder — returns the first font that covers the codepoint
// ---------------------------------------------------------------------------
std::shared_ptr<FontFace> TextRunSegmenter::findFont(
	uint32_t cp,
	const std::shared_ptr<FontFace> &primary,
	const std::vector<std::shared_ptr<FontFace>> &fallbacks) {

	if (primary && primary->hasGlyph(cp)) {
		return primary;
	}

	for (const auto &fb : fallbacks) {
		if (fb && fb->hasGlyph(cp)) {
			return fb;
		}
	}

	// No font covers this codepoint — use primary as fallback
	return primary;
}

// ---------------------------------------------------------------------------
// segment()
// ---------------------------------------------------------------------------
std::vector<std::vector<TextRun>> TextRunSegmenter::segment(
	const std::string &utf8,
	const std::shared_ptr<FontFace> &primaryFont,
	const std::vector<std::shared_ptr<FontFace>> &fallbacks,
	const ShapeOptions &baseOptions) const {

	std::vector<std::vector<TextRun>> result;
	result.emplace_back(); // start first line

	if (utf8.empty() || !primaryFont) {
		return result;
	}

	// State for the current run being accumulated
	std::shared_ptr<FontFace> currentFont;
	int runByteStart = 0;
	std::string runUtf8;

	auto flushRun = [&]() {
		if (runUtf8.empty()) {
			return;
		}
		TextRun run;
		run.utf8       = std::move(runUtf8);
		run.byteStart  = runByteStart;
		run.byteEnd    = runByteStart + static_cast<int>(run.utf8.size());
		run.font       = currentFont;
		run.shapeOptions = baseOptions;
		result.back().push_back(std::move(run));
		runUtf8.clear();
	};

	size_t bytePos = 0;

	while (bytePos < utf8.size()) {
		const size_t cpByteStart = bytePos;
		const uint32_t cp = decodeUtf8Codepoint(utf8, bytePos);
		const size_t cpByteLen = bytePos - cpByteStart;

		// '\n' — flush current run, start a new line
		if (cp == 0x0A) {
			flushRun();
			currentFont = nullptr;
			runByteStart = static_cast<int>(bytePos);
			result.emplace_back();
			continue;
		}

		const auto font = findFont(cp, primaryFont, fallbacks);

		// Font changed — flush current run and start a new one
		if (font != currentFont) {
			flushRun();
			currentFont  = font;
			runByteStart = static_cast<int>(cpByteStart);
		}

		// Append the raw UTF-8 bytes of this codepoint to the current run
		runUtf8.append(utf8, cpByteStart, cpByteLen);
	}

	flushRun();

	return result;
}
