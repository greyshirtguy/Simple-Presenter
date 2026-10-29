#pragma once

#include <QByteArray>
#include <QString>

#include <optional>
#include <utility>

// What this computer knows of a font, found by its PostScript name, for naming it as
// PowerPoint does (Export to PowerPoint). A port of ProToPPTX's FontCatalog, with
// fontconfig where it had CoreText.
//
// Why: ProPresenter names a font by its PostScript name ("CMGSans-BoldCn",
// "HelveticaNeue-Bold"); PowerPoint names it by a family and two switches, bold and
// italic. The family PowerPoint (like every Office program) means is the font's "legacy"
// family, the one in name ID 1 of its name table, in which a family has at most four
// faces: regular, bold, italic and bold italic. Every other face is a family of its own
// there: "CMG Sans Bold Condensed" is family "CMG Sans Bold Cn" or the like, whatever its
// maker wrote. The family fontconfig and Qt go by is the newer, typographic one (name ID
// 16, "CMG Sans"), which PowerPoint would not find the right face by. So the name table
// is read here, from the font's file, for IDs 1 and 2.
//
// A font that is not installed here has no face; the caller falls back to what the
// presentation itself says of it. Every lookup is kept, and the keeping is guarded: the
// export runs on a thread of its own.

namespace pptx {

struct FontFace
{
    QString postScriptName;
    // The typographic family ("CMG Sans")
    QString family;
    // The full name ("CMG Sans Bold Condensed")
    QString fullName;
    // The family Office goes by (name ID 1)
    QString legacyFamily;
    // "Regular", "Bold", "Italic" or "Bold Italic" (name ID 2), as Office reads it
    QString legacySubfamily;
    // Its weight as fontconfig counts it (FC_WEIGHT: 80 regular, 200 bold, 210 black)
    int weight = 80;

    bool isBold() const;
    bool isItalic() const;
    // Whether the legacy names follow Office's rule of four faces to a family. Apple's
    // own fonts often do not: Helvetica Neue's name ID 2 says "Condensed Black" or
    // "Medium", which no family and switches can select.
    bool hasOfficeStyle() const;
    // The name PowerPoint finds this face by: its legacy family, or, when that does not
    // follow Office's rule, its full name ("Helvetica Neue Condensed Black", which
    // PowerPoint for the Mac lists as a font of its own)
    QString officeTypeface() const;
    // PowerPoint's bold and italic switches for the face, beside officeTypeface()
    bool officeBold() const;
    bool officeItalic() const;
};

// The installed font with exactly this PostScript name, if there is one.
std::optional<FontFace> installedFace(const QString &postScriptName);

// The face drawn for this font with RTF's bold and italic switches. KNOWN (ProToPPTX:
// macOS reads RTF's \b and \i so, and so does ProPresenter): the font itself if it is
// bold (italic) already or the switch is off, else its family's bold (italic) face, of
// the same width, if the family has one: "CMGSans-Regular" with \b is drawn in CMG Sans
// Bold. A face bold by its weight though its names do not say so to Office (a Black, a
// Heavy) is not made bolder. Empty if the font is not installed.
std::optional<FontFace> installedFace(const QString &postScriptName, bool bold, bool italic);

// Name IDs 1 and 2 of a name table: Windows' English names if it has them, else the
// Mac's. (Plain, for the tests.)
std::optional<std::pair<QString, QString>> legacyNames(const QByteArray &nameTable);
// The name table of the `index`th font in a font file (a .ttc holds several); empty if
// it has none, or the file is not one this can read.
QByteArray nameTable(const QString &fontFile, int index);

} // namespace pptx
