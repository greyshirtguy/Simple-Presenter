#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The labels a workspace keeps ready to give its slides: the list in ProPresenter's
// Configuration/Labels, for the slide menu to offer and the settings to look after.
// What a label is, how the file is read and how it is changed are labelfile's (see
// labelfile.h); this is only what QML talks to.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace.
class Labels : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The labels kept ready, in the list's order: { text, color }, the colour "#rrggbb"
    // or "" for none
    Q_PROPERTY(QVariantList presets READ presets NOTIFY changed)

public:
    using QObject::QObject;

    QVariantList presets() const { return m_presets; }

    // Reads the labels of a workspace folder. Returns an error message, empty on
    // success; a workspace with no list has none.
    Q_INVOKABLE QString open(const QString &workspace);

    // Each changes the workspace's list and reads it again, and returns an error
    // message, empty on success (see labelfile.h for what is not kept).
    Q_INVOKABLE QString add(const QString &text, const QString &color);
    Q_INVOKABLE QString change(int index, const QString &text, const QString &color);
    Q_INVOKABLE QString remove(int index);

signals:
    void changed();

private:
    QString reload();

    QString m_workspace;
    QVariantList m_presets;
};
