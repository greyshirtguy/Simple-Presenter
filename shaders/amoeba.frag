#version 440

// Amoeba: what is coming spreads from the middle as a blob with a lumpy, shifting outline.
// `options.x` is the scale of the lumps: how many of them there are across the picture.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    float ratio;
    vec4 options;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
float random(vec2 at)
{
    return fract(sin(dot(at, vec2(12.9898, 78.233))) * 43758.5453);
}

// A random value that changes smoothly from place to place, between 0 and 1: random
// values at the corners of a grid, blended across each square of it.
float noise(vec2 at)
{
    vec2 corner = floor(at);
    vec2 within = fract(at);
    vec2 blend = within * within * (3.0 - 2.0 * within);
    return mix(mix(random(corner), random(corner + vec2(1.0, 0.0)), blend.x),
               mix(random(corner + vec2(0.0, 1.0)), random(corner + vec2(1.0, 1.0)), blend.x), blend.y);
}

void main()
{
    vec2 fromMiddle = qt_TexCoord0 - 0.5;
    // The blob has reached this pixel when its radius, which the noise makes anything
    // from half to the whole of `reach`, is as far as the pixel is from the middle.
    float lumps = noise(vec2(fromMiddle.x * ratio, fromMiddle.y) * options.x);
    float away = length(fromMiddle) / (0.5 + 0.5 * lumps);
    // The corners are 0.71 away, so at their slowest they are reached at 1.42.
    float reach = progress * 1.45;
    float outside = smoothstep(reach - 0.01, reach, away);
    if (progress <= 0.0)
        outside = 1.0;
    fragColor = mix(texture(toTex, qt_TexCoord0), texture(fromTex, qt_TexCoord0), outside) * qt_Opacity;
}
