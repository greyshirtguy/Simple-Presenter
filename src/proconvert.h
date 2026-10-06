#pragma once

#include "richtext.h"

#include "presentation.pb.h"

#include <QColor>
#include <QVariantMap>

// Conversion between ProPresenter's slide messages and what the rest of the app works
// with. Shared by the reader used for showing presentations and by the editor.
//
// Reading goes one way: a `Slide` message becomes a map (toSlideMap), which is all that
// QML ever sees of a slide. The map is flat and complete on purpose. Whatever has to be
// worked out is worked out here, once: which elements show (an element can be hidden,
// or shown only when another has text), and what text each shows (an element can show
// another's text in its own style). The views then only draw what they are given, and
// the output, the thumbnails and the editor cannot disagree about it.
//
// Writing goes the other way, and is never a whole slide: applyChanges() and writeText()
// alter fields of the message that was read, in place, and nothing else in it. That is
// the rule for every file this app writes: what it does not understand, it does not
// touch, so ProPresenter finds its own work as it left it.
namespace proconvert {

// Reads a presentation file. On failure returns false and sets *error.
bool readPresentation(const QString &path, rv::data::Presentation *presentation, QString *error);
// Writes one. Everything in the message, including fields this app does not know about,
// is written back as it was read. The file is replaced in one step, so a failure part
// way through leaves the original untouched. Returns an error message, empty on success.
QString writePresentation(const QString &path, const rv::data::Presentation &presentation);
QColor toColor(const rv::data::Color &color);
void setColor(rv::data::Color *target, const QColor &color);

// The text of a text element: its RTF, together with the capitalisation that the file
// keeps beside the RTF because RTF cannot express it.
RichText readText(const rv::data::Graphics::Text &text);

// Replaces the text of a text element. The RTF is rewritten, and so are the attributes
// the file keeps beside it, which describe the start of the text, and the ranges that
// carry capitalisation and the families of fonts. Ranges of kinds this app does not
// understand are kept if only the format is changing, and dropped if the characters
// are, since they are tied to character positions that the new text no longer has.
void writeText(rv::data::Graphics::Text *text, const RichText &rich);

// A slide as the QML side consumes it: size, background, label, plain text, and a list
// of elements. Each element is a map of
//   id, name, x, y, width, height, rotation, opacity, locked, hidden
//   fillOn, fillKind ("color", "gradient", "media", "other" or "none"), fillColor, and
//     fillEnabled (on, and a plain colour, which is the kind that is drawn)
//   fillLinesOnly (the fill is only behind the lines of the text) and how: lineMaskStyle
//     (0 the box's width, 1 each line's, 2 the widest line's), lineMaskWidthOffset,
//     lineMaskHeightOffset, lineMaskHorizontalOffset, lineMaskVerticalOffset
//   strokeOn, strokeColor, strokeWidth, and strokeEnabled (on, and wider than nothing)
//   shadow... and textShadow...: Enabled, Color (as drawn, opacity included), Angle,
//     Offset, Radius, and worked out from those OffsetX and OffsetY
//   text (RichText, as authored), verticalAlignment, marginLeft/Top/Right/Bottom,
//   textScale (whether the text's size is changed to suit its box, as the file has it:
//     0 no, 1 the box's height suits the text instead, which is not done here, 2 made
//     smaller if it does not fit, 3 made larger if there is room, 4 either; see
//     StrokedText),
//     textTransform
//   linkKind ("none", "element", "timer" or "other"): where the element's text comes
//     from, if it is not its own. For another element of the slide, linkElementId,
//     linkElementName and linkTransform. For a timer, linkTimerId and linkTimerName
//     (it is found by id, or failing that by name); linkTimerHours, linkTimerMinutes,
//     linkTimerSeconds and linkTimerHundredths (how each part of the time is written,
//     as Timers::Style), linkTimerHundredthsUnderMinute (the hundredths only show in
//     the last minute) and linkTimerPattern (text with "${timer}" where the time
//     goes). For anything else, linkLabel names it
//   visibilityRules (bool), visibilityCriterion (0 all, 1 any, 2 none),
//     visibilityTimed (one of the conditions is about a timer, so whether the element
//     shows is not settled until it is drawn: see Slide.qml),
//     visibilityConditions: a list of { kind: "element", elementId, elementName, hasText }
//     and, for conditions on things this app does not track, { kind: "other", index,
//     label }
// and, worked out for the slide as it stands,
//   displayText (RichText: the text actually shown, after linking and transforming),
//   hasText, and visible (false if hidden or if its visibility rules say so).
// All geometry is in slide units.
//
// An element linked to a timer shows something that is not in the slide at all, and
// that changes while the slide is on show. So its displayText only stands in for it
// (the timer at nothing, in the element's own style); what draws the element asks for
// the text as it is at the moment (SlideElement.qml). Stage layouts will be slides
// whose text boxes are linked the same way to more such things (the words of the live
// slide and of the next one, the clock), each of which is another linkKind handled in
// those same two places.
QVariantMap toSlideMap(const rv::data::Slide &slide, const QString &label);

// Changes an element of a slide. `changes` holds new values under the element map's
// keys; these can be changed:
//   x, y, width, height, opacity, name, locked, hidden
//   fillOn, fillColor (which also makes the fill a plain colour), fillLinesOnly
//   strokeOn, strokeColor, strokeWidth
//   shadowEnabled, shadowColor, shadowAngle, shadowOffset, shadowRadius, and the same
//     for textShadow...
//   verticalAlignment, textScale, marginLeft, marginTop, marginRight, marginBottom
//   linkKind ("none", "element" or "timer"), linkElementId, linkTransform,
//     linkTimerId, linkTimerName, linkTimerHours, linkTimerMinutes, linkTimerSeconds,
//     linkTimerHundredths
//   visibilityRules, visibilityCriterion, visibilityConditions (conditions of kind
//     "other" are kept as they were, by their index)
// Renaming an element also renames it where other elements of the slide refer to it.
// Returns false if the slide has no such element.
bool applyChanges(rv::data::Slide *slide, const QString &elementId, const QVariantMap &changes);

// A new text box for a slide, in the middle of it, with a name no other element of the
// slide has. Its text is set in the format of `like`'s, if given, so that it matches
// the text already on the slide.
rv::data::Slide::Element makeTextElement(const rv::data::Slide &slide, const rv::data::Slide::Element *like);

// A name for a new element: `base`, or `base` and the first number that makes it one
// no element of the slide has.
QString uniqueElementName(const rv::data::Slide &slide, const QString &base);

} // namespace proconvert
