#version 150

uniform sampler2DRect uScene;

uniform vec2  uResolution;     // FBO size in pixels: vec2(width, height)

// Glass shape: x, y, width, height in texture/screen pixels
uniform vec4  uGlassRect;
uniform float uRadius;         // corner radius in px
uniform float uEdgeSoftness;   // edge antialiasing / softness in px

// Jitter-like controls
uniform float uRefraction;     // px offset amount. 0..80くらい
uniform float uDepth;          // px. larger = thicker edge falloff
uniform float uBendFalloffPower; // exponent for inward decay profile
uniform float uDispersion;     // px RGB separation. 0..12くらい
uniform float uFrost;          // px blur radius. 0..30くらい
uniform float uLightIntensity; // 0..2
uniform float uLightAngle;     // radians
uniform float uBlend;          // 0..1, mix original and glass result
uniform float uOpacity;        // 0..1

// Optional tint
uniform vec3  uTintColor;      // e.g. vec3(0.9, 0.96, 1.0)
uniform float uTintAmount;     // 0..1, usually 0.0..0.2

in vec2 vTexCoord;
out vec4 fragColor;

vec2 clampCoord(vec2 p)
{
    return clamp(p, vec2(0.5), uResolution - vec2(0.5));
}

vec4 sampleScene(vec2 p)
{
    return texture(uScene, clampCoord(p));
}

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Signed distance to rounded rectangle.
// p: local position from center
// b: half size
// r: radius
float sdRoundRect(vec2 p, vec2 b, float r)
{
    r = min(r, min(b.x, b.y));
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float glassSdf(vec2 coord)
{
    vec2 center = uGlassRect.xy + uGlassRect.zw * 0.5;
    vec2 halfSize = uGlassRect.zw * 0.5;
    vec2 p = coord - center;
    return sdRoundRect(p, halfSize, uRadius);
}

vec2 sdfNormal(vec2 coord)
{
    float e = 1.0;

    float dx = glassSdf(coord + vec2(e, 0.0)) - glassSdf(coord - vec2(e, 0.0));
    float dy = glassSdf(coord + vec2(0.0, e)) - glassSdf(coord - vec2(0.0, e));

    vec2 n = vec2(dx, dy);
    float lenN = length(n);

    if (lenN < 0.0001) {
        return vec2(0.0, 0.0);
    }

    return n / lenN;
}

// High-quality Poisson blur.
// Uses per-pixel kernel rotation and distance weighting for smoother frosted glass.
vec4 blurScene(vec2 p)
{
    if (uFrost < 0.5) {
        return sampleScene(p);
    }

    float r = uFrost;
    const int TAP_COUNT = 20;
    const vec2 poisson[TAP_COUNT] = vec2[](
        vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696,  0.457), vec2(-0.203,  0.621),
        vec2( 0.962, -0.195), vec2( 0.473, -0.480), vec2( 0.519,  0.767), vec2( 0.185, -0.893),
        vec2( 0.507,  0.064), vec2( 0.896,  0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598),
        vec2(-0.112, -0.248), vec2( 0.296,  0.316), vec2(-0.580,  0.820), vec2(-0.028,  0.939),
        vec2( 0.763, -0.648), vec2( 0.056,  0.132), vec2(-0.970,  0.240), vec2( 0.822,  0.891)
    );

    float angle = hash12(floor(p * 0.25)) * 6.2831853;
    float ca = cos(angle);
    float sa = sin(angle);
    mat2 rot = mat2(ca, -sa, sa, ca);

    // Gaussian-like radial weighting. Sigma controls blur smoothness profile.
    float sigma = 0.65;
    float invTwoSigma2 = 1.0 / (2.0 * sigma * sigma);

    vec4 accum = sampleScene(p) * 1.0;
    float weightSum = 1.0;

    for (int i = 0; i < TAP_COUNT; ++i) {
        vec2 o = rot * poisson[i];
        float rr = dot(poisson[i], poisson[i]);
        float w = exp(-rr * invTwoSigma2);
        accum += sampleScene(p + o * r) * w;
        weightSum += w;
    }

    return accum / max(weightSum, 0.0001);
}

void main()
{
    vec2 coord = vTexCoord;

    vec4 original = sampleScene(coord);

    float d = glassSdf(coord);

    // d <= 0 means inside glass shape.
    float feather = max(uEdgeSoftness, 0.001);
    float mask = 1.0 - smoothstep(0.0, feather, d);

    if (mask <= 0.001) {
        fragColor = original;
        return;
    }

    vec2 n = sdfNormal(coord);

    // Distance from edge toward inside.
    float insideDist = max(-d, 0.0);

    // Strong near the border, weaker toward the center.
    float edgeBend = 1.0 - smoothstep(0.0, max(uDepth, 1.0), insideDist);

    // Keep refraction concentrated around the edge.
    // Larger uBendFalloffPower makes the interior decay faster.
    float bendPower = max(uBendFalloffPower, 0.001);
    float bendProfile = mask * pow(edgeBend, bendPower);

    // Outward SDF normal. Sampling in the opposite direction gives
    // a plausible refractive bend.
    vec2 refractOffset = -n * uRefraction * bendProfile;

    vec2 baseCoord = coord + refractOffset;

    // Chromatic dispersion along the same pseudo-normal.
    vec2 dispersionOffset = n * uDispersion * bendProfile;

    vec4 rSample = blurScene(baseCoord + dispersionOffset);
    vec4 gSample = blurScene(baseCoord);
    vec4 bSample = blurScene(baseCoord - dispersionOffset);

    vec3 glassRgb = vec3(rSample.r, gSample.g, bSample.b);

    // Optional cool glass tint.
    glassRgb = mix(glassRgb, uTintColor, clamp(uTintAmount, 0.0, 1.0));

    // Edge highlight.
    vec2 lightDir = normalize(vec2(cos(uLightAngle), sin(uLightAngle)));

    float facing = clamp(dot(-n, lightDir) * 0.5 + 0.5, 0.0, 1.0);

    float rimLine = 1.0 - smoothstep(0.0, max(1.0, feather * 2.0), abs(d));
    float broadRim = edgeBend * mask;

    float spec = pow(facing, 5.0) * broadRim;
    float lineSpec = pow(facing, 2.0) * rimLine;

    vec3 highlight = vec3(1.0) * uLightIntensity * (spec * 0.35 + lineSpec * 0.65);

    // Subtle edge darkening gives thickness.
    float darken = 0.0;
    glassRgb *= 1.0 - darken * edgeBend * mask;
    glassRgb += highlight;

    vec4 glassColor = vec4(glassRgb, original.a);

    // Jitter-like Blend: mix original appearance and glass result.
    vec4 blended = mix(original, glassColor, clamp(uBlend, 0.0, 1.0));

    // Sample original at distorted UV position to get alpha
    vec4 distortedOriginal = sampleScene(baseCoord);
    float distortedAlpha = distortedOriginal.a;

    // Shape opacity / mask with distorted alpha.
    float finalAlpha = mask * clamp(uOpacity, 0.0, 1.0) * distortedAlpha;
    fragColor = vec4(blended.rgb, finalAlpha);
}