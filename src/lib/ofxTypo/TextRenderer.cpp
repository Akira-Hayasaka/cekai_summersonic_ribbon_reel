#include "TextRenderer.h"

#include "TextObject.h"

void TextRenderer::setup() {
}

void TextRenderer::draw(const TextMesh &mesh, const glm::mat4 &transform) {
    ofPushMatrix();
    ofMultMatrix(transform);

    const bool hasShader = customShader != nullptr;
    if (hasShader) {
        customShader->begin();
    }

    ofSetColor(color);
    mesh.draw();

    if (hasShader) {
        customShader->end();
    }

    ofPopMatrix();
}

void TextRenderer::draw(const TextObject &text) {
    draw(text.getMesh(), glm::mat4(1.0f));
}

void TextRenderer::setShader(ofShader *shader) {
    customShader = shader;
}

void TextRenderer::setColor(const ofFloatColor &value) {
    color = value;
}
