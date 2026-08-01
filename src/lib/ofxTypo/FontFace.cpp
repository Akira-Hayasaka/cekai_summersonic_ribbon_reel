#include "FontFace.h"

#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <harfbuzz/hb-ft.h>

namespace {

glm::vec2 ftToVec2(const FT_Vector *v) {
    return glm::vec2(static_cast<float>(v->x) / 64.0f, static_cast<float>(v->y) / 64.0f);
}

struct DecomposeContext {
    GlyphOutline *outline = nullptr;
    GlyphContour *contour = nullptr;
};

int moveToCallback(const FT_Vector *to, void *user) {
    auto *ctx = static_cast<DecomposeContext *>(user);
    ctx->outline->contours.emplace_back();
    ctx->contour = &ctx->outline->contours.back();

    PathCommand cmd;
    cmd.type = PathCommandType::MoveTo;
    cmd.p0 = ftToVec2(to);
    ctx->contour->commands.push_back(cmd);
    return 0;
}

int lineToCallback(const FT_Vector *to, void *user) {
    auto *ctx = static_cast<DecomposeContext *>(user);
    if (!ctx->contour) {
        return 0;
    }

    PathCommand cmd;
    cmd.type = PathCommandType::LineTo;
    cmd.p0 = ftToVec2(to);
    ctx->contour->commands.push_back(cmd);
    return 0;
}

int conicToCallback(const FT_Vector *control, const FT_Vector *to, void *user) {
    auto *ctx = static_cast<DecomposeContext *>(user);
    if (!ctx->contour) {
        return 0;
    }

    PathCommand cmd;
    cmd.type = PathCommandType::QuadTo;
    cmd.p0 = ftToVec2(control);
    cmd.p1 = ftToVec2(to);
    ctx->contour->commands.push_back(cmd);
    return 0;
}

int cubicToCallback(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user) {
    auto *ctx = static_cast<DecomposeContext *>(user);
    if (!ctx->contour) {
        return 0;
    }

    PathCommand cmd;
    cmd.type = PathCommandType::CubicTo;
    cmd.p0 = ftToVec2(control1);
    cmd.p1 = ftToVec2(control2);
    cmd.p2 = ftToVec2(to);
    ctx->contour->commands.push_back(cmd);
    return 0;
}

} // namespace

FontFace::~FontFace() {
    clear();
}

bool FontFace::load(FT_Library library, const std::filesystem::path &path, int faceIndex) {
    clear();

    const std::string utf8Path = path.string();
    const FT_Error err = FT_New_Face(library, utf8Path.c_str(), faceIndex, &ftFace);
    if (err != 0 || !ftFace) {
        ofLogError("ofxTypo") << "FT_New_Face failed: " << utf8Path << " err=" << err;
        ftFace = nullptr;
        return false;
    }

    setPixelSize(pixelSize);
    rebuildHbFont();
    return hbFont != nullptr;
}

void FontFace::clear() {
    if (hbFont) {
        hb_font_destroy(hbFont);
        hbFont = nullptr;
    }

    if (ftFace) {
        FT_Done_Face(ftFace);
        ftFace = nullptr;
    }

    currentVariations.clear();
}

void FontFace::setId(const std::string &value) {
    id = value;
}

const std::string &FontFace::getId() const {
    return id;
}

void FontFace::setPixelSize(float px) {
    pixelSize = std::max(1.0f, px);
    if (!ftFace) {
        return;
    }

    FT_Set_Pixel_Sizes(ftFace, 0, static_cast<FT_UInt>(pixelSize));
    if (hbFont) {
        hb_ft_font_changed(hbFont);
    }
}

float FontFace::getPixelSize() const {
    return pixelSize;
}

void FontFace::setVariations(const std::vector<FontVariation> &variations) {
    currentVariations = variations;
}

FT_Face FontFace::getFTFace() const {
    return ftFace;
}

hb_font_t *FontFace::getHBFont() const {
    return hbFont;
}

std::vector<FontAxisInfo> FontFace::getAxes() const {
    return {};
}

bool FontFace::loadGlyphOutline(uint32_t glyphId, GlyphOutline &outOutline) const {
    if (!ftFace) {
        return false;
    }

    const FT_Error loadErr = FT_Load_Glyph(ftFace, glyphId, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
    if (loadErr != 0) {
        return false;
    }

    const FT_GlyphSlot slot = ftFace->glyph;
    if (slot->format != FT_GLYPH_FORMAT_OUTLINE) {
        return false;
    }

    outOutline = {};
    outOutline.glyphId = glyphId;
    outOutline.advanceX = static_cast<float>(slot->advance.x) / 64.0f;
    outOutline.advanceY = static_cast<float>(slot->advance.y) / 64.0f;

    FT_BBox box;
    FT_Outline_Get_CBox(&slot->outline, &box);
    const float left = static_cast<float>(box.xMin) / 64.0f;
    const float top = static_cast<float>(box.yMax) / 64.0f;
    const float width = static_cast<float>(box.xMax - box.xMin) / 64.0f;
    const float height = static_cast<float>(box.yMax - box.yMin) / 64.0f;
    outOutline.bounds = ofRectangle(left, top, width, height);

    FT_Outline_Funcs funcs{};
    funcs.move_to = moveToCallback;
    funcs.line_to = lineToCallback;
    funcs.conic_to = conicToCallback;
    funcs.cubic_to = cubicToCallback;
    funcs.shift = 0;
    funcs.delta = 0;

    DecomposeContext ctx;
    ctx.outline = &outOutline;

    const FT_Error decomposeErr = FT_Outline_Decompose(&slot->outline, &funcs, &ctx);
    if (decomposeErr != 0) {
        return false;
    }

    for (auto &contour : outOutline.contours) {
        if (contour.commands.empty()) {
            continue;
        }
        PathCommand closeCmd;
        closeCmd.type = PathCommandType::Close;
        contour.commands.push_back(closeCmd);
    }

    return true;
}

GlyphMetrics FontFace::getGlyphMetrics(uint32_t glyphId) const {
    GlyphMetrics metrics;
    if (!ftFace) {
        return metrics;
    }

    const FT_Error loadErr = FT_Load_Glyph(ftFace, glyphId, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
    if (loadErr != 0) {
        return metrics;
    }

    const FT_GlyphSlot slot = ftFace->glyph;
    metrics.advanceX = static_cast<float>(slot->advance.x) / 64.0f;
    metrics.advanceY = static_cast<float>(slot->advance.y) / 64.0f;
    metrics.bearingX = static_cast<float>(slot->metrics.horiBearingX) / 64.0f;
    metrics.bearingY = static_cast<float>(slot->metrics.horiBearingY) / 64.0f;
    metrics.width = static_cast<float>(slot->metrics.width) / 64.0f;
    metrics.height = static_cast<float>(slot->metrics.height) / 64.0f;
    return metrics;
}

bool FontFace::hasGlyph(uint32_t codepoint) const {
    if (!ftFace) {
        return false;
    }
    return FT_Get_Char_Index(ftFace, codepoint) != 0;
}

void FontFace::rebuildHbFont() {
    if (hbFont) {
        hb_font_destroy(hbFont);
        hbFont = nullptr;
    }

    if (!ftFace) {
        return;
    }

    hbFont = hb_ft_font_create_referenced(ftFace);
    if (!hbFont) {
        ofLogError("ofxTypo") << "hb_ft_font_create_referenced failed";
        return;
    }

    hb_ft_font_set_load_flags(hbFont, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
}
