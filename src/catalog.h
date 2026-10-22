#pragma once

#include "playlistfile.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <optional>

// What is on disk in a workspace: one folder holding everything for a setup,
//   <workspace>/Libraries/<library>/*.pro   presentations, one flat folder per library
//   <workspace>/Media/...                   images and videos, in any depth of folders
//   <workspace>/Playlists/Library           the playlists, in ProPresenter's own format
//   <workspace>/Playlists/Media             the media playlists, likewise
//   <workspace>/Playlists/Audio             the audio bin's playlists, likewise
// which is how ProPresenter lays out its own folder, so a copy of one is a workspace.
// Workspaces sit side by side in one folder, and the catalog can be switched from one to
// another.
//
// This is the one object the QML side talks to about what is on disk. It holds what the
// lists show (libraries, playlists, media playlists and their rows) as plain lists of
// maps, read whenever the folders change; `changed` says they have. Every change the app
// makes to a workspace goes through here too, as a function that writes the file and
// returns an error message, empty on success. The reading and writing themselves are in
// PlaylistFile (the two playlists files), ProDocument (presentations) and
// PlaylistImport (exported playlists); the editor has its own, PresentationEditor.
class Catalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created in main.cpp")
    // The open workspace's folder and its name, and { name, path } for it and every
    // folder beside it, sorted by name.
    Q_PROPERTY(QString workspacePath READ workspacePath NOTIFY workspaceChanged)
    Q_PROPERTY(QString workspaceName READ workspaceName NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY changed)
    Q_PROPERTY(QString librariesDirectory READ librariesDirectory NOTIFY workspaceChanged)
    Q_PROPERTY(QString mediaDirectory READ mediaDirectory NOTIFY workspaceChanged)
    // { name, path } for each library folder, sorted by name.
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY changed)
    // { name, path, depth, folder } for each playlist and playlist folder, in tree order.
    // `path` is the node's id; folder rows only hold other rows.
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
    // The same for the media playlists and their folders, which are what the media bin
    // shows.
    Q_PROPERTY(QVariantList mediaPlaylists READ mediaPlaylists NOTIFY changed)
    // And for the playlists of the audio bin and their folders.
    Q_PROPERTY(QVariantList audioPlaylists READ audioPlaylists NOTIFY changed)
    // Goes up whenever anything on disk changed; bind to it to re-query the folder contents.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    // Goes up when cached thumbnails have been discarded. Thumbnail urls carry it, so the
    // views ask for theirs again.
    Q_PROPERTY(int thumbnailRevision READ thumbnailRevision NOTIFY thumbnailsDiscarded)
    // Whether an import is still copying files.
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)
    // A filter for a file dialog, showing the media files the app can use.
    Q_PROPERTY(QString mediaDialogFilter READ mediaDialogFilter CONSTANT)
    // And the sound files the audio bin can take.
    Q_PROPERTY(QString audioDialogFilter READ audioDialogFilter CONSTANT)
    // Whether a slide has been copied, and so can be pasted
    Q_PROPERTY(bool hasCopiedSlide READ hasCopiedSlide NOTIFY copiedSlideChanged)

