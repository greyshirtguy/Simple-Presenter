#pragma once

#include "catalog.h"
#include "extendedapi.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

class Search;

// What carries out the extended API (extendedapi.h says what it is and lists its
// operations; extendedapijson.h shapes what is sent and answered): each operation done
// on the open workspace's files, and answered.
//
// There is one of these, which the HTTP API's server asks (httpapiserver.h) for a
// request to one of this app's own paths, and for an MCP tool that is called (mcp.h);
// the operator window gives it what it needs of the window (HttpApiControl.qml), and is
// told by it what has changed.
//
// How a change is made. Every change is the app's own way of making that change, by the
// functions the window's own buttons use where there are such (Catalog, ProDocument,
// PlaylistFile, themefile) and otherwise by rules of the same kind (presentationedit.h,
// slideedit.h): the file read whole, the fields the change is about altered, and the
// whole written back, so that what this app does not understand of a file of
// ProPresenter's goes back as it came.
//
// One request, one change to the file. A request may ask several things of a
// presentation at once (a slide's words, its label and its media; a presentation made
// with sixty slides and dressed in a theme), and the functions that do them each read
// and write a file. So they are not let loose on the presentation itself: the request
// works on a copy of the file in a folder of its own (Work, in the .cpp), each step
// reading and writing that, and only when every step has gone well does the copy take
// the file's place, in one step. A request that fails part way leaves the presentation
// exactly as it was; and one that changes nothing writes nothing.
//
// Never losing what was there. Before a file is changed, a copy of it as it was is kept
// beside the editor's own (workspace::keepCopy, "before the API"), the newest twenty of
// each file: whenever the file is not as the API itself last left it (so that what
// somebody did at the window, or in ProPresenter, since then is never changed or
// removed from here without a copy of it), and otherwise once in ten minutes, so that
// a run of changes keeps the file as it was before the run and not twenty moments of
// the run. (A copy is named to the second, and one second has one copy: the first.) A presentation deleted goes to the computer's Trash, and so does a theme. A
// playlist removed is gone from the playlists' file, of which the copy is whole.
//
// What is in the way. A presentation that is open in the window's editor is the
// editor's: it holds the file as it read it and writes the whole of it at each change,
// so a change made under it would be lost at the editor's next. Such a request is
// refused (409), and says why; `busy` is what the window says is open so.
//
// Everything is done on the app's own thread, as a change made at the window is: so a
// video that is playing may miss a frame as a file is written, as it may when a slide
// is edited by hand. Nothing here is done unless something asks.
class ExtendedApi : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The workspace: what is on disk, and what makes the app's own changes to it
    Q_PROPERTY(Catalog *catalog READ catalog WRITE setCatalog NOTIFY catalogChanged)
    // The workspace's own groups ({ id, name, color } each: GroupKeys.groups), which a
    // group named as one of them takes its colour from
    Q_PROPERTY(QVariantList groups READ groups WRITE setGroups NOTIFY groupsChanged)
    // The files the window's editor has open (a presentation's, a theme's): not to be
    // changed from here while they are
    Q_PROPERTY(QStringList busy READ busy WRITE setBusy NOTIFY busyChanged)
    // What finds presentations by their words (search.h), for search_presentations
    Q_PROPERTY(QObject *search READ search WRITE setSearch NOTIFY searchChanged)

public:
    struct Answer
    {
        int status = 404;
        QVariant data;
    };

    explicit ExtendedApi(QObject *parent = nullptr);

    Catalog *catalog() const { return m_catalog; }
    void setCatalog(Catalog *catalog);
    QVariantList groups() const { return m_groups; }
    void setGroups(const QVariantList &groups);
    QStringList busy() const { return m_busy; }
    void setBusy(const QStringList &busy);
    QObject *search() const;
    void setSearch(QObject *search);

    // Does an operation and answers it: 200 with what was asked for or what is now so,
    // 201 with what was made, or 400, 404, 409 or 500 with { error: why }. `theme` says
    // the operation was asked for by its path for a theme's slide; `params` are the
    // parts of the path, `query` the query's, and `body` the request's JSON.
    Answer answer(const extendedapi::Operation &operation, bool theme, const QVariantMap &params, const QVariantMap &query,
                  const QByteArray &body);
    // The same with the body read already (an MCP tool's arguments, sorted into the
    // three)
    Answer answer(const extendedapi::Operation &operation, bool theme, const QVariantMap &params, const QVariantMap &query,
                  const QVariantMap &body);

    // What the show has that a slide's action can name (timers, macros, looks, …): asked
    // of the operator window through ProPresenter's own routes, by whoever has the
    // window's answerer (the server). Given the route's pattern, it gives what the route
    // answers, or nothing.
    void setShowAsker(const std::function<QVariant(const QString &)> &asker) { m_askShow = asker; }

    // The reference (docs/features/extended-api.md, which the app carries), whole or
    // the part under one heading
    static QString reference(const QString &topic);

