#pragma once

#include "ofMain.h"

#include <cstdint>
#include <vector>

enum class PathCommandType {
    MoveTo,
    LineTo,
    QuadTo,
    CubicTo,
    Close
};

struct PathCommand {
    PathCommandType type = PathCommandType::MoveTo;
    glm::vec2 p0 = {0.0f, 0.0f};
    glm::vec2 p1 = {0.0f, 0.0f};
    glm::vec2 p2 = {0.0f, 0.0f};
};

struct GlyphContour {
    std::vector<PathCommand> commands;
};

struct GlyphOutline {
    uint32_t glyphId = 0;
    std::vector<GlyphContour> contours;
    ofRectangle bounds;
    float advanceX = 0.0f;
    float advanceY = 0.0f;

    bool empty() const {
        return contours.empty();
    }
};
