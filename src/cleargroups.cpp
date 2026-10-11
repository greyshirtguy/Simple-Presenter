#include "cleargroups.h"

#include "clearfile.h"

#include <QHash>
#include <QUuid>

namespace {

// A layer as it is called on the screen
QString layerName(const QString &key)
{
    static const QHash<QString, QString> names {
        {"audio", "Audio"}, {"audioEffects", "Audio Effects"}, {"messages", "Messages"}, {"props", "Props"},
        {"announcements", "Announcements"}, {"slide", "Slide"}, {"media", "Media"}, {"videoInput", "Video Input"},
    };
    return names.value(key, key);
}

// A picture of the user's own as an address an Image takes: the bytes themselves, in
// the address. Only for what is plainly a PNG or a JPEG by how it begins; anything else
// (ProPresenter on a Mac may well keep a TIFF) is not shown, and stays in the file.
//
// A GUESS: no file with such a picture in it has been looked at, so what ProPresenter
// keeps there is not known.
QString imageAddress(const QByteArray &image)
{
    if (image.startsWith("\x89PNG"))
        return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(image.toBase64());
    if (image.startsWith("\xff\xd8\xff"))
        return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(image.toBase64());
    return {};
}

} // namespace

QVariantList ClearGroups::layers() const
{
    QVariantList list;
    const QStringList ours = clearfile::ourLayers();
    for (const QString &key : clearfile::layers())
        list.append(QVariantMap {{"key", key}, {"name", layerName(key)}, {"ours", ours.contains(key)}});
    return list;
}

QVariantList ClearGroups::pictures() const
{
    QVariantList list;
    for (int icon = 1; icon <= clearfile::pictures(); ++icon)
        list.append(QVariantMap {{"icon", icon}, {"name", clearfile::pictureName(icon)}});
    return list;
}

QString ClearGroups::open(const QString &workspace)
{
    m_workspace = workspace;
    return reload();
}

QString ClearGroups::reload()
{
    QString error;
    m_groups.clear();
    if (!m_workspace.isEmpty()) {
        const QList<clearfile::Group> groups = clearfile::groups(m_workspace, &error);
        for (const clearfile::Group &group : groups) {
            m_groups.append(QVariantMap {
                {"id", group.id},
                {"name", group.name},
                {"layers", group.layers},
                {"icon", group.icon},
                {"picture", clearfile::pictureName(group.icon)},
                {"image", imageAddress(group.image)},
                {"tinted", group.tinted},
                {"tint", group.tint},
                {"hidden", group.hidden},
            });
        }
    }
    emit changed();
    return error;
}

QString ClearGroups::keep()
{
    return m_workspace.isEmpty() ? QString() : clearfile::keep(m_workspace);
}

QVariantMap ClearGroups::add(const QString &name)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
    const QString error = clearfile::add(m_workspace, id, name);
    reload();
    return {{"id", error.isEmpty() ? id : QString()}, {"error", error}};
}

QString ClearGroups::rename(const QString &id, const QString &name)
{
    const QString error = clearfile::rename(m_workspace, id, name);
    reload();
    return error;
}

QString ClearGroups::setLayer(const QString &id, const QString &layer, bool on)
{
    const QString error = clearfile::setLayer(m_workspace, id, layer, on);
    reload();
    return error;
}

QString ClearGroups::setIcon(const QString &id, int icon)
{
    const QString error = clearfile::setIcon(m_workspace, id, icon);
    reload();
    return error;
}

QString ClearGroups::setTint(const QString &id, bool tinted, const QString &color)
{
    const QString error = clearfile::setTint(m_workspace, id, tinted, color);
    reload();
    return error;
}

QString ClearGroups::setHidden(const QString &id, bool hidden)
{
    const QString error = clearfile::setHidden(m_workspace, id, hidden);
    reload();
    return error;
}

QString ClearGroups::move(const QString &id, int by)
{
    const QString error = clearfile::move(m_workspace, id, by);
    reload();
    return error;
}

QString ClearGroups::remove(const QString &id)
{
    const QString error = clearfile::remove(m_workspace, id);
    reload();
    return error;
}
