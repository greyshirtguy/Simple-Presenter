#version 440

// Ported from gl-transitions "windowslice" (https://gl-transitions.com), under the MIT licence
// in the LICENSE file beside this one. Its parameters are fixed at their defaults. What
// follows the declarations is the original, whose credit is:
// Author: gre
// License: MIT

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

// gl-transitions measures y upwards from the bottom; Qt measures it downwards.
vec4 getFromColor(vec2 uv) { return texture(fromTex, vec2(uv.x, 1.0 - uv.y)); }
vec4 getToColor(vec2 uv) { return texture(toTex, vec2(uv.x, 1.0 - uv.y)); }

const float count = 10.0;
const float smoothness = 0.5;

vec4 transition (vec2 p) {
  float pr = smoothstep(-smoothness, 0.0, p.x - progress * (1.0 + smoothness));
  float s = step(pr, fract(count * p.x));
  return mix(getFromColor(p), getToColor(p), s);
}

void main()
{
    fragColor = transition(vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y)) * qt_Opacity;
}
