#version 440

// Old Film: the media as a worn film shows it: brown, grainy, scratched, flickering and jumping.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Sepia Effect: how brown it is
//   values.y  Noise Level: how grainy
//   values.z  Scratches: how often a scratch runs down it
//   values.w  Vignette: how dark its corners are
//   more.x    Frequency: how often the flicker and the scratches change
//   more.y    Intensity: how far it jumps up and down (0 to 0.2)

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
    float frame = floor(time * (4.0 + more.x * 20.0));
    vec2 uv = qt_TexCoord0 + vec2(0.0, (hash(vec2(frame, 1.0)) - 0.5) * more.y * 0.3);
    vec4 c = straight(inside(uv));
    vec3 brown = vec3(dot(c.rgb, vec3(0.393, 0.769, 0.189)), dot(c.rgb, vec3(0.349, 0.686, 0.168)),
                      dot(c.rgb, vec3(0.272, 0.534, 0.131)));
    vec3 rgb = mix(c.rgb, brown, values.x);
    rgb *= 1.0 + (hash(vec2(frame, 7.0)) - 0.5) * 0.25;
    rgb += (hash(qt_TexCoord0 * size + frame) - 0.5) * values.y * 0.4;
    for (int i = 0; i < 3; ++i) {
        float n = float(i);
        if (hash(vec2(frame, n + 9.0)) < values.z * 0.6) {
            float x = hash(vec2(frame * 0.13 + n, 3.0));
            float across = abs(qt_TexCoord0.x - x) * size.x / perPixel();
            rgb = mix(rgb, vec3(0.85), (1.0 - smoothstep(0.0, 1.5, across)) * 0.7);
        }
    }
    rgb *= 1.0 - values.w * smoothstep(0.35, 0.9, length(qt_TexCoord0 - 0.5) * 1.35);
    fragColor = premultiplied(rgb, c.a) * qt_Opacity;
}
