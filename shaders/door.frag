#version 440

// Door: what is going parts down the middle and its halves slide off to the sides like a
// pair of doors, while what is coming is brought forward through the opening, fading in.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
// The picture at uv, and nothing beyond its edges. An edge fades over one pixel, so that
// one which is moving, or at an angle, is not jagged.
vec4 picture(sampler2D tex, vec2 uv)
{
    vec2 pixel = max(fwidth(uv), vec2(1e-6));
    vec2 cover = clamp(uv / pixel + 0.5, 0.0, 1.0) * clamp((1.0 - uv) / pixel + 0.5, 0.0, 1.0);
    return texture(tex, clamp(uv, 0.0, 1.0)) * (cover.x * cover.y);
}

// How far the eye is from the screen, in the units below.
const float eye = 2.4;

// A picture as a card in space, seen in perspective. At rest the card is the screen: its
// middle at the origin, reaching from -1 to 1 across and down, with the eye `eye` in
// front of it; z is towards the eye. `middle` is where the card's middle is, and `turn`
// how far it is turned about its upright axis, in radians. `screen` is this pixel, from
// -1 to 1 each way. Gives the picture where the card covers the pixel, and nothing
// elsewhere.
vec4 card(sampler2D tex, vec2 screen, vec3 middle, float turn)
{
    float c = cos(turn);
    float s = sin(turn);
    // A point u across the card is at (middle.x + u c, middle.z - u s), and is seen on the
    // screen at its x times eye / (eye - z). Solved for u:
    float u = (eye * middle.x - screen.x * (eye - middle.z)) / (screen.x * s - eye * c);
    float away = eye - middle.z + u * s;
    if (away <= 0.0)
        return vec4(0.0);
    float v = screen.y * away / eye - middle.y;
    return picture(tex, vec2(u, v) * 0.5 + 0.5);
}

// One picture over another.
vec4 over(vec4 top, vec4 under)
{
    return top + under * (1.0 - top.a);
}

void main()
{
    vec2 screen = qt_TexCoord0 * 2.0 - 1.0;
    float open = 0.5 * progress;
    float pixel = max(fwidth(qt_TexCoord0.x), 1e-6);
    // Each door is the half of the picture on its side of the middle, moved outwards.
    vec2 leftUv = qt_TexCoord0 + vec2(open, 0.0);
    vec2 rightUv = qt_TexCoord0 - vec2(open, 0.0);
    vec4 doors = picture(fromTex, leftUv) * clamp((0.5 - leftUv.x) / pixel + 0.5, 0.0, 1.0)
               + picture(fromTex, rightUv) * clamp((rightUv.x - 0.5) / pixel + 0.5, 0.0, 1.0);
    vec4 coming = card(toTex, screen, vec3(0.0, 0.0, -3.0 * (1.0 - progress)), 0.0) * progress;
    fragColor = over(doors, coming) * qt_Opacity;
}
