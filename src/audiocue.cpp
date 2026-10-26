#include "audiocue.h"

#include "audiorouting.h"

namespace audiocue {

using Transport = rv::data::Media::TransportProperties;
using AudioType = rv::data::Action::MediaType::Audio;

bool isAudio(const rv::data::Action &action)
{
    return action.has_media() && action.media().element().has_audio();
}

const rv::data::Action *audioOf(const rv::data::Cue &cue)
{
    for (const rv::data::Action &action : cue.actions()) {
        if (isAudio(action))
            return &action;
    }
    return nullptr;
}

rv::data::Action *audioOf(rv::data::Cue *cue)
{
    for (rv::data::Action &action : *cue->mutable_actions()) {
        if (isAudio(action))
            return &action;
    }
    return nullptr;
}

void Behaviour::describe(QVariantMap *row) const
{
    row->insert("playback", playback);
    row->insert("effect", effect);
    row->insert("volume", volume);
    row->insert("duration", duration);
    row->insert("loopCount", loopCount);
    row->insert("loopSeconds", loopSeconds);
    if (!sound.isEmpty())
        row->insert("sound", sound);
}

Behaviour read(const rv::data::Action &action, const rv::data::Cue *cue)
{
    Behaviour behaviour;
    const rv::data::Media::AudioTypeProperties &audio = action.media().element().audio();
    const Transport &transport = audio.transport();
    // Whether it goes round is the media's. (And the action's own word for it, which
    // the file looked at never uses, is believed if it is there: see the GUESSES.)
    const bool round = transport.playback_behavior() != Transport::PLAYBACK_BEHAVIOR_STOP
                       || action.media().audio().playback_behavior() != rv::data::Action::MediaType::PLAYBACK_BEHAVIOR_STOP;
    if (round) {
        behaviour.playback = QStringLiteral("loop");
        if (transport.playback_behavior() == Transport::PLAYBACK_BEHAVIOR_LOOP_FOR_COUNT)
            behaviour.loopCount = int(qMax(1u, transport.times_to_loop()));
        else if (transport.playback_behavior() == Transport::PLAYBACK_BEHAVIOR_LOOP_FOR_TIME)
            behaviour.loopSeconds = qMax(0.0, transport.loop_time());
    } else if (cue && cue->completion_target_type() == rv::data::Cue::COMPLETION_TARGET_TYPE_NEXT) {
        behaviour.playback = QStringLiteral("next");
    }
    behaviour.effect = action.media().audio().audio_type() == AudioType::MEDIA_ACTION_AUDIO_TYPE_SOUND;
    // (Media with nothing said of its sound is as loud as its file.)
    behaviour.volume = audio.has_audio() ? qBound(0.0, audio.audio().volume(), 1.0) : 1.0;
    // How long: the action says, and failing that where the media is to stop.
    behaviour.duration = action.duration() > 0 ? action.duration() : qMax(0.0, transport.out_point() - transport.in_point());
    behaviour.sound = audiorouting::soundOf(action.media().element());
    return behaviour;
}

bool setPlayback(rv::data::Action *action, rv::data::Cue *cue, const QString &playback)
{
    const bool loop = playback == QLatin1String("loop");
    const bool next = playback == QLatin1String("next");
    if (!loop && !next && playback != QLatin1String("stop"))
        return false;
    if (!isAudio(*action))
        return false;
    // The media's going round. (Only where it is to be other than it is: an untouched
    // row is not given a transport it had not got.)
    const Transport &now = action->media().element().audio().transport();
    const auto wanted = loop ? Transport::PLAYBACK_BEHAVIOR_LOOP : Transport::PLAYBACK_BEHAVIOR_STOP;
    if (now.playback_behavior() != wanted)
        action->mutable_media()->mutable_element()->mutable_audio()->mutable_transport()->set_playback_behavior(wanted);
    // And the action's own word for it, if a file should have used that: one that
    // says it goes round would be read as going round still.
    if (action->media().audio().playback_behavior() != rv::data::Action::MediaType::PLAYBACK_BEHAVIOR_STOP && !loop)
        action->mutable_media()->mutable_audio()->set_playback_behavior(rv::data::Action::MediaType::PLAYBACK_BEHAVIOR_STOP);
    // What follows is the cue's. A target that is neither nothing nor the next (a
    // cue named, a random one: ProPresenter's, which nothing here makes) is given up
    // for what was asked, since the row is to play as was asked.
    if (cue) {
        const auto target = next ? rv::data::Cue::COMPLETION_TARGET_TYPE_NEXT : rv::data::Cue::COMPLETION_TARGET_TYPE_NONE;
        if (cue->completion_target_type() != target) {
            cue->set_completion_target_type(target);
            cue->clear_completion_target_uuid();
        }
    }
    return true;
}

void setEffect(rv::data::Action *action, bool effect)
{
    const auto wanted = effect ? AudioType::MEDIA_ACTION_AUDIO_TYPE_SOUND : AudioType::MEDIA_ACTION_AUDIO_TYPE_TUNE;
    if (action->media().audio().audio_type() != wanted)
        action->mutable_media()->mutable_audio()->set_audio_type(wanted);
}

void make(rv::data::Cue *cue, const QString &name, const rv::data::URL &file, const QString &format, double seconds,
          const std::function<std::string()> &newId)
{
    cue->mutable_uuid()->set_string(newId());
    cue->set_name(name.toStdString());
    // (See the GUESSES: a new row goes on to the next.)
    cue->set_completion_target_type(rv::data::Cue::COMPLETION_TARGET_TYPE_NEXT);
    cue->set_completion_action_type(rv::data::Cue::COMPLETION_ACTION_TYPE_LAST);
    cue->mutable_hot_key();
    cue->set_isenabled(true);

    rv::data::Action *action = cue->add_actions();
    action->mutable_uuid()->set_string(newId());
    action->set_isenabled(true);
    if (seconds > 0)
        action->set_duration(seconds);
    action->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
    // Music, which is what an action that says nothing is.
    action->mutable_media()->mutable_audio();

    rv::data::Media *media = action->mutable_media()->mutable_element();
    media->mutable_uuid()->set_string(newId());
    *media->mutable_url() = file;
    media->mutable_metadata()->set_format(format.toStdString());
    rv::data::Media::AudioTypeProperties *audio = media->mutable_audio();
    audio->mutable_audio()->set_volume(1);
    Transport *transport = audio->mutable_transport();
    transport->set_play_rate(1);
    if (seconds > 0) {
        transport->set_out_point(seconds);
        transport->set_end_point(seconds);
    }
    transport->set_should_fade_in(true);
    transport->set_should_fade_out(true);
    transport->set_times_to_loop(1);
    transport->set_retrigger(Transport::RETRIGGER_SETTING_AUTOMATIC);
    *audio->mutable_file()->mutable_local_url() = file;
}

} // namespace audiocue
