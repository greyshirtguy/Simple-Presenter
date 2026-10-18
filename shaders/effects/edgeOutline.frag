#version 440

// Edge Outline: the edges of what is in the media drawn as lines.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Intensity: from the media as it is (0) to the lines alone (1)
//   values.y  Color: the lines white (0) or in the media's own colours (1)
// The edges are where the lightness changes fast, found by a Sobel filter.

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

float at(vec2 offset)
{
    return luma(texture(source, qt_TexCoord0 + offset / size).rgb);
}

void main()
{
    vec4 c = straight(texture(source, qt_TexCoord0));
    float tl = at(vec2(-1.0, -1.0)), t = at(vec2(0.0, -1.0)), tr = at(vec2(1.0, -1.0));
    float l = at(vec2(-1.0, 0.0)), r = at(vec2(1.0, 0.0));
    float bl = at(vec2(-1.0, 1.0)), b = at(vec2(0.0, 1.0)), br = at(vec2(1.0, 1.0));
    float gx = tr + 2.0 * r + br - tl - 2.0 * l - bl;
    float gy = bl + 2.0 * b + br - tl - 2.0 * t - tr;
    float edge = clamp(length(vec2(gx, gy)) * 1.5, 0.0, 1.0);
    vec3 line = mix(vec3(1.0), c.rgb, clamp(values.y, 0.0, 1.0)) * edge;
    fragColor = premultiplied(mix(c.rgb, line, clamp(values.x, 0.0, 1.0)), c.a) * qt_Opacity;
}
