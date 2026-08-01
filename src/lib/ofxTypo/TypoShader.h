#pragma once

#include "ofMain.h"

class TypoShader {
public:
    bool load(const std::string &basePath);
    ofShader &getShader();
    const ofShader &getShader() const;

private:
    ofShader shader;
};
