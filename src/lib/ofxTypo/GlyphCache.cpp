#include "GlyphCache.h"

#include <functional>

namespace {

std::size_t hashCombine(std::size_t seed, std::size_t value) {
    return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

} // namespace

bool GlyphCacheKey::operator==(const GlyphCacheKey &other) const {
    if (fontId != other.fontId || glyphId != other.glyphId || pixelSize != other.pixelSize) {
        return false;
    }

    if (variations.size() != other.variations.size()) {
        return false;
    }

    for (size_t i = 0; i < variations.size(); ++i) {
        if (variations[i].tag != other.variations[i].tag || variations[i].value != other.variations[i].value) {
            return false;
        }
    }

    return true;
}

std::size_t GlyphCacheKeyHash::operator()(const GlyphCacheKey &k) const {
    std::size_t h = std::hash<std::string>{}(k.fontId);
    h = hashCombine(h, std::hash<uint32_t>{}(k.glyphId));
    h = hashCombine(h, std::hash<float>{}(k.pixelSize));
    for (const auto &v : k.variations) {
        h = hashCombine(h, std::hash<std::string>{}(v.tag));
        h = hashCombine(h, std::hash<float>{}(v.value));
    }
    return h;
}

void GlyphCache::clear() {
    fillMeshCache.clear();
    hitCount = 0;
    missCount = 0;
}

std::shared_ptr<ofMesh> GlyphCache::findFillMesh(const GlyphCacheKey &key) {
    const auto it = fillMeshCache.find(key);
    if (it == fillMeshCache.end()) {
        ++missCount;
        return nullptr;
    }

    ++hitCount;
    return it->second;
}

void GlyphCache::storeFillMesh(const GlyphCacheKey &key, const std::shared_ptr<ofMesh> &mesh) {
    fillMeshCache[key] = mesh;
}

uint64_t GlyphCache::getHitCount() const {
    return hitCount;
}

uint64_t GlyphCache::getMissCount() const {
    return missCount;
}
