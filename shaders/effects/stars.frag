#version 440

// Stars: a field of stars rushing out from the middle, over the media.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Speed: how fast they come (0 to 20)
//   values.y  Blend: how much the stars take over from the media
// Four layers of stars, each coming nearer and growing brighter, then starting again far
// off.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec2 size;
    vec4 values;
    vec4 more;
    vec4 color1;
    vec4 color2;
};

layout(binding = 1) uniform sampler2D source;

// A pixel's colour as it is, not premultiplied by its opacity, with its opacity
vec4 straight(vec4 c)
{
    return c.a > 0.0 ? vec4(c.rgb / c.a, c.a) : vec4(0.0);
}

vec4 premultiplied(vec3 rgb, float a)
{
    return vec4(clamp(rgb, 0.0, 1.0) * a, a);
}

float luma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

// A number from 0 to 1 that looks random, the same for the same point
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// The width of the picture over its height, to keep what is round round
float aspect()
{
    return size.y > 0.0 ? size.x / size.y : 1.0;
}

// Pixels of this picture for each pixel of a 1080-line screen: an effect whose settings
// are sizes in pixels is the same, for the size of the picture, on the screen and in the
// small pictures of it in the operator's window and the inspector. (A GUESS that
// ProPresenter's pixels are a 1080p screen's.)
float perPixel()
{
    return max(size.y, 1.0) / 1080.0;
}

// A point of the picture, reflected back in where it is beyond an edge
vec2 mirrored(vec2 uv)
{
    return 1.0 - abs(1.0 - mod(uv, 2.0));
}

// Nothing where a point is beyond the picture's edges, which a texture would repeat
vec4 inside(vec2 uv)
{
    return uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0 ? vec4(0.0) : texture(source, uv);
}

void main()
{
    vec4 c = straight(texture(source, qt_TexCoord0));
    vec2 off = (qt_TexCoord0 - 0.5) * vec2(aspect(), 1.0);
    float glow = 0.0;
    for (int layer = 0; layer < 4; ++layer) {
        float n = float(layer);
        float depth = fract(n * 0.25 + time * values.x * 0.02);
        vec2 p = off * mix(24.0, 1.5, depth) + n * 7.3;
        vec2 cell = floor(p);
        vec2 at = (vec2(hash(cell + 1.7), hash(cell + 3.1)) - 0.5) * 0.7;
        float star = 1.0 - smoothstep(0.0, 0.03 + 0.07 * depth, length(fract(p) - 0.5 - at));
        glow += star * step(0.65, hash(cell)) * depth * (1.0 - smoothstep(0.85, 1.0, depth));
    }
    vec3 rgb = mix(c.rgb, c.rgb * 0.35 + vec3(glow), clamp(values.y, 0.0, 1.0));
    fragColor = premultiplied(rgb, max(c.a, clamp(glow * values.y, 0.0, 1.0))) * qt_Opacity;
}