public:
    explicit Catalog(const QString &workspace, QObject *parent = nullptr);

    // Switches to another workspace folder, creating what it lacks. Everything the
    // catalog reports changes with it.
    Q_INVOKABLE void openWorkspace(const QString &path);

    QString workspacePath() const { return m_root; }
    QString workspaceName() const;
    QVariantList workspaces() const { return m_workspaces; }
    QString librariesDirectory() const { return m_librariesDirectory; }
    QString mediaDirectory() const { return m_mediaDirectory; }
    QVariantList libraries() const { return m_libraries; }
    QVariantList playlists() const { return m_playlists.nodes; }
    QVariantList mediaPlaylists() const { return m_mediaPlaylists.nodes; }
    QVariantList audioPlaylists() const { return m_audioPlaylists.nodes; }
    int revision() const { return m_revision; }
    bool importing() const { return m_importing; }
    int thumbnailRevision() const { return m_thumbnailRevision; }
    QString mediaDialogFilter() const;
    QString audioDialogFilter() const;
    bool hasCopiedSlide() const { return !m_copiedSlide.isEmpty(); }

    // Discards the cached thumbnails of these media files; they are made again as the
    // views next show them.
    Q_INVOKABLE void rebuildThumbnails(const QStringList &paths);

    // A row for each presentation directly in a library folder, sorted by name, in the
    // shape PlaylistFile describes for playlist rows: here `path` and `file` are both the
    // file's path, and the arrangement is the one selected in the file.
    Q_INVOKABLE QVariantList documentsIn(const QString &library) const;
    // The rows of the media playlist with this id, as PlaylistFile describes them.
    Q_INVOKABLE QVariantList mediaIn(const QString &playlist) const;
    // The rows of the audio bin's playlist with this id, as PlaylistFile describes them.
    Q_INVOKABLE QVariantList audioIn(const QString &playlist) const;
    // The rows of the playlist with this id, as PlaylistFile describes them.
    Q_INVOKABLE QVariantList playlistItems(const QString &playlist) const;
    // Returns { name, path, uuid, slides, arrangements, arrangement, error }; error is
    // empty on success. Follows the arrangement the presentation has selected.
    Q_INVOKABLE QVariantMap open(const QString &path) const;
    // The same, following the named arrangement; "" is every group in stored order.
    Q_INVOKABLE QVariantMap openArranged(const QString &path, const QString &arrangement) const;
    // Records which arrangement of the presentation `file` a playlist row uses, "" for
    // none, and saves the playlists. Returns an error message, empty on success.
    Q_INVOKABLE QString setPlaylistItemArrangement(const QString &item, const QString &file, const QString &arrangement);
    // Where a presentation is sent when a slide of it is triggered: "announcements",
    // or "" for the slide and media layers. Returns an error message, empty on success.
    Q_INVOKABLE QString setDestination(const QString &file, const QString &destination);
    // Changes to the playlists, each saved at once. They return an error message, empty
    // on success, except the two that create, which return { id, error }. A new playlist
    // or folder goes inside the folder `parent`, or at the top level for "". Removing
    // only removes lists and rows: presentations are never touched.
    Q_INVOKABLE QVariantMap createPlaylist(const QString &name, const QString &parent);
    Q_INVOKABLE QVariantMap createPlaylistFolder(const QString &name, const QString &parent);
    Q_INVOKABLE QString renamePlaylist(const QString &id, const QString &name);
    Q_INVOKABLE QString removePlaylist(const QString &id);
    Q_INVOKABLE QString addToPlaylist(const QString &playlist, const QString &file);
    Q_INVOKABLE QString removePlaylistItem(const QString &item);
    Q_INVOKABLE QString movePlaylistItem(const QString &item, const QString &target, bool after);
    // Moves a playlist or folder "before" or "after" another, or "onto" (into) a folder.
    Q_INVOKABLE QString movePlaylistNode(const QString &id, const QString &target, const QString &where);
    // Imports an exported playlist (.proplaylist): its presentations go into the folder
    // `library`, its media under Media, and its playlists inside the folder `parent` or
    // at the top level for "". Nothing already on disk is overwritten. The copying runs
    // in the background; importFinished reports the outcome.
    Q_INVOKABLE void importPlaylist(const QUrl &archive, const QString &library, const QString &parent);
    // Makes a new library: a folder of that name under the workspace's Libraries, which
    // is all a library is. Returns { path, error }.
    Q_INVOKABLE QVariantMap createLibrary(const QString &name);
    // Gives a library another name, which is renaming its folder. Only an empty one:
    // a playlist finds its presentations by the library's name among other things, so
    // renaming one that has presentations in it could leave playlists pointing at
    // nothing. It is for naming a library that has just been made. Returns
    // { path, error }.
    Q_INVOKABLE QVariantMap renameLibrary(const QString &path, const QString &name);
    // Makes a new presentation in the library folder `library`, called `name`: one
    // slide with nothing on it (see ProDocument::create). Returns { path, error }.
    Q_INVOKABLE QVariantMap createPresentation(const QString &library, const QString &name);
    // Gives a slide a label, or with no words and no colour takes its label off (see
    // ProDocument::setCueLabel).
    Q_INVOKABLE QString setSlideLabel(const QString &path, const QString &slideId, const QString &text, const QString &color);
    // Puts a slide, and the slides after it in its group, into another group (see
    // ProDocument::setGroupFrom): `group` is the one chosen, { name, color, id }, the
    // id being the group's in the workspace's list of groups, or empty. Returns
    // { changed, error }.
    Q_INVOKABLE QVariantMap setSlideGroup(const QString &path, const QString &slideId, const QVariantMap &group);
    // Gives a presentation another name (see ProDocument::rename), and brings along
    // the rows of playlists that are that presentation, so that they still find it.
    // Returns { path, error }: where the file then is.
    Q_INVOKABLE QVariantMap renamePresentation(const QString &file, const QString &name);
    // Takes a presentation out of its library, by moving its file to the system's
    // Trash, where the file manager has it to put back: nothing is ever deleted
    // outright here, and if the file cannot be moved there it is left where it is.
    // Rows of playlists that were that presentation are left as they are, and say
    // that it is missing, which is how a row is shown here whose presentation is not
    // to be found. (A GUESS: that ProPresenter leaves such rows too. It has not been
    // looked at there.) Returns { trash, error }: where the file then is.
    Q_INVOKABLE QVariantMap deletePresentation(const QString &file);
    // Makes a presentation of a ChordPro file in the library folder `library`, with
    // `linesPerSlide` lines of its words on each slide (see songimport.h). Returns
    // { path, error }.
    Q_INVOKABLE QVariantMap importSong(const QUrl &file, const QString &library, int linesPerSlide);

    // The same changes for the media playlists, and adding media files to one. Files are
    // referred to where they are, not copied.
    Q_INVOKABLE QVariantMap createMediaPlaylist(const QString &name, const QString &parent);
    Q_INVOKABLE QVariantMap createMediaFolder(const QString &name, const QString &parent);
    Q_INVOKABLE QString renameMediaPlaylist(const QString &id, const QString &name);
    Q_INVOKABLE QString removeMediaPlaylist(const QString &id);
    Q_INVOKABLE QString moveMediaPlaylist(const QString &id, const QString &target, const QString &where);
    // Files go at the end of the playlist, or, given one of its rows as `target`, just
    // before that row or with `after` just after it.
    Q_INVOKABLE QString addMedia(const QString &playlist, const QList<QUrl> &files, const QString &target = {},
                                 bool after = false);
    // Which of these files are images and videos the app can use, as paths: what is
    // dragged in from a file manager can be anything.
    Q_INVOKABLE QStringList mediaAmong(const QList<QUrl> &files) const;
    Q_INVOKABLE QString removeMediaItem(const QString &item);
    // Makes a media playlist's row a background or a foreground. It is the row's own:
    // the same file on a slide, or in another row, keeps the behaviour it has there.
    Q_INVOKABLE QString setMediaItemForeground(const QString &item, bool foreground);
    // What follows a media playlist's row when its video ends, played from the
    // playlist: ProPresenter's Next Cue (see src/mediacue.h): "" for nothing, "next",
    // "random", "first", or "cue" with `target` the row to go to. Anything to follow
    // makes the row stop at its end.
    Q_INVOKABLE QString setMediaItemNext(const QString &item, const QString &next, const QString &target = QString());
    // Playback markers (src/playbackmarkers.h), on the media of a `place`: { kind:
    // "slide", path, slideId } for a slide's, { kind: "mediaRow", item } for a row of the
    // media bin and { kind: "audioRow", item } for a piece of the audio bin. Each returns
    // an error message, empty on success; one that makes a marker gives { id, error }.
    // A marker's actions are of the kinds a slide's are (src/actions.h).
    // (A new marker is a colour of the palette its media's markers have not, at random:
    // see playbackmarkers::newColor.)
    Q_INVOKABLE QVariantMap addMarker(const QVariantMap &place, double time);
    // The colours the marker menu offers by name (playbackmarkers::palette)
    Q_INVOKABLE QVariantList markerPalette() const;
    // The markers of the media of a `place` as its file has them now (as
    // playbackmarkers::describe gives them), for what is playing it to go by after a
    // change; empty where there is none
    Q_INVOKABLE QVariantList markersAt(const QVariantMap &place) const;
    Q_INVOKABLE QString changeMarker(const QVariantMap &place, const QString &id, const QVariantMap &changes);
    Q_INVOKABLE QString removeMarker(const QVariantMap &place, const QString &id);
    Q_INVOKABLE QVariantMap duplicateMarker(const QVariantMap &place, const QString &id);
    Q_INVOKABLE QString addMarkerAction(const QVariantMap &place, const QString &id, const QVariantMap &action);
    Q_INVOKABLE QString changeMarkerAction(const QVariantMap &place, const QString &id, const QString &actionId, const QVariantMap &action);
    Q_INVOKABLE QString removeMarkerAction(const QVariantMap &place, const QString &id, const QString &actionId);
    // What a row's Media Inspector sets (src/mediasettings.h), and the row's media
    // replaced by another file, keeping what mediasettings::carryOver keeps.
    Q_INVOKABLE QString setMediaItemSettings(const QString &item, const QVariantMap &changes);
    // All of a row's settings, which the row has only when its media is set to anything
    Q_INVOKABLE QVariantMap mediaItemSettings(const QString &item) const;
    Q_INVOKABLE QString replaceMediaItemFile(const QString &item, const QString &file);
    Q_INVOKABLE QString moveMediaItem(const QString &item, const QString &target, bool after);

    // The same again for the audio bin: its playlists and folders, and its rows, which
    // are sound files. Files are referred to where they are, not copied.
    Q_INVOKABLE QVariantMap createAudioPlaylist(const QString &name, const QString &parent);
    Q_INVOKABLE QVariantMap createAudioFolder(const QString &name, const QString &parent);
    Q_INVOKABLE QString renameAudioPlaylist(const QString &id, const QString &name);
    Q_INVOKABLE QString removeAudioPlaylist(const QString &id);
    Q_INVOKABLE QString moveAudioPlaylist(const QString &id, const QString &target, const QString &where);
    // (How long each file is, is read from it as it is added, which takes a moment for
    // each: the audio bin shows it, and ProPresenter's file has it.)
    Q_INVOKABLE QString addAudio(const QString &playlist, const QList<QUrl> &files, const QString &target = {},
                                 bool after = false);
    // Which of these files are sound files, as paths.
    Q_INVOKABLE QStringList audioAmong(const QList<QUrl> &files) const;
    Q_INVOKABLE QString removeAudioItem(const QString &item);
    Q_INVOKABLE QString moveAudioItem(const QString &item, const QString &target, bool after);
    // How a row plays on from its end ("stop", "loop" or "next"), and whether it is a
    // sound effect and not music (see audiocue.h). They are the row's own.
    Q_INVOKABLE QString setAudioPlayback(const QString &item, const QString &playback);
    Q_INVOKABLE QString setAudioEffect(const QString &item, bool effect);
    // Selects the named arrangement in the presentation file, or none for "", and saves
    // the file. Returns an error message, empty on success.
    Q_INVOKABLE QString setArrangement(const QString &path, const QString &arrangement);
    // Making and changing a presentation's arrangements, for the arrangement editor
    // over the slides (see ProDocument, and arrangementedit.h for the rules). An
    // arrangement is named by its `id`, as an opened presentation's `arrangementList`
    // has it. The presentation file is saved; each returns an error message, empty on
    // success, the two that make one as { name, error } with the name it was given.
    Q_INVOKABLE QVariantMap addArrangement(const QString &path);
    Q_INVOKABLE QVariantMap duplicateArrangement(const QString &path, const QString &id);
    Q_INVOKABLE QString renameArrangement(const QString &path, const QString &id, const QString &name);
    Q_INVOKABLE QString removeArrangement(const QString &path, const QString &id);
    Q_INVOKABLE QString insertArrangementGroup(const QString &path, const QString &id, const QString &groupId, int landing);
    Q_INVOKABLE QString moveArrangementGroup(const QString &path, const QString &id, int from, int landing);
    Q_INVOKABLE QString removeArrangementGroup(const QString &path, const QString &id, int place);
    // Makes a slide (by its `id`) trigger the given media file, as a background or a
    // foreground, replacing any media it triggered before, and saves the presentation
    // file. Returns an error message, empty on success.
    Q_INVOKABLE QString setSlideMedia(const QString &path, const QString &slideId, const QString &mediaPath,
                                      bool foreground);
    // Makes the media a slide triggers a background or a foreground (see
    // workspace::MediaBehaviour), and saves the presentation file.
    Q_INVOKABLE QString setSlideMediaForeground(const QString &path, const QString &slideId, bool foreground);
    // ProPresenter's Slide Show, which a presentation has (seconds, 0 for none), and
    // Go To Next Timer, which a slide has: see ProDocument, and show::State::advance
    // for what they do.
    Q_INVOKABLE QString setSlideShow(const QString &path, double seconds);
    Q_INVOKABLE QString setSlideGoToNext(const QString &path, const QString &slideId, bool on, double seconds, bool loop);
    // How the slide's video plays on from its end: 0 it stops, 1 it goes round, 2 it
    // plays `loopCount` times, 3 it goes round for `loopSeconds`.
    Q_INVOKABLE QString setSlideMediaPlayback(const QString &path, const QString &slideId, int playback, int loopCount, double loopSeconds);
    // What the Media Inspector sets of the media a slide triggers (src/mediasettings.h),
    // and that media replaced by another file, keeping what mediasettings::carryOver
    // keeps. Both save the presentation file.
    Q_INVOKABLE QString setSlideMediaSettings(const QString &path, const QString &slideId, const QVariantMap &changes);
    // All of the settings of the media a slide triggers, which the slide's map has only
    // when the media is set to anything
    Q_INVOKABLE QVariantMap slideMediaSettings(const QString &path, const QString &slideId) const;
    Q_INVOKABLE QString replaceSlideMediaFile(const QString &path, const QString &slideId, const QString &file);
    // The effects media can be given, for the Media Inspector's list of them (see
    // src/mediaeffects.h): [{ renderId, name, description, drawn }], in the order of
    // their names; and a new one of them, as mediasettings::newEffect makes it.
    Q_INVOKABLE QVariantList mediaEffects() const;
    Q_INVOKABLE QVariantMap newMediaEffect(const QString &renderId) const;
    // Opens the folder a file is in, in the desktop's file manager, with the file picked
    // out where the file manager can do that (its FileManager1 service, which GNOME's
    // Files and most others have), and otherwise the folder alone.
    Q_INVOKABLE void showInFolder(const QString &path) const;
    // The actions a slide has besides showing itself and its media (see src/actions.h
    // for what one is): gives it another, changes one by its id, takes one away.
    Q_INVOKABLE QString addSlideAction(const QString &path, const QString &slideId, const QVariantMap &action);
    Q_INVOKABLE QString changeSlideAction(const QString &path, const QString &slideId, const QString &actionId, const QVariantMap &action);
    Q_INVOKABLE QString removeSlideAction(const QString &path, const QString &slideId, const QString &actionId);
    // Stops a slide triggering media, and saves the presentation file.
    Q_INVOKABLE QString removeSlideMedia(const QString &path, const QString &slideId);
    // Deletes a slide from the presentation, for good, and saves the presentation file.
    Q_INVOKABLE QString removeSlide(const QString &path, const QString &slideId);
    // Adds a slide with nothing on it after the one with this id (or at the end, with
    // none named). Gives { id, error }: the new slide's id, or what went wrong.
    Q_INVOKABLE QVariantMap insertSlide(const QString &path, const QString &slideId);
    // Keeps a copy of a slide, to be pasted into this presentation or another.
    Q_INVOKABLE QString copySlide(const QString &path, const QString &slideId);
    // Adds the copied slide after the one with this id. Gives { id, error }.
    Q_INVOKABLE QVariantMap pasteSlide(const QString &path, const QString &slideId);
    // Adds a slide to the presentation for each of these media files, each with nothing
    // on it but triggering its file as a foreground (see ProDocument::insertMediaCues):
    // just before the slide with this id or with `after` just after it, or at the end
    // for "". Saves the presentation file.
    Q_INVOKABLE QString insertMediaSlides(const QString &path, const QString &slideId, bool after,
                                          const QStringList &mediaPaths);

