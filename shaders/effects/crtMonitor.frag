#version 440

// CRT Monitor: the media as an old tube television shows it.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Strength: how bright the stripes of red, green and blue are (0.5 to 1.7)
//   values.y  Size: how wide each stripe is, and how far apart the lines, in pixels (of
//             a 1080-line screen, as all the sizes of these effects are)
// The glass a little bowed; behind it, stripes of the three colours side by side, and
// the dark between the lines it is drawn in.

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
    vec2 centred = qt_TexCoord0 * 2.0 - 1.0;
    centred *= 1.0 + 0.04 * dot(centred.yx, centred.yx);
    vec2 uv = centred * 0.5 + 0.5;
    vec4 c = straight(inside(uv));
    float stripeSize = max(values.y * perPixel(), 1.0);
    vec2 px = uv * size;
    float stripe = mod(floor(px.x / stripeSize), 3.0);
    vec3 mask = stripe < 0.5 ? vec3(1.0, 0.25, 0.25) : stripe < 1.5 ? vec3(0.25, 1.0, 0.25) : vec3(0.25, 0.25, 1.0);
    float line = 0.6 + 0.4 * sin(px.y / stripeSize * 3.14159265);
    fragColor = premultiplied(c.rgb * mask * line * values.x * 1.25, c.a) * qt_Opacity;
}
