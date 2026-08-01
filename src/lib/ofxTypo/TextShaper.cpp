#include "TextShaper.h"

#include <harfbuzz/hb-ft.h>

namespace {

hb_direction_t parseDirection(const std::string &v) {
    if (v == "rtl") {
        return HB_DIRECTION_RTL;
    }
    if (v == "ttb") {
        return HB_DIRECTION_TTB;
    }
    if (v == "btt") {
        return HB_DIRECTION_BTT;
    }
    return HB_DIRECTION_LTR;
}

} // namespace

std::vector<ShapedGlyph> TextShaper::shape(
    const std::string &utf8,
    const std::shared_ptr<FontFace> &font,
    const ShapeOptions &options) const {
    std::vector<ShapedGlyph> out;
    if (utf8.empty() || !font || !font->getHBFont()) {
        return out;
    }

    font->setVariations(options.variations);

    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, utf8.c_str(), static_cast<int>(utf8.size()), 0, static_cast<int>(utf8.size()));
    hb_buffer_set_direction(buffer, parseDirection(options.direction));
    hb_buffer_set_script(buffer, hb_script_from_string(options.script.c_str(), static_cast<int>(options.script.size())));
    hb_buffer_set_language(buffer, hb_language_from_string(options.language.c_str(), static_cast<int>(options.language.size())));

    std::vector<hb_feature_t> features;
    features.reserve(options.features.size() + 2);

    if (options.enableKerning) {
        hb_feature_t feature;
        hb_feature_from_string("kern=1", -1, &feature);
        features.push_back(feature);
    }

    if (options.enableLigatures) {
        hb_feature_t feature;
        hb_feature_from_string("liga=1", -1, &feature);
        features.push_back(feature);
    }

    for (const auto &f : options.features) {
        hb_feature_t feature{};
        const std::string featureString = f.tag + "=" + std::to_string(f.value);
        if (hb_feature_from_string(featureString.c_str(), static_cast<int>(featureString.size()), &feature)) {
            feature.start = f.start;
            feature.end = f.end;
            features.push_back(feature);
        }
    }

    hb_shape(font->getHBFont(), buffer, features.empty() ? nullptr : features.data(), static_cast<unsigned int>(features.size()));

    unsigned int count = 0;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &count);
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &count);
    out.reserve(count);

    for (unsigned int i = 0; i < count; ++i) {
        ShapedGlyph g;
        g.glyphId = infos[i].codepoint;
        g.cluster = infos[i].cluster;
        g.xAdvance = static_cast<float>(positions[i].x_advance) / 64.0f;
        g.yAdvance = static_cast<float>(positions[i].y_advance) / 64.0f;
        g.xOffset = static_cast<float>(positions[i].x_offset) / 64.0f;
        g.yOffset = static_cast<float>(positions[i].y_offset) / 64.0f;
        g.charStart = static_cast<int>(infos[i].cluster);
        g.charEnd = (i + 1 < count) ? static_cast<int>(infos[i + 1].cluster) : static_cast<int>(utf8.size());
        g.font = font;
        out.push_back(g);
    }

    hb_buffer_destroy(buffer);
    return out;
}
