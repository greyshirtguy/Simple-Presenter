#pragma once

#include "slidepictures.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>

#include <functional>

class QProcess;

// The app's side of the pictures of slides (see slidepictures.h): asks the process that
// draws them (slidepictureworker.h) and hands each picture back when it comes. Everything
// here is on the app's thread and is small: a line written to a pipe, and the picture's
// bytes read from it as they come (some 25 KB). Nothing is drawn, and no file read, here.
//
// The process is started the first time a picture is asked for, with the workspace's
// folder, and closed once nothing has been asked of it for five minutes (it holds a QML
// engine of its own, some 100 MB, which a show that no Remote is looking at has no use
// for); and again if the workspace changes. Two requests for the same picture while it
// is being made are one. A request not answered in 30 seconds, or a process that goes,
// fails every request waiting (500) and the next request starts it afresh.
//
// Held still while the show changes. Though the process runs only when nothing else
// wants the processor, it shares what Linux does not share out by priority: each core's
// second thread, the processor's cache, the memory, the graphics chip. Measured with a
// video under the slides (benchmark/README.md), a slide changed while pictures were being
// made had twice the work in its click, and held twice the video's frames too long. So
// when something is taken live (hold(), from the window: a slide, media, an announcement,
// a prop, a message, a look), the process is stopped where it is (SIGSTOP) until the
// change and its transition are over, and then let go on (SIGCONT): the pictures wait
// for the show, and never the show for the pictures.
class SlidePictureService : public QObject
{
    Q_OBJECT

public:
    // The picture's status (200, 404, 500), its type, and its bytes
    using Done = std::function<void(int status, const QByteArray &type, const QByteArray &bytes)>;

    explicit SlidePictureService(QObject *parent = nullptr);
    ~SlidePictureService() override;

    // Asks for a picture: `done` is called once, later, never from within this call
    void ask(const QString &workspace, const slidepictures::Asked &asked, Done done);
    // Holds the process still for the next `ms` milliseconds (or for as long as it was
    // being held already, if that is longer); nothing if it is not running
    void hold(int ms);
    // Whether the process is running
    bool running() const { return m_process != nullptr; }

signals:
    void runningChanged();

private:
    struct Waiting
    {
        slidepictures::Asked asked;
        QList<Done> done;
    };

    void start(const QString &workspace);
    void stop();
    void readOutput();
    void finish(qint64 id, int status, const QByteArray &type, const QByteArray &bytes);
    void failAll(const QString &why);
    void release();

    QProcess *m_process = nullptr;
    QString m_workspace;
    QHash<qint64, Waiting> m_waiting;
    qint64 m_lastId = 0;
    QByteArray m_buffer;
    // The head of the answer whose bytes are being read, if one is
    bool m_inAnswer = false;
    slidepictures::Answer m_answer;
    QTimer m_idle;
    QTimer m_overdue;
    // While the process is held still (see hold)
    bool m_held = false;
    QTimer m_release;
};
