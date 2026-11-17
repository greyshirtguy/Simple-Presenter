#pragma once

#include "richtext.h"

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// What the extended API (extendedapi.h) sends and takes, shaped: plain functions on the
// maps the rest of the app passes about, with nothing of the files, the network or the
// windows in them (tests/unit/tst_extendedapijson).
//
// The app's own maps are what QML draws from: a slide as proconvert.h describes one, an
// action as actions.h does, a row of a playlist as playlistfile.h does. They are
// complete, which is what an API wants, but they are made for drawing: names in camel
// case, colours as Qt's own values, text as a value only C++ can open, numbers where a
// person would say a word (an alignment is 4), and beside what a file says a good deal
// worked out from it (whether a box shows, what it shows). So the API's JSON is made of
// them by rule, the same rule both ways wherever a thing can be sent back changed:
//
//   - A name is in lower case with underscores (fillColor is fill_color), in maps
//     within maps and lists too. The words a few properties are one of go the same way
//     (a shape's "roundedRectangle" is "rounded_rectangle").
//   - A colour is "#rrggbb", or "#rrggbbaa" for one that is partly see-through, as a web
//     page writes one (Qt writes the see-through part first, which nobody would guess).
//     One is taken in either form, as "#rgb", or by a name a web page knows ("white").
//   - Text is its words, with the format of its beginning beside them (`format`), and
//     the whole of it run by run (`paragraphs`) only where more than one format is in it
//     or it is asked for.
//   - An alignment, a way of writing capitals and a way a video plays on are words.
//   - What is only worked out, and what only a drawing wants, is left out; and what
//     says nothing while its switch is off is left out with it (a stroke's colour and
//     width of an element with no stroke), so that an element reads as what is on it.
//
// What is sent to change an element comes back through elementChanges(), which knows
// which properties can be changed (those proconvert::applyChanges takes), turns each to
// the app's own form, and leaves out what is the same as the element has already, so
// that a change that changes nothing writes nothing. A name it does not know, or one
// that can only be read, is an error that says which: whoever sent it, a person or a
// model, is told what was wrong and not left to find that nothing happened.
namespace extendedapijson {

// ---- names and values

// fillColor -> fill_color, and back. (A digit stays with the letters before it:
// linkClock24 is link_clock24.)
QString snake(const QString &camel);
QString camel(const QString &snake);

// A colour as the API writes one, and one read: false if the text is no colour
QString colorText(const QColor &color);
bool colorOf(const QVariant &value, QColor *color);

// Any value of the app's as JSON has it: a map's names in snake case, all the way down;
// a colour as text; a file's address as its path; a text as its words
QVariant plain(const QVariant &value);
// A map of the API's, its names turned to the app's (all the way down), nothing else
// changed: for what the app takes as it is, such as an action
QVariantMap named(const QVariantMap &api);

// The words for the numbers: a line's alignment ("left", "center", "right", "justify"),
// a box's ("top", "middle", "bottom"), capitals ("none", "all_caps", "small_caps",
// "title_case", "start_case"), and how a video plays on from its end ("stop", "loop",
// "loop_count", "loop_seconds"). Each read back gives -1 for what is none of them; a
// number is taken as it is.
QString alignmentWord(int alignment);
int alignmentOf(const QVariant &value);
QString verticalWord(int alignment);
int verticalOf(const QVariant &value);
QString capitalsWord(int capitalization);
int capitalsOf(const QVariant &value);
QString playbackWord(int playback);
int playbackOf(const QVariant &value);

// ---- text

// A run's format (TextRun::format) and its paragraph's alignment, as the API has one:
// { family, font_name, size, bold, italic, color, alignment }, and underline,
// strikethrough, kerning, capitalization, stroke_color and stroke_width where they are
// anything
QVariantMap format(const TextRun &run, Qt::Alignment alignment);
// One sent, as RichText::formatted and TextRun::applyFormat take it. `*error` says what
// is wrong with it, if anything is.
QVariantMap formatChanges(const QVariantMap &api, QString *error);
// A text run by run: [{ alignment, runs: [{ text, ...format }] }]
QVariantList paragraphs(const RichText &text);
// One sent that way. A run takes what it does not say from `like`, and a paragraph
// its alignment from `alignment`: so { runs: [{ text: "…" }] } is words in the look
// the box had. False, with *error, for what is not such a list.
bool paragraphsOf(const QVariantList &api, const TextRun &like, Qt::Alignment alignment, RichText *text, QString *error);
// Whether a text is all in one format and one alignment
bool uniform(const RichText &text);

// ---- an element

// An element (a map as proconvert::toSlideMap gives one of a slide's) as the API has
// it, `order` its place among the slide's from the back. With `rich` its text is given
// run by run as well, whatever it is.
QVariantMap element(const QVariantMap &internal, int order, bool rich = false);

// What is asked of an element, sorted out
struct ElementChanges
{
    // What is wrong with what was sent; nothing else is to be gone by if this is not empty
    QString error;
    // For proconvert::applyChanges: in the app's names and values, and only what differs
    // from what the element has
    QVariantMap properties;
    // Its words typed over (keeping how they are set)
    bool hasText = false;
    QString text;
    // A format for all its words, as RichText::formatted takes one
    QVariantMap format;
    // Its text anew
    bool hasParagraphs = false;
    QVariantList paragraphs;
    // Its chords anew
    bool hasChords = false;
    QVariantList chords;
    // Its place among the slide's elements, or -1 for where it is
    int order = -1;
    // For a new one: what it is, and a media element's file
    QString kind;
    QString file;
};
// `api` is what was sent (a `properties` map inside it is taken as if its things were
// beside it); `current` the element as the app has it now, or empty for one about to
// be made.
ElementChanges elementChanges(const QVariantMap &api, const QVariantMap &current);
// The names of the properties that can be changed, in the API's form, for the reference
// and the tests; and those of the app's own that applyChanges takes
QStringList changeableProperties();

// ---- a slide, a presentation

// A slide (a map as ProDocument::load gives one) as the API has it: { id, index, group,
// label, label_color, enabled, text, notes, width, height, background, media,
// go_to_next, actions } and, with `full`, `elements`
QVariantMap slide(const QVariantMap &internal, int index, bool full);
// A slide's actions (maps as actions::describe gives them) as the API has them
QVariantList actions(const QVariantList &internal);
// A theme's slide: { id, name, index, width, height, background, elements }
QVariantMap themeSlide(const QVariantMap &internal, const QString &name, int index);

// How much of a presentation is said: what it is and what it is made of, with how many
// slides it has (what a change to one is answered with); that and each slide's words,
// label, notes, media and actions; or all of that and every slide's elements
enum Detail { Summary, Outline, Full };

// A presentation, of a document as Catalog::open gives one read in the presentation's
// own order (no arrangement). `groupSlides` is the ids of each group's slides, in step
// with the document's groups, which the document itself has only the number of.
QVariantMap presentation(const QVariantMap &document, const QString &library, const QList<QStringList> &groupSlides, Detail detail);

// ---- playlists

// A row of a playlist (PlaylistFile's, for a playlist of presentations) as the API has
// it: { id, index, kind: "header", name, color } or { id, index, kind: "presentation",
// name, presentation (its uuid), library, arrangement, missing }. `uuid` and `library`
// are the row's presentation's, which the row itself knows only by its file.
QVariantMap playlistItem(const QVariantMap &row, int index, const QString &uuid, const QString &library);
// The playlists as a list in the order they are shown in (Catalog::playlists), each
// { id, name, folder, depth, parent } and, for a playlist, `items`, how many rows it has
QVariantList playlistTree(const QVariantList &nodes, const QHash<QString, int> &counts);

// ---- what is said when something is wrong

// { error: why }
QVariantMap error(const QString &why);

}
