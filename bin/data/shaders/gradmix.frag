#version 150

uniform sampler2DRect uTexA;
uniform sampler2DRect uTexB;

uniform vec2  uResolution;
uniform float uProgress;     // 0.0 -> 1.0
uniform float uTime;         // seconds
uniform float uMaxBlur;      // 例: 60.0〜120.0 pixel
uniform float uNoiseAmount;  // 例: 20.0〜50.0 pixel

in vec2 vTexCoord;
out vec4 fragColor;

const float GOLDEN_ANGLE = 2.39996323;
const float TWO_PI = 6.28318530718;

// サンプル数を大幅に増やし、雲/粉のように滑らかなブラーにする。
const int BLUR_SAMPLES = 64;


// ------------------------------------------------------------
// Noise
// ------------------------------------------------------------

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    f = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(
        mix(a, b, f.x),
        mix(c, d, f.x),
        f.y
    );
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;

    for (int i = 0; i < 5; ++i) {
        value += valueNoise(p) * amplitude;
        p = p * 2.03 + vec2(13.17, 7.91);
        amplitude *= 0.5;
    }

    return value;
}


// ------------------------------------------------------------
// Approximate disc blur
//
// プロトタイプ用。
// 本番ではDual Kawase Blurなどのマルチパスに置き換える。
// ------------------------------------------------------------

vec2 clampTexCoord(vec2 p)
{
    return clamp(
        p,
        vec2(0.5),
        uResolution - vec2(0.5)
    );
}

vec4 blurDisc(
    sampler2DRect tex,
    vec2 texCoord,
    float radius,
    float jitter
)
{
    // 半径がほぼ0のときはそのまま返す
    if (radius < 0.5) {
        return texture(tex, clampTexCoord(texCoord));
    }

    vec4 sum = vec4(0.0);
    float weightSum = 0.0;

    for (int i = 0; i < BLUR_SAMPLES; ++i) {
        float fi = float(i) + 0.5;

        // 面積が均等になるよう平方根で半径分布させる
        float normalizedRadius =
            sqrt(fi / float(BLUR_SAMPLES));

        // 黄金角スパイラル + ピクセル毎の回転ジッターで
        // 構造的なパターンを崩し、粒状感を軽減する
        float angle = fi * GOLDEN_ANGLE + jitter;

        vec2 offset =
            vec2(cos(angle), sin(angle))
            * normalizedRadius
            * radius;

        // ガウス状の重みで中心を強く、外側を滑らかに減衰させ
        // 雲/粉のような柔らかいボケにする
        float weight =
            exp(-normalizedRadius * normalizedRadius * 2.0);

        sum += texture(
            tex,
            clampTexCoord(texCoord + offset)
        ) * weight;

        weightSum += weight;
    }

    return sum / weightSum;
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

void main()
{
    float p = clamp(uProgress, 0.0, 1.0);

    vec2 uv = vTexCoord / uResolution;

    // ピクセル毎にサンプルリングを回転させ、
    // スパイラルの継ぎ目や同心円状のバンディングを隠す
    // （時間非依存にしてフレーム間のちらつきを防ぐ）
    float blurJitter = hash21(vTexCoord) * TWO_PI;

    // アスペクト比を考慮したノイズ座標
    vec2 noiseUv = vec2(
        uv.x * uResolution.x / uResolution.y,
        uv.y
    );

    // 下から上方向に流れる2つの低周波ノイズ
    float noiseA = fbm(
        noiseUv * 2.0
        + vec2(0.0, -uTime * 0.080)
    );

    float noiseB = fbm(
        noiseUv * 3.5
        + vec2(0.0, -uTime * 0.065)
        + vec2(31.7)
    );

    vec2 warp = vec2(
        noiseA - 0.5,
        noiseB - 0.5
    ) * uNoiseAmount;


    // --------------------------------------------------------
    // AとBそれぞれのブラー量
    // --------------------------------------------------------

    float blurARadius =
        uMaxBlur
        * smoothstep(0.00, 0.20, p);

    float blurBRadius =
        uMaxBlur
        * (1.0 - smoothstep(0.72, 1.00, p));

    vec4 colorA = blurDisc(
        uTexA,
        vTexCoord + warp * 0.15,
        blurARadius,
        blurJitter
    );

    // ブラー(0.72〜1.00)より早くワープを終息させる
    float warpFadeB = 1.0 - smoothstep(0.55, 0.80, p);

    vec4 colorB = blurDisc(
        uTexB,
        vTexCoord - warp * 0.15 * warpFadeB,
        blurBRadius,
        blurJitter
    );


    // --------------------------------------------------------
    // 中間の抽象的な色面
    //
    // AとBを非常に大きくブラーし、低周波ノイズで混ぜる。
    // --------------------------------------------------------

    vec2 center = uResolution * 0.5;

    // 少し中央へ座標を寄せ、元画像の形を分かりにくくする
    vec2 fieldCoordA =
        mix(vTexCoord, center, 0.28)
        + warp;

    vec2 fieldCoordB =
        mix(vTexCoord, center, 0.28)
        - warp * 0.7;

    vec4 fieldA = blurDisc(
        uTexA,
        fieldCoordA,
        uMaxBlur * 2.2,
        blurJitter + 1.7
    );

    vec4 fieldB = blurDisc(
        uTexB,
        fieldCoordB,
        uMaxBlur * 2.2,
        blurJitter + 3.1
    );

    // 中間期間でA由来の色からB由来の色へ移動
    float paletteProgress =
        smoothstep(0.22, 0.72, p);

    // 下から上へゆっくり移動する大きなグラデーション帯
    float bandCenterY =
        mix(1.20, -0.20, paletteProgress)
        + (noiseB - 0.5) * 0.15;

    float bandDistance =
        (uv.y - bandCenterY) / 0.34;

    float softBand =
        exp(-bandDistance * bandDistance);

    float localPaletteProgress = clamp(
        paletteProgress
        + (noiseA - 0.5) * 0.24
        + (softBand - 0.5) * 0.10,
        0.0,
        1.0
    );

    vec4 fieldColor =
        mix(fieldA, fieldB, localPaletteProgress);

    // 動く光量グラデーション
    fieldColor.rgb *= mix(
        0.88,
        1.10,
        softBand
    );

    // 少し彩度とコントラストを抑えて「色面」に寄せる
    float luminance = dot(
        fieldColor.rgb,
        vec3(0.2126, 0.7152, 0.0722)
    );

    fieldColor.rgb = mix(
        vec3(luminance),
        fieldColor.rgb,
        0.82
    );


    // --------------------------------------------------------
    // 3フェーズの合成
    // --------------------------------------------------------

    // Aから中間フィールドへ。
    // ノイズによってピクセルごとの進行を少しずらす。
    float toField = smoothstep(
        0.04,
        0.20,
        p + (noiseA - 0.5) * 0.035
    );

    // 中間フィールドからBへ
    float toB = smoothstep(
        0.72,
        0.96,
        p + (noiseB - 0.5) * 0.055
    );

    vec4 color = mix(
        colorA,
        fieldColor,
        toField
    );

    color = mix(
        color,
        colorB,
        toB
    );

    fragColor = color;
}