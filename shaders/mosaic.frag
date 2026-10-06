#version 440

// Mosaic: the pictures cross-fade while breaking up into squares, which grow until half
// way, when there are eight across, and shrink back to nothing. `resolution` is the size
// of the picture in pixels.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    vec2 resolution;
};

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;
void main()
{
    vec2 size = resolution;
    // How big the squares are, in pixels: one pixel, which is no change, at either end.
    float squares = mix(1.0, size.x / 8.0, 1.0 - abs(2.0 * progress - 1.0));
    // They are laid out from the middle, on a pixel boundary so that at one pixel each
    // they are exactly the pixels.
    vec2 middle = floor(size / 2.0);
    vec2 pixel = qt_TexCoord0 * size;
    vec2 uv = (middle + (floor((pixel - middle) / squares) + 0.5) * squares) / size;
    fragColor = mix(texture(fromTex, uv), texture(toTex, uv), progress) * qt_Opacity;
}
