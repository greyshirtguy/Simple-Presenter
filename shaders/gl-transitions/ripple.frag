#version 440

// Ported from gl-transitions "ripple" (https://gl-transitions.com), under the MIT licence
// in the LICENSE file beside this one. Its parameters are options here (`options` is
// amplitude and speed, in that order). What follows the declarations is the original, but
// for the one change marked, and its credit is:
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

#define amplitude options.x
#define speed options.y

void main()
{
    vec2 uv = qt_TexCoord0;
    vec2 dir = uv - vec2(0.5);
    float dist = length(dir);
    vec2 offset = dir * (sin(progress * dist * amplitude - progress * speed) + 0.5) / 30.0;

    // Not in the original: ramp the displacement in, because this shader also draws the
    // resting slide at progress 0 and the original is already displaced there.
    offset *= min(progress * 8.0, 1.0);

    fragColor = mix(texture(fromTex, uv + offset), texture(toTex, uv),
                    smoothstep(0.2, 1.0, progress)) * qt_Opacity;
}
