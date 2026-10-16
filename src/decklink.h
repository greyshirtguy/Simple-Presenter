#pragma once

#include "decklinkformats.h"

#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <QQmlParserStatus>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QOpenGLContext;
class QOpenGLFramebufferObject;
class QQuickRenderControl;
class QQuickWindow;
class QSurface;

namespace decklink {
class Card;
}

// Screens sent out through a Blackmagic DeckLink card (a DeckLink Duo 2 is the one this
// was made for), as ProPresenter sends them: video only, for now.
//
// The outputs. Blackmagic's Desktop Video Setup decides how a card's connectors are
// arranged, and names what it makes of them; the app offers what it finds, by those
// names, and changes nothing of the arrangement (nor does ProPresenter). A pair of
// connectors linked there for fill and key is one output, which can key; the connector
// it takes for the key is not an output by itself while it is, and is left out (where
// ProPresenter shows it as "none"). Under each output are the formats it can send (see
// decklinkformats.h), and choosing one is choosing the output.
//
// Keying, as ProPresenter offers it for an output that can (its help: "Alpha Keying in
// ProPresenter"). The alpha type: none, which sends the screen as it is, on black;
// premultiplied, which sends it on nothing, its colours as the app draws them; straight,
// the same with the colours made straight. Then internal keying, where the card lays the
// screen over what comes into it, or external, where it sends the fill out of the one
// connector and the key, the screen's opacity in black and white, out of the other, for
// a vision mixer to key with. And the blend, how strongly it is laid over, as a share.
//
// How a screen is sent. It is drawn out of sight, as an NDI screen is (see NdiScreen,
// whose drawing this is again), read back into a frame the card made, and handed to the
// card with DisplayVideoFrameSync on a thread of its own. Only when something has
// changed: the card goes on showing the last frame it was given until it is given
// another, so nothing is drawn while nothing changes, and a still slide costs nothing:
// not even a wake of the clock. A change is drawn at most once a frame of the format,
// on the beat of the frames drawn before it. (A choice for cost over timing:
// a frame goes out on the card's next frame after it is ready, rather than being
// scheduled to the frame. ProPresenter's way is not known.)
//
// Blackmagic's library. libDeckLinkAPI.so comes with Desktop Video, which a computer
// with a card has installed; it is not in the app's package. It is looked for the first
// time an output is asked about (the settings' screens, or a screen set to go out
// through a card), never at start otherwise, so a computer with no card pays nothing.
// The headers it is built against (third_party/decklink) want Desktop Video 14.3 or
// later.
//
// For the tests, which cannot have a card: with SP_TEST_DECKLINK set, two outputs are
// pretended (one that can key, as a linked pair is, and one that cannot), and a screen
// sent to either is drawn and read back as for a card, and kept, for the tests to look
// at (picture()). Only handing the frame to the card is left out.

// The outputs there are. There is one of these, which QML reaches by its name.
class DeckLink : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether Blackmagic's library is on this computer and could be used, as found by
    // the last refresh(); what version of it; and if it is not, why
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    // The outputs, as Desktop Video Setup names them:
    //   { id, name, model, internal, external, formats: [ name ] }
    // `internal` and `external` say which keying it can do (a linked pair can do both);
    // `formats` are those of decklinkformats it can send.
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    // Whether refresh() has been done, so that an empty list means there are none
    Q_PROPERTY(bool looked READ looked NOTIFY changed)

public:
    explicit DeckLink(QObject *parent = nullptr);

    bool available() const;
    QString version() const;
    QString problem() const;
    QVariantList outputs() const;
    bool looked() const;

    // Looks for the library, if that has not been done, and for the outputs again.
    Q_INVOKABLE void refresh();

signals:
    void changed();
};

