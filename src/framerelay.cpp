#include "framerelay.h"

#include <QVideoFrame>

void FrameRelay::setSource(QObject *source)
{
    auto *sink = qobject_cast<QVideoSink *>(source);
    if (sink == m_source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);
    m_source = sink;
    m_sinceLast.invalidate();
    if (m_source)
        connect(m_source, &QVideoSink::videoFrameChanged, this, &FrameRelay::relay);
    else if (m_target)
        m_target->setVideoFrame(QVideoFrame());
    emit sourceChanged();
}

void FrameRelay::setTarget(QObject *target)
{
    auto *sink = qobject_cast<QVideoSink *>(target);
    if (sink == m_target)
        return;
    m_target = sink;
    emit targetChanged();
}

void FrameRelay::relay(const QVideoFrame &frame)
{
    if (!m_target || (m_sinceLast.isValid() && m_sinceLast.elapsed() < m_interval))
        return;
    m_sinceLast.start();
    m_target->setVideoFrame(frame);
}
