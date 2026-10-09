#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's themes: what there are, and dressing slides in them.
//
// A theme is a set of slides that show how slides should look; themefile.h says how
// ProPresenter keeps them, and what dressing a slide in one does. Here they are listed
// for the windows, and used in the two ways a theme is:
//
//   - On a presentation, for good. apply() dresses slides of a presentation in a theme
//     slide and writes the presentation back; a copy of the file as it was is kept
//     first, beside the editor's backups.
//   - On a screen, for the moment. dressed() gives a slide as it would look in a theme
//     slide without anything being written, which is what a look that gives a screen a
//     theme shows there (see Looks): the same words, large in the room and a line
//     across the foot of the stream, and the presentation as it was.
//
// There is one of these, which QML reaches by its name.
class Themes : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The themes as they are kept, folders within folders:
    //   { kind: "folder", name, place, items: [ ... ] }
    //   { kind: "theme", name, place, slides: [ { id, name, slide } ] }
    // `place` is the path under the workspace's Themes folder, by which a theme is
    // known; `slide` is a slide map (see proconvert.h), for drawing.
    Q_PROPERTY(QVariantList tree READ tree NOTIFY changed)
    // Every theme, in the order of the alphabet, as the entries above
    Q_PROPERTY(QVariantList themes READ themes NOTIFY changed)

public:
    using QObject::QObject;

    QVariantList tree() const { return m_tree; }
    QVariantList themes() const { return m_themes; }

    // Reads the themes of a workspace folder.
    Q_INVOKABLE void open(const QString &workspace);
    Q_INVOKABLE void reload();

    // A theme by its place, and one of its slides by its id, as the entries above; or null.
    Q_INVOKABLE QVariant theme(const QString &place) const;
    Q_INVOKABLE QVariant slide(const QString &place, const QString &slideId) const;

    // Dresses slides of a presentation in a theme slide, for good: the slides with these
    // ids, or with none named every slide. Answers with what went wrong, or nothing.
    Q_INVOKABLE QString apply(const QString &presentation, const QStringList &slideIds, const QString &place, const QString &slideId);
    // A slide of a presentation as it looks in a theme slide, as a slide map; the slide
    // as it is in the file if there is no such theme slide. Nothing is written.
    Q_INVOKABLE QVariantMap dressed(const QString &presentation, const QString &slideId, const QString &place, const QString &themeSlideId);

    // Making and changing themes, each written at once. add() makes a theme of that name
    // with one slide to start from, and answers { place, slideId, error }.
    Q_INVOKABLE QVariantMap add(const QString &name);
    Q_INVOKABLE QString remove(const QString &place);
    Q_INVOKABLE QVariantMap addSlide(const QString &place);
    Q_INVOKABLE QString renameSlide(const QString &place, const QString &slideId, const QString &name);
    Q_INVOKABLE QString removeSlide(const QString &place, const QString &slideId);

signals:
    void changed();

private:
    QVariantList read(const QString &place);

    QString m_workspace;
    QVariantList m_tree;
    QVariantList m_themes;
    QSet<QString> m_elementIds;
    QHash<QString, QVariantMap> m_dressed;
};
