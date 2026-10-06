#version 440

// Ripple: what is coming spreads from the middle in a circle, with a swollen rim running
// ahead of it like the front of a ripple: within the rim what is coming is stretched, and
// fades into what is going.

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
void main()
{
    // Measured so that the circle is round on screen and the corners are 1 from the middle.
    vec2 fromMiddle = (qt_TexCoord0 - 0.5) * vec2(ratio, 1.0);
    float corner = 0.5 * length(vec2(ratio, 1.0));
    float away = length(fromMiddle) / corner;
    float reach = 1.3 * progress;
    float rim = 0.5 * progress;

    vec4 going = texture(fromTex, qt_TexCoord0);
    vec4 color = going;
    if (away < reach) {
        color = texture(toTex, qt_TexCoord0);
    } else if (away < reach + rim) {
        float through = (away - reach) / rim;
        // Within the rim what is coming is drawn from nearer the middle than it is, the
        // more so the further out.
        float drawnFrom = reach + rim * through * through;
        vec2 uv = 0.5 + fromMiddle * (drawnFrom / away) / vec2(ratio, 1.0);
        color = mix(texture(toTex, uv), going, smoothstep(0.0, 1.0, through));
    }
    fragColor = color * qt_Opacity;
}
