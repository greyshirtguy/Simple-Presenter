#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// The guide, for QML: docs/guide.md, which the app carries in its resources, as the ?
// button at the right of the toolbar shows it (GuideWindow.qml). One text for the
// repository and the app, so that the two cannot say different things. Read the first
// time it is asked for, which is when the window is first opened: nothing at start.
class Guide : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The guide, from its first heading on (what comes before it is the repository's way
    // back to the README), as rich text made from its Markdown, its links in the app's
    // orange: Qt draws a Markdown text's links in a dark blue that no colour of a Text's
    // changes, which on the window's dark ground could hardly be read
    Q_PROPERTY(QString text READ text CONSTANT)

public:
    using QObject::QObject;

    QString text() const;
};
