#include "timers.h"

#include "workspacefiles.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QTime>

#include <cmath>

namespace {

using Configuration = rv::data::Timer::Configuration;
using ToTime = Configuration::TimerTypeCountdownToTime;

// The one timer a workspace without a timers file is shown as having. Its id is the same
// wherever it turns up, so that a text box linked to it before the file has been
// written is still linked to it afterwards.
const char defaultId[] = "8E5C1B4A-6D2F-4B0E-9A37-1C5F0D2E7A64";
const double defaultDuration = 300;
const double halfDay = 12 * 3600;

rv::data::TimersDocument defaultDocument()
{
    rv::data::TimersDocument document;
    rv::data::Timer *timer = document.add_timers();
    timer->mutable_uuid()->set_string(defaultId);
    timer->set_name("Countdown");
    timer->mutable_configuration()->mutable_countdown()->set_duration(defaultDuration);
    return document;
}

QString kindOf(const rv::data::Timer &timer)
{
    if (timer.configuration().has_countdown_to_time())
        return QStringLiteral("countdownTo");
    if (timer.configuration().has_elapsed_time())
        return QStringLiteral("elapsed");
    return QStringLiteral("countdown");
}

// The time of day a countdown runs to, in seconds from midnight. The file has it either
// that way or as a time on the twelve-hour clock with which half of the day it is in.
double secondsOfDay(const ToTime &to)
{
    double time = to.time_of_day();
    if (to.period() == ToTime::TIME_PERIOD_PM && time < halfDay)
        time += halfDay;
    else if (to.period() == ToTime::TIME_PERIOD_AM && time >= halfDay)
        time -= halfDay;
    return time;
}

// How long it is from one time of day to another, both in seconds from midnight. A time
// of day does not say which day. One that went by less than six hours ago is taken to
// be behind (and the answer is negative), across midnight too; anything else is ahead,
// today or tomorrow. So a countdown to the morning's service stays at nothing while the
// service is on, one to midnight runs down through the evening, and the evening before
// a service the countdown to it is to the next morning.
double until(double timeOfDay, double now)
{
    const double day = 2 * halfDay;
    double left = timeOfDay - now;
    if (left <= -day / 4)
        left += day;
    else if (left > day * 3 / 4)
        left -= day;
    return left;
}

// Whether a timer that stands at `seconds` has got to where it stops.
bool finished(const rv::data::Timer &timer, double seconds)
{
    const Configuration &configuration = timer.configuration();
    if (configuration.allows_overrun())
        return false;
    if (configuration.has_elapsed_time())
        return configuration.elapsed_time().has_end_time() && seconds >= configuration.elapsed_time().end_time();
    return seconds <= 0;
}

rv::data::Timer *findIn(rv::data::TimersDocument *document, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (rv::data::Timer &timer : *document->mutable_timers()) {
        if (timer.uuid().string() == wanted)
            return &timer;
    }
    return nullptr;
}

} // namespace

Timers::Timers(QObject *parent)
    : QObject(parent)
{
    m_clock.setInterval(100);
    connect(&m_clock, &QTimer::timeout, this, &Timers::look);
    m_fastClock.setInterval(33);
    m_fastClock.setTimerType(Qt::PreciseTimer);
    connect(&m_fastClock, &QTimer::timeout, this, &Timers::pulse);
}

QString Timers::open(const QString &workspace, bool held)
{
    m_workspace = workspace;
    m_held = held;
    m_runs.clear();
    m_setBySlides.clear();
    m_clock.stop();
    rv::data::TimersDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        document.Clear();
    show(document);
    return error;
}

QString Timers::path() const
{
    return m_workspace + QStringLiteral("/Configuration/Timers");
}

QString Timers::read(rv::data::TimersDocument *document) const
{
    QFile file(path());
    if (!file.exists()) {
        *document = defaultDocument();
        return {};
    }
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read the timers: %1").arg(file.errorString());
    const QByteArray data = file.readAll();
    if (!document->ParseFromArray(data.constData(), int(data.size())))
        return QStringLiteral("The timers file is not one this app can read");
    return {};
}

// The file, with the timers that slides have set up differently as the slides set them.
QString Timers::readAsSetUp(rv::data::TimersDocument *document) const
{
    const QString error = read(document);
    if (!error.isEmpty())
        return error;
    for (rv::data::Timer &timer : *document->mutable_timers()) {
        const auto set = m_setBySlides.constFind(QString::fromStdString(timer.uuid().string()));
        if (set != m_setBySlides.constEnd())
            timer.mutable_configuration()->ParseFromString(*set);
    }
    return {};
}

