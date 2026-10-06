#version 440

// Color Burn: the pictures cross over in a flash of a colour. The colour is added to
// each as its end of the transition is left behind, most in the middle, where the two are
// at full strength together. `tint` is the colour, not premultiplied.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    vec4 tint;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
// The picture with the flash added, as far as white.
vec4 burnt(vec4 color, float amount)
{
    return vec4(min(color.rgb + amount * tint.rgb * tint.a * color.a, vec3(color.a)), color.a);
}

void main()
{
    float flash = 1.0 - abs(2.0 * progress - 1.0);
    vec4 going = burnt(texture(fromTex, qt_TexCoord0), flash) * clamp(2.0 * (1.0 - progress), 0.0, 1.0);
    vec4 coming = burnt(texture(toTex, qt_TexCoord0), flash) * clamp(2.0 * progress, 0.0, 1.0);
    fragColor = min(going + coming, vec4(1.0)) * qt_Opacity;
}
