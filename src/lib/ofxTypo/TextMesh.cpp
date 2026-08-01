#include "TextMesh.h"

void TextMesh::clear() {
    vertices.clear();
    indices.clear();
    vboMesh.clear();
    bounds = ofRectangle();
    glyphCount = 0;
    lineCount = 0;
    uploaded = false;
}

void TextMesh::upload() {
    vboMesh.clear();
    vboMesh.setMode(OF_PRIMITIVE_TRIANGLES);

    if (vertices.empty() || indices.empty()) {
        uploaded = true;
        bounds = ofRectangle();
        return;
    }

    bounds.set(vertices.front().position.x, vertices.front().position.y, 0.0f, 0.0f);

    for (const auto &v : vertices) {
        vboMesh.addVertex(v.position);
        vboMesh.addTexCoord(v.uv);
        bounds.growToInclude(v.position.x, v.position.y);
    }

    vboMesh.addIndices(indices);
    uploaded = true;
}

void TextMesh::draw() const {
    if (!uploaded) {
        return;
    }
    vboMesh.draw();
}
