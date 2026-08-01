#pragma once

#include "ofMain.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class FontFace;

struct FontVariation {
    std::string tag;
    float value = 0.0f;
};

struct FontAxisInfo {
    std::string tag;
    std::string name;
    float minValue = 0.0f;
    float defaultValue = 0.0f;
    float maxValue = 0.0f;
};

struct GlyphMetrics {
    float advanceX = 0.0f;
    float advanceY = 0.0f;
    float bearingX = 0.0f;
    float bearingY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct OpenTypeFeature {
    std::string tag;
    uint32_t value = 1;
    uint32_t start = 0;
    uint32_t end = UINT32_MAX;
};

struct ShapeOptions {
    std::string language = "ja";
    std::string script = "Jpan";
    std::string direction = "ltr";

    std::vector<OpenTypeFeature> features;
    std::vector<FontVariation> variations;

    bool enableKerning = true;
    bool enableLigatures = true;
};

struct ShapedGlyph {
    uint32_t glyphId = 0;
    uint32_t cluster = 0;

    float xAdvance = 0.0f;
    float yAdvance = 0.0f;
    float xOffset = 0.0f;
    float yOffset = 0.0f;

    int charStart = 0;
    int charEnd = 0;

    std::shared_ptr<FontFace> font;
};

enum class TextAlign {
    Left,
    Center,
    Right
};

struct LayoutOptions {
    float fontSize = 128.0f;
    float lineHeight = 1.2f;
    float tracking = 0.0f;
    float letterSpacing = 0.0f;
    float maxWidth = 0.0f;
    TextAlign align = TextAlign::Left;
    glm::vec2 anchor = {0.0f, 0.0f};
};

struct PositionedGlyph {
    ShapedGlyph shaped;
    glm::vec2 position = {0.0f, 0.0f};
    int lineIndex = 0;
    int glyphIndex = 0;
};

enum class DirtyFlag : uint32_t {
    None = 0,
    Shape = 1 << 0,
    Layout = 1 << 1,
    Outline = 1 << 2,
    Mesh = 1 << 3,
    GpuUpload = 1 << 4,
};

inline DirtyFlag operator|(DirtyFlag a, DirtyFlag b) {
    return static_cast<DirtyFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline DirtyFlag operator&(DirtyFlag a, DirtyFlag b) {
    return static_cast<DirtyFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline DirtyFlag &operator|=(DirtyFlag &a, DirtyFlag b) {
    a = a | b;
    return a;
}

struct TextRun {
    std::string utf8;
    int byteStart = 0;
    int byteEnd   = 0;
    std::shared_ptr<FontFace> font;
    ShapeOptions shapeOptions;
};
