#version 440

// The simplest transition, and the pattern for all of them.
//
// A transition is a fragment shader that is run for every pixel of a layer of the
// output while one thing gives way to another (see qml/TransitionLayer.qml). It is
// given
//   fromTex    what is going, as a texture
//   toTex      what is coming, as a texture
//   progress   how far along the transition is, 0 at the start and 1 at the end
// and answers with the colour of that pixel. Here that is a plain mix of the two.
//
// A shader may also declare any of these, in `buf` after `progress`. Each declares
// exactly the ones it uses.
//   float ratio       the layer's width over its height, to keep circles round
//   vec2 resolution   the layer's size in pixels
//   vec4 options      the numbers that can be adjusted about the transition, in the
//                     order qml/TransitionCatalogue.qml lists them
//   vec4 tint         the colour that can be adjusted about it: red, green, blue and
//                     opacity, not premultiplied
//   vec2 direction    the way things travel, where that can be chosen: each part -1, 0
//                     or 1, x to the right and y down
//
// Two things every transition has to get right. At 0 it must give exactly what is
// going, and at 1 exactly what is coming: the layer draws those directly before and
// after, and any difference shows as a jump. And both textures are transparent wherever
// their layer has nothing, with their colours premultiplied by that transparency, which
// has to stay true of what the shader puts out. A transition that moves a picture gives
// nothing beyond the picture's edges, where a texture would repeat its edge.
//
// To add a transition: write the shader, add it to qt_add_shaders in CMakeLists.txt,
// and add an entry for it to qml/TransitionCatalogue.qml. Nothing else needs to know.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;

void main()
{
    // Both textures are premultiplied, so a straight mix is a correct cross-fade.
    fragColor = mix(texture(fromTex, qt_TexCoord0), texture(toTex, qt_TexCoord0), progress) * qt_Opacity;
}
