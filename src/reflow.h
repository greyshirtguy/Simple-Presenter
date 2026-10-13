#pragma once

#include "chords.h"
#include "richtext.h"

#include <QList>
#include <QString>

// The reflow editor's rules for a slide's words: taking a stretch of them, putting two
// lots together, and typing over them, each keeping how the words are set; and what
// becomes of the chords over them. Plain values in and out, with nothing of the file
// or of the windows in them, so that they can be tried by themselves
// (tests/unit/tst_reflow.cpp). PresentationEditor does the rest: which text box of a
// slide the words are, the new slide a split makes, and the file.
//
// What the reflow editor is for. A presentation's words are spread over its slides,
// and getting them spread well (two lines a slide, a verse to a slide) is most of the
// work of setting a song up. In the reflow editor the slides' words are a table, a row
// to a slide, and the words are moved between slides by splitting a slide in two where
// the caret is and by joining a slide to the one before it. The words keep their
// fonts, sizes and colours through that, and their chords, without any of it being
// shown or set there.
//
// Places are counted as a text editor counts them, and as RichText does: in characters
// through the whole text, a break between paragraphs being one, and a line break
// within a paragraph one.
//
// GUESSES, where ProPresenter's own reflow editor was not looked at (its help says only
// that a slide is split at the caret and that two are combined). The maintainer, who
// knows ProPresenter, has said of the first two that they are as wanted:
//   - A slide is split where the words stop before the caret and start again after
//     it: the spaces and line breaks the caret is among go to neither slide, so that
//     neither ends or begins with an empty line.
//   - Two slides joined have the second's words on a new line after the first's.
//   - Words typed take the look of the character before them, as in any editor.
namespace reflow {

// Where a text is split for a caret: the first slide keeps what is before `leftEnd`,
// and the second has what is from `rightStart` on.
struct Split
{
    int leftEnd = 0;
    int rightStart = 0;
};
Split splitAt(const QString &words, int caret);

// The characters of a text from one place up to another, set as they were. A text with
// nothing in it still says how words typed into it are to look: as the character at
// `from` did.
RichText cut(const RichText &text, int from, int to);

// One text after another, the second beginning on a new line. Either with no words in
// it gives the other alone.
RichText joined(const RichText &first, const RichText &second);

// A text whose words have been typed over: `now` is what they are to be. What the old
// and the new words have in common at their start and at their end keeps its look,
// and what is between takes the look of the character before it.
RichText retyped(const RichText &was, const QString &now);

// How many characters a text has, and its words (as RichText::plainText gives them;
// here so that these rules stand by themselves).
int length(const RichText &text);
QString words(const RichText &text);

// The chords of the two slides a split makes, from those of the slide that was split:
// each stays over its character. One that stood among the spaces that went to neither
// slide goes to the start of the second, if no chord is there.
QList<chords::Chord> chordsLeft(const QList<chords::Chord> &chords, const Split &split);
QList<chords::Chord> chordsRight(const QList<chords::Chord> &chords, const Split &split);

// The chords of two slides joined, the first having `firstLength` characters: the
// second's stand as far along as its words now do. `brokenBetween` is whether the
// second's words begin on a new line (they do unless one of the two had no words).
QList<chords::Chord> chordsJoined(const QList<chords::Chord> &first, int firstLength, const QList<chords::Chord> &second,
                                  bool brokenBetween);

} // namespace reflow
