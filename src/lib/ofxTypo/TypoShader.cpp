#include "TypoShader.h"

bool TypoShader::load(const std::string &basePath) {
    return shader.load(basePath);
}

ofShader &TypoShader::getShader() {
    return shader;
}

const ofShader &TypoShader::getShader() const {
    return shader;
}
