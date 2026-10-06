#version 440

// Door Wipe: what is coming appears down the middle and widens to both sides, as if what
// is going were a pair of doors being slid apart, with a soft edge.

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
    float fromMiddle = abs(qt_TexCoord0.x - 0.5);
    // The opening has reached the sides when the transition ends. Its edge is soft, but
    // not at the very start, where a soft edge would show before anything has opened.
    float softness = min(0.06, progress * 0.3);
    float opened = progress * (0.5 + softness);
    float shut = smoothstep(opened - softness, opened, fromMiddle);
    if (progress <= 0.0)
        shut = 1.0;
    fragColor = mix(texture(toTex, qt_TexCoord0), texture(fromTex, qt_TexCoord0), shut) * qt_Opacity;
}
