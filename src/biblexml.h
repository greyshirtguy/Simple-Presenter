#pragma once

#include "bible.h"

#include <QByteArray>
#include <QString>

// A Bible read from an XML file the person has: the four kinds of file that Bibles are
// most often to be had in for presentation software, the same four that FreeShow
// imports (its converters were looked at for which kinds there are; nothing of its
// code is here, FreeShow being under the GPL and this app under the MIT licence).
//
//   Zefania    <XMLBIBLE biblename=…><BIBLEBOOK bnumber="1" bname="Genesis">
//              <CHAPTER cnumber="1"><VERS vnumber="1">…</VERS>
//   OSIS       <osis><osisText><div type="book" osisID="Gen"><chapter osisID="Gen.1">
//              <verse osisID="Gen.1.1">…</verse>, or the verses as milestones,
//              <verse sID="Gen.1.1"/>…<verse eID="Gen.1.1"/>, which many files have
//   Beblia     <bible translation=…><testament><book number="1"><chapter number="1">
//              <verse number="1">…</verse>  (the "Holy Bible XML" files)
//   OpenSong   <bible><b n="Genesis"><c n="1"><v n="1">…</v>
//
// Of a verse, its words are kept, with runs of white space made one space. Notes,
// cross references, Strong's numbers' tags (their words are kept), headings and the
// like are left out. Red letters are kept where the file says them: OSIS's <q
// who="Jesus">, Zefania's <STYLE> in red.
//
// Why the app does not come with any: a Bible's words are its translators' and
// publishers', and most translations are copyright. The person brings the file, and
// what it is is theirs to answer for; it is kept, as it came, with the app's own data
// (Bibles), and is never put in a workspace.
namespace biblexml {

enum class Format { Unknown, Zefania, Osis, Beblia, OpenSong };

// What kind of file it is, by its first elements
Format detect(const QByteArray &xml);
QString formatName(Format format);

// Reads a whole file. False, with *error saying why, for a file that is none of the four
// or has no verses in it. `fallbackName` is what to call a Bible whose file names none.
bool read(const QByteArray &xml, const QString &fallbackName, bible::Bible *bible, QString *error, Format *format = nullptr);

}
