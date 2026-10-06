#version 440

// Noisy Zoom: the pictures cross-fade while zooming in a little and back, by an amount
// that differs from pixel to pixel, which gives the zoom a grain.

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
    // Most half way, nothing at either end.
    float swell = sin(3.14159265 * progress);
    swell *= swell;
    float zoom = 1.0 - 0.16 * swell * mix(0.55, 1.0, random(qt_TexCoord0));
    vec2 uv = (qt_TexCoord0 - 0.5) * zoom + 0.5;
    fragColor = mix(texture(fromTex, uv), texture(toTex, uv), progress) * qt_Opacity;
}
