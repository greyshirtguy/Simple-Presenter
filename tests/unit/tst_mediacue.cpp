// What follows a row of a media playlist when its video ends, ProPresenter's Next Cue
// (src/mediacue.h): read from a row made as ProPresenter writes one, and changed there,
// with a look each time at what else of the row is still as it was. (Which row then
// plays is the show's: tst_showstate.)

#include "mediacue.h"

#include <google/protobuf/unknown_field_set.h>

#include <QTest>

using namespace mediacue;
using rv::data::Action;
using rv::data::Cue;
using Transport = rv::data::Media::TransportProperties;

namespace {

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

// A row's cue as ProPresenter 21.4 writes one in its media bin, field for field: a
// background video that goes round, with nothing to follow it. The names and the ids
// are made up. And one field that the descriptions of ProPresenter's files have no name
// for, as a later version's might be.
Cue proPresentersOwn()
{
    Cue cue;
    cue.mutable_uuid()->set_string("1A1A1A1A-1111-2222-3333-444444444444");
    cue.set_name("A Made Up Loop");
    cue.set_completion_action_type(Cue::COMPLETION_ACTION_TYPE_LAST);
    cue.mutable_hot_key();
    cue.set_isenabled(true);
    Action *action = cue.add_actions();
    action->mutable_uuid()->set_string("1B1B1B1B-1111-2222-3333-444444444444");
    action->set_isenabled(true);
    action->set_duration(30);
    action->set_type(Action::ACTION_TYPE_MEDIA);
    action->mutable_media()->mutable_audio();
    rv::data::Media *media = action->mutable_media()->mutable_element();
    media->mutable_uuid()->set_string("1C1C1C1C-1111-2222-3333-444444444444");
    rv::data::URL *url = media->mutable_url();
    url->set_absolute_string("file:///Users/someone/Documents/ProPresenter/Media/Assets/A%20Made%20Up%20Loop.mp4");
    url->set_platform(rv::data::URL::PLATFORM_MACOS);
    url->mutable_local()->set_root(rv::data::URL::LocalRelativePath::ROOT_SHOW);
    url->mutable_local()->set_path("Media/Assets/A Made Up Loop.mp4");
    media->mutable_metadata()->set_format("hvc1");
    media->mutable_metadata()->set_color_format(rv::data::Media::Metadata::COLOR_FORMAT_SDR);
    rv::data::Media::VideoTypeProperties *video = media->mutable_video();
    video->mutable_drawing()->mutable_natural_size()->set_width(3840);
    video->mutable_drawing()->mutable_natural_size()->set_height(2160);
    video->mutable_drawing()->mutable_custom_image_bounds()->mutable_origin();
    video->mutable_drawing()->mutable_custom_image_bounds()->mutable_size();
    video->mutable_drawing()->mutable_crop_insets();
    video->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
    video->mutable_audio()->set_volume(1);
    Transport *transport = video->mutable_transport();
    transport->set_play_rate(1);
    transport->set_out_point(30);
    transport->set_should_fade_in(true);
    transport->set_should_fade_out(true);
    transport->set_end_point(30);
    transport->set_playback_behavior(Transport::PLAYBACK_BEHAVIOR_LOOP);
    transport->set_times_to_loop(1);
    transport->set_retrigger(Transport::RETRIGGER_SETTING_AUTOMATIC);
    video->mutable_video()->set_frame_rate(30);
    video->mutable_video()->set_thumbnail_position(-1);
    video->mutable_video()->set_soft_loop_duration(0.5);
    *video->mutable_file()->mutable_local_url() = *url;
    cue.GetReflection()->MutableUnknownFields(&cue)->AddVarint(900, 7);
    return cue;
}

} // namespace

class TestMediaCue : public QObject
{
    Q_OBJECT

private slots:
    void proPresentersOwnRowIsFollowedByNothing()
    {
        const Cue cue = proPresentersOwn();
        QVERIFY(read(cue).kind.isEmpty());
        QVariantMap described;
        describe(cue, &described);
        QCOMPARE(described.value("cueId").toString(), QStringLiteral("1A1A1A1A-1111-2222-3333-444444444444"));
        QVERIFY(!described.contains("next"));
        QVERIFY(!described.contains("nextCue"));
    }

