#pragma once

#include "pptxdeck.h"

#include <QSizeF>
#include <QString>
#include <QTemporaryDir>

#include <functional>
#include <memory>

// A PowerPoint file (.pptx) read into a deck (pptxdeck.h): for Import PowerPoint. The
// other half of the export, which writes a deck as a .pptx (pptxwriter.h): this reads
// one back, from any maker, as nearly as the deck can hold what PowerPoint has.
//
// What a .pptx is: a zip of XML parts that point to each other by relationships
// (ppt/presentation.xml lists the slides; each slide names its layout, its pictures and
// its notes; a layout names its master, and a master its theme). The parts are read into
// small trees (pptxxml.h) and walked. What a shape looks like is settled the way
// PowerPoint settles it, from the most particular to the most general: what the shape
// itself says, then the layout's placeholder it stands in, then the master's, then the
// master's text styles, then the presentation's default text style; a colour named by
// the theme ("accent1", "tx1") is looked up through the colour map and the theme, with
// its lightening and darkening applied. KNOWN here means the standard says so (ECMA-376
// Part 1, DrawingML and PresentationML); A GUESS is marked where this reads PowerPoint's
// behaviour into something the standard leaves open.
//
// Units: the deck's, which are ProPresenter's slide units, 1080 to the slide's height
// (pptxdeck.h). PowerPoint's EMUs (914400 to the inch) are scaled by the slide's own
// height, so a 4:3 deck comes out 1440 wide. A font size is PowerPoint's points scaled the
// same way (a 7.5-inch slide, as every PowerPoint slide is, makes a point two units).
//
// Text is laid out by PowerPoint otherwise than by ProPresenter (pptxwriter.cpp, TextFit,
// has what ProToPPTX measured). The deck says what PowerPoint shows: a paragraph's line
// spacing, given as a percentage, is turned into the multiple of the font's own line
// height that puts ProPresenter's lines the same distance apart (1.2 x the size x the
// percentage, which is PowerPoint's pitch for any font, KNOWN from those measurements),
// and the text that PowerPoint shrank to fit its box ("shrink text on overflow", with
// the factor written in the file) is given at the size PowerPoint drew it. Where the
// first line falls is pptxmake's to settle, from the same measurements, when it places
// ProPresenter's box.
//
// What is left out, and said in the notes: tables, charts, SmartArt, embedded objects,
// connectors and lines, sound, slide transitions and animations (every object is shown
// as it is once its animations are over), text set vertically, and shapes of kinds this
// has no outline for, which are drawn as rectangles. Pictures and videos are taken out of
// the file into a folder of their own (`scratch`), for pptxmake to copy into the
// workspace; the deck's media name those files.
namespace pptx {

struct ReadResult
{
    Deck deck;
    QList<Note> notes;
    // Empty when the deck was read; otherwise why not
    QString error;
    // The folder the file's pictures and videos were taken out into, which goes when the
    // result does
    std::shared_ptr<QTemporaryDir> scratch;
};

// Reads the whole file. `progress` is told after each slide, with how many of them there
// are; returning false from it stops the reading (the result's error then says so).
ReadResult readDeck(const QString &path, const std::function<bool(int done, int total)> &progress = {});

// What a file holds, at a glance, from its list of slides alone: for saying what is about
// to be imported before it is.
struct Glance
{
    QString name;
    int slides = 0;
    // The slide's size in EMUs' proportions, as a size in deck units (1920 x 1080 for a
    // 16:9 deck)
    QSizeF size;
    QString error;
};
Glance glanceAt(const QString &path);

} // namespace pptx
