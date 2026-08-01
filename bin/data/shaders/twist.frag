#version 150

uniform sampler2DRect uTex;
uniform float timer;
uniform float intensity;

in vec2 vTexCoord;
out vec4 fragColor;


// 0.0〜1.0の疑似乱数
float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}


// 補間された2D Value Noise
float noise2D(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    // Hermite補間
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(a, b, u.x),
        mix(c, d, u.x),
        u.y
    );
}


// 指定した出力座標に対してノイズ変位を計算し、テクスチャをサンプリングする。
// uTex は premultiplied-alpha で格納されているため、返り値も premultiplied。
vec4 sampleWarped(vec2 texCoordPx, vec2 textureSizePx)
{
    // ピクセル座標を0〜1付近の座標に正規化
    vec2 normalizedCoord = texCoordPx / textureSizePx;

    // 値を大きくするとノイズが細かくなる
    const float noiseScale = 5.0;

    vec2 noiseCoord = normalizedCoord * noiseScale;

    // timerによってノイズ空間を移動させる
    vec2 timeOffset = vec2(
        timer * 0.17,
        timer * 0.11
    );

    // X・Y方向で異なるノイズを使用
    float noiseX = noise2D(
        noiseCoord + timeOffset
    );

    float noiseY = noise2D(
        noiseCoord + timeOffset + vec2(37.2, 91.7)
    );

    // 0〜1から-1〜1へ変換
    vec2 displacement = vec2(noiseX, noiseY) * 2.0 - 1.0;

    // intensityはピクセル単位
    vec2 distortedCoord = texCoordPx + displacement * intensity;

    // テクスチャ外への参照を防ぐ
    distortedCoord = clamp(
        distortedCoord,
        vec2(0.5),
        textureSizePx - vec2(0.5)
    );

    return texture(uTex, distortedCoord);
}


void main()
{
    vec2 textureSizePx = vec2(textureSize(uTex));

    // 変位ワープによって透過エッジがサブピクセルで折り畳まれ、単一サンプルでは
    // ジャギる。4x回転グリッドのサブピクセル位置で複数サンプリングして平均する
    // ことで、エッジをアンチエイリアスしてなめらかに描画する（シェーダ内SSAA）。
    // premultiplied-alpha なので平均は線形に正しい。
    const vec2 offsets[4] = vec2[4](
        vec2(-0.125, -0.375),
        vec2( 0.375, -0.125),
        vec2( 0.125,  0.375),
        vec2(-0.375,  0.125)
    );

    vec4 accum = vec4(0.0);
    for (int i = 0; i < 4; i++)
    {
        accum += sampleWarped(vTexCoord + offsets[i], textureSizePx);
    }

    fragColor = accum * 0.25;
}