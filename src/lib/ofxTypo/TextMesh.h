#pragma once

#include "ofMain.h"

#include <cstdint>
#include <vector>

struct TypoVertex {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec2 uv = {0.0f, 0.0f};
    glm::vec2 glyphLocalPos = {0.0f, 0.0f};
    glm::vec2 glyphBoundsMin = {0.0f, 0.0f};
    glm::vec2 glyphBoundsMax = {0.0f, 0.0f};
    float glyphIndex = 0.0f;
    float charIndex = 0.0f;
    float lineIndex = 0.0f;
    float contourIndex = 0.0f;
    float random = 0.0f;
};

struct TextMesh {
    std::vector<TypoVertex> vertices;
    std::vector<uint32_t> indices;

    ofVboMesh vboMesh;
    ofRectangle bounds;

    int glyphCount = 0;
    int lineCount = 0;

    bool uploaded = false;

    void clear();
    void upload();
    void draw() const;
};
