#pragma once

#include "richtext.h"

#include <QString>

// A text box's lines made into one, as ProPresenter's Single Line and Replace Line
// Returns transforms do to a text box's own text (Text::transform in the file).
//
// The app used to do this to the text's words alone and draw the result in the format
// of the text's first run, which lost every other run's look: the red of the words of
// Jesus, a bold word, a raised verse number. Worse, where the first run was such a
// number, the whole text was drawn raised and small (found with a Bible theme whose
// verse box is Single Line, 2026-10-15). Here each run keeps its look; only the line
// breaks go.
//
// What ProPresenter does is a guess, and this is it: each line is trimmed and an empty
// one left out (as the app did, and still does for linked text), the lines are joined
// with `separator` (a space for Single Line, the box's delimiter for Replace Line
// Returns), and the separator is drawn in the look of the end of the line before it.
// A paragraph break and a line break within a paragraph (U+2028, or "\n" in a run)
// both count as the end of a line. The result is one paragraph, aligned and spaced as
// the first was. A text with nothing in it is one empty run in its first run's look.
RichText joinedLines(const RichText &text, const QString &separator);
