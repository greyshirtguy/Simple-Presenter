#pragma once

#include "graphicsData.pb.h"
#include "url.pb.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <string>

namespace rv::data {
class Action;
}
namespace google::protobuf {
class MessageLite;
}

// The files of a workspace, as its documents see them.
//
// ProPresenter's documents (presentations, the playlists file, the media playlists
// file) never hold media or presentations themselves; they refer to files. A reference
// records the file twice: by the full path it had on the machine that wrote it, and,
// when the file was inside the ProPresenter folder, by its path relative to that folder.
// A workspace here is such a folder, so the relative form is the one that still means
// something after the folder has been copied to another machine, and it is always tried
// first. Everything that reads or writes a reference, in any document, does it through
// here, so that they all agree.
namespace workspace {

// Which files are media this app can show, going by their names.
bool isVideo(const QString &file);
bool isMedia(const QString &file);
// Patterns matching every such file, for listing a folder, and the same as a filter for
// a file dialog.
QStringList mediaPatterns();
QString mediaDialogFilter();

// The entries of one directory that match, sorted the way a file manager would
// ("Song 2" before "Song 10").
QList<QFileInfo> sortedEntries(const QString &directory, const QStringList &patterns, QDir::Filters filters);

// An identifier for something new, in the form ProPresenter uses.
std::string newUuid();

// Finds the files that references point to. In order: the path relative to the
// workspace; the path as recorded; and last, any file of that name under one folder of
// the workspace, which is how a file that was moved within it, or never was where the
// document says, is still found.
//
// Looking for a file by name means reading that folder and everything in it. That is
// done once, the first time it is needed, and the result kept, so one finder should
// serve every reference of whatever is being read: a hundred missing files then cost
// one pass over the folder, not a hundred.
class FileFinder
{
public:
    // `searchFolder` is the folder of the workspace searched by name: "Media" or
    // "Libraries".
    FileFinder(const QString &workspaceFolder, const QString &searchFolder);

    // The file's path on this machine, or empty if it cannot be found.
    QString find(const rv::data::URL &reference);

private:
    QString m_workspace;
    QString m_searchDirectory;
    // Lower-case file name to path, for every file under the search folder
    QHash<QString, QString> m_byName;
    bool m_listed = false;
};

// Makes a reference to a file: by its path on this machine and, if the file is inside
// the workspace, relative to the workspace as well.
void recordFile(rv::data::URL *reference, const QString &file, const QString &workspaceFolder);

// The name of the file a reference is to, whether or not the file can be found.
QString fileNameOf(const rv::data::URL &reference);

// A media element for an image or video file, laid out the way ProPresenter writes one;
// a video is set to loop. It is what a slide's media action and a media playlist's row
// both hold.
rv::data::Media mediaElement(const QString &file, const QString &workspaceFolder);

// Reads one of ProPresenter's files into the message it holds, and writes one back. The
// file is replaced in one step, so a failure part way through leaves the original
// untouched; and since the whole message is written, whatever this app does not know of
// in it goes back as it came. `what` names the file to the user in an error message
// ("the props"). Each returns an error message, empty on success. A file that is not
// there is an error to read: whoever expects it may not be looks first.
QString readMessage(const QString &path, google::protobuf::MessageLite *message, const QString &what);
QString writeMessage(const QString &path, const google::protobuf::MessageLite &message, const QString &what);

// How a media action plays what it holds.
//
// ProPresenter has two behaviours for media. A background goes on behind the slides and
// stays there from one slide to the next, going round again when it ends. A foreground
// is for one moment of the show: it plays once, and gives way as soon as another slide
// is shown. Each slide's media action and each row of a media playlist has a behaviour
// of its own, and the file keeps its two halves in different places: which of the two
// it is belongs to the action, and whether a video goes round again belongs to the
// media in it.
struct MediaBehaviour
{
    bool foreground = false;
    // Whether a video starts again when it reaches its end. A still does neither.
    bool loops = false;
    // Whether triggering it starts it again even when it is what is already playing
    bool retriggers = false;

    // For the maps the views are handed: `foreground`, `loops` and `retriggers`.
    void describe(QVariantMap *media) const;
};
MediaBehaviour mediaBehaviour(const rv::data::Action &action);

// Makes a media action a background or a foreground, as ProPresenter sets one up: a
// background video loops and a foreground one plays once.
void setMediaForeground(rv::data::Action *action, bool foreground);

} // namespace workspace
