#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Keeps the screens from going to sleep while the show is on one.
//
// A desktop left alone blanks its screens after a few minutes, and then locks or goes
// to sleep. Left alone is just what a presenter's computer is during a long reading or
// a sermon: nobody touches it, and the words on the wall go dark. So while the output
// or the stage window is showing, the desktop is asked not to count the time as idle,
// the way a video player asks while a film plays; when neither is showing, or the app
// closes, the asking ends and the desktop goes back to its own settings.
//
// It is asked over the session's message bus: of org.freedesktop.ScreenSaver, which
// most desktops answer to (GNOME among them, by way of its session manager), and
// failing that of org.gnome.SessionManager itself. A desktop that answers to neither is
// noted in the log once, and nothing else comes of it.
//
// There is one of these, which QML reaches by its name: the operator window sets
// `wanted`.
class Awake : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether the screens are to be kept awake
    Q_PROPERTY(bool wanted READ wanted WRITE setWanted NOTIFY wantedChanged)
    // Whether the desktop has said it will
    Q_PROPERTY(bool held READ held NOTIFY heldChanged)

public:
    using QObject::QObject;
    ~Awake() override;

    bool wanted() const { return m_wanted; }
    void setWanted(bool wanted);
    bool held() const { return m_held; }

signals:
    void wantedChanged();
    void heldChanged();

private:
    void ask(int service);
    void release();

    bool m_wanted = false;
    bool m_held = false;
    bool m_asking = false;
    bool m_refused = false;
    // Which of the services answered, and what it gave to end the asking with
    int m_service = -1;
    uint m_cookie = 0;
};
