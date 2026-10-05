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

// False with an empty *error if there is no file.
bool read(const QString &root, rv::data::PlaylistDocument *document, QString *error)
{
    QFile file(playlistPath(root));
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

void addNode(const rv::data::Playlist &node, int depth, const QString &root, PlaylistFile *result)
{
    const QString id = QString::fromStdString(node.uuid().string());
    const bool folder = node.has_playlists();
    result->nodes.append(QVariantMap {
        {"name", QString::fromStdString(node.name())},
        {"path", id},
        {"depth", depth},
        {"folder", folder},
    });
    if (folder) {
        for (const rv::data::Playlist &child : node.playlists().playlists())
            addNode(child, depth + 1, root, result);
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

PlaylistFile PlaylistFile::load(const QString &root, QString *error)
{
    PlaylistFile result;
    rv::data::PlaylistDocument document;
    if (!read(root, &document, error))
        return result;
    // The root node is only a container; what the user sees starts with its children.
    for (const rv::data::Playlist &child : document.root_node().playlists().playlists())
        addNode(child, 0, root, &result);
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

QString PlaylistFile::createPlaylist(const QString &root, const QString &name, QString *id)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document, true);
    if (!error.isEmpty())
        return error;

    // A file this app starts is laid out the way ProPresenter's is: one root node that
    // only holds the top-level playlists and folders.
    rv::data::Playlist *rootNode = document.mutable_root_node();
    if (!rootNode->has_uuid()) {
        document.set_type(rv::data::PlaylistDocument::TYPE_PRESENTATION);
        rootNode->mutable_uuid()->set_string(newId());
        rootNode->set_name("PLAYLIST");
        rootNode->set_expanded(true);
    }
    if (rootNode->has_items())
        return QStringLiteral("The playlists file has an unexpected layout");

    rv::data::Playlist *playlist = rootNode->mutable_playlists()->add_playlists();
    playlist->mutable_uuid()->set_string(newId());
    playlist->set_name(name.toStdString());
    playlist->mutable_items();
    *id = QString::fromStdString(playlist->uuid().string());
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

QString PlaylistFile::deletePlaylist(const QString &root, const QString &id)
{
    rv::data::PlaylistDocument document;
    const QString error = readForChange(root, &document);
    if (!error.isEmpty())
        return error;
    google::protobuf::RepeatedPtrField<rv::data::Playlist> *siblings = nullptr;
    const rv::data::Playlist *node = findNode(document.mutable_root_node(), id.toStdString(), &siblings);
    if (!node || !siblings)
        return QStringLiteral("That playlist is no longer there");
    if (node->has_playlists())
        return QStringLiteral("Folders cannot be deleted here");
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

    // Recorded both by its path on this machine and, when it is inside the folder the
    // playlists belong to, relative to that folder, which is the form that survives the
    // folder being moved or opened on another machine.
    rv::data::PlaylistItem::Presentation *presentation = item->mutable_presentation();
    rv::data::URL *url = presentation->mutable_document_path();
    url->set_absolute_string(QUrl::fromLocalFile(file).toString(QUrl::FullyEncoded).toStdString());
    const QString relative = QDir(root).relativeFilePath(file);
    if (!relative.startsWith(QLatin1String(".."))) {
        url->mutable_local()->set_root(rv::data::URL::LocalRelativePath::ROOT_SHOW);
        url->mutable_local()->set_path(relative.toStdString());
    }

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
