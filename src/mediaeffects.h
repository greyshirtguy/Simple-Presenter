#pragma once

#include <QColor>
#include <QList>
#include <QString>

// The effects a piece of media can be given in its Media Inspector, as ProPresenter
// has them: their names, what each says it does, and what can be adjusted about each,
// with the least, the most and the value it starts at.
//
// Those are ProPresenter's, taken from the descriptions that come with it (an .rvfx
// file for each), so that an effect is known here by the name and the settings it has
// there, and an effect a file was given in ProPresenter is found here by the id the
// file names it by (its `render_id`). The drawing is not ProPresenter's: each effect is
// drawn by a shader written for this app to give the same kind of change
// (shaders/effects/, which says how), so they are equivalents and will not match
// ProPresenter's pixel for pixel. Two of ProPresenter's work on what is behind the
// media, not on the media itself (Background Blur and Background Invert); those are
// known, so that a file that has them keeps them, but are not drawn here.
namespace mediaeffects {

struct Variable
{
    // What the file and the inspector call it, and what it says it is for
    QString name;
    QString description;
    // A whole number, a number, or a colour
    enum Kind { Int, Float, Color } kind = Float;
    double least = 0;
    double most = 1;
    // What it is when the effect is added
    double initial = 0;
    QColor initialColor;
};

struct Effect
{
    // The id ProPresenter knows the effect by: a file's `render_id`
    QString renderId;
    QString name;
    QString description;
    // The shader that draws it here, by its name under shaders/effects/ ("" for one that
    // is not drawn here); "blur" is drawn by Qt's own blur, not a shader of this app's.
    QString shader;
    // Whether it moves by itself, and so is drawn again at every frame
    bool animated = false;
    QList<Variable> variables;
};

// Every effect, in the order of their names, which is how ProPresenter lists them
const QList<Effect> &all();
// The effect with that id, or null for one this app does not know
const Effect *find(const QString &renderId);

} // namespace mediaeffects
