#pragma once

#include "FontFace.h"
#include "TypoTypes.h"

#include <string>
#include <vector>

class TextShaper {
public:
    std::vector<ShapedGlyph> shape(
        const std::string &utf8,
        const std::shared_ptr<FontFace> &font,
        const ShapeOptions &options) const;
};
