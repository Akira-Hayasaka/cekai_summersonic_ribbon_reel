#pragma once

#include "TypoTypes.h"

#include <vector>

class TextLayout {
public:
    // layout a pre-segmented 2-D glyph table (lines x glyphs-per-line)
    // and return a flat list of PositionedGlyph with correct line indices.
    std::vector<PositionedGlyph> layout(
        const std::vector<std::vector<ShapedGlyph>> &lines,
        const LayoutOptions &options) const;

    // Convenience overload: treat a flat list as a single line (backward compat)
    std::vector<PositionedGlyph> layout(
        const std::vector<ShapedGlyph> &glyphs,
        const LayoutOptions &options) const;
};
