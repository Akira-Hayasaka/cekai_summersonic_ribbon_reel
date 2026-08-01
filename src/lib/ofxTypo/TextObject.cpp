#include "TextObject.h"

#include <glm/gtc/matrix_transform.hpp>

void TextObject::setFont(const std::shared_ptr<FontFace> &value) {
    if (font == value) {
        return;
    }

    font = value;
    dirtyShape = true;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setFontManager(FontManager *fm) {
    fontManager = fm;
    dirtyShape  = true;
    dirtyLayout = true;
    dirtyMesh   = true;
}

void TextObject::setText(const std::string &utf8) {
    if (text == utf8) {
        return;
    }

    text = utf8;
    dirtyShape = true;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setFontSize(float px) {
    if (layoutOptions.fontSize == px) {
        return;
    }

    layoutOptions.fontSize = px;
    if (font) {
        font->setPixelSize(px);
    }

    dirtyShape = true;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setPosition(const glm::vec3 &pos) {
    position = pos;
}

void TextObject::setFeature(const std::string &tag, uint32_t value) {
    for (auto &feature : shapeOptions.features) {
        if (feature.tag == tag) {
            if (feature.value != value) {
                feature.value = value;
                dirtyShape = true;
                dirtyLayout = true;
                dirtyMesh = true;
            }
            return;
        }
    }

    OpenTypeFeature feature;
    feature.tag = tag;
    feature.value = value;
    shapeOptions.features.push_back(feature);
    dirtyShape = true;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setVariation(const std::string &tag, float value) {
    for (auto &variation : shapeOptions.variations) {
        if (variation.tag == tag) {
            if (variation.value != value) {
                variation.value = value;
                dirtyShape = true;
                dirtyLayout = true;
                dirtyMesh = true;
            }
            return;
        }
    }

    FontVariation variation;
    variation.tag = tag;
    variation.value = value;
    shapeOptions.variations.push_back(variation);
    dirtyShape = true;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setAlign(TextAlign align) {
    if (layoutOptions.align == align) {
        return;
    }

    layoutOptions.align = align;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setTracking(float tracking) {
    if (layoutOptions.tracking == tracking) {
        return;
    }

    layoutOptions.tracking = tracking;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setLetterSpacing(float pct) {
    if (layoutOptions.letterSpacing == pct) {
        return;
    }

    layoutOptions.letterSpacing = pct;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setLineHeight(float lineHeight) {
    if (layoutOptions.lineHeight == lineHeight) {
        return;
    }

    layoutOptions.lineHeight = lineHeight;
    dirtyLayout = true;
    dirtyMesh = true;
}

void TextObject::setShader(ofShader *shader) {
    renderer.setShader(shader);
}

void TextObject::setColor(const ofFloatColor &color) {
    renderer.setColor(color);
}

void TextObject::rebuild() {
    if (!font) {
        mesh.clear();
        return;
    }

    font->setPixelSize(layoutOptions.fontSize);

    if (dirtyShape) {
        // -- Collect fallback fonts from FontManager -------------------------
        std::vector<std::shared_ptr<FontFace>> fallbacks;
        if (fontManager) {
            const std::string &primaryId = font->getId();
            for (const auto &fbId : fontManager->getFallbacks(primaryId)) {
                auto fb = fontManager->getFont(fbId);
                if (fb) {
                    fb->setPixelSize(layoutOptions.fontSize);
                    fallbacks.push_back(fb);
                }
            }
        }

        // -- Segment text into lines of runs ---------------------------------
        const auto segmented = segmenter.segment(text, font, fallbacks, shapeOptions);

        // -- Shape each run and accumulate into shapedLines ------------------
        shapedLines.clear();
        shapedLines.reserve(segmented.size());

        for (const auto &lineRuns : segmented) {
            std::vector<ShapedGlyph> lineGlyphs;

            for (const auto &run : lineRuns) {
                if (!run.font || run.utf8.empty()) {
                    continue;
                }
                run.font->setPixelSize(layoutOptions.fontSize);
                auto shaped = shaper.shape(run.utf8, run.font, run.shapeOptions);
                for (auto &g : shaped) {
                    g.charStart += run.byteStart;
                    g.charEnd   += run.byteStart;
                }
                lineGlyphs.insert(lineGlyphs.end(), shaped.begin(), shaped.end());
            }

            shapedLines.push_back(std::move(lineGlyphs));
        }

        dirtyShape  = false;
        dirtyLayout = true;
        dirtyMesh   = true;
    }

    if (dirtyLayout) {
        positionedGlyphs = layoutEngine.layout(shapedLines, layoutOptions);
        dirtyLayout = false;
        dirtyMesh   = true;
    }

    if (!dirtyMesh) {
        return;
    }

    mesh.clear();
    mesh.glyphCount = static_cast<int>(positionedGlyphs.size());
    mesh.lineCount  = static_cast<int>(shapedLines.size());

    for (size_t i = 0; i < positionedGlyphs.size(); ++i) {
        const auto &pg = positionedGlyphs[i];
        if (!pg.shaped.font) {
            continue;
        }

        const GlyphCacheKey key = makeCacheKey(pg.shaped.glyphId, pg.shaped.font);
        auto glyphMesh = cache.findFillMesh(key);

        if (!glyphMesh) {
            GlyphOutline outline;
            if (!pg.shaped.font->loadGlyphOutline(pg.shaped.glyphId, outline)) {
                continue;
            }

            auto generated = std::make_shared<ofMesh>();
            if (!meshBuilder.buildFillMesh(outline, *generated)) {
                continue;
            }

            glyphMesh = generated;
            cache.storeFillMesh(key, glyphMesh);
        }

        const std::size_t baseIndex = mesh.vertices.size();
        for (const auto &v : glyphMesh->getVertices()) {
            TypoVertex tv;
            tv.position      = glm::vec3(v.x + pg.position.x, v.y + pg.position.y, 0.0f);
            tv.glyphLocalPos = glm::vec2(v.x, v.y);
            tv.glyphIndex    = static_cast<float>(i);
            tv.charIndex     = static_cast<float>(pg.shaped.cluster);
            tv.lineIndex     = static_cast<float>(pg.lineIndex);
            tv.random        = ofRandomuf();
            mesh.vertices.push_back(tv);
        }

        for (auto idx : glyphMesh->getIndices()) {
            mesh.indices.push_back(static_cast<uint32_t>(baseIndex + idx));
        }
    }

    mesh.upload();
    dirtyMesh = false;
}

void TextObject::update(float dt) {
    (void)dt;
}

void TextObject::draw() {
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
    renderer.draw(mesh, transform);
}

TextMesh &TextObject::getMesh() {
    return mesh;
}

const TextMesh &TextObject::getMesh() const {
    return mesh;
}

ofRectangle TextObject::getBBox() const {
	return mesh.bounds;
}

int TextObject::getGlyphCount() const {
    return static_cast<int>(positionedGlyphs.size());
}

uint64_t TextObject::getCacheHitCount() const {
    return cache.getHitCount();
}

uint64_t TextObject::getCacheMissCount() const {
    return cache.getMissCount();
}

GlyphCacheKey TextObject::makeCacheKey(uint32_t glyphId, const std::shared_ptr<FontFace> &f) const {
    GlyphCacheKey key;
    key.glyphId    = glyphId;
    key.pixelSize  = layoutOptions.fontSize;
    key.variations = shapeOptions.variations;
    key.fontId     = f ? f->getId() : std::string();
    return key;
}

void TextObject::drawDebug(bool showBBox, bool showWireframe, bool showGlyphBBoxes) const {
    if (!mesh.uploaded) {
        return;
    }

    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
    ofPushMatrix();
    ofMultMatrix(transform);

    // ---- Per-glyph ink bounding boxes (cyan) -------------------------------
    if (showGlyphBBoxes && font) {
        ofNoFill();
        ofSetLineWidth(1.0f);
        ofSetColor(0, 220, 255, 180);

        for (const auto &pg : positionedGlyphs) {
            if (!pg.shaped.font) {
                continue;
            }
            const GlyphMetrics gm = pg.shaped.font->getGlyphMetrics(pg.shaped.glyphId);
            // FreeType returns Y-up metrics; GlyphMeshBuilder flips Y
            const float x = pg.position.x + gm.bearingX;
            const float y = pg.position.y - gm.bearingY;   // top-left in screen space
            ofDrawRectangle(x, y, gm.width, gm.height);
        }
    }

    // ---- Wireframe of the fill mesh (yellow) --------------------------------
    if (showWireframe && mesh.uploaded) {
        ofSetColor(255, 220, 0, 160);
        ofSetLineWidth(1.0f);
        mesh.vboMesh.drawWireframe();
    }

    // ---- Overall text bounds (magenta) -------------------------------------
    if (showBBox && !mesh.bounds.isEmpty()) {
        ofNoFill();
        ofSetLineWidth(2.0f);
        ofSetColor(255, 0, 200, 220);
        ofDrawRectangle(mesh.bounds);
    }

    ofSetLineWidth(1.0f);
    ofFill();
    ofSetColor(255);
    ofPopMatrix();
}