QString Timers::write(const rv::data::TimersDocument &document)
{
    if (!QDir().mkpath(m_workspace + QStringLiteral("/Configuration")))
        return QStringLiteral("Cannot make the workspace's Configuration folder");
    const std::string data = document.SerializeAsString();
    QSaveFile file(path());
    if (!file.open(QIODevice::WriteOnly) || file.write(data.data(), qint64(data.size())) != qint64(data.size())
        || !file.commit())
        return QStringLiteral("Cannot write the timers: %1").arg(file.errorString());
    // What slides had set is in the file now.
    m_setBySlides.clear();
    return {};
}

// Takes the timers as a file has them as what there is to show.
void Timers::show(const rv::data::TimersDocument &document)
{
    m_document = document;
    m_timers.clear();
    QSet<QString> ids;
    for (const rv::data::Timer &timer : m_document.timers()) {
        const Configuration &configuration = timer.configuration();
        const QString id = QString::fromStdString(timer.uuid().string());
        ids.insert(id);
        m_timers.append(QVariantMap {
            {"id", id},
            {"name", QString::fromStdString(timer.name())},
            {"kind", kindOf(timer)},
            {"duration", configuration.countdown().duration()},
            {"timeOfDay", secondsOfDay(configuration.countdown_to_time())},
            {"startTime", configuration.elapsed_time().start_time()},
            {"endTime", configuration.elapsed_time().end_time()},
            {"hasEndTime", configuration.elapsed_time().has_end_time()},
            {"overrun", configuration.allows_overrun()},
        });
    }
    // How a timer that is no longer there was running is of no more use.
    m_runs.removeIf([&ids](const std::pair<const QString &, Run &> run) { return !ids.contains(run.first); });
    startClockTimers();
    emit changed();
    bump();
}

// A countdown to a time of day has nothing to wait for: what it shows is how long it is
// until then, which is so whether or not anyone has started it. So one that is at its
// start is running. It can be stopped, which holds it where it is, like any other.
void Timers::startClockTimers()
{
    for (const rv::data::Timer &timer : m_document.timers()) {
        const QString id = QString::fromStdString(timer.uuid().string());
        if (timer.configuration().has_countdown_to_time() && m_runs.value(id).fresh)
            start(id);
    }
}

const rv::data::Timer *Timers::find(const QString &id, const QString &name) const
{
    const std::string wantedId = id.toStdString();
    for (const rv::data::Timer &timer : m_document.timers()) {
        if (!wantedId.empty() && timer.uuid().string() == wantedId)
            return &timer;
    }
    const std::string wantedName = name.toStdString();
    for (const rv::data::Timer &timer : m_document.timers()) {
        if (!wantedName.empty() && timer.name() == wantedName)
            return &timer;
    }
    return nullptr;
}

// Where a timer stands, in seconds: how much is left of a countdown, or how far an
// elapsed time has got. One that is not to overrun stays at its end.
double Timers::seconds(const rv::data::Timer &timer, const Run &run) const
{
    const Configuration &configuration = timer.configuration();
    const double ran = run.running ? run.since.nsecsElapsed() / 1e9 : 0;
    double seconds;
    if (configuration.has_countdown_to_time()) {
        // Unless it has been stopped, a countdown to a time of day stands at however
        // long it is until then.
        const double now = QTime::currentTime().msecsSinceStartOfDay() / 1000.0;
        seconds = m_held || (!run.running && !run.fresh) ? run.base
                                                         : until(secondsOfDay(configuration.countdown_to_time()), now);
    } else if (configuration.has_elapsed_time()) {
        seconds = run.fresh ? configuration.elapsed_time().start_time() : run.base + ran;
    } else {
        seconds = run.fresh ? configuration.countdown().duration() : run.base - ran;
    }
    if (!configuration.allows_overrun()) {
        if (configuration.has_elapsed_time()) {
            if (configuration.elapsed_time().has_end_time())
                seconds = qMin(seconds, configuration.elapsed_time().end_time());
        } else {
            seconds = qMax(seconds, 0.0);
        }
    }
    return seconds;
}

// The whole seconds a timer shows. A countdown shows the second it is in until that
// second is over, so that it starts on its full length and reaches nothing as it ends;
// an elapsed time shows the seconds that have gone.
qint64 Timers::shownSeconds(const rv::data::Timer &timer, double seconds)
{
    return timer.configuration().has_elapsed_time() ? qint64(std::floor(seconds + 1e-6))
                                                    : qint64(std::ceil(seconds - 1e-6));
}

