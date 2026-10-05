#version 440

// Ported from gl-transitions "randomsquares" (https://gl-transitions.com), under the MIT licence
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

const ivec2 size = ivec2(10, 10);
const float smoothness = 0.5;

float rand (vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec4 transition(vec2 p) {
  float r = rand(floor(vec2(size) * p));
  float m = smoothstep(0.0, -smoothness, r - (progress * (1.0 + smoothness)));
  return mix(getFromColor(p), getToColor(p), m);
}

void main()
{
    fragColor = transition(vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y)) * qt_Opacity;
}
