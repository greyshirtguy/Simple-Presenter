#include "playlistfile.h"

#include "prodocument.h"

#include "propresenter.pb.h"

#include <QColor>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>

namespace {

QString playlistPath(const QString &root)
{
    return QDir(root).absoluteFilePath("Playlists/Library");
}

QString mediaPlaylistPath(const QString &root)
{
    return QDir(root).absoluteFilePath("Playlists/Media");
}

// False with an empty *error if there is no file.
bool readFile(const QString &path, rv::data::PlaylistDocument *document, QString *error)
{
    QFile file(path);
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Cannot open the playlists: %1").arg(file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    if (!document->ParseFromArray(data.constData(), int(data.size()))) {
        *error = QStringLiteral("The playlists file is not a ProPresenter 7 playlist document");
        return false;
    }
    return true;
}

bool read(const QString &root, rv::data::PlaylistDocument *document, QString *error)
{
    return readFile(playlistPath(root), document, error);
}

// A presentation is recorded by the path it had on the machine that wrote the playlist,
// and usually also relative to the ProPresenter folder. The relative form is the one
// that still means something here; failing that, the recorded path, and failing that
// a file of the same name in any library.
QString findPresentation(const rv::data::URL &url, const QString &root)
{
    if (url.has_local() && url.local().root() == rv::data::URL::LocalRelativePath::ROOT_SHOW) {
        const QString relative = QDir(root).absoluteFilePath(QString::fromStdString(url.local().path()));
        if (QFile::exists(relative))
            return relative;
    }
    const QString recorded = QUrl(QString::fromStdString(url.absolute_string())).toLocalFile();
    if (!recorded.isEmpty() && QFile::exists(recorded))
        return recorded;

    const QString name = recorded.isEmpty() ? QFileInfo(QString::fromStdString(url.local().path())).fileName()
                                            : QFileInfo(recorded).fileName();
    if (name.isEmpty())
        return {};
    QDirIterator it(QDir(root).absoluteFilePath("Libraries"), {name}, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

QVariantMap toRow(const rv::data::PlaylistItem &item, const QString &root)
{
    QVariantMap row {
        {"path", QString::fromStdString(item.uuid().string())},
        {"name", QString::fromStdString(item.name())},
        {"kind", QStringLiteral("other")},
        {"file", QString()},
        {"missing", false},
        {"playlistItem", true},
        {"arrangement", QString()},
        {"arrangements", QStringList()},
        {"detail", QString()},
        {"color", QString()},
    };

    if (item.has_header()) {
        const rv::data::Color &color = item.header().color();
        row.insert("kind", QStringLiteral("header"));
        if (item.header().has_color() && color.alpha() > 0) {
            const auto unit = [](float v) { return qBound(0.0f, v, 1.0f); };
            row.insert("color", QColor::fromRgbF(unit(color.red()), unit(color.green()), unit(color.blue())).name());
        }
    } else if (item.has_presentation()) {
        const QString file = findPresentation(item.presentation().document_path(), root);
        row.insert("kind", QStringLiteral("presentation"));
        row.insert("icon", QStringLiteral("presentation"));
        row.insert("file", file);
        row.insert("missing", file.isEmpty());

        // The row's arrangement is its own, by name where the presentation still has one
        // of that name, so a stale choice falls back to every group.
        QString arrangement = QString::fromStdString(item.presentation().arrangement_name());
        QStringList arrangements;
        QString selectedInFile;
        if (!file.isEmpty())
            ProDocument::arrangementsOf(file, &arrangements, &selectedInFile);
        if (!arrangements.contains(arrangement))
            arrangement.clear();
        row.insert("arrangement", arrangement);
        row.insert("arrangements", arrangements);
        if (!arrangement.isEmpty())
            row.insert("detail", u'[' + arrangement + u']');
    }
    return row;
}

void addNode(const rv::data::Playlist &node, const QString &parent, int depth, const QString &root,
             PlaylistFile *result)
{
    const QString id = QString::fromStdString(node.uuid().string());
    const bool folder = node.has_playlists();
    result->nodes.append(QVariantMap {
        {"name", QString::fromStdString(node.name())},
        {"path", id},
        {"parent", parent},
        {"depth", depth},
        {"folder", folder},
        {"icon", folder ? QStringLiteral("folder") : QStringLiteral("playlist")},
    });
    if (folder) {
        for (const rv::data::Playlist &child : node.playlists().playlists())
            addNode(child, id, depth + 1, root, result);
        return;
    }
    QVariantList rows;
    for (const rv::data::PlaylistItem &item : node.items().items()) {
        if (!item.is_hidden())
            rows.append(toRow(item, root));
    }
    result->items.insert(id, rows);
}

// Everything in the document, including what this app does not know about, is written
// back as it was read, and the file is replaced in one step.
QString write(const QString &root, const rv::data::PlaylistDocument &document)
{
    std::string bytes;
    if (!document.SerializeToString(&bytes))
        return QStringLiteral("Cannot encode the playlists");
    QDir().mkpath(QFileInfo(playlistPath(root)).absolutePath());
    QSaveFile file(playlistPath(root));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size())
        || !file.commit())
        return QStringLiteral("Cannot write the playlists: %1").arg(file.errorString());
    return {};
}

