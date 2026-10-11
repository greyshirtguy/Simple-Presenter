#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's clear groups: the list in ProPresenter's Configuration/ClearGroups,
// for the buttons under the clears, the window they are set up in, and the show (which
// is told of them, so that a group can be cleared by its button, by F1 and by an action
// on a slide or in a macro). What a group is, how the file is read and how it is
// changed are clearfile's (see clearfile.h); this is only what QML talks to.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace.
class ClearGroups : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The groups, in the file's order, each
    //   { id, name, layers, icon, picture, image, tinted, tint, hidden }
    // layers: the layers it clears, by clearfile's names for them, in the file's order;
    // icon: its picture as the file numbers them, and picture: what that is of, in a
    // word ("All", "Bell", "" for a picture of the user's own); image: that picture of
    // the user's own, as an address an Image can be given, or "" if it has none that
    // can be shown; tinted and tint ("#rrggbb" or ""): whether the picture is tinted
    // and with what; hidden: whether its button is kept out of sight.
    Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
    // The layers there are, in ProPresenter's order: { key, name, ours }, `ours` being
    // whether this app has that layer, and so whether a group's having it can be
    // changed here
    Q_PROPERTY(QVariantList layers READ layers CONSTANT)
    // The pictures ProPresenter has for a group: { icon, name }, by their numbers
    Q_PROPERTY(QVariantList pictures READ pictures CONSTANT)

public:
    using QObject::QObject;

    QVariantList groups() const { return m_groups; }
    QVariantList layers() const;
    QVariantList pictures() const;

    // Reads the clear groups of a workspace folder. Returns an error message, empty on
    // success; a workspace with no file has ProPresenter's one group to start with.
    Q_INVOKABLE QString open(const QString &workspace);

    // Sees that the workspace has a file of its clear groups (see clearfile::keep).
    Q_INVOKABLE QString keep();

    // A new group, last in the list. Returns { id, error }.
    Q_INVOKABLE QVariantMap add(const QString &name);
    // Each changes the workspace's file and reads it again, and returns an error
    // message, empty on success (see clearfile.h).
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    Q_INVOKABLE QString setLayer(const QString &id, const QString &layer, bool on);
    Q_INVOKABLE QString setIcon(const QString &id, int icon);
    Q_INVOKABLE QString setTint(const QString &id, bool tinted, const QString &color);
    Q_INVOKABLE QString setHidden(const QString &id, bool hidden);
    Q_INVOKABLE QString move(const QString &id, int by);
    Q_INVOKABLE QString remove(const QString &id);

signals:
    void changed();

private:
    QString reload();

    QString m_workspace;
    QVariantList m_groups;
};