signals:
    void changed();
    void workspaceChanged();
    void importingChanged();
    void copiedSlideChanged();
    void thumbnailsDiscarded();
    // `error` is empty on success, when `summary` says what was brought in and
    // `playlist` is the id of the first playlist added.
    void importFinished(const QString &error, const QString &summary, const QString &playlist);

private:
    void rescan();
    QVariantList listDocuments(const QString &library) const;
    QString afterChange(const QString &error);
    // Changes the media action of a marker's `place` (see addMarker) by `change`
    QString changeMarkers(const QVariantMap &place, const std::function<QString(rv::data::Action *)> &change);
    QVariantMap open(const QString &path, const std::optional<QString> &arrangement) const;

    QString m_root;
    QString m_librariesDirectory;
    QString m_mediaDirectory;
    // The slide that was copied, as its cue is in a presentation file
    QByteArray m_copiedSlide;
    QVariantList m_workspaces;
    QVariantList m_libraries;
    PlaylistFile m_playlists;
    PlaylistFile m_mediaPlaylists;
    PlaylistFile m_audioPlaylists;
    int m_revision = 0;
    bool m_importing = false;
    int m_thumbnailRevision = 0;
    // The presentations of each library that has been asked for, by its path
    mutable QHash<QString, QVariantList> m_documents;
    // Whether the workspace has been read at all yet
    bool m_scanned = false;
    QFileSystemWatcher m_watcher;
    // Runs out a little after the last change on disk, and then the workspace is read
    QTimer m_settle;
};
