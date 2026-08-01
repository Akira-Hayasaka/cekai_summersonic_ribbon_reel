#pragma once

#include "TextMesh.h"

class TextObject;

class TextRenderer {
public:
    void setup();

    void draw(const TextMesh &mesh, const glm::mat4 &transform);
    void draw(const TextObject &text);

    void setShader(ofShader *shader);
    void setColor(const ofFloatColor &color);

private:
    ofShader *customShader = nullptr;
    ofFloatColor color = ofFloatColor(1.0f);
};
