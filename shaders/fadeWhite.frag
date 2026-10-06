#version 440

// Fade White: the pictures change by way of white.
//
// Both pictures are changed by an amount that is nothing at their own end of the
// transition and complete by the middle, and are cross-faded meanwhile, so that half way
// there is only the shape of the two, in white. Colours are premultiplied by their
// transparency, which is why every change to a colour is in proportion to it.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
// The colour taken towards white.
vec4 changed(vec4 color, float amount)
{
    return vec4(mix(color.rgb, vec3(color.a), amount), color.a);
}

void main()
{
    vec4 going = changed(texture(fromTex, qt_TexCoord0), clamp(2.0 * progress, 0.0, 1.0));
    vec4 coming = changed(texture(toTex, qt_TexCoord0), clamp(2.0 * (1.0 - progress), 0.0, 1.0));
    fragColor = mix(going, coming, progress) * qt_Opacity;
}