QString Timers::written(double seconds, bool countsDown, const Format &format)
{
    // The hundredths show as one digit (tenths) or two, or not at all.
    int digits = format.hundredths == None ? 0 : format.hundredths == Short || format.hundredths == RemoveShort ? 1 : 2;
    if (format.hundredthsUnderMinuteOnly && qAbs(seconds) >= 60)
        digits = 0;

    // The time is taken to the smallest part that shows, in hundredths of a second: a
    // countdown rounded up to it, an elapsed time down.
    const qint64 unit = digits == 2 ? 1 : digits == 1 ? 10 : format.seconds != None ? 100
                      : format.minutes != None ? 6000 : format.hours != None ? 360000 : 0;
    if (unit == 0)
        return {};
    const double units = seconds * 100 / unit;
    const qint64 whole = qint64(countsDown ? std::ceil(units - 1e-6) : std::floor(units + 1e-6));
    const qint64 hundredths = qAbs(whole) * unit;

    qint64 rest = hundredths / 100;
    QStringList parts;
    // A part is left out if it is hidden, or if it is one to hide while it is nothing
    // and nothing larger has been written; but the smallest of the three is always
    // written, so that there is always something. What a hidden part would have held
    // is carried in the next.
    const int smallest = format.seconds != None ? 1 : format.minutes != None ? 60 : 3600;
    const auto part = [&parts, &rest, smallest](qint64 size, int style) {
        if (style == None)
            return;
        const qint64 value = rest / size;
        rest %= size;
        if ((style == RemoveShort || style == RemoveLong) && value == 0 && parts.isEmpty() && size != smallest)
            return;
        const QString number = QString::number(value);
        parts << (style == Long || style == RemoveLong ? number.rightJustified(2, u'0') : number);
    };
    part(3600, format.hours);
    part(60, format.minutes);
    part(1, format.seconds);
    QString time = parts.join(u':');

    const int fraction = int(hundredths % 100);
    const bool hideNothing = format.hundredths == RemoveShort || format.hundredths == RemoveLong;
    if (digits > 0 && !(hideNothing && fraction == 0)) {
        if (!time.isEmpty())
            time += u'.';
        time += digits == 2 ? QString::number(fraction).rightJustified(2, u'0') : QString::number(fraction / 10);
    }
    return whole < 0 && !time.isEmpty() ? u'-' + time : time;
}

QVariantMap Timers::state(const QString &id) const
{
    const rv::data::Timer *timer = find(id);
    if (!timer)
        return {{"running", false}, {"seconds", 0.0}, {"text", QString()}};
    const Run run = m_runs.value(id);
    const double now = seconds(*timer, run);
    // The rows of the timers themselves show hours, minutes and seconds.
    return {
        {"running", run.running},
        {"seconds", now},
        {"text", written(now, !timer->configuration().has_elapsed_time(), Format {Short, Long, Long})},
    };
}

QString Timers::linkedText(const QString &id, const QString &name, int hours, int minutes, int secondsStyle,
                           int hundredths, bool hundredthsUnderMinuteOnly, const QString &pattern)
{
    const Format format {hours, minutes, secondsStyle, hundredths, hundredthsUnderMinuteOnly};
    // A box linked to a timer that is not here shows a time all the same, at nothing:
    // whatever is said about the missing timer is said to the operator, not on a slide.
    const rv::data::Timer *timer = find(id, name);
    if (!timer)
        return linked(0, true, format, pattern);
    const Run run = m_runs.value(QString::fromStdString(timer->uuid().string()));
    // Hundredths of a running timer: what asked will have to ask again very soon.
    if (hundredths != None && run.running) {
        m_asked.start();
        if (!m_fastClock.isActive())
            m_fastClock.start();
    }
    return linked(seconds(*timer, run), !timer->configuration().has_elapsed_time(), format, pattern);
}

QString Timers::linkedTimer(const QString &id, const QString &name) const
{
    const rv::data::Timer *timer = find(id, name);
    return timer ? QString::fromStdString(timer->uuid().string()) : QString();
}

