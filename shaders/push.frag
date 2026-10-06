#version 440

// Push: what is coming slides in from one side and pushes what is going out of the
// other. `direction` is the way things travel: each part -1, 0 or 1, x to the right and y down.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    vec2 direction;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
// The picture at uv, and nothing beyond its edges. An edge fades over one pixel, so that
// one which is moving, or at an angle, is not jagged.
vec4 picture(sampler2D tex, vec2 uv)
{
    vec2 pixel = max(fwidth(uv), vec2(1e-6));
    vec2 cover = clamp(uv / pixel + 0.5, 0.0, 1.0) * clamp((1.0 - uv) / pixel + 0.5, 0.0, 1.0);
    return texture(tex, clamp(uv, 0.0, 1.0)) * (cover.x * cover.y);
}

void main()
{
    vec4 going = picture(fromTex, qt_TexCoord0 - direction * progress);
    vec4 coming = picture(toTex, qt_TexCoord0 + direction * (1.0 - progress));
    fragColor = (going + coming) * qt_Opacity;
}
