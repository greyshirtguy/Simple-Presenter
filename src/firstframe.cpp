#include "firstframe.h"

#include <QVideoFrame>
#include <QVideoFrameFormat>

void FirstFrame::setSink(QObject *sink)
{
    auto *videoSink = qobject_cast<QVideoSink *>(sink);
    if (videoSink == m_sink)
        return;
    disconnect(m_listening);
    m_sink = videoSink;
    setArrived(m_sink && m_sink->videoFrame().isValid(), m_sink ? m_sink->videoFrame() : QVideoFrame());
    if (m_sink && !m_arrived) {
        // Frames are handed over on the thread that decodes them; this is called on ours.
        // A sink is also given empty frames, when a video starts and stops.
        m_listening = connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            if (m_arrived || !frame.isValid())
                return;
            disconnect(m_listening);
            setArrived(true, frame);
        });
    }
    emit sinkChanged();
}

void FirstFrame::setArrived(bool arrived, const QVideoFrame &frame)
{
    if (arrived == m_arrived)
        return;
    m_arrived = arrived;
    m_description = !arrived || !frame.isValid() ? QString()
        : QStringLiteral("%1x%2, %3, frames arriving %4").arg(frame.width()).arg(frame.height())
              .arg(QVideoFrameFormat::pixelFormatToString(frame.pixelFormat()),
                   frame.handleType() == QVideoFrame::RhiTextureHandle
                       ? QStringLiteral("as textures (decoded by the graphics chip)")
                       : QStringLiteral("in memory (decoded by the processor, or copied back from the graphics chip)"));
    emit arrivedChanged();
}
