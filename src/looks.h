#pragma once

#include "lookfile.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's looks: which layers each audience screen gets, and in what theme.
//
// lookfile.h says what a look is and how ProPresenter keeps them, and above all that
// the live look is a look of its own, apart from the saved ones. This is the same as
// the windows see it: the saved looks, the live look, and the changes the Looks window
// makes to either, each written to the workspace's file at once.
//
// What the screens draw is `live`, and nothing else. Show keeps the id of the saved
// look that was last made live (for the tick beside it in a menu, and for actions),
// and asks for one to be made live; the copying into the live look is done here
// (makeLive), since it is the file's business.
//
// Making a look live is on the path of a click on a slide: a slide, or a macro it runs,
// can have an action that goes over to a look, and in a service many do. So it is kept
// cheap in two ways. A look that is live already, and unchanged, is not made live
// again: there is nothing to do. And when there is, the screens are told at once from
// what is in memory, and ProPresenter's file is written a moment later, since writing
// a file safely waits for the disk (some 8 ms on the laptop this is made on), which a
// slide going out should not.
//
// There is one of these, which QML reaches by its name.
class Looks : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Every saved look:
    //   { id, name, transition, screens: { <screen id>: <line> } }
    // where a line is { slide, media, props, theme, themeSlide } for the layers this app
    // has, and { messages, announcements, videoInput, mask } for ProPresenter's others,
    // which are only shown. A screen a look has nothing for gets everything, with no
    // theme (see of()).
    Q_PROPERTY(QVariantList looks READ looks NOTIFY changed)
    // The live look, which is what the screens get: the same, with
    //   origin   the id of the saved look it was made from, if that is still there
    //   changed  whether it has been changed since, so that it is no longer what that
    //            saved look is
    // Its id is "" for a workspace that has no live look yet: everything, everywhere.
    Q_PROPERTY(QVariantMap live READ live NOTIFY changed)

public:
    explicit Looks(QObject *parent = nullptr);
    ~Looks() override;

    QVariantList looks() const { return m_looks; }
    QVariantMap live() const { return m_live; }

    Q_INVOKABLE QString open(const QString &workspace);

    // What the live look gives a screen: a line, as above. This is what an audience
    // screen goes by.
    Q_INVOKABLE QVariantMap liveOf(const QString &screenId) const;
    // What a saved look gives a screen. Everything, and no theme, for a look that is not
    // there or says nothing of the screen.
    Q_INVOKABLE QVariantMap of(const QString &lookId, const QString &screenId) const;

    // Changes, each answering with what went wrong, or nothing.
    //
    // Makes a saved look the live one: the screens get what it says, and whatever the
    // live look had been changed to is gone. The screens are told at once; the file is
    // written a moment later (see above), and `failed` says if that went wrong.
    Q_INVOKABLE QString makeLive(const QString &id);
    // Keeps the live look, as it has been changed, as the saved look it was made from:
    // that look then says what the screens are showing, and the live look is no longer
    // "changed". Answers with what went wrong, or nothing.
    Q_INVOKABLE QString saveLive();
    // A new saved look: a copy of the saved look `copyOf`, or of the live look if
    // `copyOf` is the live look's id; with "" it gives each of the screens named
    // everything. Answers { id, error }.
    Q_INVOKABLE QVariantMap add(const QString &name, const QStringList &screenIds, const QString &copyOf = QString());
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    Q_INVOKABLE QString remove(const QString &id);
    // Any of slide, media, props, theme and themeSlide, for a look's line for a screen:
    // a saved look's, or with the live look's id the live look's, which the screens
    // then show at once.
    Q_INVOKABLE QString setScreen(const QString &id, const QString &screenId, const QVariantMap &changes);

signals:
    void changed();
    // The live look could not be written to the workspace's file.
    void failed(const QString &error);

private:
    void reload();
    void describe();
    // Writes the look that was made live to the file, if that is still to be done, and
    // says whether there was anything to write; `error` is given what went wrong.
    // Nothing is told to anyone: it is also what is done on the way out of the app,
    // when there may be nobody left to tell.
    bool writeMadeLive(QString *error);
    // The same, and then the looks are read again and whoever is watching is told: done
    // before anything else is written to the file or read from it.
    void flush();

    QString m_workspace;
    QList<lookfile::Look> m_list;
    lookfile::Look m_liveLook;
    QVariantList m_looks;
    QVariantMap m_live;
    // The saved look that has been made live in memory and not yet in the file, by id
    QString m_madeLive;
    QTimer m_write;
};
