#include "timecode.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace timecode {

namespace {

constexpr double kDay = 24 * 3600.0;
// How long with no frame before the timecode is taken to have stopped, in frames; and
// how far from a frame's length the time between two frames may be for the second to
// be taken as following the first
constexpr double kSilence = 3.5;
constexpr double kNear = 0.5;
// How much of what a frame is early or late by is believed, each frame: little, so
// that the clock the timecode goes on by between frames is steady, and enough for it to
// keep to a source whose clock is not quite this computer's
constexpr double kFollow = 0.1;

}

int count(Format format)
{
    return format == Fps24 ? 24 : format == Fps25 ? 25 : 30;
}

double rate(Format format)
{
    return format == Fps24 ? 24.0 : format == Fps25 ? 25.0 : format == Fps2997 ? 30000.0 / 1001.0 : 30.0;
}

QVariantList formats()
{
    return {QVariantMap {{"value", int(Fps24)}, {"name", QStringLiteral("24 fps")}},
            QVariantMap {{"value", int(Fps25)}, {"name", QStringLiteral("25 fps")}},
            QVariantMap {{"value", int(Fps2997)}, {"name", QStringLiteral("29.97 fps")}},
            QVariantMap {{"value", int(Fps30)}, {"name", QStringLiteral("30 fps")}}};
}

Format formatOf(int value)
{
    return value == Fps24 ? Fps24 : value == Fps25 ? Fps25 : value == Fps2997 ? Fps2997 : Fps30;
}

double faceSeconds(const Label &label, Format format)
{
    return label.hours * 3600.0 + label.minutes * 60.0 + label.seconds + double(label.frames) / count(format);
}

Label labelAt(double faceSeconds, Format format)
{
    const int each = count(format);
    // (In whole frames, so that a time a hair under a frame's is that frame and not the
    // one before: 3,600.4 at thirty frames is twelve frames past the hour, not eleven.)
    qint64 frames = qint64(std::floor(std::clamp(faceSeconds, 0.0, kDay - 1.0 / each) * each + 1e-6));
    Label label;
    label.frames = int(frames % each);
    frames /= each;
    label.seconds = int(frames % 60);
    label.minutes = int(frames / 60 % 60);
    label.hours = int(frames / 3600 % 24);
    return label;
}

