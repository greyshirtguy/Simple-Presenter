#version 440

// Ported from gl-transitions "flyeye" (https://gl-transitions.com), under the MIT licence
// in the LICENSE file beside this one. Its parameters are options here (`options` is zoom, size and
// colour separation, in that order), and what is going keeps its transparency, where the
// original made it solid. What follows the declarations is otherwise the original, whose
// credit is:
// Author: gre
// License: MIT

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    vec4 options;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;

// gl-transitions measures y upwards from the bottom; Qt measures it downwards.
vec4 getFromColor(vec2 uv) { return texture(fromTex, vec2(uv.x, 1.0 - uv.y)); }
vec4 getToColor(vec2 uv) { return texture(toTex, vec2(uv.x, 1.0 - uv.y)); }
#define zoom options.x
#define size options.y
#define colorSeparation options.z

vec4 transition(vec2 p) {
  float inv = 1. - progress;
  vec2 disp = size*vec2(cos(zoom*p.x), sin(zoom*p.y));
  vec4 texTo = getToColor(p + inv*disp);
  vec4 middle = getFromColor(p + progress*disp);
  vec4 texFrom = vec4(
    getFromColor(p + progress*disp*(1.0 - colorSeparation)).r,
    middle.g,
    getFromColor(p + progress*disp*(1.0 + colorSeparation)).b,
    middle.a);
  return texTo*progress + texFrom*inv;
}

void main()
{
    fragColor = transition(vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y)) * qt_Opacity;
}
