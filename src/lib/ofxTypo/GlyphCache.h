#pragma once

#include "TextMesh.h"
#include "TypoTypes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct GlyphCacheKey {
    std::string fontId;
    uint32_t glyphId = 0;
    float pixelSize = 0.0f;
    std::vector<FontVariation> variations;

    bool operator==(const GlyphCacheKey &other) const;
};

struct GlyphCacheKeyHash {
    std::size_t operator()(const GlyphCacheKey &k) const;
};

class GlyphCache {
public:
    void clear();

    std::shared_ptr<ofMesh> findFillMesh(const GlyphCacheKey &key);
    void storeFillMesh(const GlyphCacheKey &key, const std::shared_ptr<ofMesh> &mesh);

    uint64_t getHitCount() const;
    uint64_t getMissCount() const;

private:
    std::unordered_map<GlyphCacheKey, std::shared_ptr<ofMesh>, GlyphCacheKeyHash> fillMeshCache;
    uint64_t hitCount = 0;
    uint64_t missCount = 0;
};