bool Timers::meets(const QString &id, const QString &name, int criterion) const
{
    // The criteria, as the file format numbers them
    enum { HasTimeLeft = 0, HasRunOut = 1, IsRunning = 2, IsNotRunning = 3 };
    const rv::data::Timer *timer = find(id, name);
    if (!timer)
        return criterion == IsNotRunning;
    const Run run = m_runs.value(QString::fromStdString(timer->uuid().string()));
    // Run out is run out whether or not the timer is let carry on past it.
    const Configuration &configuration = timer->configuration();
    const double now = seconds(*timer, run);
    const bool runOut = configuration.has_elapsed_time()
        ? configuration.elapsed_time().has_end_time() && now >= configuration.elapsed_time().end_time()
        : now <= 0;
    switch (criterion) {
    case HasTimeLeft: return !runOut;
    case HasRunOut: return runOut;
    case IsRunning: return run.running;
    default: return !run.running;
    }
}

QString Timers::linked(double seconds, bool countsDown, const Format &format, const QString &pattern)
{
    const QString time = written(seconds, countsDown, format);
    const QString placeholder = QStringLiteral("${timer}");
    return pattern.contains(placeholder) ? QString(pattern).replace(placeholder, time) : time;
}

void Timers::start(const QString &id)
{
    const rv::data::Timer *timer = find(id);
    if (!timer || m_held || m_runs.value(id).running)
        return;
    Run &run = m_runs[id];
    double now = seconds(*timer, run);
    // One that has run out starts again from its start.
    if (finished(*timer, now)) {
        run.fresh = true;
        now = seconds(*timer, run);
    }
    run.base = now;
    run.since.start();
    run.running = true;
    run.fresh = false;
    run.shown = shownSeconds(*timer, now);
    if (!m_clock.isActive())
        m_clock.start();
    bump();
}

void Timers::stop(const QString &id)
{
    const rv::data::Timer *timer = find(id);
    if (!timer || !m_runs.value(id).running)
        return;
    Run &run = m_runs[id];
    run.base = seconds(*timer, run);
    run.running = false;
    bump();
}

void Timers::reset(const QString &id)
{
    m_runs.remove(id);
    startClockTimers();
    bump();
}

void Timers::act(const QVariantMap &action)
{
    const rv::data::Timer *found = find(action.value("timerId").toString(), action.value("timerName").toString());
    if (!found)
        return;
    const QString id = QString::fromStdString(found->uuid().string());

    // Set up as the action says, if it says and the timer is not so already. That is
    // a change to the timer like one made by hand, except that nothing is written.
    Configuration configuration;
    const QByteArray asked = QByteArray::fromBase64(action.value("configuration").toString().toLatin1());
    if (action.contains("configuration") && configuration.ParseFromArray(asked.constData(), int(asked.size()))
        && configuration.SerializeAsString() != found->configuration().SerializeAsString()) {
        m_setBySlides.insert(id, configuration.SerializeAsString());
        rv::data::TimersDocument document = m_document;
        *findIn(&document, id)->mutable_configuration() = configuration;
        m_runs.remove(id);
        show(document);
    }

    switch (action.value("action").toInt()) {
    case Start:
        start(id);
        break;
    case Stop:
        stop(id);
        break;
    case Reset:
    case StopAndReset:
        reset(id);
        break;
    case ResetAndStart:
        reset(id);
        start(id);
        break;
    case Increment: {
        // More time on a countdown, or on an elapsed time. A countdown to a time of
        // day has no time of its own to add to.
        const rv::data::Timer *timer = find(id);
        if (timer->configuration().has_countdown_to_time())
            break;
        Run &run = m_runs[id];
        run.base = seconds(*timer, run) + action.value("amount").toDouble();
        run.fresh = false;
        if (run.running)
            run.since.start();
        bump();
        break;
    }
    default:
        break;
    }
}

// Looks at the timers that are running: stops any that has got to its end, and says so
// if what any of them shows has changed.
void Timers::look()
{
    bool anyRunning = false;
    bool anyChanged = false;
    for (const rv::data::Timer &timer : m_document.timers()) {
        const auto found = m_runs.find(QString::fromStdString(timer.uuid().string()));
        if (found == m_runs.end() || !found->running)
            continue;
        Run &run = *found;
        const double now = seconds(timer, run);
        if (finished(timer, now)) {
            run.base = now;
            run.running = false;
            anyChanged = true;
            continue;
        }
        anyRunning = true;
        const qint64 shown = shownSeconds(timer, now);
        if (shown != run.shown) {
            run.shown = shown;
            anyChanged = true;
        }
    }
    if (!anyRunning)
        m_clock.stop();
    if (anyChanged)
        bump();
}

void Timers::bump()
{
    ++m_tick;
    emit ticked();
}

