#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QSize>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

class QOffscreenSurface;
class QSurface;
class QWindow;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QQuickRenderControl;
class QQuickWindow;

// A screen sent over the network, as an NDI source.
//
// NDI is a way of sending video between programs and machines on a local network: a
// program announces a source by name, and anything on the network that takes NDI (a
// vision mixer, OBS, a monitor on another computer) can pick it. Here a screen can be
// such a source, as well as or in place of being on a display.
//
// How a screen gets there. The screen's scene (an OutputScene or a StageScene, as for a
// window) is given to an NdiScreen, which has no window anyone sees. It draws the scene
// with the graphics card into a picture held in memory, at the size and the rate set
// for it, reads each picture back, and hands it to NDI's library to compress and send.
// A scene that has not changed since the last picture is not drawn again: the picture
// is sent as it was. All of it happens only while something on the network is taking
// the source; with nothing taking it, a picture goes out once a second so that the
// source can be found and previewed, and the rest of the work is saved.
//
// NDI's library is not part of this app, and is not open: it is NDI's own, free to
// have, and is looked for when the app starts (see Ndi). Without it everything here
// stands still and says why.
//
// NDI(R) is a registered trademark of Vizrt NDI AB. https://ndi.video
class Ndi : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether NDI's library was found, and can be used on this processor
    Q_PROPERTY(bool available READ available CONSTANT)
    // Which it is, as it says itself; and where it was found
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString path READ path CONSTANT)
    // Why it is not available, for whoever is setting a screen up: where it was looked
    // for, and where to get it
    Q_PROPERTY(QString problem READ problem CONSTANT)
    // The folder of the app's own that the library can be put in
    Q_PROPERTY(QString folder READ folder CONSTANT)

public:
    using QObject::QObject;

    bool available() const;
    QString version() const;
    QString path() const;
    QString problem() const;
    QString folder() const;
};

class NdiScreen : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // What the source is called on the network (after the computer's name, which NDI
    // puts first)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    // The size it is drawn and sent at, in pixels, and its frames a second as two whole
    // numbers (30000 over 1001 for 29.97)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY sizeChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY sizeChanged)
    Q_PROPERTY(int rateNumerator MEMBER m_rateNumerator NOTIFY rateChanged)
    Q_PROPERTY(int rateDenominator MEMBER m_rateDenominator NOTIFY rateChanged)
    // Whether it is to be sent at all
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // What is drawn: the screen's scene, which is made to fill it
    Q_PROPERTY(QQuickItem *scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_CLASSINFO("DefaultProperty", "scene")
    // Whether it is on the network; if it is not though it should be, why; how many
    // are taking it; and how many pictures have gone out
    Q_PROPERTY(bool sending READ sending NOTIFY statusChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY statusChanged)
    Q_PROPERTY(int receivers READ receivers NOTIFY statusChanged)
    Q_PROPERTY(int framesSent READ framesSent NOTIFY statusChanged)

public:
    explicit NdiScreen(QObject *parent = nullptr);
    ~NdiScreen() override;

    QString name() const { return m_name; }
    void setName(const QString &name);
    int width() const { return m_size.width(); }
    void setWidth(int width);
    int height() const { return m_size.height(); }
    void setHeight(int height);
    bool active() const { return m_active; }
    void setActive(bool active);
    QQuickItem *scene() const { return m_scene; }
    void setScene(QQuickItem *scene);
    bool sending() const { return m_sender != nullptr; }
    QString problem() const { return m_problem; }
    int receivers() const { return m_receivers; }
    int framesSent() const { return m_framesSent; }

signals:
    void nameChanged();
    void sizeChanged();
    void rateChanged();
    void activeChanged();
    void sceneChanged();
    void statusChanged();

private:
    // Brings what there is into line with what is wanted: started, stopped, or started
    // again with another name, size or rate.
    void settle();
    bool start();
    void stop();
    bool prepareDrawing();
    void releaseDrawing();
    void tick();
    bool draw();
    void fail(const QString &why);

    QString m_name;
    QSize m_size {1920, 1080};
    int m_rateNumerator = 30;
    int m_rateDenominator = 1;
    bool m_active = false;
    QPointer<QQuickItem> m_scene;

    // The drawing
    QQuickRenderControl *m_control = nullptr;
    QQuickWindow *m_window = nullptr;
    QOpenGLContext *m_context = nullptr;
    // What the graphics card is told the drawing is for, though it goes into a picture
    // in memory: the window, which is never shown
    QSurface *m_surface = nullptr;
    QOpenGLFramebufferObject *m_target = nullptr;
    bool m_initialised = false;
    bool m_changed = true;

    // The sending: NDI's sender, and the two pictures that take turns (NDI keeps the
    // one it was last handed until it is handed the next)
    void *m_sender = nullptr;
    std::vector<uchar> m_pictures[2];
    int m_picture = 0;
    bool m_hasPicture = false;
    QString m_sendingAs;
    QSize m_sendingSize;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_due = 0;
    qint64 m_lastSent = 0;
    qint64 m_lastCounted = 0;
    QString m_problem;
    int m_receivers = 0;
    int m_framesSent = 0;
};
