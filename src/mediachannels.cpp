#include "mediachannels.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
}

#include <QCoreApplication>
#include <QFile>
#include <QPointer>
#include <QThreadPool>

namespace mediachannels {

Found read(const QString &path)
{
    Found found;
    if (path.isEmpty() || !QFile::exists(path))
        return found;
    AVFormatContext *context = nullptr;
    if (avformat_open_input(&context, QFile::encodeName(path).constData(), nullptr, nullptr) < 0)
        return found;
    // The header says, for nearly every file; only where it does not are the first
    // packets read to find out (which is slower)
    int stream = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (stream < 0 || context->streams[stream]->codecpar->ch_layout.nb_channels <= 0) {
        if (avformat_find_stream_info(context, nullptr) >= 0)
            stream = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    }
    if (stream >= 0) {
        const AVChannelLayout &layout = context->streams[stream]->codecpar->ch_layout;
        found.channels = qMax(0, layout.nb_channels);
        if (layout.order != AV_CHANNEL_ORDER_UNSPEC) {
            for (int index = 0; index < found.channels; ++index) {
                char name[32] = {};
                const AVChannel channel = av_channel_layout_channel_from_index(&layout, unsigned(index));
                if (channel == AV_CHAN_NONE || av_channel_name(name, sizeof name, channel) < 0) {
                    found.names.clear();
                    break;
                }
                found.names << QString::fromLatin1(name);
            }
        }
    }
    avformat_close_input(&context);
    return found;
}

} // namespace mediachannels

MediaChannels::MediaChannels(QObject *parent)
    : QObject(parent)
{
}

void MediaChannels::setPath(const QString &path)
{
    if (path == m_path)
        return;
    m_path = path;
    m_channels = -1;
    m_names.clear();
    const int asked = ++m_asked;
    emit pathChanged();
    emit found();
    if (path.isEmpty())
        return;
    // (The answer is taken on this one's thread, and only if this is still here and
    // still wants it)
    QThreadPool::globalInstance()->start([path, asked, self = QPointer<MediaChannels>(this)] {
        const mediachannels::Found read = mediachannels::read(path);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, asked, read] {
            if (!self || self->m_asked != asked)
                return;
            self->m_channels = read.channels;
            self->m_names = read.names;
            emit self->found();
        }, Qt::QueuedConnection);
    });
}