// Reads the file for changing. With no file yet, *document is left empty for the caller
// to start one, if `mayBeAbsent`; otherwise that is an error.
QString readForChange(const QString &root, rv::data::PlaylistDocument *document, bool mayBeAbsent = false)
{
    QString error;
    if (read(root, document, &error) || (error.isEmpty() && mayBeAbsent))
        return {};
    return error.isEmpty() ? QStringLiteral("There are no playlists yet") : error;
}

std::string newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

// The node with this id, and through *siblings the list it sits in (null for the root).
rv::data::Playlist *findNode(rv::data::Playlist *node, const std::string &id,
                             google::protobuf::RepeatedPtrField<rv::data::Playlist> **siblings = nullptr)
{
    if (node->uuid().string() == id)
        return node;
    if (!node->has_playlists())
        return nullptr;
    auto *children = node->mutable_playlists()->mutable_playlists();
    for (rv::data::Playlist &child : *children) {
        if (child.uuid().string() == id) {
            if (siblings)
                *siblings = children;
            return &child;
        }
        if (rv::data::Playlist *found = findNode(&child, id, siblings))
            return found;
    }
    return nullptr;
}

// A file this app starts is laid out the way ProPresenter's is: one root node that only
// holds the top-level playlists and folders.
rv::data::Playlist *rootNode(rv::data::PlaylistDocument *document)
{
    rv::data::Playlist *node = document->mutable_root_node();
    if (!node->has_uuid()) {
        document->set_type(rv::data::PlaylistDocument::TYPE_PRESENTATION);
        node->mutable_uuid()->set_string(newId());
        node->set_name("PLAYLIST");
        node->set_expanded(true);
    }
    return node;
}

// Records a presentation file both by its path on this machine and, when it is inside
// the folder the playlists belong to, relative to that folder, which is the form that
// survives the folder being moved or opened on another machine.
void setDocumentPath(rv::data::URL *url, const QString &file, const QString &root)
{
    url->Clear();
    url->set_absolute_string(QUrl::fromLocalFile(file).toString(QUrl::FullyEncoded).toStdString());
    const QString relative = QDir(root).relativeFilePath(file);
    if (!relative.startsWith(QLatin1String(".."))) {
        url->mutable_local()->set_root(rv::data::URL::LocalRelativePath::ROOT_SHOW);
        url->mutable_local()->set_path(relative.toStdString());
    }
}

// Gives a copied node, and everything in it, ids of its own, and points its
// presentations at the files of that name in `library`.
void adopt(rv::data::Playlist *node, const QString &library, const QString &root)
{
    node->mutable_uuid()->set_string(newId());
    if (node->has_playlists()) {
        for (rv::data::Playlist &child : *node->mutable_playlists()->mutable_playlists())
            adopt(&child, library, root);
        return;
    }
    for (rv::data::PlaylistItem &item : *node->mutable_items()->mutable_items()) {
        item.mutable_uuid()->set_string(newId());
        if (!item.has_presentation())
            continue;
        rv::data::URL *url = item.mutable_presentation()->mutable_document_path();
        QString name = QFileInfo(QString::fromStdString(url->local().path())).fileName();
        if (name.isEmpty())
            name = QFileInfo(QUrl(QString::fromStdString(url->absolute_string())).path()).fileName();
        if (!name.isEmpty())
            setDocumentPath(url, QDir(library).absoluteFilePath(name), root);
    }
}

