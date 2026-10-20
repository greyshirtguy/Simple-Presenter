#include "playlistactions.h"

#include <QUuid>

namespace playlistactions {

namespace {

using Action = rv::data::Action;

QString text(const std::string &value)
{
    return QString::fromStdString(value);
}

QString quoted(const QString &name)
{
    return name.isEmpty() ? QStringLiteral("(unnamed)") : QStringLiteral("“%1”").arg(name);
}

} // namespace

bool isPlaylist(const Action &action)
{
    return action.type() == Action::ACTION_TYPE_MEDIA_BIN_PLAYLIST || action.type() == Action::ACTION_TYPE_AUDIO_BIN_PLAYLIST;
}

QVariantMap describe(const Action &action)
{
    if (!isPlaylist(action))
        return {};
    const bool media = action.type() == Action::ACTION_TYPE_MEDIA_BIN_PLAYLIST;
    const Action::PlaylistItemType &playlist = action.playlist_item();
    const QString name = text(playlist.playlist_name());
    return {
        {"kind", media ? QStringLiteral("mediaPlaylist") : QStringLiteral("audioPlaylist")},
        {"playlistId", text(playlist.playlist_uuid().string())},
        {"playlistName", name},
        {"itemId", text(playlist.item_uuid().string())},
        {"itemName", text(playlist.item_name())},
        {"selects", playlist.select_playlist()},
        {"retrigger", playlist.always_retrigger()},
        {"title", QStringLiteral("Play the %1 playlist %2%3").arg(media ? QStringLiteral("media") : QStringLiteral("audio"), quoted(name),
                                                                  playlist.always_retrigger() ? QStringLiteral(", again if it is playing") : QString())},
    };
}

QString build(const QVariantMap &wanted, Action *action)
{
    const QString kind = wanted.value("kind").toString();
    if (kind != QLatin1String("mediaPlaylist") && kind != QLatin1String("audioPlaylist"))
        return QStringLiteral("That is not an action for a playlist");
    const QString id = wanted.value("playlistId").toString();
    const QString name = wanted.value("playlistName").toString();
    if (id.isEmpty() && name.isEmpty())
        return QStringLiteral("An action for a playlist needs the playlist");
    if (action->uuid().string().empty())
        action->mutable_uuid()->set_string(QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString());
    action->set_isenabled(true);
    action->set_type(kind == QLatin1String("mediaPlaylist") ? Action::ACTION_TYPE_MEDIA_BIN_PLAYLIST : Action::ACTION_TYPE_AUDIO_BIN_PLAYLIST);
    Action::PlaylistItemType *playlist = action->mutable_playlist_item();
    // Another playlist: an item of the one before is no item of this one.
    if (text(playlist->playlist_uuid().string()) != id || text(playlist->playlist_name()) != name) {
        playlist->clear_item_uuid();
        playlist->clear_item_name();
    }
    if (id.isEmpty())
        playlist->clear_playlist_uuid();
    else
        playlist->mutable_playlist_uuid()->set_string(id.toStdString());
    playlist->set_playlist_name(name.toStdString());
    if (wanted.contains("retrigger"))
        playlist->set_always_retrigger(wanted.value("retrigger").toBool());
    return {};
}

} // namespace playlistactions
