#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

// Font names as Qt, fontconfig and ProPresenter each give them, and how they are put
// side by side. Plain rules on names, with nothing of the fonts themselves in them
// (tests/unit/tst_fontnames); fontresolver.h does the looking up.
//
// A font is one face of a family ("CMG Sans" Light), and the three name it differently.
// ProPresenter names a run's face by its PostScript name ("CMGSans-Light"), with the
// family and the face's style name beside it in a text box's own record of its font.
// Fontconfig, which finds the fonts installed here, knows a face by all three, and a
// font file may give its face more than one family ("CMG Sans" and "CMG Sans Light"),
// each with a style of its own ("Light" and "Regular"). Qt lists a family by its name,
// except where it has the family from more than one foundry, the maker named inside
// each file: then it lists the family once for each, as "CMG Sans [ULA ]" and
// "CMG Sans [PYRS]" (the foundry exactly as fontconfig gives it, spaces and all), and
// never as "CMG Sans", though asking it for "CMG Sans" finds the first foundry's faces.
// A face of the other foundry (CMG Sans's condensed ones) is found only by asking for
// the family under that foundry's name. (Found with the maintainer's fonts, 2026-10-27:
// the editor called "CMG Sans" missing, and once wrote "CMG Sans [ULA ]" into a file.)
namespace fontnames {

// A family as its fonts name it: Qt's "CMG Sans [ULA ]" as "CMG Sans"; any other name as
// it is
QString plain(const QString &qtFamily);

// The families Qt lists, for a menu: by their own names, each once, in Qt's order
QStringList plainFamilies(const QStringList &qtFamilies);

// What to ask Qt for to have a face of `family` made by `foundry`: the family under the
// foundry's name where Qt lists it so (`qtFamilies`, its list), and otherwise the family
QString qtFamily(const QString &family, const QString &foundry, const QSet<QString> &qtFamilies);

// A style name as two of them are compared: case, spaces and hyphens taken out, so that
// "Bold Italic", "BoldItalic" and "bold-italic" are the same
QString styleKey(const QString &style);

// Whether a style is one that bold and italic say by themselves: Regular, Bold, Italic,
// Bold Italic (and their other names: Book, Roman, Oblique)
bool saidByBoldAndItalic(const QString &style);

// The style part of a PostScript name, after its last hyphen ("CMGSans-LightItalic" ->
// "LightItalic"), or empty for a name with none
QString postScriptStyle(const QString &postScriptName);

} // namespace fontnames
