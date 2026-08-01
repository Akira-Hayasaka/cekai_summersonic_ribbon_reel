#pragma once

#include "GlyphOutline.h"

class GlyphMeshBuilder {
public:
    bool buildFillMesh(const GlyphOutline &outline, ofMesh &outMesh) const;
};