// Where a media file is on this machine: by its path relative to the ProPresenter
// folder, else by the path it was recorded with, else by its name anywhere under Media.
QString findMediaFile(const rv::data::URL &url, const QString &root)
{
    if (url.has_local() && url.local().root() == rv::data::URL::LocalRelativePath::ROOT_SHOW) {
        const QString relative = QDir(root).absoluteFilePath(QString::fromStdString(url.local().path()));
        if (QFile::exists(relative))
            return relative;
    }
    const QString recorded = QUrl(QString::fromStdString(url.absolute_string())).toLocalFile();
    if (!recorded.isEmpty() && QFile::exists(recorded))
        return recorded;
    const QString name = recorded.isEmpty() ? QFileInfo(QString::fromStdString(url.local().path())).fileName()
                                            : QFileInfo(recorded).fileName();
    if (name.isEmpty())
        return {};
    QDirIterator it(QDir(root).absoluteFilePath("Media"), {name}, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

void addMediaNode(const rv::data::Playlist &node, const QString &parent, int depth, const QString &root,
                  PlaylistFile *result)
{
    const QString id = QStringLiteral("playlist:") + QString::fromStdString(node.uuid().string());
    const bool folder = node.has_playlists();
    result->nodes.append(QVariantMap {
        {"name", QString::fromStdString(node.name())},
        {"path", id},
        {"parent", parent},
        {"depth", depth},
        {"folder", folder},
        {"icon", folder ? QStringLiteral("folder") : QStringLiteral("mediaPlaylist")},
    });
    if (folder) {
        for (const rv::data::Playlist &child : node.playlists().playlists())
            addMediaNode(child, id, depth + 1, root, result);
        return;
    }

    // Each row is a cue whose action is the media to play.
    QVariantList rows;
    for (const rv::data::PlaylistItem &item : node.items().items()) {
        if (item.is_hidden() || !item.has_cue())
            continue;
        for (const rv::data::Action &action : item.cue().actions()) {
            if (!action.has_media())
                continue;
            const rv::data::Media &media = action.media().element();
            if (!media.has_video() && !media.has_image())
                continue;
            const QString file = findMediaFile(media.url(), root);
            QString name = QString::fromStdString(item.name());
            if (name.isEmpty())
                name = QFileInfo(QUrl(QString::fromStdString(media.url().absolute_string())).path()).fileName();
            rows.append(QVariantMap {
                {"name", file.isEmpty() ? name : QFileInfo(file).fileName()},
                {"path", file},
                {"source", file.isEmpty() ? QUrl() : QUrl::fromLocalFile(file)},
                {"video", media.has_video()},
                {"missing", file.isEmpty()},
            });
            break;
        }
    }
    result->items.insert(id, rows);
}

rv::data::PlaylistItem *findItem(rv::data::Playlist *node, const std::string &id)
{
    if (node->has_playlists()) {
        for (rv::data::Playlist &child : *node->mutable_playlists()->mutable_playlists()) {
            if (rv::data::PlaylistItem *item = findItem(&child, id))
                return item;
        }
    } else if (node->has_items()) {
        for (rv::data::PlaylistItem &item : *node->mutable_items()->mutable_items()) {
            if (item.uuid().string() == id)
                return &item;
        }
    }
    return nullptr;
}

} // namespace

PlaylistFile PlaylistFile::loadMedia(const QString &root, QString *error)
{
    PlaylistFile result;
    rv::data::PlaylistDocument document;
    if (!readFile(mediaPlaylistPath(root), &document, error))
        return result;
    for (const rv::data::Playlist &child : document.root_node().playlists().playlists())
        addMediaNode(child, QString(), 0, root, &result);
    return result;
}

PlaylistFile PlaylistFile::load(const QString &root, QString *error)
{
    PlaylistFile result;
    rv::data::PlaylistDocument document;
    if (!read(root, &document, error))
        return result;
    // The root node is only a container; what the user sees starts with its children.
    for (const rv::data::Playlist &child : document.root_node().playlists().playlists())
        addNode(child, QString(), 0, root, &result);
    return result;
}

QString PlaylistFile::setItemArrangement(const QString &root, const QString &itemId,
                                         const QString &arrangementId, const QString &arrangementName)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;

    rv::data::PlaylistItem *item = findItem(document.mutable_root_node(), itemId.toStdString());
    if (!item || !item->has_presentation())
        return QStringLiteral("The playlist no longer has that entry");

    rv::data::PlaylistItem::Presentation *presentation = item->mutable_presentation();
    if (arrangementName.isEmpty()) {
        presentation->clear_arrangement();
        presentation->clear_arrangement_name();
    } else {
        presentation->mutable_arrangement()->set_string(arrangementId.toStdString());
        presentation->set_arrangement_name(arrangementName.toStdString());
    }

    return write(root, document);
}

