#pragma once

#include <QString>

#include "presentation.pb.h"

// Presentations made up in ProPresenter's text form, for tst_pptx.cpp: see pptxfixtures.cpp.

// A slide's cue: a text box over the whole slide with these words (RTF in Helvetica, the
// box's record naming "MadeUpFont-CondensedBlack" of "Made Up Font"), an element fill if
// `fill` says one, `textExtra` in its text's attributes, and the cue's own actions before
// its slide
QString cueText(const QString &id, const QString &name, bool enabled, const QString &actions, const QString &textExtra,
                const QString &fill, const QString &words, const QString &notes = QString());
// A media action putting a picture of this file name behind the slide
QString backgroundAction(const QString &file);
// Verse (A, and B, which is turned off) and Chorus (C), an arrangement "Chorus First", and
// a cue D that no group lists; A puts a red picture (Back.png) behind it, which C, cut out
// of a black box, shows through its letters
rv::data::Presentation madePresentation();
