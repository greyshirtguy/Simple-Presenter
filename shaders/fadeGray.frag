#version 440

// Fade Gray: the colour drains out of what is going, the pictures cross-fade in grey, and
// the colour comes back into what is coming.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
// The colour with some of its colourfulness taken out. The weights are how bright the
// eye finds red, green and blue (Rec. 709).
vec4 drained(vec4 color, float amount)
{
    float grey = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    return vec4(mix(color.rgb, vec3(grey), amount), color.a);
}

void main()
{
    vec4 going = drained(texture(fromTex, qt_TexCoord0), smoothstep(0.0, 0.4, progress));
    vec4 coming = drained(texture(toTex, qt_TexCoord0), smoothstep(0.0, 0.4, 1.0 - progress));
    fragColor = mix(going, coming, progress) * qt_Opacity;
}
