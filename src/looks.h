#pragma once

#include "lookfile.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's looks: which layers each audience screen gets, and in what theme.
//
// lookfile.h says what a look is and how ProPresenter keeps them; this is the list as
// the windows see it, and the changes the Looks section of the settings makes, each
// written to the workspace's file at once. Which look is live is Show's business.
//
// There is one of these, which QML reaches by its name.
class Looks : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Every look:
    //   { id, name, transition, screens: { <screen id>: { slide, media, props, theme, themeSlide } } }
    // A screen a look has nothing for gets everything, with no theme (see of()).
    Q_PROPERTY(QVariantList looks READ looks NOTIFY changed)
    // The look that was live when ProPresenter last wrote the file, by id, or "": where
    // a workspace opened here for the first time starts
    Q_PROPERTY(QString startsWith READ startsWith NOTIFY changed)

public:
    using QObject::QObject;

    QVariantList looks() const { return m_looks; }
    QString startsWith() const { return m_startsWith; }

    Q_INVOKABLE QString open(const QString &workspace);

    // What a look gives a screen: { slide, media, props, theme, themeSlide }. Everything,
    // and no theme, for a look that is not there ("" is no look at all) or says nothing
    // of the screen.
    Q_INVOKABLE QVariantMap of(const QString &lookId, const QString &screenId) const;

    // Changes, each answering with what went wrong, or nothing. A new look gives each
    // of the screens named everything; add() answers { id, error }.
    Q_INVOKABLE QVariantMap add(const QString &name, const QStringList &screenIds);
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    Q_INVOKABLE QString remove(const QString &id);
    // Any of slide, media, props, theme and themeSlide, for a look's line for a screen
    Q_INVOKABLE QString setScreen(const QString &id, const QString &screenId, const QVariantMap &changes);

signals:
    void changed();

private:
    void reload();

    QString m_workspace;
    QList<lookfile::Look> m_list;
    QVariantList m_looks;
    QString m_startsWith;
};
