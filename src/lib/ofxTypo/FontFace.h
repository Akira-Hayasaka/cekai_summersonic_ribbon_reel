#pragma once

#include "GlyphOutline.h"
#include "TypoTypes.h"

#include <filesystem>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <harfbuzz/hb.h>

class FontFace {
public:
    FontFace() = default;
    ~FontFace();

    bool load(FT_Library library, const std::filesystem::path &path, int faceIndex = 0);
    void clear();

    void setId(const std::string &value);
    const std::string &getId() const;

    void setPixelSize(float px);
    float getPixelSize() const;

    void setVariations(const std::vector<FontVariation> &variations);

    FT_Face getFTFace() const;
    hb_font_t *getHBFont() const;

    std::vector<FontAxisInfo> getAxes() const;

    bool loadGlyphOutline(uint32_t glyphId, GlyphOutline &outOutline) const;
    GlyphMetrics getGlyphMetrics(uint32_t glyphId) const;
    bool hasGlyph(uint32_t codepoint) const;

private:
    void rebuildHbFont();

    std::string id;
    FT_Face ftFace = nullptr;
    hb_font_t *hbFont = nullptr;
    float pixelSize = 64.0f;
    std::vector<FontVariation> currentVariations;
};
