#pragma once

#include "pptxdeck.h"

#include <QString>

#include <functional>

namespace rv::data {
class Presentation;
}

// A deck (pptxdeck.h) made into a ProPresenter presentation, with its media copied into
// the workspace: the last step of Import PowerPoint (pptxread.h reads the file into the
// deck). The other half of pptximport.h, which reads a presentation into a deck for the
// export; what that reads, this writes, by the same functions the editor writes with
// (proconvert.h), so that a presentation made here is one the editor could have made.
//
// What goes where, slide by slide (the deck's slides back to front become ProPresenter's
// elements front to back, as its files list them):
//
//   - The slide's background colour, where PowerPoint's slide (or its layout, or its
//     master) has one, is the ProPresenter slide's own background colour, drawn over the
//     whole slide; a gradient becomes a rectangle over the whole slide at the back; and a
//     picture or a video becomes the cue's background media, on the media layer under
//     the slide (the maintainer's choice, 2026-11-09), which is what the export turns a
//     background media cue into, going the other way. With `leaveOutBackgrounds` none of
//     the three is kept, as ProPresenter 22's import offers (its words for it are not
//     known; A GUESS that it leaves colours out as well as pictures).
//   - A shape is an element of the shape's outline (a rectangle, a rounded rectangle, an
//     ellipse, or the points of anything else, straight-sided), with its fill, its line
//     and its shadow; a shape with words is a text element as well. A picture is an
//     element filled with its file, cropped and stretched as PowerPoint had it; a video
//     the same, playing once or looping.
//   - Words are written as ProPresenter keeps them (RTF, see proconvert::writeText), run
//     by run: font, size, bold, italic, underline, strike, colour, letter spacing,
//     superscript, capitals, and an outline. A bulleted paragraph gets its marker written
//     into its words, since ProPresenter's own text has no lists this app draws: said in
//     the notes. PowerPoint's space before and after a paragraph is not kept.
//   - The box's insets become the element's margins, less the 5 units ProPresenter keeps
//     clear beyond its margins (KNOWN, ProToPPTX: measured); and the top and bottom
//     margins are moved together so that the first line falls where PowerPoint draws it,
//     the reverse of what the export does (pptxwriter.cpp, TextFit: PowerPoint's first
//     baseline is 0.9 x the size x the line spacing down, KNOWN from ProToPPTX's
//     measurements of PowerPoint for the Mac; this app's own layout says where
//     ProPresenter's is). Only where PowerPoint was given a line spacing: given none it
//     goes by the font's metrics, as ProPresenter does, and the two are close.
//   - PowerPoint's sections become groups, by name; a deck with no sections has one
//     group with no name, which shows as plain slides (as a song's unnamed part does,
//     songimport.h). A hidden slide is a disabled one, since neither is shown (A GUESS
//     at what ProPresenter's import does with them). Speaker notes become slide notes.
//
// The slide is 1920 x 1080, as every presentation this app makes is. A deck of another
// shape is fitted inside it and centred, a 4:3 deck with 240 units clear each side,
// black like the output where nothing is drawn.
//
// Media goes into <workspace>/Media/Imported/<presentation name>/, by the names the
// pictures have in the file ("image1.png"). That ProPresenter's own import puts media
// under Media/Imported is A GUESS from the empty folder of that name in the maintainer's
// workspace; the folder for each presentation is this app's own tidiness.
//
// A GUESS, and untested: that ProPresenter opens a presentation made here. Every part of
// it is written by the functions the editor writes with, which ProPresenter has opened
// the output of; none made from a PowerPoint file has been opened in it yet.
namespace pptx {

struct MakeOptions
{
    // Leave out the slides' backgrounds: colours, gradients and pictures alike
    bool leaveOutBackgrounds = false;
};

// The presentation as a message, its media written into `mediaFolder` (which is made if
// it is not there) and named relative to `workspace`. `name` is the presentation's. Notes
// about what could not be kept go to `notes`. On failure the error is set and the message
// is not to be used.
rv::data::Presentation buildPresentation(const Deck &deck, const QString &name, const QString &workspace, const QString &mediaFolder,
                                         const MakeOptions &options, NoteCollector *notes, QString *error);

struct Made
{
    // The file written, and the name the presentation got (the one asked for, or that
    // with a number after it where the library had one of the name already)
    QString path;
    QString name;
    int slides = 0;
    QString error;
};

// Makes the presentation `name` of the deck in the library folder `library` of
// `workspace`, its media under Media/Imported/<name>/, and writes it. `progress` is told
// as the media are copied; returning false from it stops the work, and nothing is left
// behind.
Made makePresentation(const Deck &deck, const QString &name, const QString &library, const QString &workspace, const MakeOptions &options,
                      NoteCollector *notes, const std::function<bool(int done, int total)> &progress = {});

} // namespace pptx
