#pragma once

#include <QString>

struct ResolvedFont
{
    QString family;
    bool bold = false;
    bool italic = false;
};

// ProPresenter names fonts by PostScript name ("HelveticaNeue-Bold"). Looks the name up
// in fontconfig; if no installed font has it, falls back to `familyHint` when given, or
// to a family guessed from the name, and leaves substitution to fontconfig.
ResolvedFont resolvePostScriptName(const QString &postScriptName, const QString &familyHint = {});

// The PostScript name to write for a font chosen by family and style: that of the
// installed face if there is one, otherwise a name made up the way such names usually
// are ("Helvetica Neue" bold -> "HelveticaNeue-Bold").
QString postScriptNameFor(const QString &family, bool bold, bool italic);

// The PostScript name for the same font as `postScriptName` in another style. The font
// may well not be installed here, so this goes by the installed faces if it can and
// otherwise swaps the style part of the name.
QString restyledPostScriptName(const QString &postScriptName, const QString &family, bool bold, bool italic);
