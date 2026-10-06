#version 440

// Random Pixels: what is coming takes over a pixel at a time, in no order.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
float random(vec2 at)
{
    return fract(sin(dot(at, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    // Each pixel changes over when the transition reaches a moment of its own.
    bool changed = random(floor(gl_FragCoord.xy)) < progress;
    fragColor = (changed ? texture(toTex, qt_TexCoord0) : texture(fromTex, qt_TexCoord0)) * qt_Opacity;
}
