#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// What is on disk under the app's root folder:
//   <root>/Libraries/<library>/*.pro   presentations, one flat folder per library
//   <root>/Media/...                   images and videos, in any depth of folders
// Kept up to date as files and folders come and go.
class Catalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created in main.cpp")
    Q_PROPERTY(QString librariesDirectory READ librariesDirectory CONSTANT)
    Q_PROPERTY(QString mediaDirectory READ mediaDirectory CONSTANT)
    // { name, path } for each library folder, sorted by name.
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY changed)
    // { name, path, depth } for the media folder and every folder under it, in tree order.
    Q_PROPERTY(QVariantList mediaFolders READ mediaFolders NOTIFY changed)
    // Goes up whenever anything on disk changed; bind to it to re-query the folder contents.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit Catalog(const QString &root, QObject *parent = nullptr);

    QString librariesDirectory() const { return m_librariesDirectory; }
    QString mediaDirectory() const { return m_mediaDirectory; }
    QVariantList libraries() const { return m_libraries; }
    QVariantList mediaFolders() const { return m_mediaFolders; }
    int revision() const { return m_revision; }

    // { name, path, arrangements, arrangement, detail } for each presentation directly in a
    // library folder, sorted by name. `detail` is the selected arrangement in brackets.
    Q_INVOKABLE QVariantList documentsIn(const QString &library) const;
    // { name, path, source, video } for each image and video directly in a folder, sorted by name.
    Q_INVOKABLE QVariantList mediaIn(const QString &folder) const;
    // Returns { name, path, slides, arrangements, arrangement, error }; error is empty on
    // success. Follows the arrangement the presentation has selected.
    Q_INVOKABLE QVariantMap open(const QString &path) const;
    // Selects the named arrangement in the presentation file, or none for "", and saves
    // the file. Returns an error message, empty on success.
    Q_INVOKABLE QString setArrangement(const QString &path, const QString &arrangement);
    // Makes a slide (by its `id`) trigger the given media file, replacing any media it
    // triggered before, and saves the presentation file. Returns an error message, empty
    // on success.
    Q_INVOKABLE QString setSlideMedia(const QString &path, const QString &slideId, const QString &mediaPath);
    // Stops a slide triggering media, and saves the presentation file.
    Q_INVOKABLE QString removeSlideMedia(const QString &path, const QString &slideId);

signals:
    void changed();

private:
    void rescan();

    QString m_librariesDirectory;
    QString m_mediaDirectory;
    QVariantList m_libraries;
    QVariantList m_mediaFolders;
    int m_revision = 0;
    QFileSystemWatcher m_watcher;
};
