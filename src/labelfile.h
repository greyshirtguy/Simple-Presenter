#pragma once

#include "cue.pb.h"

#include <QList>
#include <QString>

// Slide labels: the label a slide has, and the labels a workspace keeps ready to give.
// Plain rules on ProPresenter's files, with nothing of the windows in them, so that they
// can be tried by themselves (tests/unit/tst_labelfile).
//
// How ProPresenter keeps them. A label is a few words and a colour. A slide's own is in
// its cue, on the action that shows the slide (the cue also has a name, which is another
// thing: of the 472 slides looked at whose label had words, 395 had a name that was not
// those words, so the name is left alone here). Of the 1,934 slides looked at, 1,458
// had no label written at all, which is what a slide with no label has, and so taking a
// label off takes it out of the file; 2 had a colour and no words, so a colour alone is
// a label too. A label with no colour has no colour written (402 of the 472); 8 had
// white that is wholly see-through instead, which is read as none.
//
// The labels kept ready are a list of the same thing, in the workspace's
// Configuration/Labels: the one in the workspace this was worked out from has four, of
// words alone ("Blank Slide", "Instrumental", "Repeat", "Spoken Word"). A workspace that
// has no such file has no labels kept ready, and gets the file with the first one made.
//
// Every change reads the file, changes the one thing and writes the whole of it back,
// so that what else is in it, and anything this does not know of, is kept.
namespace labelfile {

struct Label
{
    QString text;
    // "#rrggbb", or empty for no colour
    QString color;

    bool operator==(const Label &other) const { return text == other.text && color == other.color; }
};

// A label as the file has it, and written into the file. An empty colour writes none.
Label read(const rv::data::Action::Label &label);
void write(rv::data::Action::Label *target, const Label &label);

// A slide's label: that of the action of its cue that shows the slide. A cue with no
// such action has none.
Label of(const rv::data::Cue &cue);
// Gives the slide a label, or with no words and no colour takes its label off. Nothing
// else of the cue is touched. Answers whether the cue has a slide to label.
bool set(rv::data::Cue *cue, const Label &label);

// The workspace's labels kept ready, in their order. A workspace with no file has none,
// which is not an error.
QList<Label> presets(const QString &workspace, QString *error);
// Each of these changes the file and writes it back, and returns an error message,
// empty on success. A label of no words is not kept; one the list has already, words
// and colour alike, is not added a second time.
QString add(const QString &workspace, const Label &label);
QString change(const QString &workspace, int index, const Label &label);
QString remove(const QString &workspace, int index);

} // namespace labelfile
