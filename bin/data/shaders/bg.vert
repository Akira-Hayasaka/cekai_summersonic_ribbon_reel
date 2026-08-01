#version 150

in vec4 position;
in vec2 texcoord;
uniform mat4 modelViewProjectionMatrix;

out vec2 vTexCoord;

void main() {
    // Pass through the incoming oF texcoord. The fragment shader accepts either
    // normalized UVs or pixel texcoords and converts when necessary.
    vTexCoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
