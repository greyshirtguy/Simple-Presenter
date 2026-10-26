// Where sound goes (src/audiorouting.h): ProPresenter's Auto for a piece of media, as its
// files have it, and for the device; matrices cut to size and clicked; who is heard by
// Mute and Solo; a channel's gain by its decibels; the ramps; and what a player is told of
// a media's sound, nothing for the media ProPresenter writes as it is, and the rest from
// its fields.

#include "audiorouting.h"

#include <QTest>

using namespace audiorouting;

class TestAudioRouting : public QObject
{
    Q_OBJECT

private slots:
    void autoForMedia()
    {
        QCOMPARE(autoMedia(2, 2), (Routes{{0}, {1}}));
        QCOMPARE(autoMedia(2, 4), (Routes{{0}, {1}}));
        // As ProPresenter wrote a file of sixteen channels for a workspace of two
        const Routes sixteen = autoMedia(16, 2);
        QCOMPARE(sixteen.size(), 16);
        for (int channel = 0; channel < 16; ++channel)
            QCOMPARE(sixteen[channel], QList<int>{channel % 2});
        QCOMPARE(autoMedia(6, 4), (Routes{{0}, {1}, {2}, {3}, {0}, {1}}));
        // One channel: to the first two (a GUESS), and to the one where there is one
        QCOMPARE(autoMedia(1, 2), (Routes{{0, 1}}));
        QCOMPARE(autoMedia(1, 1), (Routes{{0}}));
        QVERIFY(autoMedia(0, 2).isEmpty());
        QCOMPARE(autoMedia(2, 0), (Routes{{}, {}}));
    }

    void autoForDevice()
    {
        QCOMPARE(autoDevice(2, 2), (Routes{{0}, {1}}));
        QCOMPARE(autoDevice(4, 2), (Routes{{0}, {1}, {}, {}}));
        QCOMPARE(autoDevice(2, 8), (Routes{{0}, {1}}));
    }

    void cutAndClicked()
    {
        QCOMPARE(fitted({{0, 5, 1, 1}, {3}}, 3, 4), (Routes{{0, 1}, {3}, {}}));
        Routes routes = toggled({{0}, {1}}, 0, 2, 2, 4);
        QCOMPARE(routes, (Routes{{0, 2}, {1}}));
        routes = toggled(routes, 0, 0, 2, 4);
        QCOMPARE(routes, (Routes{{2}, {1}}));
        // A cell outside the matrix changes nothing
        QCOMPARE(toggled(routes, 5, 0, 2, 4), routes);
        QVERIFY(same({{1, 0}, {}}, {{0, 1}}));
        QVERIFY(!same({{0}}, {{1}}));
    }

    void muteAndSolo()
    {
        QCOMPARE(heard({false, false, false}, {false, false, false}), (QList<bool>{true, true, true}));
        QCOMPARE(heard({false, true, false}, {false, false, false}), (QList<bool>{true, false, true}));
        QCOMPARE(heard({false, false, false}, {false, true, false}), (QList<bool>{false, true, false}));
        // Muted and soloed: muted
        QCOMPARE(heard({false, true, false}, {false, true, true}), (QList<bool>{false, false, true}));
    }

    void gains()
    {
        QCOMPARE(gainOf(0, false), 1.0f);
        QCOMPARE(gainOf(0, true), 0.0f);
        QVERIFY(qAbs(gainOf(-6.0206, false) - 0.5f) < 0.001f);
        QVERIFY(qAbs(gainOf(6.0206, false) - 2.0f) < 0.002f);
        QCOMPARE(gainOf(qQNaN(), false), 0.0f);
    }

    void ramps()
    {
        QCOMPARE(rampGain(5, 0, -1, 0), 1.0);
        // Coming in over its first two seconds
        QCOMPARE(rampGain(0, 2, -1, 0), 0.0);
        QVERIFY(rampGain(1, 2, -1, 0) > 0.1 && rampGain(1, 2, -1, 0) < 0.9);
        QCOMPARE(rampGain(2, 2, -1, 0), 1.0);
        QCOMPARE(rampGain(-1, 2, -1, 0), 0.0);
        // Going out over its last two
        QCOMPARE(rampGain(7, 0, 3, 2), 1.0);
        QVERIFY(rampGain(9, 0, 1, 2) > 0.1 && rampGain(9, 0, 1, 2) < 0.9);
        QCOMPARE(rampGain(10, 0, 0, 2), 0.0);
        // No end known: no ramp out
        QCOMPARE(rampGain(9, 0, -1, 2), 1.0);
        // Both at once, on a short sound
        QVERIFY(rampGain(0.5, 1, 0.5, 1) < rampGain(0.5, 1, -1, 0));
    }

    void soundOfProPresenters()
    {
        // As ProPresenter writes most media: its two channels to the first two, by Auto
        rv::data::Media media;
        auto *audio = media.mutable_video()->mutable_audio();
        audio->set_volume(1);
        audio->add_audio_channels()->add_outputs();
        auto *second = audio->add_audio_channels();
        second->set_index(1);
        second->add_outputs()->set_channel_index(1);
        QVERIFY(soundOf(media).isEmpty());
        // Its own mapping: the second to the third and fourth
        audio->set_is_custom_mapping(true);
        second->clear_outputs();
        second->add_outputs()->set_channel_index(2);
        second->add_outputs()->set_channel_index(3);
        QVariantMap sound = soundOf(media);
        QCOMPARE(sound.value("outputs").toList().size(), 2);
        QCOMPARE(sound.value("outputs").toList()[0].toList(), (QVariantList{0}));
        QCOMPARE(sound.value("outputs").toList()[1].toList(), (QVariantList{2, 3}));
        QVERIFY(!sound.contains("gains"));
        // A channel 6 dB down, and one muted
        audio->mutable_audio_channels(0)->set_volume(-6.0206);
        second->set_muted(true);
        sound = soundOf(media);
        QVERIFY(qAbs(sound.value("gains").toList()[0].toDouble() - 0.5) < 0.001);
        QCOMPARE(sound.value("gains").toList()[1].toDouble(), 0.0);
        // Ramps, only where they are switched on
        auto *transport = media.mutable_video()->mutable_transport();
        transport->set_fade_in_duration(2);
        QVERIFY(!soundOf(media).contains("rampIn"));
        transport->set_should_fade_in(true);
        transport->set_should_fade_out(true);
        transport->set_fade_out_duration(3.5);
        sound = soundOf(media);
        QCOMPARE(sound.value("rampIn").toDouble(), 2.0);
        QCOMPARE(sound.value("rampOut").toDouble(), 3.5);
        // A piece of audio the same; a picture nothing
        rv::data::Media piece;
        piece.mutable_audio()->mutable_transport()->set_should_fade_out(true);
        piece.mutable_audio()->mutable_transport()->set_fade_out_duration(1);
        QCOMPARE(soundOf(piece).value("rampOut").toDouble(), 1.0);
        rv::data::Media still;
        still.mutable_image();
        QVERIFY(soundOf(still).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAudioRouting)
#include "tst_audiorouting.moc"
