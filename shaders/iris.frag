#version 440

// Iris: what is coming opens out from the middle in a round, soft-edged hole that keeps the
// shape of the picture, so that it reaches all four edges together.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
void main()
{
    float fromMiddle = length(qt_TexCoord0 - 0.5);
    // The corners are 0.71 from the middle; the hole is past them when the transition ends.
    const float softness = 0.05;
    float opened = progress * (0.72 + softness);
    float outside = smoothstep(opened - softness, opened, fromMiddle);
    if (progress <= 0.0)
        outside = 1.0;
    fragColor = mix(texture(toTex, qt_TexCoord0), texture(fromTex, qt_TexCoord0), outside) * qt_Opacity;
}
