#pragma once

#include "chords.h"

#include <QList>
#include <QString>

// A song as the lyrics file SongSelect gives out has it.
//
// CCLI's SongSelect lets a church download a song's words as a plain text file (the
// "lyrics" download, "<Title>-lyrics.txt"), and the same words can be copied from its
// page. The file is laid out like this (from the one the maintainer downloaded,
// 2026-11-07):
//
//   the title, on the first line
//   a blank line
//   a label ("Verse 1", "Chorus", "Interlude 2", "Bridge") on a line of its own,
//   then that part's lines of words, then a blank line; and so on for each part
//   the authors, on one line with commas between them
//   "CCLI Song #" and the song's number
//   "©", the year, and the publishers with semicolons between them
//   a line saying the words are for use under the SongSelect terms
//   "CCLI License #" and the church's own licence number
//
// This reads one into the parts and the details a presentation is made from (see
// songimport.h): each labelled block is a group of that name, a block with no label a
// group with none (a verse, no doubt, but naming it would be a guess), and the
// details go where ProPresenter keeps a song's for the copyright display
// (Presentation.ccli, see ccli.h): the authors as one line as ProPresenter's own files
// from SongSelect have them ("Brett Younker, Karl Martin, ..."), the publishers one a
// line as they have them, the year, and the song's number. The church's licence number
// is the workspace's, not the song's, and is left out.
//
// GUESSES, this being read from one file and the usual names of a song's parts: the
// labels a block may start with (`isLabel`); that the year goes in the year field alone
// and not at the start of the publisher as some of ProPresenter's own files from
// SongSelect have it (the copyright display has a place of its own for the year, and
// would otherwise show it twice); and that a block with a label and nothing under it
// is left out, there being no words for a slide. ProPresenter's own import of these
// files has not been watched.
namespace songselect {

struct Song
{
    QString title;
    // "Nobody Inparticular, Another Nobody": as the file has them
    QString author;
    // The publishers, one a line
    QString publisher;
    // Of the copyright line; 0 where it has none
    int year = 0;
    // As written, "7097216"; empty where there is none
    QString songNumber;
    // The parts, in the file's order, each with its name ("" for none) and lines
    QList<chords::Section> sections;
};

// Reads the text of such a file, or of its page copied. A song with no sections is one
// with no words in it. Line endings of any kind, and a mark at the start, are taken in
// their stride.
Song parse(const QString &text);

// Whether a line is a part's label: one of the usual names of a part, by itself or
// with a number or a letter after it ("Verse", "Verse 2", "Chorus 1a", "Pre-Chorus",
// "Interlude 1", "Tag"), whatever its case
bool isLabel(const QString &line);

}
