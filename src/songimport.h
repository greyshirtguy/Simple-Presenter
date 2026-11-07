#pragma once

#include "chords.h"

#include "presentation.pb.h"

#include <QString>

// A new presentation made from a song in a text file: a ChordPro file, or the lyrics
// file SongSelect gives out (or its words copied).
//
// ChordPro is the plain-text way of writing a song with its chords: the words, with
// each chord in square brackets where it is played, and lines in curly brackets that
// say what the song is and where its verses and choruses start. chords::parseSong
// reads one. SongSelect's file is the words alone, each part under its label, with the
// song's details at the foot; songselect::parse reads one. This makes a presentation
// of what was read, as ProPresenter would have written it had it imported the song
// itself, as far as that can be told:
//   - a group for each part of the song (Verse 1, Chorus, ...), by name, as the
//     Multitracks imports to hand have them: a name and an id and no colour, the
//     colour being the workspace's for a group of that name;
//   - a slide for every so many lines of a part (two unless asked otherwise), each
//     with one text box named "Lyrics" over the whole slide, its words centred in
//     white, and its chords over them (see proconvert::readChords);
//   - the song's key, as the key its chords are written in and are shown in;
//   - its title and the rest of its details where ProPresenter keeps a song's (the
//     CCLI block, see ccli.h), as far as the file gives them.
// A song from SongSelect can be dressed in a theme slide as it comes in (importLyrics),
// before its file is first written, so that there is no earlier version to keep.
// What is NOT set, on purpose: the block that says a song came from Multitracks and is
// licensed by them. A song typed or imported by hand is not theirs.
//
// A GUESS, and untested: that ProPresenter opens a presentation made here. It starts
// as every new presentation here does (proconvert::newPresentation), and every field
// ProPresenter writes for an empty slide is written (proconvert::addBlankCue and
// makeTextElement are what the editor has always added slides and text boxes with),
// but none has been opened in ProPresenter yet.
namespace songimport {

// Reads a text file as a song's: UTF-8 as nearly all are, or Latin-1 for one that is
// not. Empty, with *error set, if it cannot be read or is too large to be a song.
QString readTextFile(const QString &path, QString *error);

// Reads a ChordPro file from disk. An empty song, with *error set, if it cannot be read
// or has no words in it.
chords::Song readFile(const QString &path, QString *error);

// A song's details, for the CCLI block: each left out of the file where it is empty.
// `display` is whether the copyright display shows the song's, which ProPresenter's
// own files from SongSelect have on.
struct Credits
{
    QString title;
    QString artist;
    QString author;
    QString publisher;
    int year = 0;
    QString songNumber;
    bool display = false;
};

// The presentation for a song's parts. `name` is what it is called; `linesPerSlide`
// how many lines of words go on a slide; `key` the key its chords are in, or "".
rv::data::Presentation build(const QList<chords::Section> &sections, const QString &name, int linesPerSlide, const Credits &credits,
                             const QString &key);
// The same for a ChordPro song: its title, artist, copyright and number are its credits.
rv::data::Presentation build(const chords::Song &song, const QString &name, int linesPerSlide);

// Reads `file` and writes the presentation into the library folder `library`, under
// the song's title (or the file's name, for a song that has none), with a number after
// it if there is one of that name already. Gives the new file's path. Returns an error
// message, empty on success.
QString importFile(const QString &file, const QString &library, int linesPerSlide, QString *made);

// Makes a presentation of SongSelect lyrics `text` (see songselect.h) in the library
// folder `library`, under the song's title (or "Song", for one with none), with a
// number after it if there is one of that name already; with `themePlace` named, every
// slide is dressed in that theme's slide `themeSlideId` (see themefile.h) before the
// file is written. `workspace` is the workspace folder, where the themes are. Gives
// the new file's path. Returns an error message, empty on success.
QString importLyrics(const QString &text, const QString &library, int linesPerSlide, const QString &workspace, const QString &themePlace,
                     const QString &themeSlideId, QString *made);

}