    void eachKindIsReadAsTheCueSaysIt()
    {
        Cue cue = proPresentersOwn();
        cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_NEXT);
        QCOMPARE(read(cue).kind, QStringLiteral("next"));
        cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_RANDOM);
        QCOMPARE(read(cue).kind, QStringLiteral("random"));
        cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_FIRST);
        QCOMPARE(read(cue).kind, QStringLiteral("first"));
        cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_CUE);
        QVERIFY(read(cue).kind.isEmpty());
        cue.mutable_completion_target_uuid()->set_string("2A2A2A2A-1111-2222-3333-444444444444");
        QCOMPARE(read(cue).kind, QStringLiteral("cue"));
        QCOMPARE(read(cue).target, QStringLiteral("2A2A2A2A-1111-2222-3333-444444444444"));
        QVariantMap described;
        describe(cue, &described);
        QCOMPARE(described.value("next").toString(), QStringLiteral("cue"));
        QCOMPARE(described.value("nextCue").toString(), QStringLiteral("2A2A2A2A-1111-2222-3333-444444444444"));
    }

    void anythingToFollowStopsTheVideoAndLeavesTheRestAlone()
    {
        Cue cue = proPresentersOwn();
        Cue expected = proPresentersOwn();
        QVERIFY(set(&cue, cue.mutable_actions(0), QStringLiteral("next"), {}));
        expected.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_NEXT);
        expected.mutable_actions(0)->mutable_media()->mutable_element()->mutable_video()->mutable_transport()->set_playback_behavior(
            Transport::PLAYBACK_BEHAVIOR_STOP);
        QCOMPARE(encoded(cue), encoded(expected));
        // (The count that went with its going round is left, unused.)
        QCOMPARE(cue.actions(0).media().element().video().transport().times_to_loop(), 1u);
    }

    void aCueToGoToIsNamedAndTakenAwayAgain()
    {
        Cue cue = proPresentersOwn();
        QVERIFY(set(&cue, cue.mutable_actions(0), QStringLiteral("cue"), QStringLiteral("2A2A2A2A-1111-2222-3333-444444444444")));
        QCOMPARE(cue.completion_target_type(), Cue::COMPLETION_TARGET_TYPE_CUE);
        QCOMPARE(cue.completion_target_uuid().string(), std::string("2A2A2A2A-1111-2222-3333-444444444444"));
        QVERIFY(set(&cue, cue.mutable_actions(0), QStringLiteral("random"), {}));
        QCOMPARE(cue.completion_target_type(), Cue::COMPLETION_TARGET_TYPE_RANDOM);
        QVERIFY(!cue.has_completion_target_uuid());
        // Back to nothing: the cue is as ProPresenter's was but for the video's stop,
        // which is left as it was made (it is the user's to set going round again).
        QVERIFY(set(&cue, cue.mutable_actions(0), QString(), {}));
        Cue expected = proPresentersOwn();
        expected.mutable_actions(0)->mutable_media()->mutable_element()->mutable_video()->mutable_transport()->set_playback_behavior(
            Transport::PLAYBACK_BEHAVIOR_STOP);
        QCOMPARE(encoded(cue), encoded(expected));
    }

    void nothingIsChangedThatIsAsAskedOrNotAThing()
    {
        Cue cue = proPresentersOwn();
        const std::string was = encoded(cue);
        QVERIFY(set(&cue, cue.mutable_actions(0), QString(), {}));
        QCOMPARE(encoded(cue), was);
        QVERIFY(!set(&cue, cue.mutable_actions(0), QStringLiteral("sideways"), {}));
        QVERIFY(!set(&cue, cue.mutable_actions(0), QStringLiteral("cue"), {}));
        QCOMPARE(encoded(cue), was);
        // A picture has no going round to stop.
        Cue picture = proPresentersOwn();
        picture.mutable_actions(0)->mutable_media()->mutable_element()->mutable_image();
        const std::string pictureWas = encoded(picture);
        QVERIFY(set(&picture, picture.mutable_actions(0), QStringLiteral("next"), {}));
        QCOMPARE(picture.completion_target_type(), Cue::COMPLETION_TARGET_TYPE_NEXT);
        picture.clear_completion_target_type();
        QCOMPARE(encoded(picture), pictureWas);
    }
};

QTEST_GUILESS_MAIN(TestMediaCue)
#include "tst_mediacue.moc"
