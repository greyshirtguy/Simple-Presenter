#pragma once

#include <QObject>
#include <QPointer>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

// Says when a video sink has been given its first picture.
//
// A video that has just been started has nothing to show for a moment: about a tenth of
// a second, while the file is opened and its first frame decoded. Put on screen during
// that moment it is a flash of nothing, so whoever is about to show it waits for
// `arrived` (see TransitionLayer.qml). `sink` takes a QVideoSink (a VideoOutput's
// videoSink).
//
// Once the picture has arrived this stops listening, so it costs nothing while the video
// plays.
class FirstFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *sink READ sink WRITE setSink NOTIFY sinkChanged)
    Q_PROPERTY(bool arrived READ arrived NOTIFY arrivedChanged)

public:
    using QObject::QObject;

    QObject *sink() const { return m_sink; }
    void setSink(QObject *sink);
    bool arrived() const { return m_arrived; }

signals:
    void sinkChanged();
    void arrivedChanged();

private:
    void setArrived(bool arrived);

    QPointer<QVideoSink> m_sink;
    QMetaObject::Connection m_listening;
    bool m_arrived = false;
};
