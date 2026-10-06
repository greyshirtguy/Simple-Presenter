#version 440

// Color Push: the red, green and blue of what is coming slide in from three sides (red
// from the right, green from the left, blue from below) and meet, while those of what is
// going slide off the opposite ways, fading.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
// The picture at uv, and nothing beyond its edges.
vec4 picture(sampler2D tex, vec2 uv)
{
    vec2 pixel = max(fwidth(uv), vec2(1e-6));
    vec2 cover = clamp(uv / pixel + 0.5, 0.0, 1.0) * clamp((1.0 - uv) / pixel + 0.5, 0.0, 1.0);
    return texture(tex, clamp(uv, 0.0, 1.0)) * (cover.x * cover.y);
}

// Both pictures as they are for the colour that comes from `side`.
vec4 pushed(vec2 side)
{
    return picture(toTex, qt_TexCoord0 - side * (1.0 - progress))
         + picture(fromTex, qt_TexCoord0 + side * progress) * (1.0 - progress);
}

void main()
{
    vec4 red = pushed(vec2(1.0, 0.0));
    vec4 green = pushed(vec2(-1.0, 0.0));
    vec4 blue = pushed(vec2(0.0, 1.0));
    fragColor = vec4(red.r, green.g, blue.b, (red.a + green.a + blue.a) / 3.0) * qt_Opacity;
}