QString PlaylistFile::createNode(const QString &root, const QString &name, const QString &parentId, bool folder,
                                 QString *id)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document, true);
    if (!error.isEmpty())
        return error;

    rv::data::Playlist *top = rootNode(&document);
    rv::data::Playlist *parent = parentId.isEmpty() ? top : findNode(top, parentId.toStdString());
    if (!parent)
        return QStringLiteral("That folder is no longer there");
    if (parent->has_items())
        return QStringLiteral("A playlist cannot hold other playlists");

    rv::data::Playlist *node = parent->mutable_playlists()->add_playlists();
    node->mutable_uuid()->set_string(newId());
    node->set_name(name.toStdString());
    // Which of the two lists a node has is what makes it a folder or a playlist.
    if (folder) {
        node->set_expanded(true);
        node->mutable_playlists();
    } else {
        node->mutable_items();
    }
    *id = QString::fromStdString(node->uuid().string());
    return write(root, document);
}

QString PlaylistFile::renameNode(const QString &root, const QString &id, const QString &name)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;
    rv::data::Playlist *node = findNode(document.mutable_root_node(), id.toStdString());
    if (!node)
        return QStringLiteral("That playlist is no longer there");
    node->set_name(name.toStdString());
    return write(root, document);
}

QString PlaylistFile::removeNode(const QString &root, const QString &id)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;
    google::protobuf::RepeatedPtrField<rv::data::Playlist> *siblings = nullptr;
    const rv::data::Playlist *node = findNode(document.mutable_root_node(), id.toStdString(), &siblings);
    if (!node || !siblings)
        return QStringLiteral("That is no longer there");
    for (int i = 0; i < siblings->size(); ++i) {
        if (&siblings->Get(i) == node) {
            siblings->DeleteSubrange(i, 1);
            break;
        }
    }
    return write(root, document);
}

QString PlaylistFile::addPresentation(const QString &root, const QString &playlistId, const QString &file)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;
    rv::data::Playlist *playlist = findNode(document.mutable_root_node(), playlistId.toStdString());
    if (!playlist || playlist->has_playlists())
        return QStringLiteral("That playlist is no longer there");

    rv::data::PlaylistItem *item = playlist->mutable_items()->add_items();
    item->mutable_uuid()->set_string(newId());
    item->set_name(QFileInfo(file).completeBaseName().toStdString());

    rv::data::PlaylistItem::Presentation *presentation = item->mutable_presentation();
    setDocumentPath(presentation->mutable_document_path(), file, root);

    QStringList arrangements;
    QString selected;
    if (ProDocument::arrangementsOf(file, &arrangements, &selected) && !selected.isEmpty()) {
        presentation->mutable_arrangement()->set_string(ProDocument::arrangementId(file, selected).toStdString());
        presentation->set_arrangement_name(selected.toStdString());
    }
    return write(root, document);
}

QString PlaylistFile::removeItem(const QString &root, const QString &itemId)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;

    // Walk every playlist for the one holding the row.
    const std::string id = itemId.toStdString();
    QList<rv::data::Playlist *> pending {document.mutable_root_node()};
    while (!pending.isEmpty()) {
        rv::data::Playlist *node = pending.takeFirst();
        if (node->has_playlists()) {
            for (rv::data::Playlist &child : *node->mutable_playlists()->mutable_playlists())
                pending.append(&child);
            continue;
        }
        auto *items = node->mutable_items()->mutable_items();
        for (int i = 0; i < items->size(); ++i) {
            if (items->Get(i).uuid().string() == id) {
                items->DeleteSubrange(i, 1);
                return write(root, document);
            }
        }
    }
    return QStringLiteral("The playlist no longer has that entry");
}

