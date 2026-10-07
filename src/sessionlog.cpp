#include "sessionlog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QMutex>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGRendererInterface>
#include <QScreen>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#include <execinfo.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

namespace {

// ---- The file, and a line into it

// The file, as the system knows it, or -1 while there is no log. A line goes to it with
// one call to the system, from whichever thread has it to say; the system keeps the
// lines whole and in order.
std::atomic<int> fileDescriptor {-1};
// Held for as long as it takes to put a line together and hand it over.
QBasicMutex lineMutex;
QString logFile;
// The file's name, and the name the part of a long log before it is kept under, as the
// system takes them
QByteArray logFileName;
QByteArray earlierFileName;
// A log that passes this size goes on in a new file, and the one before it is kept.
const qint64 partLimit = 2 * 1024 * 1024;
qint64 bytesInPart = 0;
// The lines that say what the app is running on, kept so that each further part of a
// long log can start with them.
QByteArray preamble;
const qsizetype preambleLimit = 24 * 1024;
// Seconds east of Greenwich, so that a line's time can be worked out by arithmetic alone.
std::atomic<long> utcOffset {0};
QElapsedTimer running;

// A line is: the time (12 characters), two spaces, what kind of thing it is (10), two
// spaces, and the text.
const int timeWidth = 12;
const int kindWidth = 10;
const int headWidth = timeWidth + 2 + kindWidth + 2;

void twoDigits(char *out, long value)
{
    out[0] = char('0' + value / 10 % 10);
    out[1] = char('0' + value % 10);
}

// The time of day as "HH:MM:SS.mmm", with nothing after it. It asks the system for the
// time and does sums, which is all that is safe to do while a crash is being written up.
void stamp(char *out)
{
    timespec now {};
    clock_gettime(CLOCK_REALTIME, &now);
    long seconds = (now.tv_sec + utcOffset) % 86400;
    if (seconds < 0)
        seconds += 86400;
    twoDigits(out, seconds / 3600);
    out[2] = ':';
    twoDigits(out + 3, seconds / 60 % 60);
    out[5] = ':';
    twoDigits(out + 6, seconds % 60);
    out[8] = '.';
    const long milliseconds = now.tv_nsec / 1000000;
    out[9] = char('0' + milliseconds / 100);
    twoDigits(out + 10, milliseconds % 100);
}

void readClockOffset()
{
    const time_t now = time(nullptr);
    tm local {};
    localtime_r(&now, &local);
    utcOffset = local.tm_gmtoff;
}

// The start of a line: its time and its kind, filled out to the width of the columns.
void head(char *out, const char *kind)
{
    stamp(out);
    memset(out + timeWidth, ' ', headWidth - timeWidth);
    memcpy(out + timeWidth + 2, kind, qMin(strlen(kind), size_t(kindWidth)));
}

// Text made fit to be one entry: further lines of it set in under the first, and an
// endless one cut short.
QByteArray entry(const QString &text)
{
    QByteArray bytes = text.toUtf8().trimmed();
    const qsizetype limit = 6000;
    if (bytes.size() > limit) {
        const qsizetype more = bytes.size() - limit;
        bytes.truncate(limit);
        bytes += " ... (" + QByteArray::number(more) + " more bytes left out)";
    }
    bytes.replace('\r', "");
    bytes.replace('\n', '\n' + QByteArray(headWidth, ' '));
    return bytes;
}

void putLine(const char *kind, const QByteArray &text);

// Starts a new file for a log that has grown long. What there was becomes the part
// before it, in place of any part before that; the new one starts with what the app is
// running on, so that it can be read by itself.
// (Nothing of Qt's is used for it that could have something to say: this is done with
// the lock held, which what Qt says would wait on for ever.)
void newPart()
{
    const QByteArray kept = earlierFileName.mid(earlierFileName.lastIndexOf('/') + 1);
    putLine("log", "This file is full. It is kept as \"" + kept + "\", and the log goes on in a new file.");
    ::close(fileDescriptor);
    ::unlink(earlierFileName.constData());
    ::rename(logFileName.constData(), earlierFileName.constData());
    fileDescriptor = ::open(logFileName.constData(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    bytesInPart = 0;
    if (fileDescriptor < 0)
        return;
    const QByteArray again = "Simple Presenter session log, continued. The lines down to the next gap are from the start of the session.\n\n"
                             + preamble + '\n';
    bytesInPart += qMax<ssize_t>(0, ::write(fileDescriptor, again.constData(), size_t(again.size())));
    putLine("log", "Continued from \"" + kept + "\", which has what came before this.");
}

// One line into the file. lineMutex is held.
void putLine(const char *kind, const QByteArray &text)
{
    if (fileDescriptor < 0)
        return;
    char start[headWidth];
    head(start, kind);
    char newline = '\n';
    const iovec parts[] = {{start, size_t(headWidth)}, {const_cast<char *>(text.constData()), size_t(text.size())}, {&newline, 1}};
    bytesInPart += qMax<ssize_t>(0, ::writev(fileDescriptor, parts, 3));
    const bool sayingWhatItRunsOn = !strcmp(kind, "start") || !strcmp(kind, "system") || !strcmp(kind, "display") || !strcmp(kind, "graphics");
    if (sayingWhatItRunsOn && preamble.size() < preambleLimit) {
        preamble.append(start, headWidth);
        preamble.append(text);
        preamble.append('\n');
    }
    if (bytesInPart > partLimit && strcmp(kind, "log") != 0)
        newPart();
}

// ---- What Qt has to say

// Qt's messages are not the app's to pace, and a fault can have one coming with every
// frame. So one that is the same as the last is counted and not written again, and of a
// flood only one a second gets through, after the first sixty.
QByteArray lastMessage;
char lastMessageKind[kindWidth + 1] = "";
int lastMessageRepeats = 0;
char lastMessageAt[timeWidth] = {};
const double mostTokens = 60;
double tokens = mostTokens;
qint64 tokensAt = 0;
int leftOut = 0;
char leftOutSince[timeWidth] = {};

// Says how often the last message came again, if it did. lineMutex is held.
void settleRepeats()
{
    if (lastMessageRepeats > 0) {
        putLine("qt", "The message above came " + QByteArray::number(lastMessageRepeats) + " more time"
                      + (lastMessageRepeats == 1 ? "" : "s") + ", the last at " + QByteArray(lastMessageAt, timeWidth) + ".");
    }
    lastMessageRepeats = 0;
    lastMessage.clear();
}

// A message of Qt's into the file, if it is not one too many. lineMutex is held.
void putMessage(const char *kind, const QByteArray &text)
{
    if (text == lastMessage && !strcmp(kind, lastMessageKind)) {
        ++lastMessageRepeats;
        stamp(lastMessageAt);
        return;
    }
    settleRepeats();
    const qint64 now = running.elapsed();
    tokens = qMin(mostTokens, tokens + (now - tokensAt) / 1000.0);
    tokensAt = now;
    if (tokens < 1) {
        if (leftOut++ == 0)
            stamp(leftOutSince);
        return;
    }
    if (leftOut > 0) {
        putLine("qt", QByteArray::number(leftOut) + " messages from Qt are left out here, from " + QByteArray(leftOutSince, timeWidth)
                      + " on: they were coming faster than is worth keeping.");
        leftOut = 0;
    }
    tokens -= 1;
    lastMessage = text;
    qstrncpy(lastMessageKind, kind, sizeof(lastMessageKind));
    putLine(kind, text);
}

QtMessageHandler handlerBefore = nullptr;
thread_local bool insideHandler = false;

// Everything Qt says, and everything said through it (a console.log in QML, a qWarning
// here), goes where it went before and into the log.
void qtMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (handlerBefore)
        handlerBefore(type, context, message);
    if (fileDescriptor < 0 || insideHandler)
        return;
    insideHandler = true;
    const char *kind = type == QtWarningMsg ? "WARNING" : type == QtCriticalMsg ? "ERROR" : type == QtFatalMsg ? "FATAL" : "qt";
    QString text = message;
    if (context.category && strcmp(context.category, "default") != 0)
        text.prepend(QString::fromLatin1(context.category) + QStringLiteral(": "));
    if (context.file && context.line > 0)
        text += QStringLiteral("  (%1:%2)").arg(QString::fromLocal8Bit(context.file)).arg(context.line);
    const QByteArray bytes = entry(text);
    {
        QMutexLocker lock(&lineMutex);
        if (type == QtFatalMsg) {
            // The last thing Qt says before it stops the app: never one too many.
            settleRepeats();
            putLine(kind, bytes);
        } else {
            putMessage(kind, bytes);
        }
    }
    insideHandler = false;
}

// ---- A crash, and being stopped from outside

const char *signalMeaning(int signal)
{
    switch (signal) {
    case SIGSEGV: return "SIGSEGV: it used memory that is not its own";
    case SIGBUS: return "SIGBUS: it used memory that is not there";
    case SIGILL: return "SIGILL: it met an instruction the processor does not have";
    case SIGFPE: return "SIGFPE: arithmetic that cannot be done, such as dividing by nought";
    case SIGABRT: return "SIGABRT: the app, Qt or a library gave up on purpose";
    case SIGTERM: return "SIGTERM: it was asked to stop";
    case SIGINT: return "SIGINT: it was interrupted from the keyboard";
    case SIGHUP: return "SIGHUP: its terminal or its session went away";
    default: return "a signal";
    }
}

void say(const char *text)
{
    if (::write(fileDescriptor, text, strlen(text)) < 0) {
        // There is nothing to be done about it here.
    }
}

// Something has gone wrong that the app cannot go on from. What it was and where the
// app was at the time go into the log, and then the system is let do what it does with
// such a thing (stop the app, and keep what its crash reporter wants). Everything here
// is of the few things that may be done at such a moment: no memory is asked for, and
// no lock is waited on.
void crashed(int signal, siginfo_t *info, void *)
{
    static std::atomic_flag writingItUp = ATOMIC_FLAG_INIT;
    if (writingItUp.test_and_set()) {
        // Another thread is already doing this; it will end the app.
        for (;;)
            pause();
    }
    if (fileDescriptor >= 0) {
        char start[headWidth + 1];
        head(start, "CRASH");
        start[headWidth] = 0;
        say(start);
        say("The app has stopped. ");
        say(signalMeaning(signal));
        if (info && info->si_code <= 0) {
            // Not something the app did: the signal was sent to it.
            say(" (the signal was sent to it by another program)");
        } else if ((signal == SIGSEGV || signal == SIGBUS) && info) {
            char address[2 + 16 + 1];
            auto value = quintptr(info->si_addr);
            address[0] = '0';
            address[1] = 'x';
            for (int digit = 15; digit >= 0; --digit, value >>= 4)
                address[2 + digit] = "0123456789abcdef"[value & 15];
            address[18] = 0;
            say(" (at address ");
            say(address);
            say(")");
        }
        say(".\n");
        char indent[headWidth + 1];
        memset(indent, ' ', headWidth);
        indent[headWidth] = 0;
        say(indent);
        say("The lines above are what it was doing. Where it was, innermost first, as the system names the places\n");
        say(indent);
        say("(a place in the app itself is a number, which the list of names made with each release turns into a name):\n");
        void *frames[64];
        const int count = backtrace(frames, 64);
        backtrace_symbols_fd(frames, count, fileDescriptor);
        say(indent);
        say("(That is all that could be written.)\n");
    }
    // The handler was taken off as this was called, so the same signal now ends the app.
    raise(signal);
}

void stoppedFromOutside(int signal)
{
    if (fileDescriptor >= 0) {
        char start[headWidth + 1];
        head(start, "end");
        start[headWidth] = 0;
        say(start);
        say("Stopped from outside, without being closed. ");
        say(signalMeaning(signal));
        say(".\n");
    }
    raise(signal);
}

void watchForSignals()
{
    // The first use of this loads what it needs, which must not happen in the handler.
    void *frame[1];
    backtrace(frame, 1);

    // A place for the handler to work when the thread's own is what has run out. For
    // the thread this is called on, which is the one the windows run on.
    static char spareStack[64 * 1024];
    stack_t stack {};
    stack.ss_sp = spareStack;
    stack.ss_size = sizeof(spareStack);
    sigaltstack(&stack, nullptr);

    struct sigaction crash {};
    crash.sa_sigaction = crashed;
    crash.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND | SA_NODEFER;
    sigemptyset(&crash.sa_mask);
    for (const int signal : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
        sigaction(signal, &crash, nullptr);

    struct sigaction stop {};
    stop.sa_handler = stoppedFromOutside;
    stop.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&stop.sa_mask);
    for (const int signal : {SIGTERM, SIGINT, SIGHUP})
        sigaction(signal, &stop, nullptr);
}

// ---- What the app is running on

QByteArray fileText(const char *path, qint64 atMost = 64 * 1024)
{
    QFile file(QString::fromLatin1(path));
    return file.open(QIODevice::ReadOnly) ? file.read(atMost) : QByteArray();
}

// The value of a line "name: value" or "name value" of one of the system's own files
QString valueOf(const QByteArray &text, const char *name)
{
    for (const QByteArray &line : text.split('\n')) {
        if (line.startsWith(name)) {
            QByteArray value = line.mid(qsizetype(strlen(name))).trimmed();
            if (value.startsWith(':'))
                value = value.mid(1).trimmed();
            return QString::fromUtf8(value);
        }
    }
    return {};
}

QString gigabytes(const QString &kilobytes)
{
    return QString::number(kilobytes.section(QLatin1Char(' '), 0, 0).toDouble() / 1024 / 1024, 'f', 1) + QStringLiteral(" GB");
}

QString span(qint64 milliseconds)
{
    const qint64 seconds = milliseconds / 1000;
    if (seconds >= 3600)
        return QStringLiteral("%1 h %2 min").arg(seconds / 3600).arg(seconds / 60 % 60);
    if (seconds >= 60)
        return QStringLiteral("%1 min %2 s").arg(seconds / 60).arg(seconds % 60);
    return QStringLiteral("%1 s").arg(seconds);
}

QString screenText(const QScreen *screen)
{
    const QRect geometry = screen->geometry();
    QString text = QStringLiteral("\"%1\": %2x%3 at %4,%5, scale %6, %7 Hz").arg(screen->name()).arg(geometry.width())
                       .arg(geometry.height()).arg(geometry.x()).arg(geometry.y()).arg(screen->devicePixelRatio())
                       .arg(qRound(screen->refreshRate()));
    const QString maker = (screen->manufacturer() + QLatin1Char(' ') + screen->model()).trimmed();
    if (!maker.isEmpty())
        text += QStringLiteral(", ") + maker;
    if (screen == QGuiApplication::primaryScreen())
        text += QStringLiteral(" (the main one)");
    return text;
}

void followScreen(QScreen *screen)
{
    QObject::connect(screen, &QScreen::geometryChanged, screen, [screen] {
        SessionLog::write("display", QStringLiteral("screen changed: ") + screenText(screen));
    });
}

// ---- The log files there are

const QString sessionPattern = QStringLiteral("SimplePresenter ????" "-??" "-?? ??.??.??*.log");
const int sessionsKept = 20;

// The logs of a folder, a name for each session, the oldest first.
QStringList sessions(const QDir &directory)
{
    QStringList names = directory.entryList({sessionPattern}, QDir::Files, QDir::Name);
    names.removeIf([](const QString &name) { return name.contains(QLatin1String("(earlier)")); });
    return names;
}

QString partBefore(const QString &name)
{
    return name.chopped(4) + QStringLiteral(" (earlier).log");
}

// What became of the session before this one, if it did not end as it should have:
// from how its log ends. Empty if it closed normally, or there was none.
QString sessionBefore(const QDir &directory)
{
    const QStringList logs = sessions(directory);
    if (logs.isEmpty())
        return {};
    QFile file(directory.filePath(logs.last()));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray top = file.read(2048);
    file.seek(qMax<qint64>(0, file.size() - 6000));
    const QByteArray tail = file.readAll();
    char column[2 + kindWidth + 2 + 1];
    const auto has = [&](const char *kind) {
        memset(column, ' ', sizeof(column) - 1);
        column[sizeof(column) - 1] = 0;
        memcpy(column + 2, kind, strlen(kind));
        return tail.contains(column);
    };
    if (has("CRASH"))
        return QStringLiteral("The session before this one crashed. Its log is \"%1\".").arg(logs.last());
    if (has("end"))
        return {};
    // It may not have ended at all.
    const qsizetype at = top.indexOf("process ");
    const int process = at < 0 ? 0 : top.mid(at + 8, 12).split('\n').first().trimmed().toInt();
    if (process > 0 && fileText(QByteArray("/proc/" + QByteArray::number(process) + "/comm").constData()).trimmed() == "SimplePresenter") {
        return QStringLiteral("Another copy of the app seems to be running as well (process %1). Its log is \"%2\".")
            .arg(process).arg(logs.last());
    }
    return QStringLiteral("The session before this one did not close normally: it was killed, or the computer stopped under it. "
                          "Its log is \"%1\".").arg(logs.last());
}

// Takes away the oldest logs, so that the folder does not grow for ever. Only files
// with names as this app gives its logs are touched.
void prune(const QDir &directory)
{
    QStringList logs = sessions(directory);
    while (logs.size() > sessionsKept - 1) {
        const QString oldest = logs.takeFirst();
        QFile::remove(directory.filePath(oldest));
        QFile::remove(directory.filePath(partBefore(oldest)));
    }
}

// ---- The windows

struct Watched
{
    QPointer<QQuickWindow> window;
    QString name;
    // Added to on the thread that draws the window, as each frame is shown
    std::atomic<int> frames {0};
    int framesAtLastLook = 0;
    QString graphics;
};

// Never taken from, so that what points into it stays good.
std::deque<Watched> watchedWindows;
QBasicMutex watchedMutex;
QString graphicsFirstSaid;

QString glText(QOpenGLFunctions *gl, GLenum name)
{
    const GLubyte *text = gl->glGetString(name);
    return text ? QString::fromLatin1(reinterpret_cast<const char *>(text)) : QStringLiteral("?");
}

// Says what draws a window: the graphics chip and its driver, as the driver gives them.
// Called on the thread that draws it, which is the only one that can ask.
void describeGraphics(Watched *watched)
{
    QQuickWindow *window = watched->window;
    if (!window)
        return;
    QString text;
    switch (window->rendererInterface()->graphicsApi()) {
    case QSGRendererInterface::Software:
        text = QStringLiteral("drawn by the processor alone (Qt's software renderer); no graphics chip is in use");
        break;
    case QSGRendererInterface::OpenGL:
        if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
            QOpenGLFunctions *gl = context->functions();
            text = QStringLiteral("%1; OpenGL%2 %3; shading language %4; driver by %5")
                       .arg(glText(gl, GL_RENDERER), context->isOpenGLES() ? QStringLiteral(" ES") : QString(),
                            glText(gl, GL_VERSION), glText(gl, GL_SHADING_LANGUAGE_VERSION), glText(gl, GL_VENDOR));
        } else {
            text = QStringLiteral("OpenGL");
        }
        break;
    case QSGRendererInterface::Vulkan:
        text = QStringLiteral("Vulkan");
        break;
    default:
        text = QStringLiteral("a graphics interface this log has no name for (%1)").arg(int(window->rendererInterface()->graphicsApi()));
        break;
    }
    text += QThread::currentThread() == QCoreApplication::instance()->thread() ? QStringLiteral("; drawn on the app's own thread")
                                                                              : QStringLiteral("; drawn on a thread of its own");
    QString said;
    {
        QMutexLocker lock(&watchedMutex);
        if (watched->graphics == text)
            return;
        watched->graphics = text;
        // Every window is drawn by the same thing, as a rule, which need not be said again.
        said = text == graphicsFirstSaid ? QStringLiteral("the same") : text;
        if (graphicsFirstSaid.isEmpty())
            graphicsFirstSaid = text;
    }
    SessionLog::write("graphics", watched->name + QStringLiteral(": ") + said);
}

class DescribeGraphics : public QRunnable
{
public:
    explicit DescribeGraphics(Watched *watched) : m_watched(watched) {}
    void run() override { describeGraphics(m_watched); }

private:
    Watched *m_watched;
};

QString shownAs(const QQuickWindow *window)
{
    const QScreen *screen = window->screen();
    const QString where = screen ? QStringLiteral(" on screen \"%1\"").arg(screen->name()) : QString();
    switch (window->visibility()) {
    case QWindow::Hidden:
        return QStringLiteral("hidden");
    case QWindow::Minimized:
        return QStringLiteral("minimised");
    case QWindow::FullScreen:
        return QStringLiteral("fullscreen") + where
               + (screen ? QStringLiteral(" (%1x%2)").arg(screen->geometry().width()).arg(screen->geometry().height()) : QString());
    case QWindow::Maximized:
        return QStringLiteral("maximised") + where;
    default:
        return QStringLiteral("a window of %1x%2").arg(window->width()).arg(window->height()) + where;
    }
}

// ---- Whether the app is still answering, and what it is using

struct Watcher
{
    std::mutex mutex;
    std::condition_variable wake;
    bool stop = false;
    std::thread thread;
};
// Made once and never unmade, so that the thread can never outlive what it waits on.
Watcher *watcher = nullptr;
// When the app was last asked whether it is answering and has not yet answered, in
// milliseconds since the start; 0 when it has.
std::atomic<qint64> askedAt {0};
// How long it had not answered when that was last said, in seconds; 0 if it is answering.
std::atomic<int> saidStalledFor {0};

double processorSeconds()
{
    rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_stime.tv_sec + (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1e6;
}

// A line saying how much of a processor and how much memory the app has been using,
// and how many frames each window has shown, since this was last said.
void sayHealth(double &processorBefore, qint64 &timeBefore)
{
    readClockOffset();
    const qint64 now = running.elapsed();
    const double processor = processorSeconds();
    const double share = now > timeBefore ? 100.0 * (processor - processorBefore) / ((now - timeBefore) / 1000.0) : 0;
    const qint64 pages = fileText("/proc/self/statm").split(' ').value(1).toLongLong();
    rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    QStringList frames;
    {
        QMutexLocker lock(&watchedMutex);
        for (Watched &watched : watchedWindows) {
            const int shown = watched.frames.load(std::memory_order_relaxed);
            frames << QStringLiteral("%1 %2").arg(watched.name).arg(shown - watched.framesAtLastLook);
            watched.framesAtLastLook = shown;
        }
    }
    SessionLog::write("health", QStringLiteral("Over the last %1: %2% of one processor; %3 MB of memory in use now, %4 MB at most; frames shown: %5")
                                    .arg(span(now - timeBefore)).arg(share, 0, 'f', share < 10 ? 1 : 0)
                                    .arg(pages * sysconf(_SC_PAGESIZE) / 1024 / 1024).arg(usage.ru_maxrss / 1024)
                                    .arg(frames.isEmpty() ? QStringLiteral("none") : frames.join(QStringLiteral(", "))));
    processorBefore = processor;
    timeBefore = now;
}

// The app has answered: it is not stuck, or no longer.
void answered()
{
    const qint64 asked = askedAt.exchange(0);
    if (saidStalledFor.exchange(0) > 0) {
        SessionLog::write("recovered", QStringLiteral("The app is answering again. It did not for at least %1 seconds.")
                                           .arg((running.elapsed() - asked) / 1000.0, 0, 'f', 1));
    }
}

void watching()
{
    double processorBefore = processorSeconds();
    qint64 timeBefore = running.elapsed();
    std::unique_lock lock(watcher->mutex);
    for (int round = 1; !watcher->wake.wait_for(lock, std::chrono::seconds(1), [] { return watcher->stop; }); ++round) {
        // Is the app answering? It is asked by having something left for it to do, as a
        // click is. If that is still waiting two seconds later it is not, which anyone
        // watching will have seen; it is said again if it goes on.
        const qint64 now = running.elapsed();
        const qint64 asked = askedAt.load();
        if (asked == 0) {
            askedAt.store(now);
            if (QCoreApplication *app = QCoreApplication::instance())
                QMetaObject::invokeMethod(app, answered, Qt::QueuedConnection);
        } else {
            const int waited = int((now - asked) / 1000);
            const int said = saidStalledFor.load();
            if ((said == 0 && waited >= 2) || (said < 10 && waited >= 10) || (said < 30 && waited >= 30) || (said < 120 && waited >= 120)) {
                saidStalledFor.store(waited);
                SessionLog::write("STALLED", said == 0
                    ? QStringLiteral("The app has not answered for %1 seconds: it is busy with what the lines above say, or stuck in it.").arg(waited)
                    : QStringLiteral("Still not answering, after %1 seconds.").arg(waited));
            }
        }
        // After the first minute, and then every five.
        if (round == 60 || (round > 60 && (round - 60) % 300 == 0))
            sayHealth(processorBefore, timeBefore);
    }
}

void stopWatching()
{
    if (!watcher || !watcher->thread.joinable())
        return;
    {
        std::lock_guard lock(watcher->mutex);
        watcher->stop = true;
    }
    watcher->wake.notify_all();
    watcher->thread.join();
}

} // namespace

void SessionLog::start(int argc, char *argv[], const QString &version)
{
    QString selfTestFolder;
    QStringList command;
    for (int i = 0; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        command << (argument.contains(QLatin1Char(' ')) ? QLatin1Char('"') + argument + QLatin1Char('"') : argument);
        // A run that only answers a question is not a session.
        if (i > 0 && (argument == QLatin1String("--help") || argument == QLatin1String("-h") || argument == QLatin1String("--help-all")
                      || argument == QLatin1String("--version") || argument == QLatin1String("-v")
                      || argument == QLatin1String("--list-screens")))
            return;
        if (argument == QLatin1String("--selftest") && i + 1 < argc)
            selfTestFolder = QString::fromLocal8Bit(argv[i + 1]);
        else if (argument.startsWith(QLatin1String("--selftest=")))
            selfTestFolder = argument.mid(11);
    }

    // The self-test leaves the user's own folders alone, its log included: that goes
    // with its pictures.
    const QDateTime now = QDateTime::currentDateTime();
    const QDir folder(selfTestFolder.isEmpty()
                          ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/SimplePresenter/Logs")
                          : selfTestFolder);
    if (!folder.mkpath(QStringLiteral("."))) {
        fprintf(stderr, "SimplePresenter: no log is kept, because %s could not be made\n", qPrintable(folder.path()));
        return;
    }
    QString before;
    if (selfTestFolder.isEmpty()) {
        before = sessionBefore(folder);
        prune(folder);
        const QString base = QStringLiteral("SimplePresenter ") + now.toString(QStringLiteral("yyyy-MM-dd HH.mm.ss"));
        logFile = folder.filePath(base + QStringLiteral(".log"));
        // Two started in the same second
        for (int copy = 2; QFile::exists(logFile); ++copy)
            logFile = folder.filePath(QStringLiteral("%1 (%2).log").arg(base).arg(copy));
    } else {
        logFile = folder.filePath(QStringLiteral("SimplePresenter self-test.log"));
    }
    logFileName = QFile::encodeName(logFile);
    earlierFileName = QFile::encodeName(partBefore(logFile));
    fileDescriptor = ::open(logFileName.constData(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC, 0644);
    if (fileDescriptor < 0) {
        fprintf(stderr, "SimplePresenter: no log is kept, because %s could not be written\n", qPrintable(logFile));
        logFile.clear();
        return;
    }
    running.start();
    readClockOffset();

    // In English whatever the system speaks, like the rest of the log: it is read by
    // whoever is asked for help.
    const long east = utcOffset;
    const QString offset = QStringLiteral("UTC%1%2:%3").arg(east < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                               .arg(qAbs(east) / 3600, 2, 10, QLatin1Char('0')).arg(qAbs(east) / 60 % 60, 2, 10, QLatin1Char('0'));
    const QByteArray top = QStringLiteral(
        "Simple Presenter %1 session log, started %2 (%3)\n"
        "\n"
        "This says what the app was running on and what it did, for working out what happened\n"
        "when something has gone wrong. It holds the names of files, presentations and playlists,\n"
        "and nothing of what is in them. A line is the time, what kind of thing it is, and what\n"
        "happened. A kind written in capitals is something that went wrong.\n"
        "\n").arg(version, QLocale(QLocale::English).toString(now, QStringLiteral("dddd d MMMM yyyy 'at' HH:mm:ss")), offset).toUtf8();
    bytesInPart += qMax<ssize_t>(0, ::write(fileDescriptor, top.constData(), size_t(top.size())));

    write("start", QStringLiteral("Simple Presenter %1, with Qt %2 (built with %3); process %4")
                       .arg(version, QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)).arg(getpid()));
    write("start", QStringLiteral("started as: ") + command.join(QLatin1Char(' ')));
    if (!before.isEmpty())
        write("start", before);

    QString kernel = QSysInfo::kernelType();
    if (!kernel.isEmpty())
        kernel[0] = kernel.at(0).toUpper();
    write("system", QStringLiteral("%1; %2 %3; %4").arg(QSysInfo::prettyProductName(), kernel, QSysInfo::kernelVersion(),
                                                         QSysInfo::currentCpuArchitecture()));
    const QString processor = valueOf(fileText("/proc/cpuinfo"), "model name");
    const QByteArray memory = fileText("/proc/meminfo");
    write("system", QStringLiteral("%1, %2 processors; %3 of memory, %4 of it free")
                        .arg(processor.isEmpty() ? QStringLiteral("a processor that does not give its name") : processor)
                        .arg(sysconf(_SC_NPROCESSORS_ONLN)).arg(gigabytes(valueOf(memory, "MemTotal")), gigabytes(valueOf(memory, "MemAvailable"))));
    // What of the environment has a say in how the app draws and plays
    QStringList environment;
    for (const char *name : {"XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "WAYLAND_DISPLAY", "DISPLAY", "LANG", "QT_QPA_PLATFORM",
                             "QT_QUICK_BACKEND", "QSG_RHI_BACKEND", "QSG_RENDER_LOOP", "QSG_INFO", "QT_SCALE_FACTOR", "QT_MEDIA_BACKEND",
                             "QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "QT_DISABLE_HW_TEXTURES_CONVERSION", "LIBVA_DRIVER_NAME",
                             "LIBVA_DRIVERS_PATH", "LIBGL_ALWAYS_SOFTWARE", "MESA_LOADER_DRIVER_OVERRIDE", "DRI_PRIME",
                             "__GLX_VENDOR_LIBRARY_NAME", "__NV_PRIME_RENDER_OFFLOAD"}) {
        if (qEnvironmentVariableIsSet(name))
            environment << QString::fromLatin1(name) + QLatin1Char('=') + qEnvironmentVariable(name);
    }
    write("system", QStringLiteral("environment: ")
                        + (environment.isEmpty() ? QStringLiteral("nothing set that matters here") : environment.join(QLatin1Char(' '))));

    handlerBefore = qInstallMessageHandler(qtMessage);
    watchForSignals();
}

void SessionLog::describeDisplay()
{
    if (fileDescriptor < 0)
        return;
    const auto screens = QGuiApplication::screens();
    write("display", QStringLiteral("Qt is drawing through \"%1\"; %2 screen%3").arg(QGuiApplication::platformName()).arg(screens.size())
                         .arg(screens.size() == 1 ? QString() : QStringLiteral("s")));
    for (qsizetype i = 0; i < screens.size(); ++i) {
        write("display", QStringLiteral("screen %1 ").arg(i) + screenText(screens.at(i)));
        followScreen(screens.at(i));
    }
    QObject::connect(qGuiApp, &QGuiApplication::screenAdded, qGuiApp, [](QScreen *screen) {
        write("display", QStringLiteral("screen connected: ") + screenText(screen));
        followScreen(screen);
    });
    QObject::connect(qGuiApp, &QGuiApplication::screenRemoved, qGuiApp, [](QScreen *screen) {
        write("display", QStringLiteral("screen disconnected: \"%1\"").arg(screen->name()));
    });
    QObject::connect(qGuiApp, &QGuiApplication::primaryScreenChanged, qGuiApp, [](QScreen *screen) {
        if (screen)
            write("display", QStringLiteral("the main screen is now \"%1\"").arg(screen->name()));
    });
}

void SessionLog::watchForStalls()
{
    if (fileDescriptor < 0 || watcher)
        return;
    watcher = new Watcher;
    // Not while the app is closing down, when it has stopped answering for good.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [] { stopWatching(); }, Qt::DirectConnection);
    watcher->thread = std::thread(watching);
}

void SessionLog::finish()
{
    stopWatching();
    if (fileDescriptor < 0)
        return;
    write("end", QStringLiteral("Closed normally, after %1.").arg(span(running.elapsed())));
    QMutexLocker lock(&lineMutex);
    ::close(fileDescriptor);
    fileDescriptor = -1;
}

void SessionLog::write(const char *kind, const QString &text)
{
    if (fileDescriptor < 0)
        return;
    const QByteArray bytes = entry(text);
    QMutexLocker lock(&lineMutex);
    settleRepeats();
    putLine(kind, bytes);
}

QString SessionLog::filePath()
{
    return logFile;
}

QString SessionLog::folder() const
{
    return logFile.isEmpty() ? QString() : QFileInfo(logFile).absolutePath();
}

void SessionLog::showFolder() const
{
    if (logFile.isEmpty())
        return;
    write("settings", QStringLiteral("the folder of logs was asked for, from the About section"));
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder()));
}

