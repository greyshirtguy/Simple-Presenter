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
    passOnWhatIsThere();
    emit sourceChanged();
}

// The picture the source has now, for a target that would otherwise wait for the next:
// which, from a video that is standing still, never comes.
void FrameRelay::passOnWhatIsThere()
{
    if (m_source && m_target && m_source->videoFrame().isValid())
        m_target->setVideoFrame(m_source->videoFrame());
}

void FrameRelay::setTarget(QObject *target)
{
    auto *sink = qobject_cast<QVideoSink *>(target);
    if (sink == m_target)
        return;
    m_target = sink;
    passOnWhatIsThere();
    emit targetChanged();
}

void FrameRelay::relay(const QVideoFrame &frame)
{
    if (!m_target || (m_sinceLast.isValid() && m_sinceLast.elapsed() < m_interval))
        return;
    m_sinceLast.start();
    m_target->setVideoFrame(frame);
    emit relayed();
}

void MediaFeeds::offer(int playId, QObject *sink)
{
    m_sinks.insert(playId, sink);
    ++m_revision;
    emit changed();
}

void MediaFeeds::withdraw(int playId, QObject *sink)
{
    if (m_sinks.value(playId) != sink)
        return;
    m_sinks.remove(playId);
    ++m_revision;
    emit changed();
}

QObject *MediaFeeds::sink(int playId) const
{
    return m_sinks.value(playId);
}