QString PlaylistFile::moveItem(const QString &root, const QString &itemId, const QString &targetId, bool after)
{
    if (itemId == targetId)
        return {};
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;

    const auto indexOf = [](const google::protobuf::RepeatedPtrField<rv::data::PlaylistItem> &items, const QString &id) {
        for (int i = 0; i < items.size(); ++i) {
            if (items.Get(i).uuid().string() == id.toStdString())
                return i;
        }
        return -1;
    };

    QList<rv::data::Playlist *> pending {document.mutable_root_node()};
    while (!pending.isEmpty()) {
        rv::data::Playlist *node = pending.takeFirst();
        if (node->has_playlists()) {
            for (rv::data::Playlist &child : *node->mutable_playlists()->mutable_playlists())
                pending.append(&child);
            continue;
        }
        auto *items = node->mutable_items()->mutable_items();
        const int from = indexOf(*items, itemId);
        if (from < 0)
            continue;
        if (indexOf(*items, targetId) < 0)
            return QStringLiteral("Rows can only be moved within their own playlist");

        // Take the row out, find where the target now is, add the row at the end and
        // walk it back up to its place.
        const rv::data::PlaylistItem moved = items->Get(from);
        items->DeleteSubrange(from, 1);
        const int to = indexOf(*items, targetId) + (after ? 1 : 0);
        *items->Add() = moved;
        for (int i = items->size() - 1; i > to; --i)
            items->SwapElements(i, i - 1);
        return write(root, document);
    }
    return QStringLiteral("The playlist no longer has that entry");
}

QString PlaylistFile::moveNode(const QString &root, const QString &id, const QString &targetId, const QString &where)
{
    if (id == targetId)
        return {};
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;

    rv::data::Playlist *top = document.mutable_root_node();
    google::protobuf::RepeatedPtrField<rv::data::Playlist> *siblings = nullptr;
    rv::data::Playlist *node = findNode(top, id.toStdString(), &siblings);
    if (!node || !siblings || !findNode(top, targetId.toStdString()))
        return QStringLiteral("That is no longer there");
    if (findNode(node, targetId.toStdString()))
        return QStringLiteral("A folder cannot be moved into itself");

    // Take the node out, then find the target afresh and put the node by or in it.
    const rv::data::Playlist moved = *node;
    for (int i = 0; i < siblings->size(); ++i) {
        if (&siblings->Get(i) == node) {
            siblings->DeleteSubrange(i, 1);
            break;
        }
    }

    google::protobuf::RepeatedPtrField<rv::data::Playlist> *destination = nullptr;
    rv::data::Playlist *target = findNode(top, targetId.toStdString(), &destination);
    if (where == QLatin1String("onto")) {
        if (!target->has_playlists())
            return QStringLiteral("Only a folder can hold playlists");
        *target->mutable_playlists()->add_playlists() = moved;
        return write(root, document);
    }
    if (!destination)
        return QStringLiteral("That is no longer there");
    int to = 0;
    while (to < destination->size() && &destination->Get(to) != target)
        ++to;
    if (where == QLatin1String("after"))
        ++to;
    *destination->Add() = moved;
    for (int i = destination->size() - 1; i > to; --i)
        destination->SwapElements(i, i - 1);
    return write(root, document);
}

QString PlaylistFile::importPlaylists(const QString &root, const QByteArray &data, const QString &library,
                                      const QString &parentId, QString *id)
{
    rv::data::PlaylistDocument imported;
    if (!imported.ParseFromArray(data.constData(), int(data.size())) || !imported.root_node().has_playlists())
        return QStringLiteral("The archive's playlist cannot be read");

    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document, true);
    if (!error.isEmpty())
        return error;
    rv::data::Playlist *top = rootNode(&document);
    rv::data::Playlist *parent = parentId.isEmpty() ? top : findNode(top, parentId.toStdString());
    if (!parent || parent->has_items())
        parent = top;

    for (const rv::data::Playlist &source : imported.root_node().playlists().playlists()) {
        rv::data::Playlist *node = parent->mutable_playlists()->add_playlists();
        *node = source;
        adopt(node, library, root);
        if (id->isEmpty())
            *id = QString::fromStdString(node->uuid().string());
    }
    return write(root, document);
}