QString text(const Label &label)
{
    const auto two = [](int value) { return QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0')); };
    return two(label.hours) + u':' + two(label.minutes) + u':' + two(label.seconds) + u';' + two(label.frames);
}

QString faceText(double faceSeconds, Format format)
{
    return text(labelAt(faceSeconds, format));
}

double typed(const QString &text, Format format)
{
    const QString given = text.trimmed();
    static const QRegularExpression allowed(QStringLiteral("^[0-9:;.]+$"));
    if (given.isEmpty() || !allowed.match(given).hasMatch())
        return -1;
    QList<int> parts;
    static const QRegularExpression marks(QStringLiteral("[:;.]"));
    if (!given.contains(marks)) {
        // Pairs of digits from the right: frames, seconds, minutes, hours
        if (given.size() > 8)
            return -1;
        for (qsizetype end = given.size(); end > 0; end -= 2)
            parts.prepend(given.mid(std::max<qsizetype>(0, end - 2), std::min<qsizetype>(2, end)).toInt());
    } else {
        const QStringList split = given.split(marks);
        if (split.size() > 4)
            return -1;
        for (const QString &part : split) {
            if (part.size() > 2)
                return -1;
            parts.append(part.toInt());
        }
        // With colons alone and fewer than four parts, the last is seconds: "1:00:00"
        // is an hour. With frames marked off (";" or "."), the last is frames.
        if (split.size() < 4 && !given.contains(u';') && !given.contains(u'.'))
            parts.append(0);
    }
    while (parts.size() < 4)
        parts.prepend(0);
    const Label label {parts.at(0), parts.at(1), parts.at(2), parts.at(3)};
    if (label.hours > 23 || label.minutes > 59 || label.seconds > 59 || label.frames >= count(format))
        return -1;
    return faceSeconds(label, format);
}

qint64 frameNumber(const Label &label, Format format, bool drop)
{
    const qint64 each = count(format);
    qint64 frames = (label.hours * 3600LL + label.minutes * 60LL + label.seconds) * each + label.frames;
    if (drop && format == Fps2997) {
        // Two numbers are passed over at the start of every minute but each tenth
        const qint64 minutes = label.hours * 60LL + label.minutes;
        frames -= 2 * (minutes - minutes / 10);
    }
    return frames;
}

Label labelOf(qint64 frameNumber, Format format, bool drop)
{
    qint64 frames = std::max<qint64>(0, frameNumber);
    const qint64 each = count(format);
    if (drop && format == Fps2997) {
        // The numbers passed over, put back: 17,982 frames in ten minutes, 1,798 in each
        // minute after the first of them
        const qint64 tens = frames / 17982;
        const qint64 within = frames % 17982;
        frames += 18 * tens + (within > 1 ? 2 * ((within - 2) / 1798) : 0);
    }
    Label label;
    label.frames = int(frames % each);
    frames /= each;
    label.seconds = int(frames % 60);
    label.minutes = int(frames / 60 % 60);
    label.hours = int(frames / 3600 % 24);
    return label;
}

double realSeconds(const Label &label, Format format, bool drop)
{
    return frameNumber(label, format, drop) / rate(format);
}

SampleClock::SampleClock(int sampleRate) : m_rate(std::max(1, sampleRate))
{
}

void SampleClock::arrived(qint64 total, double at)
{
    const double zero = at - double(total) / m_rate;
    if (!m_any) {
        m_any = true;
        m_zero = zero;
        m_since = at;
        m_hasBefore = false;
        return;
    }
    if (at - m_since > 1.0) {
        m_zeroBefore = m_zero;
        m_hasBefore = true;
        m_zero = zero;
        m_since = at;
    } else if (zero < m_zero) {
        m_zero = zero;
    }
}

double SampleClock::at(double index) const
{
    return (m_hasBefore ? std::min(m_zero, m_zeroBefore) : m_zero) + index / m_rate;
}

void Chase::setFormat(Format format)
{
    if (format == m_format)
        return;
    m_format = format;
    reset();
}

void Chase::reset()
{
    m_running = false;
    m_drop = false;
    m_frame = 0;
    m_began = 0;
    m_heard = 0;
    m_odd = false;
}

Chase::Change Chase::frame(const Label &label, bool drop, double at)
{
    const double period = 1 / rate(m_format);
    const qint64 number = frameNumber(label, m_format, drop);
    if (m_running) {
        // The frame that has just ended, if the timecode has gone on as it was going
        const double due = m_frame + (at - m_began) / period - 1;
        if (std::abs(double(number) - due) <= kNear) {
            // The next begins now, or would have by the clock: mostly as the clock has
            // it, and a little as this frame does
            const double byClock = m_began + (double(number) + 1 - m_frame) * period;
            m_frame = double(number) + 1;
            m_began = byClock + (at - byClock) * kFollow;
            m_heard = at;
            m_drop = drop;
            m_odd = false;
            return Nothing;
        }
    }
    // Out of place (or the first after silence): believed if the one before it was the
    // frame before it, a frame's length ago
    const bool follows = m_odd && number == m_oddFrame + 1 && std::abs((at - m_oddAt) / period - 1) <= kNear;
    if (!follows) {
        m_odd = true;
        m_oddFrame = number;
        m_oddAt = at;
        return Nothing;
    }
    const Change change = m_running ? Jumped : Started;
    m_running = true;
    m_frame = double(number) + 1;
    m_began = at;
    m_heard = at;
    m_drop = drop;
    m_odd = false;
    return change;
}

Chase::Change Chase::heard(double at)
{
    const double period = 1 / rate(m_format);
    if (m_odd && at - m_oddAt > kSilence * period)
        m_odd = false;
    if (!m_running || at - m_heard <= kSilence * period)
        return Nothing;
    // Stopped, at the frame after the last that was read (which is what m_frame is)
    m_running = false;
    return Stopped;
}

double Chase::frames(double now) const
{
    if (!m_running)
        return m_frame;
    const double period = 1 / rate(m_format);
    // (No further on than it could have got to without a frame being read)
    return m_frame + std::clamp((now - m_began) / period, 0.0, kSilence + 1);
}

Label Chase::label(double now) const
{
    return labelOf(qint64(std::floor(frames(now) + 1e-6)), m_format, m_drop);
}

QVariantMap under(const QVariantList &rows, double frames, Format format, bool drop)
{
    QVariantMap found;
    double foundStart = -1;
    for (const QVariant &entry : rows) {
        const QVariantMap row = entry.toMap();
        const double start = double(frameNumber(labelAt(row.value("start").toDouble(), format), format, drop));
        const double position = (frames - start) / rate(format);
        const double duration = row.value("duration").toDouble();
        if (position < 0 || position >= duration || start <= foundStart)
            continue;
        found = row;
        found.insert("position", position);
        foundStart = start;
    }
    return found;
}

} // namespace timecode
