#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Formats put on the text of many slides at once, as ProPresenter's Text button in its
// toolbar puts them: its popover's font, style, colour, capitals and size; alignment in
// the line and in the box; outline and shadow (TextFormatPanel, which shows them, and
// ProDocument's formatCuesText, which goes through each slide's text boxes and their runs
// with these). Plain rules on maps, with nothing of the files or the windows in them
// (tests/unit/tst_textformat).
//
// A format is a map of what is to change, and nothing else changes: a run or a box that
// has a value already is not given it again, so that a format that changes nothing of a
// box leaves the file as it was, byte for byte (a family given again would have its font
// looked up again, and the name written may differ; a shadow turned off where there is
// none would leave an empty one in the file).
//
//   family, style (one of styles()), color, capitalization, size, strokeColor,
//     strokeWidth: put on every run as they are;
//   sizeBy: points added to every run's own size, never below one point. It is what the
//     size's arrows do, so that each run keeps its size against the others, a point
//     larger or smaller: the popover leaves its size box empty while sizes differ, as
//     ProPresenter's does, and the arrows still step (how far a step goes there is a
//     GUESS: one point);
//   strokeOn: off takes the outline off every run; on gives one to each run without,
//     three hundredths of its size wide (as the editor does), and leaves the others; a
//     stroke colour alone, while a run has none, gives it one too, as the editor does;
//   strokeWidthBy: added to the width of each run's outline, never below half a point,
//     for the runs that have one (the others are left without);
//   alignment: a horizontal Qt::Alignment for every paragraph;
//   and for the box (proconvert::applyChanges): verticalAlignment, textShadowEnabled,
//     textShadowRadius (ProPresenter's Blur), textShadowOpacity (0 to 1),
//     textShadowSolidColor (its colour, its opacity as it was: which also turns the
//     shadow on, as the editor's colour does), textShadowAngle, textShadowOffset; and
//     textShadowRadiusBy, textShadowOpacityBy, textShadowAngleBy and textShadowOffsetBy,
//     added to each box's own (the blur and the offset never below nothing, the opacity
//     kept from 0 to 1, the angle turned round past 360), whether its shadow is on or not.
//
// The ...By ones are what the arrows beside the popover's numbers do, as the size's do: a
// step of what each has, which is a step of the number shown where all agree. (That
// ProPresenter's arrows step each one's own where they differ is a GUESS, but for the
// size's, which the maintainer said.)
namespace textformat {

// The faces the popover's style menu offers, and which a run is. The app knows a face
// by bold and italic, so these four are all there is: a Light or a Medium face is shown
// as Regular. (ProPresenter's menu likely lists each family's own faces, as a Mac's font
// menus do; that has not been seen.)
QStringList styles();
QString styleOf(bool bold, bool italic);

// The change to make to one run, whose format is `run` (as TextRun::format gives it), as
// TextRun::applyFormat takes it: the run's keys of `format` that differ from what it has,
// the relative ones made absolute for this run. Empty when there is nothing to change.
QVariantMap forRun(const QVariantMap &format, const QVariantMap &run);

// The alignment `format` puts on every paragraph, or -1 for none
int alignmentOf(const QVariantMap &format);

// The change to make to one box, whose settings are `box` (as proconvert describes an
// element: verticalAlignment and the textShadow ones), as proconvert::applyChanges takes
// it: the box's keys of `format` that differ from what it has. Empty when there is
// nothing to change.
QVariantMap forBox(const QVariantMap &format, const QVariantMap &box);

// What the text of many boxes has in common, gathered a run and a box at a time: a key
// is kept while every one so far has had the same value for it, and dropped for good at
// the first that differs, which is how the popover knows to leave a control empty.
class Common
{
public:
    // A run's format (TextRun::format), with `alignment`, its paragraph's
    void addRun(const QVariantMap &run);
    // A box's settings as proconvert describes an element (verticalAlignment and the
    // textShadow ones); each box once
    void addBox(const QVariantMap &box);
    // What every one agreed on, of family, style, size, color, capitalization,
    // alignment, strokeOn, strokeWidth, strokeColor, verticalAlignment, shadowOn,
    // shadowBlur, shadowOpacity, shadowColor, shadowAngle and shadowOffset; and `boxes`,
    // how many boxes there were
    QVariantMap values() const;

private:
    void add(const QString &key, const QVariant &value);

    QVariantMap m_values;
    QStringList m_mixed;
    int m_boxes = 0;
};

} // namespace textformat
