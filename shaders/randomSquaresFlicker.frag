#version 440

// Random Squares Flicker: the pictures cross-fade behind a flicker of squares of light,
// which is strongest half way.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    float ratio;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
float random(vec2 at)
{
    return fract(sin(dot(at, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    float strength = 1.0 - abs(2.0 * progress - 1.0);
    // Twenty squares across. Which of them are lit changes sixty times in the course of
    // the transition.
    vec2 square = floor(qt_TexCoord0 * vec2(20.0, 20.0 / ratio));
    float lit = random(square + 31.0 * floor(progress * 60.0));
    vec4 color = mix(texture(fromTex, qt_TexCoord0), texture(toTex, qt_TexCoord0), progress);
    // Light is added in proportion to how solid the picture is, and no further than white.
    color.rgb = min(color.rgb + lit * strength * 0.5 * color.a, vec3(color.a));
    fragColor = color * qt_Opacity;
}
