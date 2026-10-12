#include "audioprobe.h"

#include <QEventLoop>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>

namespace audioprobe {

double seconds(const QString &file)
{
    QMediaPlayer player;
    QEventLoop loop;
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    // Once the file has been opened its length is known, if it is ever to be; one that
    // cannot be opened says so.
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::InvalidMedia)
            loop.quit();
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, [&] { loop.quit(); });
    player.setSource(QUrl::fromLocalFile(file));
    if (player.mediaStatus() != QMediaPlayer::LoadedMedia && player.mediaStatus() != QMediaPlayer::InvalidMedia
        && player.error() == QMediaPlayer::NoError) {
        limit.start(2000);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    return player.duration() > 0 ? player.duration() / 1000.0 : 0.0;
}

} // namespace audioprobe
