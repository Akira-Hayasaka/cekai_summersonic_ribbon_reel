#version 150

uniform sampler2DRect uTex;
uniform vec2 uTexSize;

// x: 左端の変形強度
// y: 右端の変形強度
//
// 正の値:
// 上半分が上へ、下半分が下へ広がる
//
// 負の値:
// 上下から中央へ縮む
uniform vec2 uStrengthLR;

// 左右それぞれ、どの程度内側まで変形させるか。
// 0.1〜0.5程度。
uniform float uEdgeWidth;

// 大きいほど変形が端に集中する。
// 1.0〜4.0程度。
uniform float uEdgePower;

// Y方向の変形中心。
// 通常は0.5。
uniform float uCenterY;

in vec2 vTexCoord;

out vec4 fragColor;

void main()
{
    vec2 uv = vTexCoord / uTexSize;

    float edgeWidth = clamp(
        uEdgeWidth,
        0.001,
        0.5
    );

    // 左端:
    // x = 0で1、uEdgeWidthより内側では0
    float leftInfluence =
        1.0 - smoothstep(
            0.0,
            edgeWidth,
            uv.x
        );

    // 右端:
    // x = 1で1、1-uEdgeWidthより内側では0
    float rightInfluence =
        smoothstep(
            1.0 - edgeWidth,
            1.0,
            uv.x
        );

    float edgePower = max(uEdgePower, 0.001);

    leftInfluence = pow(
        leftInfluence,
        edgePower
    );

    rightInfluence = pow(
        rightInfluence,
        edgePower
    );

    float warp =
        leftInfluence * uStrengthLR.x +
        rightInfluence * uStrengthLR.y;

    // 1.0に近づくと、端の縦一列がほぼ中央だけを
    // サンプリングして極端に引き伸ばされる。
    warp = clamp(warp, -1.0, 0.95);

    vec2 sourceUv = uv;

    // Y中央を基準にサンプリング座標を圧縮する。
    //
    // warp = 0:
    //   sourceY = outputY
    //
    // warp > 0:
    //   sourceYが中央へ近づくため、
    //   表示上は中央から上下へ引き伸ばされる。
    float centeredY = uv.y - uCenterY;
    float sourceScaleY = 1.0 - warp;

    sourceUv.y =
        uCenterY +
        centeredY * sourceScaleY;

    sourceUv = clamp(
        sourceUv,
        vec2(0.0),
        vec2(1.0)
    );

    fragColor = texture(
        uTex,
        sourceUv * uTexSize
    );

    // fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}