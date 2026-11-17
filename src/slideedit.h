#pragma once

#include "extendedapijson.h"

#include "slide.pb.h"

#include <QString>

// What is on a slide changed as the extended API asks (extendedapi.h): an element
// added, changed, taken off or put elsewhere in the order they are drawn in; the words
// of a slide's text box typed over; its background. Plain functions on a slide as the
// file has it, which is the same thing in a presentation and in a theme, with nothing
// of the files or the network in them (tests/unit/tst_slideedit).
//
// They are the editor's own changes, made the way the editor makes them
// (PresentationEditor: the same functions of proconvert, in the same order), for a
// caller that has no window: so that a text box added through the API is the text box
// the Text button adds, and a property changed is changed as the inspector changes it.
// Only the fields a change is about are altered; all else on the slide is as it was.
namespace slideedit {

// Which of a slide's elements is meant: by its id, failing that by its name, failing
// that by its number from 0, which is the one furthest back. -1 for none.
int indexOf(const rv::data::Slide &slide, const QString &which);

// Adds an element, in front of the others: a text box (the kind without one said), a
// shape ("rectangle", "roundedRectangle", "ellipse", "arrow") or a picture or video
// ("media", with its file), and then gives it what else `wanted` says, as change()
// does. A new text box says "Text", as the editor's does, until it is given words: a
// box with words is what a theme takes for one of its text boxes. `*id` is the new
// element's. Returns an error message, empty on success; on an error the slide is as it
// was.
QString add(rv::data::Slide *slide, const extendedapijson::ElementChanges &wanted, QString *id);

// Changes the element at `index`: its properties (proconvert::applyChanges), its text
// anew (`paragraphs`), its words typed over (`text`, which keeps how they are set and
// the chords over words that stay: reflow::retyped), a format put on all of it, its
// chords, and its place in the order. On an error the slide is as it was.
QString change(rv::data::Slide *slide, int index, const extendedapijson::ElementChanges &changes);

// Takes the element at `index` off, and out of the order the slide's builds run in
void remove(rv::data::Slide *slide, int index);

// The slide's text box for its words, as the reflow editor and the chord editors find
// one: the first element with words of its own, or failing that the first with a text
// that has none; -1 for a slide with no text box.
int wordsBox(const rv::data::Slide &slide);

// Makes the slide say these words: typed over those of its words' box, keeping how they
// are set. A slide with no text box is given one first: a copy of `like` (another
// slide's box of words, for it to look as that one does), or with none a box over the
// whole slide but for a margin, named "Lyrics", its words centred in white, which is
// what the song imports here make.
void setWords(rv::data::Slide *slide, const QString &words, const rv::data::Slide::Element *like);

// The slide's background colour ("#rrggbb" and the rest, as the API takes colours), or
// with "" none. False if the text is no colour.
bool setBackground(rv::data::Slide *slide, const QString &color);

}
