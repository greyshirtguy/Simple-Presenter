#include "firstframe.h"

#include <QVideoFrame>

void FirstFrame::setSink(QObject *sink)
{
    auto *videoSink = qobject_cast<QVideoSink *>(sink);
    if (videoSink == m_sink)
        return;
    disconnect(m_listening);
    m_sink = videoSink;
    setArrived(m_sink && m_sink->videoFrame().isValid());
    if (m_sink && !m_arrived) {
        // Frames are handed over on the thread that decodes them; this is called on ours.
        // A sink is also given empty frames, when a video starts and stops.
        m_listening = connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            if (m_arrived || !frame.isValid())
                return;
            disconnect(m_listening);
            setArrived(true);
        });
    }
    emit sinkChanged();
}

void FirstFrame::setArrived(bool arrived)
{
    if (arrived == m_arrived)
        return;
    m_arrived = arrived;
    emit arrivedChanged();
}
