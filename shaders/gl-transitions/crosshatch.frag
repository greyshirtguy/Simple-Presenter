#version 440

// Ported from gl-transitions "crosshatch" (https://gl-transitions.com), under the MIT licence
// in the LICENSE file beside this one. Its parameters are fixed at their defaults. What
// follows the declarations is the original, whose credit is:
// License: MIT
// Author: pthrasher
// adapted by gre from https://gist.github.com/pthrasher/04fd9a7de4012cbb03f6

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;

// gl-transitions measures y upwards from the bottom; Qt measures it downwards.
vec4 getFromColor(vec2 uv) { return texture(fromTex, vec2(uv.x, 1.0 - uv.y)); }
vec4 getToColor(vec2 uv) { return texture(toTex, vec2(uv.x, 1.0 - uv.y)); }
const vec2 center = vec2(0.5);
const float threshold = 3.0;
const float fadeEdge = 0.1;

float rand(vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
vec4 transition(vec2 p) {
  float dist = distance(center, p) / threshold;
  float r = progress - min(rand(vec2(p.y, 0.0)), rand(vec2(0.0, p.x)));
  return mix(getFromColor(p), getToColor(p), mix(0.0, mix(step(dist, r), 1.0, smoothstep(1.0-fadeEdge, 1.0, progress)), smoothstep(0.0, fadeEdge, progress)));
}

void main()
{
    fragColor = transition(vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y)) * qt_Opacity;
}
