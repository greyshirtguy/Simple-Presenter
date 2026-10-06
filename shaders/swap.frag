#version 440

// Swap: the two pictures change places like two cards, what is going swinging out to one
// side and back into the distance while what is coming swings round the other side to the
// front.

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
    const float halfTurn = 3.14159265;
    float sideways = sin(halfTurn * progress);
    // How far back each is: what is coming from 5 to 0, what is going from 0 to 5.
    float comingBack = 2.5 * (1.0 + cos(halfTurn * progress));
    float goingBack = 5.0 - comingBack;
    vec4 coming = card(toTex, screen, vec3(-sideways, 0.0, -comingBack), -0.6 * (1.0 - progress))
                  * smoothstep(0.0, 0.3, progress);
    vec4 going = card(fromTex, screen, vec3(sideways, 0.0, -goingBack), 0.6 * progress)
                 * (1.0 - smoothstep(0.7, 1.0, progress));
    // Whichever is nearer is in front.
    fragColor = (comingBack < goingBack ? over(coming, going) : over(going, coming)) * qt_Opacity;
}
