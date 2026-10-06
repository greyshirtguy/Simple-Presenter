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
// Shaders that draw shapes may also declare `float ratio`, the layer's width over its
// height, to keep circles round; each shader declares in `buf` exactly the uniforms it
// uses, after the two that Qt always passes.
//
// To add a transition: write the shader, add it to qt_add_shaders in CMakeLists.txt,
// and add a line for it to `transitions` in qml/Main.qml. Nothing else needs to know.
//
// Both textures are transparent wherever their layer has nothing, and their colours
// are premultiplied by that transparency, which a transition has to keep true of what
// it puts out.

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
