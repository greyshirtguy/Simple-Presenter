#pragma once

#include "pptxdeck.h"

#include <QString>
#include <QStringList>

#include <functional>

// A deck (pptxdeck.h) written out as a PowerPoint file (.pptx, Office Open XML), for
// Export to PowerPoint. A port of ProToPPTX's PPTXWriter, with its SlideXML, TextFit,
// FixedParts, MediaRegistry and Units, which say what they are where they are below.
//
// The file is a zip of XML parts and media (pptxzip.h). Which parts there are, and how
// they point to each other, is fixed by the standard; the parts are written as text, by
// hand, rather than through a DOM: each is small, its shape is fixed by the standard, and
// a string can be read in a test as it will be in the file. Everything that comes from
// a presentation (words, names, file names) is escaped, which also drops the characters
// XML 1.0 cannot hold at all. The file is written beside where it is to go and moved over
// it when it is whole, so a file that was there before is never left half written.
//
// Nothing on a slide uses the theme: every colour and font is written out on the slide
// itself, so the deck looks the same whatever theme PowerPoint has to hand. Sections
// (PowerPoint's way of grouping slides, where ProPresenter's groups go) are an extension
// PowerPoint 2010 added; PowerPoint and Keynote read them, and anything that does not
// simply ignores them.
//
// One thing is added to ProToPPTX's writing: a picture can fill the letters of a run
// (a:blipFill in its a:rPr), which DrawingML allows and PowerPoint draws ("Text Fill:
// Picture"). It is what a blur or invert of what is behind the words becomes
// (pptximport.h). That PowerPoint stretches such a picture over the text's box, as it
// does a shape's, is A GUESS; the pictures it is used for are blurred, so a little out
// of place they look the same. (LibreOffice draws such letters in their colour.)

namespace pptx {

// Writes the deck to `path`, replacing what is there. What could not be put in is added
// to `notes`. `progress`, if given, is told how far it has got (the slides written, of
// the slides and the media), and the writing stops if it says false.
bool writeDeck(const Deck &deck, const QString &path, const Options &options, NoteCollector *notes, QString *error,
               const std::function<bool(int done, int total)> &progress = {});

// What is out of the ranges the standard gives the values written ("a:pPr marL=-100"),
// in one part: ProToPPTX's OOXMLLint, for the tests. A value out of its range makes a
// part that is good XML but not valid PresentationML, and PowerPoint then offers to
// "repair" the whole deck (ProToPPTX, 2026-10-19: RTF's \li-100 written as a negative
// left margin).
QStringList outOfRange(const QString &xml);

} // namespace pptx
