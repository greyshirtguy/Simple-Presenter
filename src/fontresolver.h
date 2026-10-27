#pragma once

#include <QString>

// Finding here the fonts a ProPresenter file names, and naming them back as it does.
//
// ProPresenter names a run's face by its PostScript name ("CMGSans-Light"), in the RTF
// of its words, and keeps the family ("CMG Sans") and the face's style name beside it in
// the text box's record of its font (fontnames.h says how the names differ). The face is
// looked up in fontconfig by its PostScript name, and drawn as that very face: Qt is
// asked for its family, under its foundry's name where Qt lists the family once for each
// foundry, and for its style by name, since a family's faces are more than bold and
// italic (Light, Medium, Black, Condensed, CAPS, Rounded). Where the name is not
// installed here, the family is looked for, and in it a face whose style is the style
// part of the name ("Light" of "CMGSans-Light"), for a file whose maker's fonts have
// other PostScript names than the ones here. That ProPresenter goes by the family and
// the style in the same way, where it has not got the PostScript name, is a GUESS (it
// keeps both, and its Windows fonts go by family and style); the ProPresenter files
// looked at name their faces by PostScript name, with the style name left empty. Failing
// that, the family alone, bold and italic, and fontconfig's own substitute for it.
//
// Every lookup is kept, once for each name, and the keeping is guarded: the search reads
// presentations on a thread of its own as the windows read them.

struct ResolvedFont
{
    // The family as fontconfig names the face's (its first family: "CMG Sans"), or the
    // one guessed for a face not installed here
    QString family;
    bool bold = false;
    bool italic = false;
    // Whether the face itself is installed, and then what to ask Qt for to have it: the
    // family as Qt lists it ("CMG Sans [PYRS]" for one of two foundries) and the face's
    // style name ("Light", "Bold Condensed")
    bool installed = false;
    QString qtFamily;
    QString style;
};

// ProPresenter names fonts by PostScript name ("HelveticaNeue-Bold"). Looks the name up
// in fontconfig, then the family (`familyHint`, or one guessed from the name) and a face
// of it of the name's style; and failing those gives the family, bold and italic, for
// fontconfig to put another font in its place.
ResolvedFont resolvePostScriptName(const QString &postScriptName, const QString &familyHint = {});

// The PostScript name to write for a font chosen by family and style: that of the
// installed face if there is one, otherwise a name made up the way such names usually
// are ("Helvetica Neue" bold -> "HelveticaNeue-Bold"). A family as Qt lists it ("CMG Sans
// [ULA ]") is taken by its own name.
QString postScriptNameFor(const QString &family, bool bold, bool italic);

// The PostScript name for the same font as `postScriptName` in another style. The font
// may well not be installed here, so this goes by the installed faces if it can and
// otherwise swaps the style part of the name.
QString restyledPostScriptName(const QString &postScriptName, const QString &family, bool bold, bool italic);
