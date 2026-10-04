#version 440

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
    // Both textures are premultiplied, so a straight mix is a correct cross-fade.
    fragColor = mix(texture(fromTex, qt_TexCoord0), texture(toTex, qt_TexCoord0), progress) * qt_Opacity;
}
