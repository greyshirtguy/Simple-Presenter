#include "transitionkinds.h"

namespace transitionkinds {

namespace {

// From ProPresenter's own descriptions of its transitions (see the header), in the order
// of their names. The least, most and initial value of a colour mean nothing; a colour
// starts at its own, which is kept as ProPresenter's file has it (red, green, blue and
// opacity), not as a QColor, which would keep it to sixteen bits and write it back a
// shade off.
const QList<Transition> transitions {
    {"B5E9AD29-DCEC-4AD2-A78D-3C49A101E1D2", "Amoeba", "Amoeba", "Wipes", 0.6, {
        {"Scale", "Size of the spread", Variable::Float, 10, 100, 40, {}, 0}}},
    {"8448FC67-6E70-4B7F-AC7A-A423952E69FC", "Blur Reveal", "Blur Reveal", "TextAnimation", 0.6, {
        {"Grouping", "How parts of text are grouped in the animation", Variable::Grouping, 0, 0, 1, {}, 15},
        {"Direction", "Order of the text animation", Variable::Direction, 0, 0, 3, {}, 56},
        {"Motion", "Motion of text after transitioning in", Variable::Motion, 0, 0, 1, {}, 31},
        {"Blur Bias", "Amount of blur", Variable::Float, 0.1, 1, 0.25, {}, 0},
        {"Ease In Out", "Ease in and out value", Variable::Float, 1, 8, 2, {}, 0},
        {"Motion Ramp", "How quickly motion goes to zero", Variable::Float, 1, 8, 4.5, {}, 0},
        {"Motion Speed", "Speed of motion", Variable::Float, 0.1, 2, 0.2, {}, 0}}},
    {"E33DBC2C-502B-4D1E-8C97-95B51D1E9E60", "Blur Reveal Random", "Blur Reveal Random", "TextAnimation", 0.6, {
        {"Grouping", "How parts of text are grouped in the animation", Variable::Grouping, 0, 0, 1, {}, 15},
        {"Motion", "Motion of text after transitioning in", Variable::Motion, 0, 0, 1, {}, 31},
        {"Blur Bias", "Amount of blur", Variable::Float, 0.1, 1, 0.25, {}, 0},
        {"Ease In Out", "Ease in and out value", Variable::Float, 1, 8, 2, {}, 0},
        {"Motion Ramp", "How quickly motion goes to zero", Variable::Float, 1, 8, 4.5, {}, 0},
        {"Motion Speed", "Speed of motion", Variable::Float, 0.1, 2, 0.2, {}, 0}}},
    {"2B855B65-6ABC-4F65-8198-A19A5EF97821", "Color Burn", "Color Burn", "Color", 0.6, {
        {"Burn Color", "color to burn through", Variable::Color, 0, 1, 0, {0.5f, 0.5f, 0.5f, 1}, 0}}},
    {"E958E7B0-E5C2-4C10-9B22-D1FE4FEA1E49", "Color Push", "Color Push", "Color", 0.6, {}},
    {"1EC1B05C-4E79-460C-9429-4129D9094E73", "Color Warp", "Color Warp", "Color", 0.6, {
        {"Zoom", "Adjust size", Variable::Float, 10.01, 40.1, 20.05, {}, 0},
        {"Size", "Adjust size", Variable::Float, 0.01, 1.1, 0.05, {}, 0},
        {"Color Separation", "Adjust color", Variable::Float, 0.01, 4.1, 0.85, {}, 0}}},
    {"5584E51F-5C92-47B5-9B65-3C2540C1F20C", "Cross Hatch", "Cross Hatch", "Objects", 0.6, {}},
    {"CD545FC3-70FA-4120-8A48-29EFE903BD10", "Cube", "Rotating Cube", "Objects", 0.6, {}},
    {"AB29D07B-E9E2-4E0A-93BD-AD3EA58120FA", "Cut", "Cut.", "None", 0.6, {}},
    {"F1475C78-329A-4897-B5C0-266C6433CCCC", "Dispersion Blur", "Dispersion Blur", "Blurs", 0.6, {
        {"Radius", "Adjust distortion", Variable::Float, 0, 2, 0.5, {}, 0},
        {"Angle", "Adjust distortion", Variable::Float, 0, 4, 2.3, {}, 0}}},
    {"EC52A828-AD85-4602-B70C-1DEE7C904DB6", "Dissolve", "Cross Dissolve.", "Dissolves", 0.6, {}},
    {"DAA798F1-359A-46C3-AEC6-5C3DFE93635A", "Door", "Door", "Objects", 0.6, {}},
    {"645A6350-128C-48B9-AA9D-D7D85ABEF81E", "Door Wipe", "Door wipe", "Objects", 0.6, {}},
    {"99BDD1C3-EE98-4E80-A8DE-3699CE9F338E", "Fade", "Fade", "Dissolves", 0.6, {}},
    {"01E95287-E84D-4638-9E28-18C8735ABE47", "Fade Black", "Fade Black", "Dissolves", 0.6, {}},
    {"802181F9-E290-4FA9-A9ED-06CAA2306A12", "Fade Bright", "Fade Bright", "Dissolves", 0.6, {}},
    {"E4D84DA7-E8E3-43B5-820E-8D883416D355", "Fade Dark", "Fade Dark", "Dissolves", 0.6, {}},
    {"C30699D4-3249-4B51-A809-191662294ED0", "Fade Gray", "Fade Gray", "Dissolves", 0.6, {}},
    {"D455B154-7036-4F88-8242-0D6B9B4F9418", "Fade Reveal", "Fade Reveal", "TextAnimation", 0.6, {
        {"Grouping", "How parts of text are grouped in the animation", Variable::Grouping, 0, 0, 1, {}, 15},
        {"Direction", "Order of the text animation", Variable::Direction, 0, 0, 3, {}, 56},
        {"Motion", "Motion of text after transitioning in", Variable::Motion, 0, 0, 1, {}, 31},
        {"Ease In Out", "Ease in and out value", Variable::Float, 1, 8, 2, {}, 0},
        {"Motion Ramp", "How quickly motion goes to zero", Variable::Float, 1, 8, 4.5, {}, 0},
        {"Motion Speed", "Speed of motion", Variable::Float, 0.1, 2, 0.2, {}, 0}}},
    {"C2123A39-357E-4626-9257-783AF40918FC", "Fade White", "Fade White", "Dissolves", 0.6, {}},
    {"2CB0361E-5E77-4E7F-AEDB-EA6673F743EF", "Film Burn", "Film Burn", "Color", 0.6, {}},
    {"82DE5438-1851-4011-929C-D59A292D9C79", "Flip", "Flip", "Movements", 0.6, {}},
    {"ADF02083-C3B8-4A47-BC9A-BA57A887C404", "Fly In", "Fly In", "Movements", 0.6, {
        {"Direction", "Direction of Fly In", Variable::Direction, 0, 0, 1, {}, 511}}},
    {"F694B97E-2C1C-4888-B6AE-0619617A16CF", "Iris", "Iris", "Movements", 0.6, {}},
    {"5717B340-8ED9-457C-A6EE-74A8D2679E43", "Kaleidoscope Wipe", "Kaleidoscope Wipe", "Objects", 0.6, {}},
    {"1F5074C8-6F81-45F6-ABC9-BF6EDBEDF1A2", "Mask Reveal", "Mask Reveal", "TextAnimation", 0.6, {
        {"Grouping", "How parts of text are grouped in the animation", Variable::Grouping, 0, 0, 1, {}, 15},
        {"Direction", "Direction of the reveal", Variable::Direction, 0, 0, 1, {}, 495},
        {"Ease In Out", "Ease in and out value", Variable::Float, 1, 8, 2, {}, 0}}},
    {"9BE0AB3A-9239-4976-BA34-417EADB27995", "Mosaic", "Mosaic", "Movements", 0.6, {}},
    {"31B698F6-500B-4097-B209-C6F811A99A7C", "Move In", "Move In", "Movements", 0.6, {
        {"Direction", "Direction of Move In", Variable::Direction, 0, 0, 1, {}, 495}}},
    {"F6AF1CC0-F049-40DC-926E-8CA350C2909D", "Noisy Zoom", "Noisy Zoom", "Blurs", 0.6, {}},
    {"B400F82E-CF9D-4700-89FD-F3889DAA06C5", "Push", "Push", "Movements", 0.6, {
        {"Direction", "Direction of Push", Variable::Direction, 0, 0, 2, {}, 170}}},
    {"DF2C1B6C-5B62-49A1-B72E-C80CB3C0CA75", "Random Pixels", "Random Pixels", "Objects", 0.6, {}},
    {"39030834-E94F-417E-A8B4-4870C9C7D413", "Random Squares", "Random Squares", "Objects", 0.6, {
        {"Size", "Square Size", Variable::Float, 1, 10, 2.5, {}, 0}}},
    {"E4650E1F-B42E-4492-9824-BE4D8E27E833", "Random Squares Flicker", "Random Squares Flicker", "Objects", 0.6, {}},
    {"3EA3BC78-9A06-480D-8E6E-3A51A47BD0D6", "Reveal", "Reveal", "Wipes", 0.6, {
        {"Direction", "Direction of Reveal", Variable::Direction, 0, 0, 1, {}, 495}}},
    {"47A0842A-E064-4282-854A-27224E48D7B0", "Ripple", "Ripple", "Movements", 0.6, {}},
    {"DF000B14-0F80-4A8B-86A5-903564032437", "Square Wipe", "Square Wipe", "Wipes", 0.6, {
        {"Direction", "Direction of Wipe", Variable::Direction, 0, 0, 1, {}, 495}}},
    {"6E087942-135D-4239-9093-98AD4BA86274", "Swap", "Swap", "Movements", 0.6, {}},
    {"72091328-AA13-4312-86FD-D54A5FC10A9C", "Warp Fade", "Warp Fade", "Dissolves", 0.6, {}},
    {"0540598C-5CED-4F80-AB0E-3566FEA85BCF", "Wave Dissolve", "Wave Dissolve", "Dissolves", 0.6, {}},
    {"7084A71F-C12C-4EEA-8D90-A2D30FB0B256", "Wipe", "Wipe", "Wipes", 0.6, {
        {"Direction", "Direction of Wipe", Variable::Direction, 0, 0, 1, {}, 495}}},
    {"4CD58DE3-5427-477E-A549-F88B09C025B8", "Zoom", "Zoom", "TextAnimation", 0.6, {
        {"Grouping", "How parts of text are grouped in the animation", Variable::Grouping, 0, 0, 1, {}, 15},
        {"Direction", "Order of the text animation", Variable::Direction, 0, 0, 3, {}, 56},
        {"Motion", "Motion of text after transitioning in", Variable::Motion, 0, 0, 1, {}, 31},
        {"Ease In Out", "Ease in and out value", Variable::Float, 1, 8, 2, {}, 0},
        {"Motion Ramp", "How quickly motion goes to zero", Variable::Float, 1, 8, 4.5, {}, 0},
        {"Motion Speed", "Speed of motion", Variable::Float, 0.1, 2, 0.2, {}, 0}}},
    {"FD363A7E-3204-46BD-B2DB-43F8BD842091", "Zoom Blur", "Zoom Blur", "TextAnimation", 0.6, {
        {"Grouping", "How parts of text are grouped in the animation", Variable::Grouping, 0, 0, 1, {}, 15},
        {"Direction", "Order of the text animation", Variable::Direction, 0, 0, 3, {}, 56},
        {"Motion", "Motion of text after transitioning in", Variable::Motion, 0, 0, 1, {}, 31},
        {"Blur Bias", "Amount of blur", Variable::Float, 0.1, 1, 0.25, {}, 0},
        {"Ease In Out", "Ease in and out value", Variable::Float, 1, 8, 2, {}, 0},
        {"Motion Ramp", "How quickly motion goes to zero", Variable::Float, 1, 8, 4.5, {}, 0},
        {"Motion Speed", "Speed of motion", Variable::Float, 0.1, 2, 0.2, {}, 0}}},
    {"928B442C-4912-42F6-B526-60921E0F0D40", "Zoom In", "Zoom In", "Movements", 0.6, {
        {"Direction", "Direction of Zoom In", Variable::Direction, 0, 0, 1, {}, 511}}},
};

} // namespace

const QList<Transition> &all()
{
    return transitions;
}

const Transition *find(const QString &renderId)
{
    for (const Transition &transition : transitions) {
        if (transition.renderId.compare(renderId, Qt::CaseInsensitive) == 0)
            return &transition;
    }
    return nullptr;
}

} // namespace transitionkinds
