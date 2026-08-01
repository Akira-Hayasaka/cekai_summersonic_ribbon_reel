#include "TextLayout.h"

// ---------------------------------------------------------------------------
// Primary multiline implementation
// ---------------------------------------------------------------------------
std::vector<PositionedGlyph> TextLayout::layout(
    const std::vector<std::vector<ShapedGlyph>> &lines,
    const LayoutOptions &options) const {

    std::vector<PositionedGlyph> positioned;

    const float lineHeightPx = options.fontSize * options.lineHeight;
    int globalGlyphIndex = 0;

    for (int lineIdx = 0; lineIdx < static_cast<int>(lines.size()); ++lineIdx) {
        const auto &line = lines[lineIdx];

        // -- accumulate raw x positions for this line -----------------------
        float cursorX  = 0.0f;
        float lineWidth = 0.0f;

        const float letterSpacingPx = (options.letterSpacing / 100.0f) * options.fontSize;

        std::vector<PositionedGlyph> lineGlyphs;
        lineGlyphs.reserve(line.size());

        for (size_t i = 0; i < line.size(); ++i) {
            const auto &g = line[i];

            PositionedGlyph pg;
            pg.shaped     = g;
            pg.glyphIndex = globalGlyphIndex++;
            pg.lineIndex  = lineIdx;
            pg.position   = glm::vec2(cursorX + g.xOffset, g.yOffset);
            lineGlyphs.push_back(pg);

            cursorX += g.xAdvance + options.tracking + letterSpacingPx;
        }
        lineWidth = cursorX;

        // -- per-line horizontal alignment -----------------------------------
        float alignOffsetX = 0.0f;
        if (options.align == TextAlign::Center) {
            alignOffsetX = -lineWidth * 0.5f;
        } else if (options.align == TextAlign::Right) {
            alignOffsetX = -lineWidth;
        }

        // -- vertical offset for this line -----------------------------------
        const float lineBaselineY = static_cast<float>(lineIdx) * lineHeightPx;

        // -- apply offsets and push to output --------------------------------
        for (auto &pg : lineGlyphs) {
            pg.position.x += alignOffsetX - options.anchor.x;
            pg.position.y += lineBaselineY - options.anchor.y;
            positioned.push_back(std::move(pg));
        }
    }

    return positioned;
}

// ---------------------------------------------------------------------------
// Backward-compat overload: wrap a flat glyph list as a single line
// ---------------------------------------------------------------------------
std::vector<PositionedGlyph> TextLayout::layout(
    const std::vector<ShapedGlyph> &glyphs,
    const LayoutOptions &options) const {

    return layout(std::vector<std::vector<ShapedGlyph>>{glyphs}, options);
}

