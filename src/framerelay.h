#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

// Passes video frames from one video sink to another, so that a second view of a
// playing video costs the handing over of its frames and not a second decoder: at a
// reduced rate for a small view (the preview), or every frame (`interval` 0) for
// another screen. Both properties take a QVideoSink (a VideoOutput's videoSink). A
// view that starts being fed while the video is standing still (paused, or at its end)
// is given the picture that is standing there.
//
// It says `relayed` each time it passes a frame on. Whatever else in the same window
// follows the video can change then, and be drawn in the redraw the frame is about to
// cause, at no cost of its own: the transport does.
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
    void relayed();

private:
    void relay(const QVideoFrame &frame);
    void passOnWhatIsThere();

    QPointer<QVideoSink> m_source;
    QPointer<QVideoSink> m_target;
    int m_interval = 100;
    QElapsedTimer m_sinceLast;
};

// Where the screens that do not play a video themselves find its frames.
//
// A video is played once, by the scene of one screen, however many screens show it (see
// MediaContent). That scene offers the sink its frames arrive at under the number the
// playing was given when it was asked for (`playId`, which the operator window counts
// up), and the scene of every other screen, told to show the same playing, asks for the
// sink of that number and feeds itself from it with a FrameRelay.
//
// There is one of these, which QML reaches by its name.
class MediaFeeds : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Goes up whenever a sink is offered or withdrawn: read it in a binding that asks
    // for one, to have the binding ask again.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    using QObject::QObject;

    int revision() const { return m_revision; }
    Q_INVOKABLE void offer(int playId, QObject *sink);
    // (Only if it is still that sink's: a later playing may have been given the number.)
    Q_INVOKABLE void withdraw(int playId, QObject *sink);
    Q_INVOKABLE QObject *sink(int playId) const;

signals:
    void changed();

private:
    QHash<int, QPointer<QObject>> m_sinks;
    int m_revision = 0;
};
