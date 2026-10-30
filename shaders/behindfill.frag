#version 440

// Not a transition: what is behind a slide element, drawn through a mask, for ProPresenter's
// fills of letters and of shapes that are a blur or an invert of what is behind them
// (qml/TextFills.qml, src/textfills.h).
//
// `source` is the part of what is under the slide that the element covers, with nothing
// where nothing is under it. It is laid over `base`, the colour under everything (black, as
// a screen is where nothing is drawn), so that an invert of nothing is white, as an invert
// of a black screen is; and under `over`, the slide's own background colour, premultiplied
// (none where it has none), which is drawn over the media; then inverted, for an invert;
// then given its saturation, a multiple of its colours' own (1 as they are). It shows where
// `mask` does: the letters, or the shape. (A blur is drawn by Qt's MultiEffect instead.)

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // 1 to invert the colours, 0 not to
    float invert;
    // A multiple of the colours' own saturation
    float saturation;
    vec4 base;
    vec4 over;
};

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D mask;

void main()
{
    vec4 behind = texture(source, qt_TexCoord0);
    // (Premultiplied: what is not there lets the base through)
    vec3 colour = behind.rgb + (1.0 - behind.a) * base.rgb;
    colour = over.rgb + (1.0 - over.a) * colour;
    colour = mix(colour, vec3(1.0) - colour, invert);
    float grey = dot(colour, vec3(0.2126, 0.7152, 0.0722));
    colour = clamp(mix(vec3(grey), colour, saturation), 0.0, 1.0);
    float shown = texture(mask, qt_TexCoord0).a;
    fragColor = vec4(colour, 1.0) * shown * qt_Opacity;
}
