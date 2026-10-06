#version 440

// Wipe: a soft edge crosses the picture, with what is coming behind it. `direction` is
// the way the edge travels: each part -1, 0 or 1, x to the right and y down.

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
void main()
{
    // How far across this pixel is, from 0 where the edge starts to 1 where it ends: for
    // a diagonal, from one corner to the opposite one.
    float across = dot(qt_TexCoord0 - 0.5, direction) / max(abs(direction.x) + abs(direction.y), 1.0) + 0.5;
    const float softness = 0.1;
    float edge = progress * (1.0 + softness);
    float ahead = smoothstep(edge - softness, edge, across);
    fragColor = mix(texture(toTex, qt_TexCoord0), texture(fromTex, qt_TexCoord0), ahead) * qt_Opacity;
}
