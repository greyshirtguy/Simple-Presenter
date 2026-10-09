#pragma once

#include <QByteArray>
#include <QString>

// Reading NDI's own installer, so that the app can fetch NDI's library for whoever
// wants it (see Ndi in ndi.h, which does the fetching, and NdiSetup.qml, which asks).
//
// NDI's library is not part of this app and cannot be shipped with it: it is NDI's, and
// whoever has it has it under NDI's licence, which they have to have agreed to
// themselves. What NDI gives out for Linux is a shell script with two things in it: the
// text of that licence, which it shows and asks to have agreed to, and after a line
// that marks the place, an archive of the SDK, the library among it. The app does what
// the script does, by its own hand: shows the licence, and only if it is agreed to
// takes the library out of the archive.
//
// These are plain functions, with nothing of the app behind them, so that they can be
// tested by themselves (tests/unit/tst_ndisetup.cpp).
namespace ndisetup {

struct Installer
{
    // The licence the script shows, as text
    QString licence;
    // Where in the script the archive starts (a gzipped tar), or -1
    qsizetype archiveAt = -1;

    bool understood() const { return !licence.trimmed().isEmpty() && archiveAt > 0; }
};

// What is in an installer script; one that is not laid out as NDI's is, is not understood.
Installer read(const QByteArray &script);

// The folder of the SDK's `lib` that has the library for a processor, as Qt names
// processors ("x86_64", "arm64", "i386"); "" for one NDI has none for.
QString libraryFolder(const QString &processor);

}
