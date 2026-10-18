#version 440

// Picasso: the media broken into facets, each a little out of place, as a cubist painting.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Animate: how fast the facets shift (0 they stay)
// The facets are the cells of a Voronoi pattern; each shows the media from a little to
// one side, and is shaded at its edges.

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

vec2 seedOf(vec2 cell)
{
    vec2 rand = vec2(hash(cell), hash(cell + 17.3));
    return cell + 0.5 + 0.4 * sin(time * values.x * 2.0 + rand * 6.2831853);
}

void main()
{
    vec2 p = qt_TexCoord0 * vec2(aspect(), 1.0) * 9.0;
    vec2 base = floor(p);
    float nearest = 10.0;
    float second = 10.0;
    vec2 owner = base;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 cell = base + vec2(float(x), float(y));
            float d = distance(p, seedOf(cell));
            if (d < nearest) {
                second = nearest;
                nearest = d;
                owner = cell;
            } else if (d < second) {
                second = d;
            }
        }
    }
    vec2 shift = (vec2(hash(owner + 3.1), hash(owner + 5.7)) - 0.5) * 0.05;
    vec4 c = straight(texture(source, mirrored(qt_TexCoord0 + shift)));
    float edge = smoothstep(0.0, 0.08, second - nearest);
    fragColor = premultiplied(c.rgb * (0.7 + 0.3 * edge), c.a) * qt_Opacity;
}
