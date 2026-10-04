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
