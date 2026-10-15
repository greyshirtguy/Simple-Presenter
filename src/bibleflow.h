#pragma once

#include "bible.h"

#include <QList>
#include <QString>

#include <functional>

// How a Bible passage is flowed across slides: which words go on which slide.
//
// ProPresenter fills each slide with as much as fits in the theme's text box and goes
// on to the next ("Fill" here), or with Break on New Verse starts a slide at every
// verse, filling it the same way. Two things about that look wrong on a screen: the
// last slide is often a few words, after full ones (an orphan); and a slide ends
// wherever the box is full, in the middle of a phrase, where there may be a comma or
// a full stop a word or two away at which anyone reading it aloud would pause. The
// maintainer asked (2026-10-15) for that to be done better, by rules and not by a
// model of language: "break the text into atomic sentence units (by period, comma,
// semi colon etc) and then attempt to flow those parts evenly across slides ... but
// falling back to splitting an atomic unit when absolutely needed".
//
// "Even" does that, as one sum over every way of breaking the passage that has as many
// slides as Fill would make (never more slides than ProPresenter would show):
//
//   - Every place between two words is a place a slide could end, and has a cost by
//     what is there: the end of a sentence that is also the end of a verse costs
//     nothing, the end of a sentence 1, the end of a verse inside a sentence 3 (or 2
//     after a semicolon, colon or dash there), a semicolon, colon or dash 4, a comma 6,
//     and between two words with nothing between them 20. So the "atomic units" are
//     what lies between places of low cost, and an atomic unit is broken only where no
//     way round it is cheaper: where it is longer than a slide, or where keeping it
//     whole would leave the slides far from even.
//   - A break inside a sentence that leaves two words or fewer of a phrase on one side
//     of it (a slide ending "… and" or starting "him.") costs 8 more.
//   - Each slide costs by how far its length is from the passage's length shared out
//     evenly: (difference / share)² × 40. That is what puts the last few words back
//     with the others, a slide's worth of text going three ways rather than full, full
//     and a scrap; and it is what lets a comma near the middle win over a full stop at
//     the far end.
//   - Nothing goes on a slide that does not fit it (asked of `fits`, which lays the
//     words out in the theme's text box). A word that does not fit even alone is put
//     on a slide of its own, which is all that can be done with it.
//
// The numbers are a first setting of them, tried on made-up passages in the unit tests
// (tests/unit/tst_bibleflow.cpp), not on real Bible text: they are where to turn if a
// passage breaks in a way that looks wrong.
//
// WHERE PAUSES CANNOT BE FOUND THIS WAY (the edge cases the maintainer asked for):
//   - A phrase longer than a slide, with no comma in it (genealogies, lists of names,
//     long translations without commas): it has to break between words, and Even
//     picks the place nearest even that leaves no word or two dangling.
//   - A word longer than the box is wide: it overflows, whatever is done.
//   - Full stops that end no sentence: "Mr.", "St.", "v. 3", initials. Bibles have few,
//     and a break there costs 1 where it should cost 20.
//   - Quotations: a break inside one ('He said, "Go. | Do this."') is priced as any
//     other; the open quote mark is not followed across the slides.
//   - Poetry (the Psalms in most modern translations) breaks at its lines, which an XML
//     file seldom keeps; read as prose, a line's end is often no mark at all.
//   - A sentence that ends one verse and runs on into the next with no mark at all, or
//     a verse that ends with a comma in one translation and a full stop in another:
//     the cost follows the marks, so the same passage breaks differently in each.
//   - Languages that do not put spaces between words (Chinese, Japanese, Thai): there
//     are no words to break between, and this does not try.
//   - Even makes every slide a little emptier than Fill does. With two slides of a
//     passage that just overflows one, both are about half full: that is the point,
//     but some will prefer Fill's one full slide and a scrap.
namespace bibleflow {

// One word of a passage, with the marks after it ("loved,", "world.”"), which verse of
// the passage it is in, whether it is that verse's first (and so carries its number,
// where numbers are shown), and whether it is red letter.
struct Word
{
    QString text;
    int verse = 0;
    bool first = false;
    bool red = false;
};

// The passage's verses as words, each verse's words in order
QList<Word> wordsOf(const QList<bible::PassageVerse> &verses);

// What it costs to end a slide after word `after` (and start the next with the word
// after it): see above. 0 after the last word.
int breakCost(const QList<Word> &words, int after);

enum class Mode { Fill, Even };

struct Options
{
    Mode mode = Mode::Even;
    bool breakOnNewVerse = false;
};

// Whether words `from` to `to` (not counting `to`) go on one slide
using Fits = std::function<bool(int from, int to)>;

// The slides, each as the words it holds: { from, to }, `to` not counted.
struct Slide
{
    int from = 0;
    int to = 0;

    bool operator==(const Slide &other) const = default;
};
QList<Slide> flow(const QList<Word> &words, const Options &options, const Fits &fits);

// The same, with slides already made that are to stay as they are: a passage that has
// grown round a slide of it that is live (← Verse and Verse → add a verse to
// the passage, and what is on the screen must not change under the person). `kept` are
// slides of `words`, in order and one after another; the words before the first of
// them and after the last are each flowed by themselves, as flow() flows a passage, so
// that what is added is shared out evenly too, and the slides come back in order.
//
// The maintainer's own thought (2026-10-15) was to flow each passage with four or five
// verses either side of it taken into account, unseen, so that what Verse → adds
// would already have its place. That keeps nothing still once a verse is added (the
// even flow of the whole is then another one); keeping the slides up to the one that
// is live does, and flows the rest with what is added as if it had been asked for so.
QList<Slide> flow(const QList<Word> &words, const Options &options, const Fits &fits, const QList<Slide> &kept);

}