// One screen sent out through a card's output. (Started once all it is given is there,
// and not as each part of it is: a card started and started again as it was set up
// would show the screen blinking.)
class DeckLinkScreen : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT
    // The output, by its id (see DeckLink), and the name it had, for saying which it was
    // when it is not there
    Q_PROPERTY(QString output READ output WRITE setOutput NOTIFY settingsChanged)
    Q_PROPERTY(QString outputName MEMBER m_outputName NOTIFY settingsChanged)
    // The format's name (see decklinkformats)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY settingsChanged)
    // "none", "premultiplied" or "straight"; "internal" or "external"; a share, 0 to 100
    Q_PROPERTY(QString alpha READ alpha WRITE setAlpha NOTIFY settingsChanged)
    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY settingsChanged)
    Q_PROPERTY(int blend READ blend WRITE setBlend NOTIFY settingsChanged)
    // The format's size
    Q_PROPERTY(int width READ width NOTIFY settingsChanged)
    Q_PROPERTY(int height READ height NOTIFY settingsChanged)
    // Whether it is to be sent at all
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // What is drawn: the screen's scene, which is made to fill it
    Q_PROPERTY(QQuickItem *scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_CLASSINFO("DefaultProperty", "scene")
    // Whether it is going out; if it is not though it should be, why; and how many
    // frames have been handed to the card
    Q_PROPERTY(bool sending READ sending NOTIFY statusChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY statusChanged)
    // (Of its own, as it changes at every frame: the status's bindings are not to run so.)
    Q_PROPERTY(int framesSent READ framesSent NOTIFY framesSentChanged)

public:
    explicit DeckLinkScreen(QObject *parent = nullptr);
    ~DeckLinkScreen() override;

    QString output() const { return m_output; }
    void setOutput(const QString &output);
    QString format() const { return m_format; }
    void setFormat(const QString &format);
    QString alpha() const { return m_alpha; }
    void setAlpha(const QString &alpha);
    QString key() const { return m_key; }
    void setKey(const QString &key);
    int blend() const { return m_blend; }
    void setBlend(int blend);
    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    bool active() const { return m_active; }
    void setActive(bool active);
    QQuickItem *scene() const { return m_scene; }
    void setScene(QQuickItem *scene);
    bool sending() const { return m_sending; }
    QString problem() const { return m_problem; }
    int framesSent() const { return m_framesSent; }
    // The picture last handed to the card, as the card was given it (blue, green, red
    // and opacity); a null image before there is one. (For the tests.)
    QImage picture() const;

    void classBegin() override {}
    void componentComplete() override;

signals:
    void settingsChanged();
    void activeChanged();
    void sceneChanged();
    void statusChanged();
    void framesSentChanged();

private:
    // Brings what there is into line with what is wanted: started, stopped, or started
    // again on another output, in another format or keyed otherwise. Asked for soon,
    // once for all the changes of a moment: a screen given another output is given its
    // name and its format too, one after the other, and a card started again for each
    // would be started on an output by a name it no longer has.
    void settleSoon();
    void settle();
    bool start();
    void stop();
    bool prepareDrawing();
    void releaseDrawing();
    void fit();
    // Something has changed: drawn when it is next allowed, and a frame of the card is free
    void schedule();
    void tick();
    void draw(int slot);
    void fail(const QString &why);
    // A frame the card has shown, and so can be drawn into again (on this object's thread),
    // in the sending started `run`th
    void shown(int run, int slot, bool ok);

    bool m_complete = false;
    bool m_settling = false;
    QString m_output;
    QString m_outputName;
    QString m_format = QStringLiteral("1080p29.97");
    QString m_alpha = QStringLiteral("none");
    QString m_key = QStringLiteral("external");
    int m_blend = 100;
    QSize m_size {1920, 1080};
    bool m_active = false;
    QPointer<QQuickItem> m_scene;

    // The drawing, as NdiScreen's
    QQuickRenderControl *m_control = nullptr;
    QQuickWindow *m_window = nullptr;
    QOpenGLContext *m_context = nullptr;
    QSurface *m_surface = nullptr;
    QOpenGLFramebufferObject *m_target = nullptr;
    bool m_initialised = false;
    bool m_changed = true;

    // The sending: the card's side (a real one, or the tests' pretend one), the thread it
    // is handed frames on, which of its frames are free to be drawn into, and what it was
    // started with
    std::unique_ptr<decklink::Card> m_card;
    QThread m_thread;
    QObject *m_worker = nullptr;
    QList<int> m_free;
    bool m_sending = false;
    QString m_sendingAs;
    // The size it was started at, which is the size of the card's frames: a format
    // changed is the size changed at once, and the card a moment later
    QSize m_sentSize;
    int m_framesSent = 0;
    int m_lastSlot = -1;
    // Which start this is: a frame said to be shown after the sending was stopped and
    // started again is of the frames before, and not put back among the new ones
    int m_run = 0;
    QString m_problem;
    QTimer m_timer;
    QElapsedTimer m_clock;
    // When the next frame may be drawn, on m_clock
    qint64 m_next = 0;
};
