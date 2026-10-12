// How a piece of audio plays (src/audiocue.h): read from a row of the audio bin made
// as ProPresenter writes one, and changed there, with a look each time at what else of
// the row is still as it was.

#include "audiocue.h"

#include <google/protobuf/unknown_field_set.h>

#include <QTest>

using namespace audiocue;
using rv::data::Action;
using rv::data::Cue;
using Transport = rv::data::Media::TransportProperties;
using AudioType = rv::data::Action::MediaType::Audio;

namespace {

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

// A row's cue as the one file of ProPresenter's own that was looked at has them, field
// for field: version 21.4 on a Mac, a row that goes on to the next. The names and the
// ids are made up. (Which outputs each channel goes to was the user's own doing in
// that file, and is here as something a change must leave alone.) And one field that
// the descriptions of ProPresenter's files have no name for, as a later version's
// might be.
Cue proPresentersOwn()
{
    Cue cue;
    cue.mutable_uuid()->set_string("0A0A0A0A-1111-2222-3333-444444444444");
    cue.set_name("A Made Up Tune");
    cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_NEXT);
    cue.set_completion_action_type(Cue::COMPLETION_ACTION_TYPE_LAST);
    cue.mutable_hot_key();
    cue.set_isenabled(true);
    Action *action = cue.add_actions();
    action->mutable_uuid()->set_string("0B0B0B0B-1111-2222-3333-444444444444");
    action->set_isenabled(true);
    action->set_duration(280.57974243164062);
    action->set_type(Action::ACTION_TYPE_MEDIA);
    action->mutable_media()->mutable_audio();
    rv::data::Media *media = action->mutable_media()->mutable_element();
    media->mutable_uuid()->set_string("0C0C0C0C-1111-2222-3333-444444444444");
    rv::data::URL *url = media->mutable_url();
    url->set_absolute_string("file:///Users/someone/Library/Application%20Support/RenewedVision/ProPresenter/Media/Assets/A%20Made%20Up%20Tune.mp3");
    url->set_platform(rv::data::URL::PLATFORM_MACOS);
    url->mutable_local()->set_root(rv::data::URL::LocalRelativePath::ROOT_SHOW);
    url->mutable_local()->set_path("Media/Assets/A Made Up Tune.mp3");
    media->mutable_metadata()->set_artist("unknown");
    media->mutable_metadata()->set_format("mp3float");
    media->mutable_metadata()->set_color_format(rv::data::Media::Metadata::COLOR_FORMAT_SDR);
    rv::data::Media::AudioTypeProperties *audio = media->mutable_audio();
    audio->mutable_audio()->set_volume(1);
    audio->mutable_audio()->add_audio_channels()->add_outputs()->set_channel_index(2);
    rv::data::Media::Audio::Channel *second = audio->mutable_audio()->add_audio_channels();
    second->set_index(1);
    second->add_outputs()->set_channel_index(3);
    audio->mutable_audio()->set_is_custom_mapping(true);
    Transport *transport = audio->mutable_transport();
    transport->set_play_rate(1);
    transport->set_out_point(280.57974243164062);
    transport->set_should_fade_in(true);
    transport->set_should_fade_out(true);
    transport->set_end_point(280.57974243164062);
    transport->set_times_to_loop(1);
    transport->set_retrigger(Transport::RETRIGGER_SETTING_AUTOMATIC);
    *audio->mutable_file()->mutable_local_url() = *url;
    action->GetReflection()->MutableUnknownFields(action)->AddVarint(4000, 7);
    return cue;
}

// The same row as that file's one row that goes round has it: the media goes round,
// and the cue names nothing to follow.
Cue proPresentersLooping()
{
    Cue cue = proPresentersOwn();
    cue.clear_completion_target_type();
    cue.mutable_actions(0)->mutable_media()->mutable_element()->mutable_audio()->mutable_transport()->set_playback_behavior(
        Transport::PLAYBACK_BEHAVIOR_LOOP);
    return cue;
}

} // namespace

class TestAudioCue : public QObject
{
    Q_OBJECT

private slots:
    // ---- Reading
    void aRowIsToldByItsMediaBeingASoundFile()
    {
        Cue cue = proPresentersOwn();
        QVERIFY(isAudio(cue.actions(0)));
        QCOMPARE(audioOf(cue), &cue.actions(0));
        // A video, a clear, a cue with nothing: none of them audio
        Action video;
        video.set_type(Action::ACTION_TYPE_MEDIA);
        video.mutable_media()->mutable_element()->mutable_video();
        QVERIFY(!isAudio(video));
        Action clear;
        clear.mutable_clear();
        QVERIFY(!isAudio(clear));
        QVERIFY(audioOf(Cue()) == nullptr);
    }

