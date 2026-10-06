#version 440

// Dispersion Blur: each picture comes apart into copies of itself that spread out along a
// spiral, furthest half way, while the two cross-fade. `options.x` is how far they
// spread, as a part of the picture's size, and `options.y` the angle from each copy to the
// next, in radians.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    vec4 options;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
const int copies = 10;

vec4 dispersed(sampler2D tex, float spread)
{
    vec4 sum = vec4(0.0);
    for (int i = 0; i < copies; ++i) {
        // Further out with each copy, evenly over the area of the circle.
        float out_ = sqrt(float(i) / float(copies)) * spread;
        float angle = float(i) * options.y;
        sum += texture(tex, qt_TexCoord0 + out_ * vec2(cos(angle), sin(angle)));
    }
    return sum / float(copies);
}

void main()
{
    float spread = options.x * (1.0 - abs(2.0 * progress - 1.0));
    fragColor = mix(dispersed(fromTex, spread), dispersed(toTex, spread), progress) * qt_Opacity;
}
