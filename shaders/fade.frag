#version 440

// Fade: what is going fades away to nothing, and only then does what is coming fade in.

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
    vec4 color = progress < 0.5 ? texture(fromTex, qt_TexCoord0) * (1.0 - 2.0 * progress)
                                : texture(toTex, qt_TexCoord0) * (2.0 * progress - 1.0);
    fragColor = color * qt_Opacity;
}
