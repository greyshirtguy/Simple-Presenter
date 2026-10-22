#version 440

// Not a transition: this draws a text box's Scrolling Text (see qml/ScrollingText.qml),
// its words going across the box, or up or down it, once or over and over, feathered at
// the two ends of the box along the way they go.
//
// It is drawn over the box, and given the box's words as a texture, drawn once, as long
// as they are along the way they go. For every pixel of the box it works out which going
// of the words is there, if any, and where in the words that is: the rule of
// textscroller::positions (src/textscroller.h, with its unit test), which this follows,
// worked backwards from the pixel. So a box costs one pass over it a frame, however many
// goings of its words are in it, and nothing is drawn but this: only `travelled` changes
// from one frame to the next.
//
// All lengths are in pixels, along the way the words go, as if they went towards the
// start of the box (left, or up); words going right or down are that the other way round.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // 1 for words that go across, 0 up or down; 1 for right or down
    float across;
    float backwards;
    // 1 for over and over
    float repeating;
    // The box along the way the words go; the words; one going and the gap after it
    float room;
    float words;
    float cycle;
    // Where the words started from (the far end of the box, for Off-Screen), how far
    // they have gone, and how far past the words' own length their texture reaches at
    // each end (strokes and shadows: SlideElement's textBleed)
    float begin;
    float travelled;
    float bleed;
    // The box and the words' texture across the way the words go, and how far the
    // texture reaches past the box's edges that way
    float span;
    float spanTexture;
    // The feathering at the start and the end of the box, as shares of it
    float fadeStart;
    float fadeEnd;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    float along = across > 0.5 ? qt_TexCoord0.x : qt_TexCoord0.y;
    float athwart = across > 0.5 ? qt_TexCoord0.y : qt_TexCoord0.x;
    // Where the pixel is along the box, from its start, the other way round for right
    // and down
    float c = (backwards > 0.5 ? 1.0 - along : along) * room;
    // How far into the goings of the words: a going's near edge is at
    // begin + i * cycle - travelled
    float u = c - begin + travelled;
    float i = repeating > 0.5 ? floor((u + bleed) / cycle) : 0.0;
    float local = u - i * cycle;
    // (Before the first going there is nothing: words starting off the box that have
    // not yet come in)
    float shown = i >= 0.0 && local > -bleed && local < words + bleed ? 1.0 : 0.0;
    if (backwards > 0.5)
        local = words - local;
    float s = (local + bleed) / (words + 2.0 * bleed);
    float t = (athwart * span + (spanTexture - span) * 0.5) / spanTexture;
    vec2 at = across > 0.5 ? vec2(s, t) : vec2(t, s);
    float feather = smoothstep(0.0, max(fadeStart, 0.0001), along) * smoothstep(0.0, max(fadeEnd, 0.0001), 1.0 - along);
    // The texture's colours are premultiplied by its transparency, so fading is a plain
    // multiplication.
    fragColor = texture(source, clamp(at, 0.0, 1.0)) * shown * feather * qt_Opacity;
}
