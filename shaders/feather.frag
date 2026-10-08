#version 440

// Not a transition: this fades the edges of a slide element out, which ProPresenter
// calls feathering (see qml/SlideElement.qml).
//
// It is given the element's fill as a texture, and for every pixel works out how far
// inside the element's shape that pixel is. From the edge to `feather` pixels in, the
// fill goes from nothing to all there. The shape is one of the three whose edge can be
// told from a formula: a rectangle, a rectangle with round corners, an ellipse. For
// any other shape there is no feathering.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // 0 a rectangle, 1 one with round corners, 2 an ellipse
    float kind;
    // The radius of the round corners, and how far in the fading reaches, in pixels
    float corner;
    float feather;
    // The element's size in pixels
    vec2 extent;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec2 halfExtent = extent * 0.5;
    vec2 place = (qt_TexCoord0 - 0.5) * extent;
    float inside;
    if (kind > 1.5) {
        // Near enough for an ellipse: how far short of the edge, as a part of the way
        // out from the middle, taken along the shorter half
        inside = (1.0 - length(place / halfExtent)) * min(halfExtent.x, halfExtent.y);
    } else {
        float radius = kind > 0.5 ? corner : 0.0;
        vec2 beyond = abs(place) - halfExtent + radius;
        inside = radius - length(max(beyond, 0.0)) - min(max(beyond.x, beyond.y), 0.0);
    }
    // The texture's colours are premultiplied by its transparency, so fading is a
    // plain multiplication.
    fragColor = texture(source, qt_TexCoord0) * smoothstep(0.0, max(feather, 0.001), inside) * qt_Opacity;
}