void SessionLog::note(const QString &kind, const QString &text) const
{
    write(kind.toLatin1().constData(), text);
}

void SessionLog::problem(const QString &text) const
{
    fprintf(stderr, "SimplePresenter: %s\n", qPrintable(text));
    write("PROBLEM", text);
}

void SessionLog::watch(QQuickWindow *window, const QString &name) const
{
    if (!window || fileDescriptor < 0)
        return;
    Watched *watched = nullptr;
    {
        QMutexLocker lock(&watchedMutex);
        for (Watched &known : watchedWindows) {
            if (known.window == window)
                return;
        }
        watched = &watchedWindows.emplace_back();
        watched->window = window;
        watched->name = name;
    }
    // On the thread that draws the window, and nothing but adding one.
    connect(window, &QQuickWindow::frameSwapped, window, [watched] { watched->frames.fetch_add(1, std::memory_order_relaxed); },
            Qt::DirectConnection);
    connect(window, &QQuickWindow::sceneGraphInitialized, window, [watched] { describeGraphics(watched); }, Qt::DirectConnection);
    if (window->isSceneGraphInitialized())
        window->scheduleRenderJob(new DescribeGraphics(watched), QQuickWindow::BeforeSynchronizingStage);
    connect(window, &QWindow::visibilityChanged, window, [window, name] { write("window", name + QStringLiteral(": ") + shownAs(window)); });
    connect(window, &QWindow::screenChanged, window, [window, name](QScreen *screen) {
        if (screen && window->isVisible())
            write("window", QStringLiteral("%1: now on screen \"%2\"").arg(name, screen->name()));
    });
    if (window->isVisible())
        write("window", name + QStringLiteral(": ") + shownAs(window));
}

int SessionLog::frames(QQuickWindow *window) const
{
    QMutexLocker lock(&watchedMutex);
    for (const Watched &watched : watchedWindows) {
        if (watched.window == window)
            return watched.frames.load(std::memory_order_relaxed);
    }
    return 0;
}
