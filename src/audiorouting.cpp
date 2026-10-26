#include "audiorouting.h"

#include "audiolevel.h"

#include <QVariantList>

#include <algorithm>
#include <cmath>

namespace audiorouting {

Routes autoMedia(int mediaChannels, int appChannels)
{
    Routes routes;
    if (mediaChannels <= 0)
        return routes;
    for (int channel = 0; channel < mediaChannels; ++channel)
        routes.append(appChannels > 0 ? QList<int>{channel % appChannels} : QList<int>{});
    if (mediaChannels == 1 && appChannels >= 2)
        routes[0] = {0, 1};
    return routes;
}

Routes autoDevice(int appChannels, int deviceChannels)
{
    Routes routes;
    for (int channel = 0; channel < qMax(0, appChannels); ++channel)
        routes.append(channel < deviceChannels ? QList<int>{channel} : QList<int>{});
    return routes;
}

Routes fitted(const Routes &routes, int inputs, int outputs)
{
    Routes fit;
    for (int input = 0; input < qMax(0, inputs); ++input) {
        QList<int> to;
        for (const int output : input < routes.size() ? routes[input] : QList<int>{}) {
            if (output >= 0 && output < outputs && !to.contains(output))
                to.append(output);
        }
        std::sort(to.begin(), to.end());
        fit.append(to);
    }
    return fit;
}

Routes toggled(const Routes &routes, int input, int output, int inputs, int outputs)
{
    Routes fit = fitted(routes, inputs, outputs);
    if (input < 0 || input >= inputs || output < 0 || output >= outputs)
        return fit;
    if (fit[input].contains(output))
        fit[input].removeAll(output);
    else
        fit[input].append(output);
    std::sort(fit[input].begin(), fit[input].end());
    return fit;
}

bool same(const Routes &a, const Routes &b)
{
    const int inputs = int(qMax(a.size(), b.size()));
    int outputs = 0;
    for (const Routes *routes : {&a, &b}) {
        for (const QList<int> &to : *routes) {
            for (const int output : to)
                outputs = qMax(outputs, output + 1);
        }
    }
    return fitted(a, inputs, outputs) == fitted(b, inputs, outputs);
}

QList<bool> heard(const QList<bool> &muted, const QList<bool> &soloed)
{
    const bool anySolo = soloed.contains(true);
    QList<bool> heard;
    for (qsizetype channel = 0; channel < muted.size(); ++channel)
        heard.append(!muted[channel] && (!anySolo || soloed.value(channel)));
    return heard;
}

float gainOf(double decibels, bool muted)
{
    if (muted || !std::isfinite(decibels))
        return 0.0f;
    return float(std::pow(10.0, qBound(-120.0, decibels, 24.0) / 20.0));
}

double rampGain(double sinceStart, double rampIn, double untilEnd, double rampOut)
{
    double gain = 1;
    if (rampIn > 0 && sinceStart < rampIn)
        gain *= audiolevel::fadeIn(qMax(0.0, sinceStart) / rampIn);
    if (rampOut > 0 && untilEnd >= 0 && untilEnd < rampOut)
        gain *= audiolevel::fadeOut(1 - untilEnd / rampOut);
    return gain;
}

QVariantMap soundOf(const rv::data::Media::AudioProperties &audio, const rv::data::Media::TransportProperties &transport)
{
    QVariantMap sound;
    if (audio.is_custom_mapping()) {
        Routes outputs;
        for (const auto &channel : audio.audio_channels()) {
            const int index = int(channel.index());
            if (index < 0 || index >= 64)
                continue;
            while (outputs.size() <= index)
                outputs.append(QList<int>{});
            for (const auto &output : channel.outputs()) {
                if (output.channel_index() >= 0 && !outputs[index].contains(output.channel_index()))
                    outputs[index].append(output.channel_index());
            }
        }
        QVariantList list;
        for (const QList<int> &to : std::as_const(outputs)) {
            QVariantList each;
            for (const int output : to)
                each.append(output);
            list.append(QVariant(each));
        }
        sound.insert("outputs", list);
    }
    QVariantList gains;
    bool anyGain = false;
    for (const auto &channel : audio.audio_channels()) {
        const int index = int(channel.index());
        if (index < 0 || index >= 64)
            continue;
        while (gains.size() <= index)
            gains.append(1.0);
        const float gain = gainOf(channel.volume(), channel.muted());
        gains[index] = double(gain);
        anyGain = anyGain || gain != 1.0f;
    }
    if (anyGain)
        sound.insert("gains", gains);
    if (transport.should_fade_in() && transport.fade_in_duration() > 0)
        sound.insert("rampIn", transport.fade_in_duration());
    if (transport.should_fade_out() && transport.fade_out_duration() > 0)
        sound.insert("rampOut", transport.fade_out_duration());
    return sound;
}

QVariantMap soundOf(const rv::data::Media &media)
{
    if (media.has_video())
        return soundOf(media.video().audio(), media.video().transport());
    if (media.has_audio())
        return soundOf(media.audio().audio(), media.audio().transport());
    return {};
}

} // namespace audiorouting
