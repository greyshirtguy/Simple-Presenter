#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace rv::data {
class Slide;
class Template_Document;
class Template_Slide;
}

// The themes of a workspace, as ProPresenter keeps them, and dressing a slide in one.
//
// A theme is a set of slides built to show how slides should look: where the words go,
// in what font and colour, over what shapes and pictures. ProPresenter keeps each in a
// folder of its own under the workspace's Themes folder, by the theme's name, as a file
// called Theme with an Assets folder beside it for the theme's pictures; a folder there
// with no Theme file in it is a folder of themes. A theme's place is its path under
// Themes: "Samples/Black Box".
//
// Dressing a slide in a theme slide makes the slide look like the theme slide, with the
// slide's own words:
//
//   - The slide's text boxes that have words in them are matched with the theme's text
//     boxes (see thememath.h: by name, then by size, then in order). A theme's text
//     boxes are the things on it with words in them, which stand for the words to come
//     ("Verse", "Lyrics").
//   - A matched text box takes everything from the theme's: where it is and how large,
//     its fill, its outline, its shadow, and the one format the theme's text is in (font,
//     size, colour, alignment, capitals, how it fits its box). It keeps its words, and
//     what else is its own (what its text is linked to, how it comes on).
//   - What else the theme slide has (shapes, pictures, text boxes that get no words) comes
//     with it, the text boxes empty.
//   - Words of the slide that have no box in the theme stay as they were, on top: words
//     are never thrown away.
//   - Things of the slide's own that are not words (a picture someone put on it) stay,
//     underneath. Things an earlier dressing brought are taken away again, so that going
//     from one theme to another leaves no trace of the first: they are known by having
//     the ids of things in the workspace's themes, which what a theme brings keeps.
//   - The slide's background colour becomes the theme slide's. What media the slide's
//     cue triggers, and its other actions, are not the slide's look and are not touched.
//
// ProPresenter does not say how it does this; the above is this app's reading of what
// it is for, and may differ from ProPresenter's in the corners.
namespace themefile {

// The workspace's Themes folder
QString folder(const QString &workspace);

// Every theme of a workspace, by its place, in the order of the alphabet
QStringList places(const QString &workspace);

bool read(const QString &workspace, const QString &place, rv::data::Template_Document *theme, QString *error);
QString write(const QString &workspace, const QString &place, const rv::data::Template_Document &theme);

// The slide of a theme with this id, or null
const rv::data::Template_Slide *slideOf(const rv::data::Template_Document &theme, const QString &id);

// The ids of every element of every slide of every theme of a workspace: what marks
// something on a slide as having come from a theme
QSet<QString> elementIds(const QString &workspace);

// Dresses a slide in a theme slide (see above).
void dress(rv::data::Slide *slide, const rv::data::Slide &theme, const QSet<QString> &themeElements);

}