// The fast clock: thirty times a second for as long as something that shows hundredths
// keeps asking. Half a second with nobody asking (the slide has gone, or the timer has
// stopped) and it stops.
void Timers::pulse()
{
    if (!m_asked.isValid() || m_asked.elapsed() > 500) {
        m_fastClock.stop();
        return;
    }
    ++m_beat;
    emit beaten();
    if (m_beat % 6 == 0) {
        ++m_slowBeat;
        emit slowBeaten();
    }
}

QVariantMap Timers::add()
{
    rv::data::TimersDocument document;
    QString error = readAsSetUp(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};

    // "Timer", or "Timer 2" and so on if that is taken.
    const auto taken = [&document](const QString &name) {
        for (const rv::data::Timer &timer : document.timers()) {
            if (QString::fromStdString(timer.name()) == name)
                return true;
        }
        return false;
    };
    QString name = QStringLiteral("Timer");
    for (int n = 2; taken(name); ++n)
        name = QStringLiteral("Timer %1").arg(n);

    const QString id = QString::fromStdString(workspace::newUuid());
    rv::data::Timer *timer = document.add_timers();
    timer->mutable_uuid()->set_string(id.toStdString());
    timer->set_name(name.toStdString());
    timer->mutable_configuration()->mutable_countdown()->set_duration(defaultDuration);

    error = write(document);
    if (error.isEmpty())
        show(document);
    return {{"id", id}, {"error", error}};
}

QString Timers::remove(const QString &id)
{
    rv::data::TimersDocument document;
    QString error = readAsSetUp(&document);
    if (!error.isEmpty())
        return error;
    for (int i = 0; i < document.timers_size(); ++i) {
        if (document.timers(i).uuid().string() == id.toStdString()) {
            document.mutable_timers()->DeleteSubrange(i, 1);
            error = write(document);
            if (error.isEmpty())
                show(document);
            return error;
        }
    }
    return QStringLiteral("There is no longer such a timer");
}

QString Timers::configure(const QString &id, const QVariantMap &changes)
{
    rv::data::TimersDocument document;
    QString error = readAsSetUp(&document);
    if (!error.isEmpty())
        return error;
    rv::data::Timer *timer = findIn(&document, id);
    if (!timer)
        return QStringLiteral("There is no longer such a timer");

    Configuration *configuration = timer->mutable_configuration();
    if (changes.contains("name"))
        timer->set_name(changes.value("name").toString().toStdString());
    if (changes.contains("kind") && changes.value("kind").toString() != kindOf(*timer)) {
        // Setting one kind up takes the others' settings away. Each starts as something
        // usable: five minutes, ten in the morning, and from nothing with no end.
        const QString kind = changes.value("kind").toString();
        if (kind == QLatin1String("countdownTo")) {
            configuration->mutable_countdown_to_time()->set_time_of_day(10 * 3600);
            configuration->mutable_countdown_to_time()->set_period(ToTime::TIME_PERIOD_24);
        } else if (kind == QLatin1String("elapsed")) {
            configuration->mutable_elapsed_time();
        } else {
            configuration->mutable_countdown()->set_duration(defaultDuration);
        }
    }
    if (changes.contains("duration") && configuration->has_countdown())
        configuration->mutable_countdown()->set_duration(qMax(0.0, changes.value("duration").toDouble()));
    if (changes.contains("timeOfDay") && configuration->has_countdown_to_time()) {
        // Written on the 24-hour clock, which says which half of the day by itself.
        const double timeOfDay = qBound(0.0, changes.value("timeOfDay").toDouble(), 2 * halfDay - 1);
        configuration->mutable_countdown_to_time()->set_time_of_day(timeOfDay);
        configuration->mutable_countdown_to_time()->set_period(ToTime::TIME_PERIOD_24);
    }
    if (configuration->has_elapsed_time()) {
        if (changes.contains("startTime"))
            configuration->mutable_elapsed_time()->set_start_time(qMax(0.0, changes.value("startTime").toDouble()));
        if (changes.contains("endTime"))
            configuration->mutable_elapsed_time()->set_end_time(qMax(0.0, changes.value("endTime").toDouble()));
        if (changes.contains("hasEndTime"))
            configuration->mutable_elapsed_time()->set_has_end_time(changes.value("hasEndTime").toBool());
    }
    if (changes.contains("overrun"))
        configuration->set_allows_overrun(changes.value("overrun").toBool());

    error = write(document);
    if (!error.isEmpty())
        return error;
    // A timer that has been set up differently starts again from its start, except
    // when all that changed is what it is called.
    if (changes.size() > (changes.contains("name") ? 1 : 0))
        m_runs.remove(id);
    show(document);
    return {};
}
