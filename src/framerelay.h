#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

// Passes video frames from one video sink to another at a reduced rate, so a second,
// small view of a playing video costs a few frame uploads a second instead of a second
// decoder. Both properties take a QVideoSink (a VideoOutput's videoSink).
class FrameRelay : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QObject *target READ target WRITE setTarget NOTIFY targetChanged)
    // Minimum milliseconds between relayed frames.
    Q_PROPERTY(int interval MEMBER m_interval NOTIFY intervalChanged)

public:
    using QObject::QObject;

    QObject *source() const { return m_source; }
    void setSource(QObject *source);
    QObject *target() const { return m_target; }
    void setTarget(QObject *target);

signals:
    void sourceChanged();
    void targetChanged();
    void intervalChanged();

private:
    void relay(const QVideoFrame &frame);

    QPointer<QVideoSink> m_source;
    QPointer<QVideoSink> m_target;
    int m_interval = 100;
    QElapsedTimer m_sinceLast;
};
