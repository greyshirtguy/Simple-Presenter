#include "mediaeffects.h"

namespace mediaeffects {

namespace {

// From ProPresenter's own descriptions of its effects (see the header), in the order of
// their names. The least, most and initial value of a colour mean nothing; a colour
// starts at its own.
const QList<Effect> effects {
    {"D4C199A9-64D7-4CCB-8C26-443E7A9C5C03", "Adjust Color", "Filters based on color.", "adjustColor", false, {
        {"Hue", "Adjust hue", Variable::Float, -3.141592653589793, 3.141592653589793, 0, {}},
        {"Saturation", "Adjust saturation", Variable::Float, 0, 2, 1, {}},
        {"Brightness", "Adjust brightness", Variable::Float, -1, 1, 0, {}},
        {"Contrast", "Adjust contrast", Variable::Float, 0, 2, 1, {}}}},
    {"56C8A640-8BFF-4AFF-BBA5-23134683DB2F", "Alpha Adjust", "Adjusts the alpha value.", "alphaAdjust", false, {
        {"Amount", "Amount to multiply the alpha by", Variable::Float, 0, 1, 1, {}}}},
    {"F8907C4C-D768-41C1-8EBF-5B6AC07A4517", "Background Blur", "Gaussian Blur of the background image.", "", false, {
        {"Saturation", "mix between object and effect", Variable::Float, 0, 1, 0.5, {}},
        {"Intensity", "amount to blur by", Variable::Float, 0, 1, 0.5, {}}}},
    {"19CC0CED-E4E3-498C-9B10-D0D019D20E77", "Background Invert", "Invert of the background image.", "", false, {
        {"Intensity", "amount to blur by", Variable::Float, 0, 1, 0.5, {}}}},
    {"D84CE4A3-D1C3-4F7C-AA75-4C645A3621F2", "Blur", "Gaussian Blur of the image.", "blur", false, {
        {"Intensity", "amount to blur by", Variable::Float, 0, 1, 0.5, {}}}},
    {"0630A39C-69E9-4403-B428-3138939673A3", "Color Blobs", "Filters based on color.", "colorBlobs", true, {
        {"Blend Amount", "Amount to Blend", Variable::Float, 0.01, 1, 0.5, {}},
        {"Speed", "Movement velocity", Variable::Float, 0.01, 1, 0.5, {}}}},
    {"85A535EF-4EFC-4105-B8C7-C65C53376B87", "Color Filter", "Filters based on color.", "colorFilter", false, {
        {"Blend Color", "color to blend to", Variable::Color, 0, 1, 0, QColor::fromRgbF(1, 0, 0, 1)}}},
    {"EDF55F11-1A1E-4BF3-9718-FA2D3731626D", "Color Invert", "Applies a color negative filter.", "colorInvert", false, {}},
    {"469387A2-681A-458E-9414-DAB953C1564F", "Color Key", "Makes selected color clear.", "colorKey", false, {
        {"Color", "color to change", Variable::Color, 0, 1, 0, QColor::fromRgbF(0.5, 0.5, 0.5, 1)},
        {"Tolerance", "Amount to Blend", Variable::Float, 0.01, 1, 0.5, {}}}},
    {"F888F8FF-605A-4B2E-A20D-6FFA7C3E6E00", "Color Posterize", "Limits number of colors.", "colorPosterize", false, {
        {"Color Count", "Adjust number of colors", Variable::Float, 1, 14, 7, {}},
        {"Gamma", "Adjust Gamma", Variable::Float, 0.1, 1.1, 0.5, {}}}},
    {"7511AE15-3C2D-4840-BD66-DDC0C4A3DABE", "Color Swap", "Swaps one color for another.", "colorSwap", false, {
        {"Tolerance", "Amount to Blend", Variable::Float, 0.01, 1, 0.5, {}},
        {"Old Color", "color to change", Variable::Color, 0, 1, 0, QColor::fromRgbF(0.5, 0.5, 0.5, 1)},
        {"New Color", "color to change to", Variable::Color, 0, 1, 0, QColor::fromRgbF(0.5, 0.5, 0.5, 1)}}},
    {"0AD74569-93D8-4849-8E07-A05CCFB15620", "CRT Monitor", "Old CRT Monitor.", "crtMonitor", false, {
        {"Strength", "strength of RGB", Variable::Float, 0.5, 1.7, 1.2, {}},
        {"Size", "size of RGB pixel", Variable::Float, 1.5, 5, 3, {}}}},
    {"A6FD4AC9-6C4C-47C5-9FC0-AAD551C4642E", "Dots", "Blurs the edges.", "dots", false, {
        {"Intensity", "amount to blur by", Variable::Float, 0, 1, 0.5, {}},
        {"Color", "amount to blur by", Variable::Float, 0, 1, 0.5, {}}}},
    {"AF83B133-D6A1-487C-BD9E-8579FA49B4E0", "Edge Blur", "Blurs the edges.", "edgeBlur", false, {
        {"Amount", "amount to blur by", Variable::Float, 0, 1, 0.5, {}},
        {"Radius", "amount to blur by", Variable::Float, 0, 1, 0.5, {}}}},
    {"86BA6E3E-358F-4DA2-93C4-6A2C0EC831DA", "Edge Outline", "Blurs the edges.", "edgeOutline", false, {
        {"Intensity", "amount to blur by", Variable::Float, 0, 1, 0.5, {}},
        {"Color", "amount to blur by", Variable::Float, 0, 1, 0.5, {}}}},
    {"D2C1718A-571B-4844-BD72-7C13193E8F26", "Emboss", "Emboss effect.", "emboss", false, {
        {"Light X", "amount to blur by", Variable::Float, 0, 1, 0, {}},
        {"Light Y", "amount to blur by", Variable::Float, 0, 1, 1, {}}}},
    {"4FDA3849-C39D-4269-AADA-B963944900B7", "Gray Invert", "Inverts a gray scale image.", "grayInvert", false, {}},
    {"ADF80D5E-FD27-47BD-AFA6-8D4167364567", "Halftone", "Reprographic technique that simulates continuous tone imagery through the use of dots, varying either in size or in spacing, thus generating a gradient like effect.", "halftone", false, {
        {"Sharpness", "Radius to darken", Variable::Float, 0, 1, 0.5, {}},
        {"Gray Components", "Radius to darken", Variable::Float, 0, 2, 1, {}},
        {"Color Removal", "Radius to darken", Variable::Float, 0, 1, 0.5, {}}}},
    {"F3F254AF-BF83-412D-B802-51E42A25FDF9", "Heat Signature", "Simluates a thermal camera.", "heatSignature", false, {}},
    {"690831F6-A8A4-4B70-9AEB-E9A5E7FBDF63", "Hexagon", "Samples image to nearest Hexagon.", "hexagon", false, {
        {"Size", "amount to blur by", Variable::Float, 10, 100, 40, {}}}},
    {"8A0F0EB7-1F75-484B-8367-C390D94EA6C0", "Image Distortion", "Distorts as if looking through a bubble.", "imageDistortion", false, {
        {"Scale", "Adjust distortion", Variable::Float, 1, 2, 1.5, {}}}},
    {"17A42D1F-BC36-48D6-B9F6-110CB3AA93F9", "Kaleidoscope 1", "Filters based on color.", "kaleidoscope1", true, {
        {"Speed", "Speed of effect", Variable::Float, 0, 1, 0.5, {}},
        {"Sides", "Number of sides", Variable::Int, 2, 20, 7, {}},
        {"Center X", "Horizontal center", Variable::Float, -1, 1, 0, {}},
        {"Center Y", "Vertical center", Variable::Float, -1, 1, 0, {}}}},
    {"47BAC53A-761B-478D-AC74-2CA7A8CFB7D5", "Kaleidoscope 2", "Filters based on color.", "kaleidoscope2", true, {
        {"Speed", "Speed of effect", Variable::Float, 0, 1, 0.5, {}},
        {"Sides", "Number of sides", Variable::Int, 4, 20, 7, {}},
        {"Center X", "Horizontal center", Variable::Float, -1, 1, 0, {}},
        {"Center Y", "Vertical center", Variable::Float, -1, 1, 0, {}}}},
    {"291C7C20-07C5-42D2-92C9-45C173D69898", "Lens Distortion", "Distorts as if looking through big lens.", "lensDistortion", false, {
        {"Lens Size", "Adjust Lens", Variable::Float, 0, 2, 1, {}},
        {"Color Distortion", "Adjust Color distortion", Variable::Float, 0, 2, 1, {}}}},
    {"18CB8C8C-F23D-44B0-98BE-ECEB22B29D35", "Old Film", "Simulates distortions caused by old film projector.", "oldFilm", true, {
        {"Sepia Effect", "Radius to darken", Variable::Float, 0, 1, 0.5, {}},
        {"Noise Level", "Radius to darken", Variable::Float, 0, 1, 0.5, {}},
        {"Scratches", "Radius to darken", Variable::Float, 0, 1, 0.5, {}},
        {"Vignette", "Radius to darken", Variable::Float, 0, 1, 0.5, {}},
        {"Frequency", "Frequency of shake", Variable::Float, 0, 1, 0.5, {}},
        {"Intensity", "Frequency of shake", Variable::Float, 0, 0.2, 0.02, {}}}},
    {"9A5ED7A4-8339-43C2-BABB-180AC20D4C07", "Picasso", "Picasso effect.", "picasso", true, {
        {"Animate", "Speed of animation", Variable::Float, 0, 1, 0, {}}}},
    {"54E8E7F0-2302-4CBE-A63D-02D56915A79A", "Pixelize", "Increase pixel size to generate color blocks.", "pixelize", false, {
        {"Size", "Size of Pixel", Variable::Float, 0.01, 1, 0.5, {}}}},
    {"E19D17CE-03B7-4C58-BD56-FF79C6875651", "Plasma", "Mix Plasma with image.", "plasma", true, {
        {"Blend", "Amount to Blend", Variable::Float, 0.01, 1, 0.5, {}},
        {"Speed", "Amount to Blend", Variable::Float, 0.01, 1, 0.5, {}}}},
    {"6BC757C8-3F76-4E7E-885A-7A7C944465A5", "Radial Blur", "Mix Plasma with image.", "radialBlur", true, {
        {"Blend", "Amount to Blend", Variable::Float, 0.01, 1, 0.5, {}},
        {"Speed", "Amount to Blend", Variable::Float, 0.01, 1, 0.5, {}},
        {"Iterations", "Amount to Blend", Variable::Float, 4, 24, 10, {}},
        {"Decay", "Amount to Blend", Variable::Float, 0.8, 1.05, 0.97, {}}}},
    {"453B8725-AAB6-427A-8FCE-570B8AB52790", "RGB Adjust", "Filters based on color.", "rgbAdjust", false, {
        {"Red", "Adjust Red", Variable::Float, 0, 1, 1, {}},
        {"Green", "Adjust Green", Variable::Float, 0, 1, 1, {}},
        {"Blue", "Adjust Blue", Variable::Float, 0, 1, 1, {}}}},
    {"D0C7FA30-C4D6-4F6D-86D9-CA27099D56EC", "Ripple Center", "Ripple distortion from the center.", "rippleCenter", true, {
        {"Speed", "Speed of ripple", Variable::Float, 0, 2, 1, {}}}},
    {"2A34B3FB-0AC5-4C7D-B07E-05FE83D796B4", "Ripple Side", "Ripple distortion from the side.", "rippleSide", true, {
        {"Speed", "Speed of ripple", Variable::Float, 0, 2, 1, {}}}},
    {"06157C8E-D971-4F06-9ECA-2663664E22A6", "Sepia", "Simulates the Sepia tone.", "sepia", false, {}},
    {"5472C6A3-59A0-4ED5-AE13-0059E2D185C9", "Shake", "Random shake.", "shake", true, {
        {"Frequency", "Frequency of shake", Variable::Float, 0, 1, 0.5, {}},
        {"Intensity", "Frequency of shake", Variable::Float, 0, 1, 0.5, {}}}},
    {"55BB6AA1-9133-4CE4-991B-7BF5149E6834", "Stars", "Generates animated Star field.", "stars", true, {
        {"Speed", "Speed of animation", Variable::Float, 0, 20, 10, {}},
        {"Blend", "amount to blend by", Variable::Float, 0, 1, 0.5, {}}}},
    {"DA373B28-6AD6-499D-8AD4-11E2ADC73475", "Strobe", "Random strobe.", "strobe", true, {
        {"Frequency", "Frequency of strobe", Variable::Float, 0, 1, 0.5, {}},
        {"Length", "Time of strobe", Variable::Float, 0, 1, 0.05, {}},
        {"Blend", "Time of strobe", Variable::Float, 0, 1, 1, {}},
        {"Randomness", "Time of strobe", Variable::Float, 0, 1, 0, {}},
        {"Color", "color strobe", Variable::Color, 0, 1, 0, QColor::fromRgbF(1, 1, 1, 1)}}},
    {"4FF40112-9D47-43BA-A381-B685D13F5A89", "Tile", "Divides the images up into repeating tiles.", "tile", false, {
        {"Cells", "Number of divisions", Variable::Float, 1, 32, 2, {}},
        {"Size", "Size of grid", Variable::Int, 0, 20, 5, {}}}},
    {"3A7A9FFF-F027-4EFD-94CC-429A491485C7", "Toon", "Posterizes image yielding a cartoon effect.", "toon", false, {
        {"Number of colors", "Number of rows", Variable::Int, 1, 16, 8, {}},
        {"Saturation", "amount to blend by", Variable::Float, 0, 1, 0.5, {}}}},
    {"258D3830-8496-4712-AAAE-A68BCA23D805", "Two Color", "Linear interpolation between two colors based on brightness.", "twoColor", false, {
        {"Dark Color", "color for dark regions", Variable::Color, 0, 1, 0, QColor::fromRgbF(0, 0, 0, 1)},
        {"Bright Color", "color for bright regions", Variable::Color, 0, 1, 0, QColor::fromRgbF(1, 1, 1, 1)}}},
    {"42E83016-41DC-43AB-8061-9A93256DDA46", "Vignette", "Reduction of an image's brightness or saturation at the periphery.", "vignette", false, {
        {"Radius", "Radius to darken", Variable::Float, 0, 100, 50, {}}}},
};

} // namespace

const QList<Effect> &all()
{
    return effects;
}

const Effect *find(const QString &renderId)
{
    for (const Effect &effect : effects) {
        if (effect.renderId.compare(renderId, Qt::CaseInsensitive) == 0)
            return &effect;
    }
    return nullptr;
}

} // namespace mediaeffects
