#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's masks, for the windows: what there are, each with its slide ready to
// be drawn, and the making, naming and removing of them.
//
// A mask is a slide laid over everything else a screen shows (maskfile.h says what it
// is, how ProPresenter keeps it, and what of that is a guess). Which mask a screen
// gets is a look's to say (see Looks): an audience screen draws the mask the live look
// gives it, over its messages, and that is all there is to showing one. What is on a
// mask is edited in the editor, as a prop's slide is (PresentationEditor::openMasks).
//
// There is one of these, which QML reaches by its name.
class Masks : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Every mask, in the file's order: { id, name, slide }, `slide` being a slide map
    // (see proconvert.h), for drawing
    Q_PROPERTY(QVariantList masks READ masks NOTIFY changed)
    // The file they are in, which is what the editor opens to edit them
    Q_PROPERTY(QString path READ path NOTIFY changed)

public:
    using QObject::QObject;

    QVariantList masks() const { return m_masks; }
    QString path() const;

    // Reads the masks of a workspace; and reads them again, after the editor has been
    // at them. Each answers with what went wrong, or nothing.
    Q_INVOKABLE QString open(const QString &workspace);
    Q_INVOKABLE QString reload();

    // The mask of that id, as above; an empty map for an id no mask has.
    Q_INVOKABLE QVariantMap find(const QString &id) const;

    // A new mask with nothing on it, last in the list: answers { id, error }. Named
    // `name`, or "Mask", with a number after it if another has the name.
    Q_INVOKABLE QVariantMap add(const QString &name = QString());
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    // The mask goes, and with it every look's giving of it to a screen (the looks are
    // to be read again by whoever asked: see maskfile::remove).
    Q_INVOKABLE QString remove(const QString &id);

signals:
    void changed();

private:
    QString m_workspace;
    QVariantList m_masks;
};
