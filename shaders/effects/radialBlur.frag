#version 440

// Radial Blur: the media blurred outwards from its middle, as if rushing towards it.
//
// An effect a piece of media can be given in its Media Inspector, under ProPresenter's
// name for it, with ProPresenter's settings (src/mediaeffects.h). Written for this app to
// give the same kind of change, not taken from ProPresenter, so it will not match
// ProPresenter's pixel for pixel. What every effect is given is said in
// qml/MediaEffects.qml.
//
//   values.x  Blend: how much of the blur there is
//   values.y  Speed: how fast the blur's reach pulses, between seven tenths of it and all
//             of it (0 it stays at its longest)
//   values.z  Iterations: how many samples make it (4 to 24)
//   values.w  Decay: how much less each sample counts than the one before (0.8 to 1.05)

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
    float pulse = values.y > 0.0 ? 0.85 + 0.15 * sin(time * values.y * 3.0) : 1.0;
    float samples = clamp(values.z, 1.0, 24.0);
    vec2 step = (qt_TexCoord0 - 0.5) * 0.5 * pulse / samples;
    vec2 uv = qt_TexCoord0;
    vec4 sum = vec4(0.0);
    float weight = 1.0;
    float total = 0.0;
    for (int i = 0; i < 24; ++i) {
        if (float(i) >= samples)
            break;
        sum += texture(source, uv) * weight;
        total += weight;
        weight *= values.w;
        uv -= step;
    }
    fragColor = mix(texture(source, qt_TexCoord0), sum / max(total, 0.0001), clamp(values.x, 0.0, 1.0)) * qt_Opacity;
}
