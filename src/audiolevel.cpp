#include "audiolevel.h"

#include <QtMath>

#include <cstring>

namespace audiolevel {

namespace {

template <typename Sample, typename Loudness>
Peaks peaksOf(const void *data, qsizetype bytes, int channels, Loudness loudness)
{
    Peaks found;
    const qsizetype frames = bytes / qsizetype(sizeof(Sample) * channels);
    const auto *bytesAt = static_cast<const uchar *>(data);
    for (qsizetype frame = 0; frame < frames; ++frame) {
        // (Read by copying: the samples need not start where the processor would have
        // such a number start.)
        Sample left;
        std::memcpy(&left, bytesAt + frame * channels * sizeof(Sample), sizeof(Sample));
        found.left = qMax(found.left, loudness(left));
        if (channels > 1) {
            Sample right;
            std::memcpy(&right, bytesAt + (frame * channels + 1) * sizeof(Sample), sizeof(Sample));
            found.right = qMax(found.right, loudness(right));
        }
    }
    if (channels == 1)
        found.right = found.left;
    found.left = qMin(1.0f, found.left);
    found.right = qMin(1.0f, found.right);
    return found;
}

} // namespace

Peaks peaks(const void *data, qsizetype bytes, Format format, int channels)
{
    if (!data || bytes <= 0 || channels < 1)
        return {};
    switch (format) {
    case Format::UInt8:
        // (Silence is the middle of what a byte can say.)
        return peaksOf<quint8>(data, bytes, channels, [](quint8 sample) { return qAbs(int(sample) - 128) / 128.0f; });
    case Format::Int16:
        return peaksOf<qint16>(data, bytes, channels, [](qint16 sample) { return qAbs(int(sample)) / 32768.0f; });
    case Format::Int32:
        return peaksOf<qint32>(data, bytes, channels, [](qint32 sample) { return float(qAbs(double(sample)) / 2147483648.0); });
    case Format::Float:
        // (A sample that is no number at all is taken for silence.)
        return peaksOf<float>(data, bytes, channels, [](float sample) { return qIsFinite(sample) ? qAbs(sample) : 0.0f; });
    }
    return {};
}

double metered(double peak)
{
    if (!(peak > 0))
        return 0;
    const double decibels = 20 * std::log10(qMin(1.0, peak));
    return qBound(0.0, 1 + decibels / 60, 1.0);
}

double settled(double shown, double level, double seconds)
{
    const double fallen = qMax(0.0, shown - qMax(0.0, seconds) / kFall);
    return qBound(0.0, qMax(level, fallen), 1.0);
}

double fadeIn(double along)
{
    return std::sin(qBound(0.0, along, 1.0) * M_PI_2);
}

double fadeOut(double along)
{
    return std::cos(qBound(0.0, along, 1.0) * M_PI_2);
}

} // namespace audiolevel
