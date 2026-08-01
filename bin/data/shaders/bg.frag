#version 150

in vec2 vTexCoord;
out vec4 fragColor;

uniform vec4 color0;
uniform vec4 color1;
uniform vec4 color2;
uniform vec4 color3;

uniform vec2 p0;
uniform vec2 p1;
uniform vec2 p2;
uniform vec2 p3;

// Pixel dimensions of the render target.
// p0..p3 and vTexCoord are assumed to be normalized UV coordinates.
uniform vec2 resolution;

// Gaussian radius measured as a fraction of the render-target height.
// Good initial range: 0.30 .. 0.80
uniform float gradientRadius;

// Optional relative influence of each color source. Normally vec4(1.0).
// Useful when coincident or clustered points should not be averaged equally.
uniform vec4 colorMass;

vec2 normalizeByResolution(vec2 value)
{
    if (resolution.x > 0.0 && resolution.y > 0.0) {
        return value / resolution;
    }
    return value;
}

float aspectCorrectDistanceSquared(vec2 a, vec2 b)
{
    a = normalizeByResolution(a);
    b = normalizeByResolution(b);

    vec2 metric = vec2(1.0);
    if (resolution.y > 0.0) {
        metric.x = resolution.x / resolution.y;
    }

    vec2 d = (a - b) * metric;
    return dot(d, d);
}

float logGaussianWeight(vec2 coord, vec2 point, float mass)
{
    float radius = (gradientRadius > 0.0) ? gradientRadius : 0.62;
    radius = max(radius, 1e-4);
    float d2 = aspectCorrectDistanceSquared(coord, point);

    // Work in log space so points far outside the screen do not underflow.
    return log(max(mass, 1e-6)) - 0.5 * d2 / (radius * radius);
}

void main()
{
    vec2 coord = vTexCoord;

    vec4 score = vec4(
        logGaussianWeight(coord, p0, colorMass.x),
        logGaussianWeight(coord, p1, colorMass.y),
        logGaussianWeight(coord, p2, colorMass.z),
        logGaussianWeight(coord, p3, colorMass.w)
    );

    // Stable softmax / normalized radial-basis-function weights.
    float maxScore = max(max(score.x, score.y), max(score.z, score.w));
    vec4 weight = exp(score - vec4(maxScore));
    weight /= max(dot(weight, vec4(1.0)), 1e-20);

    // The weights are always non-negative and sum to one, so the result
    // cannot overshoot outside the convex hull of the four input colors.
    fragColor =
        weight.x * color0 +
        weight.y * color1 +
        weight.z * color2 +
        weight.w * color3;
}
