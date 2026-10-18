#version 440

// Color Blobs: soft patches of colour drifting about over the media.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Blend Amount: how strongly they colour it
//   values.y  Speed: how fast they drift
// Five round patches, each its own colour, going about on slow curves of their own; where
// they meet their colours run together.

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
    float wide = aspect();
    vec2 p = qt_TexCoord0 * vec2(wide, 1.0);
    float t = time * (0.15 + values.y * 1.5);
    float field = 0.0;
    vec3 tint = vec3(0.0);
    for (int i = 0; i < 5; ++i) {
        float n = float(i);
        vec2 centre = vec2(wide * (0.5 + 0.42 * sin(t * (0.31 + 0.07 * n) + n * 1.7)),
                           0.5 + 0.42 * cos(t * (0.23 + 0.05 * n) + n * 2.3));
        vec2 off = p - centre;
        float weight = exp(-dot(off, off) * 7.0);
        field += weight;
        tint += weight * (0.5 + 0.5 * cos(6.2831853 * (n / 5.0 + vec3(0.0, 0.33, 0.67))));
    }
    tint /= max(field, 0.0001);
    float amount = clamp(field, 0.0, 1.0) * values.x;
    fragColor = premultiplied(mix(c.rgb, c.rgb * tint * 1.4 + tint * 0.2, amount), c.a) * qt_Opacity;
}