signals:
    void catalogChanged();
    void groupsChanged();
    void busyChanged();
    void searchChanged();
    // A presentation's file has been changed: whatever shows it reads it again
    void presentationChanged(const QString &path);
    // One is about to be renamed, and has been: what was at `from` is at `to`. (Between
    // the two the window leaves its lists as they are: the catalog says they have
    // changed as soon as the file has its new name, when the window would take the
    // presentation it is viewing for gone, where it is only called something else.)
    void renaming(bool underWay);
    void presentationRenamed(const QString &from, const QString &to);
    // The themes are not as they were
    void themesChanged();
    // Something was done, in words for the log
    void did(const QString &what);

private:
    struct Found
    {
        QString path;
        QString library;
        // Why none was found, and the status to say it with
        QString error;
        int status = 404;
        explicit operator bool() const { return error.isEmpty(); }
    };
    struct ThemeFound
    {
        QString place;
        QString error;
        explicit operator bool() const { return error.isEmpty(); }
    };

    Answer run(const extendedapi::Operation &operation, bool theme, const QVariantMap &params, const QVariantMap &query,
               const QVariantMap &body);
    QString root() const;
    Found presentation(const QString &which, const QString &library) const;
    Found presentationOf(const QVariantMap &params, const QVariantMap &query) const;
    QString libraryPath(const QString &name) const;
    QString libraryOf(const QString &file) const;
    ThemeFound theme(const QString &which) const;
    QString playlist(const QString &which, bool *folder = nullptr) const;
    QString playlistItem(const QString &playlist, const QString &which) const;
    bool isBusy(const QString &file) const;
    // Keeps a copy of a file before it is changed, unless it is as the API last left
    // it and a copy was kept of it lately (see above)
    void keep(const QString &file);

    // The answers
    QVariantMap described(const QString &path, bool full, QString *error) const;
    QVariantMap describedTheme(const QString &place, QString *error) const;
    QVariantMap describedPlaylist(const QString &id) const;
    QVariantList themeList() const;

    Answer workspace() const;
    Answer libraries() const;
    Answer createLibrary(const QVariantMap &body);
    Answer listPresentations(const QString &library) const;
    Answer searchPresentations(const QVariantMap &query) const;
    Answer createPresentation(const QString &library, const QVariantMap &body);
    Answer updatePresentation(const Found &found, const QVariantMap &body);
    Answer deletePresentation(const Found &found);
    Answer duplicatePresentation(const Found &found, const QVariantMap &body);
    Answer applyTheme(const Found &found, const QVariantMap &body);
    Answer formatText(const Found &found, const QVariantMap &body);
    Answer changeGroup(const Found &found, const QString &group, const QVariantMap &body, bool remove);
    Answer changeArrangement(const Found &found, const QString &arrangement, const QVariantMap &body, bool remove);
    Answer updateTimeline(const Found &found, const QVariantMap &body);
    Answer addSlides(const Found &found, const QVariantMap &body);
    Answer getSlide(const Found &found, const QString &slide) const;
    Answer updateSlide(const Found &found, const QString &slide, const QVariantMap &body);
    Answer deleteSlide(const Found &found, const QString &slide);
    Answer duplicateSlide(const Found &found, const QString &slide, const QVariantMap &body);
    Answer changeElement(const Found &found, const QString &slide, const QString &element, const QVariantMap &body, const QByteArray &operation);
    Answer changeThemeElement(const QString &place, const QString &slide, const QString &element, const QVariantMap &body,
                              const QByteArray &operation);
    Answer changeAction(const Found &found, const QString &slide, const QString &action, const QVariantMap &body, const QByteArray &operation);
    Answer getTheme(const QString &place, const QString &slide) const;
    Answer createTheme(const QVariantMap &body);
    Answer deleteTheme(const QString &place);
    Answer changeThemeSlide(const QString &place, const QString &slide, const QVariantMap &body, const QByteArray &operation);
    Answer createPlaylist(const QVariantMap &body);
    Answer updatePlaylist(const QString &id, const QVariantMap &body);
    Answer deletePlaylist(const QString &id);
    Answer setPlaylistItems(const QString &id, const QVariantMap &body, bool replace);
    Answer updatePlaylistItem(const QString &playlist, const QString &item, const QVariantMap &body, bool remove);
    Answer listMedia(const QVariantMap &query) const;
    Answer actionTargets() const;

    // What a slide's cue does besides, given to it in a presentation's working copy:
    // its media, its Go To Next Timer and its actions, as a slide is described
    QString cueExtras(const QString &copy, const QString &cueId, const QVariantMap &wanted) const;
    // A playlist's items as they are asked for, as PlaylistFile takes them. An error
    // message, empty on success, and the status to say it with.
    QString wantedItems(const QVariantList &items, QList<PlaylistFile::Wanted> *wanted, int *status) const;

    QPointer<Catalog> m_catalog;
    QVariantList m_groups;
    QStringList m_busy;
    QPointer<QObject> m_search;
    std::function<QVariant(const QString &)> m_askShow;
    // Of each file a copy has been kept of: when the last was, and what the file was
    // (its bytes' hash) when the request that changed it was done with it
    struct Kept
    {
        QDateTime when;
        QByteArray left;
    };
    QHash<QString, Kept> m_kept;
    // The files the request being answered has kept copies for, or looked to
    QStringList m_changing;
};
