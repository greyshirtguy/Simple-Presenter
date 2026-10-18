#version 440

// Halftone: the media printed in dots of cyan, magenta, yellow and black.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Sharpness: how hard the dots' edges are
//   values.y  Gray Components: how much of the grey in each colour is printed in black
//             rather than in all three inks (0 to 2)
//   values.z  Color Removal: how much of the three inks is taken out under the black
// Each ink is a screen of dots, as large as there is to be of that ink, each screen at
// its own angle (cyan 15°, magenta 75°, yellow 0°, black 45°), as a printer has them.

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

float screen(float amount, float degrees, vec2 px, float softness)
{
    float a = radians(degrees);
    vec2 turned = mat2(cos(a), -sin(a), sin(a), cos(a)) * px;
    float cell = max(3.0, 7.0 * perPixel());
    vec2 local = (fract(turned / cell) - 0.5) * cell;
    float radius = sqrt(clamp(amount, 0.0, 1.0)) * cell * 0.7071;
    return 1.0 - smoothstep(radius - softness, radius + softness, length(local));
}

void main()
{
    vec4 c = straight(texture(source, qt_TexCoord0));
    vec3 cmy = 1.0 - c.rgb;
    float black = min(cmy.x, min(cmy.y, cmy.z)) * clamp(values.y * 0.5, 0.0, 1.0);
    cmy = clamp(cmy - black * clamp(values.z, 0.0, 1.0), 0.0, 1.0);
    vec2 px = qt_TexCoord0 * size;
    float softness = mix(1.6, 0.25, clamp(values.x, 0.0, 1.0));
    vec3 inks = vec3(screen(cmy.x, 15.0, px, softness), screen(cmy.y, 75.0, px, softness), screen(cmy.z, 0.0, px, softness));
    vec3 rgb = (1.0 - inks) * (1.0 - screen(black, 45.0, px, softness));
    fragColor = premultiplied(rgb, c.a) * qt_Opacity;
}
