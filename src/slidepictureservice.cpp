#include "slidepictureservice.h"

#include "sessionlog.h"

#include <QCoreApplication>
#include <QProcess>

#include <signal.h>

namespace {

constexpr int idleMs = 5 * 60 * 1000;
constexpr int overdueMs = 30 * 1000;

}

SlidePictureService::SlidePictureService(QObject *parent)
    : QObject(parent)
{
    m_idle.setSingleShot(true);
    m_idle.setInterval(idleMs);
    connect(&m_idle, &QTimer::timeout, this, [this] {
        if (m_waiting.isEmpty())
            stop();
    });
    m_overdue.setSingleShot(true);
    m_overdue.setInterval(overdueMs);
    connect(&m_overdue, &QTimer::timeout, this, [this] { failAll(QStringLiteral("it answered nothing for 30 seconds")); });
    m_release.setSingleShot(true);
    connect(&m_release, &QTimer::timeout, this, &SlidePictureService::release);
}

void SlidePictureService::hold(int ms)
{
    if (!m_process || m_process->state() != QProcess::Running)
        return;
    if (!m_held) {
        ::kill(pid_t(m_process->processId()), SIGSTOP);
        m_held = true;
        // (Its pictures are late by as long as it is held: not overdue)
        m_overdue.stop();
    }
    if (!m_release.isActive() || m_release.remainingTime() < ms)
        m_release.start(ms);
}

void SlidePictureService::release()
{
    m_release.stop();
    if (!m_held)
        return;
    m_held = false;
    if (m_process)
        ::kill(pid_t(m_process->processId()), SIGCONT);
    if (!m_waiting.isEmpty())
        m_overdue.start();
}

SlidePictureService::~SlidePictureService()
{
    stop();
}

void SlidePictureService::ask(const QString &workspace, const slidepictures::Asked &asked, Done done)
{
    if (m_process && workspace != m_workspace)
        stop();
    if (!m_process)
        start(workspace);
    // (The same picture asked for again while it is being made waits for the same answer)
    for (auto it = m_waiting.begin(); it != m_waiting.end(); ++it) {
        if (it->asked == asked) {
            it->done.append(std::move(done));
            return;
        }
    }
    const qint64 id = ++m_lastId;
    m_waiting.insert(id, {asked, {std::move(done)}});
    m_process->write(slidepictures::askedLine(id, asked));
    m_idle.stop();
    if (!m_overdue.isActive() && !m_held)
        m_overdue.start();
}

void SlidePictureService::start(const QString &workspace)
{
    m_workspace = workspace;
    m_buffer.clear();
    m_inAnswer = false;
    m_process = new QProcess(this);
    // (Its own messages, and Qt's, go to the app's log, a line each)
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        const QList<QByteArray> lines = m_process->readAllStandardError().trimmed().split('\n');
        for (const QByteArray &line : lines) {
            if (!line.trimmed().isEmpty())
                SessionLog::write("network", QString::fromUtf8(line.trimmed()));
        }
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, &SlidePictureService::readOutput);
    QProcess *process = m_process;
    connect(m_process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        if (process != m_process)
            return;
        if (status == QProcess::CrashExit || code != 0)
            SessionLog::write("network", QStringLiteral("the process that draws the pictures of slides has stopped (%1)")
                                             .arg(status == QProcess::CrashExit ? QStringLiteral("it crashed") : QStringLiteral("code %1").arg(code)));
        m_process->deleteLater();
        m_process = nullptr;
        m_held = false;
        m_release.stop();
        emit runningChanged();
        if (!m_waiting.isEmpty())
            failAll(QStringLiteral("it stopped"));
    });
    m_process->start(QCoreApplication::applicationFilePath(), {QStringLiteral("--slide-picture-worker"), workspace});
    SessionLog::write("network", QStringLiteral("the process that draws the pictures of slides for the API is started"));
    emit runningChanged();
}

void SlidePictureService::stop()
{
    if (!m_process)
        return;
    // (A process held still would not see its input close)
    release();
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    // (Its input closed, it finishes the picture it is on, if it is on one, and stops. It
    // is not waited for: that would hold up the app's thread. One that has not stopped in
    // three seconds is stopped; and when the app quits, the object's going stops it.)
    process->closeWriteChannel();
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    QTimer::singleShot(3000, process, [process] { process->kill(); });
    m_overdue.stop();
    if (!m_waiting.isEmpty())
        failAll(QStringLiteral("it was closed"));
    SessionLog::write("network", QStringLiteral("the process that draws the pictures of slides is closed"));
    emit runningChanged();
}

void SlidePictureService::readOutput()
{
    m_buffer += m_process->readAllStandardOutput();
    for (;;) {
        if (!m_inAnswer) {
            const qsizetype end = m_buffer.indexOf('\n');
            if (end < 0)
                return;
            const QByteArray line = m_buffer.left(end);
            m_buffer.remove(0, end + 1);
            if (!slidepictures::readAnswer(line, &m_answer)) {
                failAll(QStringLiteral("it said what could not be read"));
                return;
            }
            m_inAnswer = true;
        }
        if (m_buffer.size() < m_answer.length)
            return;
        const QByteArray bytes = m_buffer.left(m_answer.length);
        m_buffer.remove(0, m_answer.length);
        m_inAnswer = false;
        if (m_answer.status != 200 && m_answer.status != 404)
            SessionLog::write("network", QStringLiteral("a picture of a slide could not be made: %1").arg(m_answer.error));
        finish(m_answer.id, m_answer.status, m_answer.type, bytes);
    }
}

void SlidePictureService::finish(qint64 id, int status, const QByteArray &type, const QByteArray &bytes)
{
    const Waiting waiting = m_waiting.take(id);
    if (m_waiting.isEmpty()) {
        m_overdue.stop();
        m_idle.start();
    } else if (!m_held) {
        m_overdue.start();
    }
    for (const Done &done : waiting.done)
        done(status, type, bytes);
}

void SlidePictureService::failAll(const QString &why)
{
    if (m_waiting.isEmpty())
        return;
    SessionLog::write("network", QStringLiteral("%1 pictures of slides were not made: the process that draws them %2").arg(m_waiting.size()).arg(why));
    const QHash<qint64, Waiting> waiting = std::exchange(m_waiting, {});
    m_overdue.stop();
    // (A process that answers nothing is let go: the next request starts it afresh)
    if (m_process && why.startsWith(QStringLiteral("it answered nothing"))) {
        QProcess *process = std::exchange(m_process, nullptr);
        process->disconnect(this);
        process->kill();
        process->deleteLater();
        m_held = false;
        m_release.stop();
        emit runningChanged();
    }
    // (Each answered after this call, never within it: see ask)
    QMetaObject::invokeMethod(this, [waiting] {
        for (const Waiting &one : waiting) {
            for (const Done &done : one.done)
                done(500, {}, {});
        }
    }, Qt::QueuedConnection);
}
