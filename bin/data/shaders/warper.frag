#version 150

uniform sampler2D uTex;
uniform vec2 uResolution;
uniform float uTime;
uniform float uDistortionAmount;

// 0.0 - 1.0 UV
in vec2 vTexCoord;

out vec4 fragColor;

vec2 applyLens(
    vec2 uv,
    vec2 center,
    vec2 radius,
    float strength
) {
    // 楕円座標系に変換
    vec2 p = (uv - center) / radius;
    float r = length(p);
    float r2 = dot(p, p);

    // 標準的な radial distortion:
    // strength > 0: pincushion
    // strength < 0: barrel
    vec2 pDistorted = p * (1.0 + strength * r2);

    // 半径の外は元画像を優先し、境界は滑らかに接続
    float mask = 1.0 - smoothstep(0.9, 1.0, r);
    vec2 pMixed = mix(p, pDistorted, mask);

    return center + pMixed * radius;
}

void main() {
    vec2 uv = vTexCoord / uResolution;

    // 中央にレンズを置く
    vec2 center = vec2(0.5, 0.5);

    // 楕円の半径
    // x, y を変えると横長 / 縦長レンズになる
    float scale = 0.75;
    vec2 radius = vec2(scale, scale);

    float strength = mix(0.0, -0.8, clamp(uDistortionAmount, 0.0, 1.0));

    vec2 warpedUv = applyLens(
        uv,
        center,
        radius,
        strength
    );

    // UV範囲外をクランプ
    float a = 1.0;
    if (warpedUv.x < 0.0 || warpedUv.x > 1.0 || warpedUv.y < 0.0 || warpedUv.y > 1.0) {
        a = 0.0;
    }
    warpedUv = clamp(warpedUv, vec2(0.0), vec2(1.0));

    vec4 col = texture(uTex, warpedUv);

    fragColor = col * a;
}