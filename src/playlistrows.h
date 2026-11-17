#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <string>

namespace rv::data {
class Playlist;
class PlaylistItem;
}

// The rows of a playlist of presentations changed as the extended API asks
// (extendedapi.h): a heading made and changed, and a playlist given a whole list of
// rows at once. Plain rules on a playlist as ProPresenter's file has it, with nothing of
// the files or the windows in them (tests/unit/tst_playlistrows); PlaylistFile reads the
// file, calls these and writes it back.
//
// How ProPresenter keeps a playlist's rows (playlistfile.h has the rest): each is an
// item with an id and a name, and is one of several kinds: a heading (a colour, and the
// name its words), a presentation (the file, and the arrangement it is shown in), and
// others this app shows only as a row it cannot open (a cue, a Planning Center item, a
// placeholder). A row can also be hidden, and is then not shown here at all.
//
// A heading as ProPresenter writes one, seen in the playlists of two workspaces: the
// id, the words as the item's name, and a header holding the colour, with its alpha at
// one, and nothing else. A GUESS: that a heading with no colour is a header with none in
// it. Every heading seen had been given a colour, and what ProPresenter gives a new one
// before a colour is chosen is not known.
namespace playlistrows {

// One of the rows a playlist is to be left with
struct Wanted
{
    enum Kind {
        Row,          // a row the playlist has, by its id: kept whole, where it is put
        Header,       // a heading
        Presentation, // a presentation, by its file
    };
    Kind kind = Row;
    QString id;
    // A heading's words, and its colour: "#rrggbb", "" for none, or null (QString())
    // to leave the colour of a heading that is kept as it is
    QString name;
    QString color;
    // A presentation's file, and the arrangement the row shows it in: the row's own
    // (the presentation's choice, for a new row) while `ownArrangement`, otherwise the
    // one named by id and name, both empty for every group in order
    QString file;
    bool ownArrangement = true;
    QString arrangementId;
    QString arrangementName;
};

// Makes `item` a heading of these words and this colour, with this id. False if the
// colour is neither "" nor "#rrggbb".
bool makeHeader(rv::data::PlaylistItem *item, const QString &name, const QString &color, const std::string &id);

// Changes a heading's words and its colour, each where it is given (a null pointer
// leaves that as it is), and only where it is other than the heading has: a colour
// written again would be the same colour in other digits. An error message, empty on
// success: a row that is not a heading, or a colour that is none.
QString changeHeader(rv::data::PlaylistItem *item, const QString *name, const QString *color);

// Makes a playlist hold these rows and no others, in this order. What is wanted is
// matched to the rows there are, so that a row that stays keeps its id and everything
// else of it, known here or not:
//   - one named by its id is that row (those are taken first, so that none is taken by
//     a heading or a presentation that only looks like it);
//   - a heading is the first heading of those words not yet taken, and takes the colour
//     asked, if one is;
//   - a presentation is the first row of that file not yet taken (`files` says which
//     file each of the playlist's rows is on this computer, in step with its rows, ""
//     for one that is no presentation or whose file is not to be found), and takes the
//     arrangement asked, if one is;
//   - what matches none is made: a heading here, a presentation by `makeRow`.
// The rows left over are removed, but for the hidden ones, which this app does not show
// and so nothing could have asked for: each is kept just after the row it came after
// (so that a playlist given the rows it has, in their order, is as it was to the byte),
// or after all the others if that row is going. `*ids`, if given, is the rows' ids, in
// step with what was wanted. On an error (a row named that is not there, a colour that
// is none) the playlist is as it was.
QString set(rv::data::Playlist *playlist, const QList<Wanted> &wanted, const QStringList &files,
            const std::function<rv::data::PlaylistItem(const QString &file)> &makeRow, const std::function<std::string()> &newId,
            QStringList *ids);


// Adds rows to a playlist, in order: just under its row `afterId`, or at its end for "".
// Each is a new row of its own, whatever the playlist has already (a presentation may
// well be wanted twice), and the rows there are, hidden or not, stay where they are.
// `*ids`, if given, is the new rows' ids. On an error (a row to go under that is not
// there, a colour that is none, or a row asked for by an id, which is no new row) the
// playlist is as it was.
QString insert(rv::data::Playlist *playlist, const QList<Wanted> &wanted, const QString &afterId,
               const std::function<rv::data::PlaylistItem(const QString &file)> &makeRow, const std::function<std::string()> &newId,
               QStringList *ids);

}
