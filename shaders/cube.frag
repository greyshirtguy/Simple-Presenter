#version 440

// Cube: the picture is one face of a cube that turns a quarter of the way round about its
// upright axis, taking what is going off to one side and bringing what is coming round
// from the other on the next face.

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

// The face of the cube that has been turned `turn` from facing the eye. The cube's
// middle is one behind the screen, so a face is one from it in whichever way it faces.
// A face is only seen from outside the cube: once it has turned so far that the eye is
// behind it, it is hidden by the rest of the cube.
vec4 face(sampler2D tex, vec2 screen, float turn)
{
    if (cos(turn) * (eye + 1.0) <= 1.0)
        return vec4(0.0);
    return card(tex, screen, vec3(sin(turn), 0.0, cos(turn) - 1.0), turn);
}

void main()
{
    vec2 screen = qt_TexCoord0 * 2.0 - 1.0;
    const float quarterTurn = 1.57079633;
    float turn = quarterTurn * progress;
    // The two faces meet along an edge and never overlap.
    fragColor = (face(fromTex, screen, turn) + face(toTex, screen, turn - quarterTurn)) * qt_Opacity;
}