    void aRowWhoseCueNamesTheNextGoesOnToTheNext()
    {
        const Cue cue = proPresentersOwn();
        const Behaviour behaviour = read(cue.actions(0), &cue);
        QCOMPARE(behaviour.playback, "next");
        QVERIFY(!behaviour.effect);
        QCOMPARE(behaviour.volume, 1.0);
        QCOMPARE(behaviour.duration, 280.57974243164062);
        QCOMPARE(behaviour.loopCount, 0);
        QCOMPARE(behaviour.loopSeconds, 0.0);
    }

    void aRowWhoseMediaGoesRoundLoopsWhateverItsCueSays()
    {
        const Cue looping = proPresentersLooping();
        QCOMPARE(read(looping.actions(0), &looping).playback, "loop");
        // (It never comes to an end for the next to follow.)
        Cue both = proPresentersLooping();
        both.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_NEXT);
        QCOMPARE(read(both.actions(0), &both).playback, "loop");
    }

    void aRowThatNamesNothingAndDoesNotGoRoundStops()
    {
        Cue cue = proPresentersOwn();
        cue.clear_completion_target_type();
        QCOMPARE(read(cue.actions(0), &cue).playback, "stop");
        // A target that is neither nothing nor the next is not the next.
        cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_RANDOM);
        QCOMPARE(read(cue.actions(0), &cue).playback, "stop");
    }

    void anActionWithNoCueHasNothingThatCouldBeNext()
    {
        const Cue cue = proPresentersOwn();
        QCOMPARE(read(cue.actions(0), nullptr).playback, "stop");
        const Cue looping = proPresentersLooping();
        QCOMPARE(read(looping.actions(0), nullptr).playback, "loop");
    }

    void goingRoundSoManyTimesOrForSoLongIsToldWithIt()
    {
        Cue cue = proPresentersOwn();
        Transport *transport = cue.mutable_actions(0)->mutable_media()->mutable_element()->mutable_audio()->mutable_transport();
        transport->set_playback_behavior(Transport::PLAYBACK_BEHAVIOR_LOOP_FOR_COUNT);
        transport->set_times_to_loop(3);
        Behaviour behaviour = read(cue.actions(0), &cue);
        QCOMPARE(behaviour.playback, "loop");
        QCOMPARE(behaviour.loopCount, 3);
        transport->set_playback_behavior(Transport::PLAYBACK_BEHAVIOR_LOOP_FOR_TIME);
        transport->set_loop_time(90);
        behaviour = read(cue.actions(0), &cue);
        QCOMPARE(behaviour.playback, "loop");
        QCOMPARE(behaviour.loopCount, 0);
        QCOMPARE(behaviour.loopSeconds, 90.0);
    }

    void theActionsOwnWordForGoingRoundIsBelievedWhereAFileHasIt()
    {
        Cue cue = proPresentersOwn();
        cue.mutable_actions(0)->mutable_media()->mutable_audio()->set_playback_behavior(Action::MediaType::PLAYBACK_BEHAVIOR_LOOP);
        QCOMPARE(read(cue.actions(0), &cue).playback, "loop");
    }

    void musicOrASoundEffectIsTheActions()
    {
        Cue cue = proPresentersOwn();
        QVERIFY(!read(cue.actions(0), &cue).effect);
        cue.mutable_actions(0)->mutable_media()->mutable_audio()->set_audio_type(AudioType::MEDIA_ACTION_AUDIO_TYPE_SOUND);
        QVERIFY(read(cue.actions(0), &cue).effect);
    }

    void howLoudAndHowLong()
    {
        Cue cue = proPresentersOwn();
        rv::data::Media::AudioTypeProperties *audio = cue.mutable_actions(0)->mutable_media()->mutable_element()->mutable_audio();
        audio->mutable_audio()->set_volume(0.4);
        QCOMPARE(read(cue.actions(0), &cue).volume, 0.4);
        audio->mutable_audio()->set_volume(3);
        QCOMPARE(read(cue.actions(0), &cue).volume, 1.0);
        // Media that says nothing of its sound is as loud as its file.
        audio->clear_audio();
        QCOMPARE(read(cue.actions(0), &cue).volume, 1.0);
        // An action that does not say how long: from where the media starts to where it stops
        cue.mutable_actions(0)->clear_duration();
        audio->mutable_transport()->set_in_point(10);
        QCOMPARE(read(cue.actions(0), &cue).duration, 270.57974243164062);
        audio->mutable_transport()->clear_out_point();
        QCOMPARE(read(cue.actions(0), &cue).duration, 0.0);
    }

    void aRowOfAListHasWhatPlaysIt()
    {
        const Cue cue = proPresentersLooping();
        QVariantMap row;
        read(cue.actions(0), &cue).describe(&row);
        QCOMPARE(row.value("playback").toString(), "loop");
        QCOMPARE(row.value("effect").toBool(), false);
        QCOMPARE(row.value("volume").toDouble(), 1.0);
        QVERIFY(row.contains("duration") && row.contains("loopCount") && row.contains("loopSeconds"));
    }

    // ---- Changing, and what is left alone
    void loopingIsSetOnTheMediaAndNothingElseIsTouched()
    {
        Cue cue = proPresentersOwn();
        cue.clear_completion_target_type();
        const Cue stopped = cue;
        QVERIFY(setPlayback(cue.mutable_actions(0), &cue, "loop"));
        QCOMPARE(read(cue.actions(0), &cue).playback, "loop");
        // It is, to the byte, the row that goes round as ProPresenter wrote one.
        QCOMPARE(encoded(cue), encoded(proPresentersLooping()));
        // And back: as it was, to the byte, but that the media now says in so many
        // words that it stops, which says nothing.
        QVERIFY(setPlayback(cue.mutable_actions(0), &cue, "stop"));
        QCOMPARE(read(cue.actions(0), &cue).playback, "stop");
        QCOMPARE(encoded(cue), encoded(stopped));
    }

    void nextIsSetOnTheCueAndNothingElseIsTouched()
    {
        Cue cue = proPresentersOwn();
        cue.clear_completion_target_type();
        QVERIFY(setPlayback(cue.mutable_actions(0), &cue, "next"));
        QCOMPARE(read(cue.actions(0), &cue).playback, "next");
        // It is, to the byte, the row that goes on to the next as ProPresenter wrote it.
        QCOMPARE(encoded(cue), encoded(proPresentersOwn()));
    }

    void fromGoingRoundToTheNextStopsItGoingRound()
    {
        Cue cue = proPresentersLooping();
        QVERIFY(setPlayback(cue.mutable_actions(0), &cue, "next"));
        QCOMPARE(read(cue.actions(0), &cue).playback, "next");
        QCOMPARE(encoded(cue), encoded(proPresentersOwn()));
        // And from the next to going round takes the next off.
        QVERIFY(setPlayback(cue.mutable_actions(0), &cue, "loop"));
        QCOMPARE(encoded(cue), encoded(proPresentersLooping()));
    }

    void whatItHasAlreadyChangesNothing()
    {
        Cue cue = proPresentersOwn();
        const std::string before = encoded(cue);
        QVERIFY(setPlayback(cue.mutable_actions(0), &cue, "next"));
        QCOMPARE(encoded(cue), before);
        setEffect(cue.mutable_actions(0), false);
        QCOMPARE(encoded(cue), before);
    }

    void aCueNamedToFollowIsGivenUpForWhatWasAsked()
    {
        Cue cue = proPresentersOwn();
        cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_CUE);
        cue.mutable_completion_target_uuid()->set_string("0D0D0D0D-1111-2222-3333-444444444444");
        QVERIFY(setPlayback(cue.mutable_actions(0), &cue, "stop"));
        QCOMPARE(cue.completion_target_type(), Cue::COMPLETION_TARGET_TYPE_NONE);
        QVERIFY(!cue.has_completion_target_uuid());
        // (The completion action is left as it was found.)
        QCOMPARE(cue.completion_action_type(), Cue::COMPLETION_ACTION_TYPE_LAST);
    }

    void anActionOnASlideCanStopOrGoRoundAndNextIsStop()
    {
        Cue cue = proPresentersLooping();
        Action action = cue.actions(0);
        QVERIFY(setPlayback(&action, nullptr, "next"));
        QCOMPARE(read(action, nullptr).playback, "stop");
        QVERIFY(setPlayback(&action, nullptr, "loop"));
        QCOMPARE(read(action, nullptr).playback, "loop");
    }

    void whatIsNoWayOfPlayingAndWhatIsNoAudioIsRefused()
    {
        Cue cue = proPresentersOwn();
        const std::string before = encoded(cue);
        QVERIFY(!setPlayback(cue.mutable_actions(0), &cue, "shuffle"));
        QVERIFY(!setPlayback(cue.mutable_actions(0), &cue, ""));
        QCOMPARE(encoded(cue), before);
        Action video;
        video.mutable_media()->mutable_element()->mutable_video();
        QVERIFY(!setPlayback(&video, nullptr, "loop"));
    }

    void aSoundEffectIsSetOnTheActionAndNothingElseIsTouched()
    {
        Cue cue = proPresentersOwn();
        const std::string before = encoded(cue);
        setEffect(cue.mutable_actions(0), true);
        QVERIFY(read(cue.actions(0), &cue).effect);
        Cue expected = proPresentersOwn();
        expected.mutable_actions(0)->mutable_media()->mutable_audio()->set_audio_type(AudioType::MEDIA_ACTION_AUDIO_TYPE_SOUND);
        QCOMPARE(encoded(cue), encoded(expected));
        // And music again: the row as ProPresenter wrote it, to the byte.
        setEffect(cue.mutable_actions(0), false);
        QCOMPARE(encoded(cue), before);
    }

    void whatTheDescriptionsHaveNoNameForIsKept()
    {
        Cue cue = proPresentersOwn();
        setPlayback(cue.mutable_actions(0), &cue, "loop");
        setEffect(cue.mutable_actions(0), true);
        const auto &unknown = cue.actions(0).GetReflection()->GetUnknownFields(cue.actions(0));
        QCOMPARE(unknown.field_count(), 1);
        QCOMPARE(unknown.field(0).number(), 4000);
        // And the user's own choice of outputs for the channels
        const auto &sound = cue.actions(0).media().element().audio().audio();
        QCOMPARE(sound.audio_channels_size(), 2);
        QVERIFY(sound.is_custom_mapping());
        QCOMPARE(sound.audio_channels(1).outputs(0).channel_index(), 3u);
    }

    // ---- A new row
    void aNewRowIsAsProPresentersAreLessWhatWasTheUsersOwn()
    {
        int made = 0;
        const auto newId = [&made] { return "ID-" + std::to_string(++made); };
        rv::data::URL file;
        file.set_absolute_string("file:///somewhere/A%20Made%20Up%20Tune.mp3");
        Cue cue;
        make(&cue, "A Made Up Tune", file, "MP3", 12.5, newId);
        QCOMPARE(made, 3);
        QCOMPARE(cue.name(), "A Made Up Tune");
        QCOMPARE(cue.uuid().string(), "ID-1");
        QVERIFY(cue.isenabled() && cue.has_hot_key());
        QCOMPARE(cue.completion_action_type(), Cue::COMPLETION_ACTION_TYPE_LAST);
        QCOMPARE(cue.actions_size(), 1);
        const Action &action = cue.actions(0);
        QVERIFY(isAudio(action));
        QVERIFY(action.isenabled());
        QCOMPARE(action.type(), Action::ACTION_TYPE_MEDIA);
        QCOMPARE(action.duration(), 12.5);
        QVERIFY(action.media().has_audio());
        const Behaviour behaviour = read(action, &cue);
        QCOMPARE(behaviour.playback, "next");
        QVERIFY(!behaviour.effect);
        QCOMPARE(behaviour.volume, 1.0);
        QCOMPARE(behaviour.duration, 12.5);
        const rv::data::Media &media = action.media().element();
        QCOMPARE(media.url().absolute_string(), file.absolute_string());
        QCOMPARE(media.audio().file().local_url().absolute_string(), file.absolute_string());
        QCOMPARE(media.metadata().format(), "MP3");
        const Transport &transport = media.audio().transport();
        QCOMPARE(transport.play_rate(), 1.0);
        QCOMPARE(transport.out_point(), 12.5);
        QCOMPARE(transport.end_point(), 12.5);
        QVERIFY(transport.should_fade_in() && transport.should_fade_out());
        QCOMPARE(transport.times_to_loop(), 1u);
        QCOMPARE(transport.retrigger(), Transport::RETRIGGER_SETTING_AUTOMATIC);
        // Nothing is said of which outputs the channels go to.
        QCOMPARE(media.audio().audio().audio_channels_size(), 0);
        QVERIFY(!media.audio().audio().is_custom_mapping());
    }

    void aNewRowWhoseLengthIsNotKnownSaysNothingOfIt()
    {
        int made = 0;
        Cue cue;
        make(&cue, "A Made Up Tune", rv::data::URL(), "WAV", 0, [&made] { return "ID-" + std::to_string(++made); });
        QCOMPARE(cue.actions(0).duration(), 0.0);
        QCOMPARE(cue.actions(0).media().element().audio().transport().out_point(), 0.0);
        QCOMPARE(read(cue.actions(0), &cue).duration, 0.0);
    }
};

QTEST_APPLESS_MAIN(TestAudioCue)
#include "tst_audiocue.moc"
