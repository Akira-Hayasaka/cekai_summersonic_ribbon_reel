#pragma once

#include "FontFace.h"
#include "FontManager.h"
#include "GlyphCache.h"
#include "GlyphMeshBuilder.h"
#include "TextLayout.h"
#include "TextMesh.h"
#include "TextRenderer.h"
#include "TextRunSegmenter.h"
#include "TextShaper.h"
#include "TypoTypes.h"

#include <memory>
#include <string>
#include <vector>

class TextObject {
public:
    void setFont(const std::shared_ptr<FontFace> &value);
    void setFontManager(FontManager *fm);
    void setText(const std::string &utf8);
    void setFontSize(float px);
    void setPosition(const glm::vec3 &pos);

    void setFeature(const std::string &tag, uint32_t value);
    void setVariation(const std::string &tag, float value);

    void setAlign(TextAlign align);
    void setTracking(float tracking);
    void setLetterSpacing(float pct);
    void setLineHeight(float lineHeight);

    void setShader(ofShader *shader);
    void setColor(const ofFloatColor &color);

    void rebuild();
    void update(float dt);
    void draw();
    // showBBox: overall text bounds, showWireframe: triangle mesh, showGlyphBBoxes: per-glyph ink rect
    void drawDebug(bool showBBox, bool showWireframe, bool showGlyphBBoxes) const;

    TextMesh &getMesh();
    const TextMesh &getMesh() const;

	ofRectangle getBBox() const;

    int getGlyphCount() const;
    uint64_t getCacheHitCount() const;
    uint64_t getCacheMissCount() const;

private:
    GlyphCacheKey makeCacheKey(uint32_t glyphId, const std::shared_ptr<FontFace> &f) const;

    std::string text;
    std::shared_ptr<FontFace> font;

    // Optional FontManager for fallback-font resolution
    FontManager *fontManager = nullptr;

    ShapeOptions shapeOptions;
    LayoutOptions layoutOptions;

    // Shaped glyphs per line (after segmentation + shaping)
    std::vector<std::vector<ShapedGlyph>> shapedLines;
    std::vector<PositionedGlyph> positionedGlyphs;

    TextMesh mesh;
    GlyphCache cache;

    TextShaper shaper;
    TextLayout layoutEngine;
    TextRunSegmenter segmenter;
    GlyphMeshBuilder meshBuilder;
    TextRenderer renderer;

    glm::vec3 position = {0.0f, 0.0f, 0.0f};

    bool dirtyShape  = true;
    bool dirtyLayout = true;
    bool dirtyMesh   = true;
};
