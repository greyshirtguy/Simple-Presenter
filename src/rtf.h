#pragma once

#include "richtext.h"

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QString>

struct RtfDefaults
{
    // Used for text with no \cf, or \cf0.
    QColor textColor = Qt::black;
    // RTF font tables name fonts by PostScript name; the document often knows the family.
    QHash<QString, QString> familyForPostScriptName;
};

// Parses the subset of RTF that ProPresenter stores in slide text elements: font and
// colour tables (including Cocoa's expanded colour table), runs with font, size, bold,
// italic, fill colour and stroke, and paragraphs with alignment.
RichText parseRtf(const QByteArray &rtf, const RtfDefaults &defaults = {});
