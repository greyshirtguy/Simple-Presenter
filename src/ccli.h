#pragma once

#include "ccli.pb.h"
#include "presentation.pb.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <string>

// ProPresenter's copyright display, for the CCLI licence a church shows songs under: a
// line made of a song's copyright details, laid over the slides on the audience screens
// on the slides the workspace says (its first, its last, both, or all of them). Plain
// rules on the files' own messages, with nothing of the windows or of the disk in them,
// so that they can be tried by themselves (tests/unit/tst_ccli).
//
// Where it is kept (ProPresenter's help, "How to Display Copyright Information in
// ProPresenter", and the maintainer's own files):
//   - The workspace's settings are a file of their own, Configuration/CCLI (a
//     CCLIDocument): whether the display is on ("Enable Copyright Display"), the
//     church's CCLI licence number, which slides show it, the layout (tokens: the
//     details, and words between them), and the template, a slide with a text box
//     linked to the copyright, which says where it is drawn and how.
//   - Each presentation has its own details (Presentation.ccli): title, artist, author,
//     publisher, copyright year, CCLI song number, album, and whether it shows them
//     (`display`), which songs from SongSelect come with.
//
// GUESSES, no part of ProPresenter's own having been seen doing these: that the words of
// the layout before a detail are that detail's, and are not shown where the song has
// nothing of it (a song with no artist does not show the line break before one), see
// compose;
// that the licence number is shown as it is typed, the layout's words saying what it
// is; that "first" and "last" are the presentation's first and last slides as they are
// played (its arrangement's, with one); and what a workspace with no file of them has
// (made: ProPresenter's tokens as the maintainer's file has them, off, on the first
// slide, and a template of this app's own, a text box along the foot of the slide).
namespace ccli {

// ---- The workspace's settings

// { enabled, license, display ("first", "last", "firstAndLast" or "all"), tokens: [{ kind,
// text }] }, `kind` "text", "title", "artist", "author", "publisher", "year", "license"
// or "songNumber"
QVariantMap describe(const rv::data::CCLIDocument &document);
// Changes what `changes` has of those (`tokens` the whole list), and only what is to be
// other than it is. Returns an error message, empty on success.
QString change(rv::data::CCLIDocument *document, const QVariantMap &changes);
// What a workspace with no file of them is taken to have (see the GUESSES), but for the
// template's text box, which the app makes as the editor makes one (see CopyrightDisplay):
// a slide of 1920 by 1080 with an id from `newId`, and nothing on it.
rv::data::CCLIDocument made(const std::function<std::string()> &newId);

// ---- A presentation's details

// { title, artist, author, publisher, year (0 for none), songNumber (0 for none), album,
// display }
QVariantMap describe(const rv::data::Presentation::CCLI &song);
// Changes what `changes` has of those, and only what is to be other than it is
void change(rv::data::Presentation::CCLI *song, const QVariantMap &changes);

// ---- What is shown

// The line a presentation's details make by the layout's `tokens` (as describe gives
// them), with the workspace's licence number: see the GUESSES for the words between.
QString compose(const QVariantList &tokens, const QVariantMap &song, const QString &license);
// Whether the slide at `index` of a presentation of `count` slides shows it
bool shownOn(const QString &display, int index, int count);

} // namespace ccli
