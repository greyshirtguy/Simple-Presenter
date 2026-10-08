#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The hotkeys of a workspace's groups: which key goes to which group.
//
// How ProPresenter keeps them. A workspace has a list of groups, Configuration/Groups:
// the names a presentation's groups are given from ("Verse 1", "Chorus"), each with a
// colour and, if it has one, a hotkey, which is a key of the keyboard and nothing held
// with it. That list is where a group's hotkey is, and ProPresenter's own list comes
// with them set: A for the first verse, S for the second, C for the chorus and so on.
// A second file, Configuration/KeyMappings, holds the key mappings a user has made
// beyond what ProPresenter starts with, to menu items, macros, props, timers and the
// like, and a mapping there may be to a group too.
//
// What is done with them here. The hotkeys in the list of groups are read, and are the
// ones that are changed: a change reads the file, sets that group's hotkey and writes
// the file back, with everything else in it as it was. The key mappings file is only
// read, for plain keys mapped to groups, which are honoured beside the others; nothing
// is written to it. Pressed while showing, a hotkey goes to the first slide of its
// group (see the operator window).
//
// A workspace with no list of groups has no hotkeys of its own, which `exists` says,
// and the operator window then goes by those a new installation starts with until one
// is changed, which makes the list.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace.
class GroupKeys : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether the workspace has a list of groups
    Q_PROPERTY(bool exists READ exists NOTIFY changed)
    // { name, label, key } for each group in the list that has a hotkey, in the list's
    // order: the name in lower case, for matching, and as it is written, for showing;
    // the key a capital letter or a digit
    Q_PROPERTY(QVariantList keys READ keys NOTIFY changed)
    // The same for the plain keys mapped to groups in the key mappings file
    Q_PROPERTY(QVariantList mappedKeys READ mappedKeys NOTIFY changed)
    // The list of groups, which may not exist yet
    Q_PROPERTY(QString path READ path NOTIFY changed)

public:
    using QObject::QObject;

    bool exists() const { return m_exists; }
    QVariantList keys() const { return m_keys; }
    QVariantList mappedKeys() const { return m_mappedKeys; }
    QString path() const;

    // Reads the hotkeys of a workspace folder. Returns an error message, empty on
    // success; a file that cannot be read is left alone, and gives no hotkeys.
    Q_INVOKABLE QString open(const QString &workspace);
    Q_INVOKABLE QString reload();

    // Sets the hotkeys of groups in the workspace's list of them. `groups` is
    // [{ name, color, key }], the key "" for none. A group the list has, by its name,
    // is given its key; one it does not have is added if it has a key. Groups of the
    // list that are not named are left as they are. If the workspace has no list, one
    // is made of all the groups given. Returns an error message, empty on success.
    Q_INVOKABLE QString setKeys(const QVariantList &groups);

signals:
    void changed();

private:
    QString m_workspace;
    bool m_exists = false;
    QVariantList m_keys;
    QVariantList m_mappedKeys;
};
