#pragma once

#include <QList>
#include <QString>

// Which text box of a slide goes into which text box of a theme.
//
// A theme slide is a slide built to show how slides should look. Dressing a slide in it
// means pouring the slide's words into the theme's text boxes, so the first thing to
// settle is which box's words go where. ProPresenter does not say how it decides; this
// is how it is decided here, in order:
//
//   1. By name. A text box of the slide goes into the theme's text box of the same name
//      (whatever its capitals), which is how themes with a "Verse" and a "Reference"
//      are meant to work.
//   2. By size. Of what is left, the largest box of the slide goes into the largest of
//      the theme, the next into the next, and so on. A theme moves boxes about (that is
//      what it is for), so where a box is says little; but the box with the main words
//      in it is the big one in the slide and in the theme alike.
//   3. By order. Boxes much of a size (within a tenth of each other's area) are taken in
//      the order the slide and the theme have them.
//
// Boxes left over on either side are matched with nothing: a theme's box that gets no
// words is left empty, and a slide's words that have no box in the theme stay as they
// are, since words are never thrown away.
//
// Plain values in, plain values out, so that this can be tested by itself
// (tests/unit/tst_thememath.cpp); themefile.h does the dressing.
namespace thememath {

struct Box
{
    QString name;
    double width = 0;
    double height = 0;
};

// For each of the theme's text boxes, in the theme's order, which of the slide's goes
// into it: its place in `slide`, or -1 for none.
QList<int> match(const QList<Box> &slide, const QList<Box> &theme);

}
