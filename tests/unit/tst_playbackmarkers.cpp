// ProPresenter's Playback Markers (src/playbackmarkers.h): on a media action made as
// ProPresenter writes one, markers added, changed, duplicated and taken away, and their
// actions, with a look each time at what else of the action is still as it was.

#include "playbackmarkers.h"

#include <google/protobuf/unknown_field_set.h>

#include <QSet>
#include <QTest>

using namespace playbackmarkers;
using rv::data::Action;

namespace {

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

// A slide's media action as ProPresenter 21.4 writes one for a video, the names and the
// ids made up; and one field that the descriptions of ProPresenter's files have no name
// for, as a later version's might be.
Action proPresentersOwn()
{
    Action action;
    action.mutable_uuid()->set_string("3A3A3A3A-1111-2222-3333-444444444444");
    action.set_isenabled(true);
    action.set_duration(30);
    action.set_type(Action::ACTION_TYPE_MEDIA);
    action.mutable_media()->mutable_audio();
    action.mutable_media()->set_layer_type(Action::LAYER_TYPE_FOREGROUND);
    rv::data::Media *media = action.mutable_media()->mutable_element();
    media->mutable_uuid()->set_string("3B3B3B3B-1111-2222-3333-444444444444");
    media->mutable_url()->set_absolute_string("file:///Users/someone/Documents/ProPresenter/Media/Assets/A%20Made%20Up%20Video.mp4");
    media->mutable_url()->set_platform(rv::data::URL::PLATFORM_MACOS);
    media->mutable_metadata()->set_format("avc1");
    rv::data::Media::VideoTypeProperties *video = media->mutable_video();
    video->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
    video->mutable_audio()->set_volume(1);
    video->mutable_transport()->set_play_rate(1);
    video->mutable_transport()->set_out_point(30);
    video->mutable_video()->set_frame_rate(30);
    action.GetReflection()->MutableUnknownFields(&action)->AddVarint(900, 7);
    return action;
}

// Ids made in turn: MARKER-1, MARKER-2 and so on
std::function<std::string()> ids()
{
    auto next = std::make_shared<int>(0);
    return [next] { return "MARKER-" + std::to_string(++*next); };
}

QStringList timesOf(const Action &action)
{
    QStringList times;
    for (const auto &marker : action.media().markers())
        times << QString::number(marker.time());
    return times;
}

Action anAction(const char *id)
{
    Action made;
    made.mutable_uuid()->set_string(id);
    made.set_isenabled(true);
    made.set_type(Action::ACTION_TYPE_MACRO);
    made.mutable_macro()->mutable_identification()->set_parameter_name("Lights");
    return made;
}

} // namespace

class TestPlaybackMarkers : public QObject
{
    Q_OBJECT

private slots:
    void aVideoOrAudioTakesThemAndAPictureDoesNot()
    {
        QVERIFY(takes(proPresentersOwn()));
        Action audio = proPresentersOwn();
        audio.mutable_media()->mutable_element()->mutable_audio();
        QVERIFY(takes(audio));
        Action picture = proPresentersOwn();
        picture.mutable_media()->mutable_element()->mutable_image();
        QVERIFY(!takes(picture));
        QVERIFY(!add(&picture, 1, ids()).isEmpty());
        QVERIFY(!picture.media().markers_size());
    }

    void aMarkerIsAddedInTimeOrderWithTheRestLeftAlone()
    {
        Action action = proPresentersOwn();
        const auto newId = ids();
        QString made;
        QVERIFY(add(&action, 12.5, newId, &made).isEmpty());
        QCOMPARE(made, QStringLiteral("MARKER-1"));
        QVERIFY(add(&action, 3, newId).isEmpty());
        QVERIFY(add(&action, -2, newId).isEmpty());
        QCOMPARE(timesOf(action), QStringList({"0", "3", "12.5"}));
        const QVariantList shown = describe(action, nullptr);
        QCOMPARE(shown.size(), 3);
        QCOMPARE(shown.at(2).toMap().value("id").toString(), QStringLiteral("MARKER-1"));
        // (The first given the palette's first colour, its media having none yet)
        QCOMPARE(shown.at(2).toMap().value("color").toString(), QStringLiteral("#e53935"));
        QVERIFY(shown.at(2).toMap().value("name").toString().isEmpty());
        // Taken away again, the action is as ProPresenter wrote it, byte for byte.
        for (const QString &id : {"MARKER-1", "MARKER-2", "MARKER-3"})
            QVERIFY(remove(&action, id).isEmpty());
        action.mutable_media()->clear_markers();
        QCOMPARE(encoded(action), encoded(proPresentersOwn()));
    }

    void aNewMarkerHasAColourItsMediaHasNot()
    {
        Action action = proPresentersOwn();
        const auto newId = ids();
        // With none there, `roll` picks among the seven (white is not given).
        QCOMPARE(newColor(action, 0).name(), QStringLiteral("#e53935"));
        QCOMPARE(newColor(action, 4).name(), QStringLiteral("#3d8be0"));
        QCOMPARE(newColor(action, 7).name(), QStringLiteral("#e53935"));
        // Red and orange taken, it is among the other five.
        QVERIFY(add(&action, 1, newId, nullptr, 0).isEmpty());
        QVERIFY(add(&action, 2, newId, nullptr, 0).isEmpty());
        QCOMPARE(describe(action, nullptr).at(1).toMap().value("color").toString(), QStringLiteral("#fb8c00"));
        QCOMPARE(newColor(action, 0).name(), QStringLiteral("#fdd835"));
        QCOMPARE(newColor(action, 5).name(), QStringLiteral("#fdd835"));
        QCOMPARE(newColor(action, 4).name(), QStringLiteral("#d81b60"));
        // Every one taken, among all seven again.
        for (int i = 0; i < 5; ++i)
            QVERIFY(add(&action, 3 + i, newId, nullptr, 0).isEmpty());
        QSet<QString> seen;
        for (const QVariant &marker : describe(action, nullptr))
            seen.insert(marker.toMap().value("color").toString());
        QCOMPARE(seen.size(), 7);
        QVERIFY(!seen.contains("#ffffff"));
        QCOMPARE(newColor(action, 2).name(), QStringLiteral("#fdd835"));
        // The palette has white last, for picking.
        QCOMPARE(palette().size(), 8);
        QCOMPARE(palette().last().toMap().value("name").toString(), QStringLiteral("White"));
    }

    void aMarkerIsRenamedRecolouredAndMoved()
    {
        Action action = proPresentersOwn();
        const auto newId = ids();
        QVERIFY(add(&action, 5, newId).isEmpty());
        QVERIFY(add(&action, 10, newId).isEmpty());
        QVERIFY(change(&action, "MARKER-1", {{"name", "Enter Stage Now"}, {"color", "#ff0000"}}).isEmpty());
        QVERIFY(change(&action, "MARKER-1", {{"time", 20.0}}).isEmpty());
        const QVariantList shown = describe(action, nullptr);
        QCOMPARE(shown.at(1).toMap().value("name").toString(), QStringLiteral("Enter Stage Now"));
        QCOMPARE(shown.at(1).toMap().value("color").toString(), QStringLiteral("#ff0000"));
        QCOMPARE(timesOf(action), QStringList({"10", "20"}));
        QVERIFY(!change(&action, "MARKER-9", {{"name", "x"}}).isEmpty());
        QVERIFY(!change(&action, "MARKER-1", {{"color", "not a colour"}}).isEmpty());
    }

    void aMarkersActionsAreAddedChangedAndTakenOff()
    {
        Action action = proPresentersOwn();
        QVERIFY(add(&action, 5, ids()).isEmpty());
        QVERIFY(addAction(&action, "MARKER-1", anAction("ACTION-1")).isEmpty());
        QVERIFY(addAction(&action, "MARKER-1", anAction("ACTION-2")).isEmpty());
        QCOMPARE(action.media().markers(0).actions_size(), 2);
        int described = 0;
        const QVariantList shown = describe(action, [&described](const Action &each) {
            ++described;
            return QVariantMap {{"id", QString::fromStdString(each.uuid().string())}};
        });
        QCOMPARE(described, 2);
        QCOMPARE(shown.at(0).toMap().value("actions").toList().at(1).toMap().value("id").toString(), QStringLiteral("ACTION-2"));
        QVERIFY(changeAction(&action, "MARKER-1", "ACTION-1", [](Action *each) {
            each->mutable_macro()->mutable_identification()->set_parameter_name("House");
            return QString();
        }).isEmpty());
        QCOMPARE(action.media().markers(0).actions(0).macro().identification().parameter_name(), std::string("House"));
        QVERIFY(removeAction(&action, "MARKER-1", "ACTION-1").isEmpty());
        QCOMPARE(action.media().markers(0).actions_size(), 1);
        QVERIFY(!removeAction(&action, "MARKER-1", "ACTION-1").isEmpty());
        QVERIFY(!addAction(&action, "MARKER-9", anAction("ACTION-3")).isEmpty());
    }

    void aLinkFindsTheMarkerItMeans()
    {
        const QVariantList markers {QVariantMap {{"time", 5.0}, {"name", "Intro"}}, QVariantMap {{"time", 20.0}, {"name", "Enter Stage Now"}},
                                    QVariantMap {{"time", 40.0}, {"name", "End"}}};
        QCOMPARE(pick(markers, 12, 0, {}), 0);
        QCOMPARE(pick(markers, 12, 1, {}), 0);
        QCOMPARE(pick(markers, 12, 2, {}), 1);
        QCOMPARE(pick(markers, 12, 3, {}), 2);
        QCOMPARE(pick(markers, 12, 4, "End"), 2);
        // Reached exactly: previous now, and the next is the one after
        QCOMPARE(pick(markers, 20, 1, {}), 1);
        QCOMPARE(pick(markers, 20, 2, {}), 2);
        // Before the first, there is no previous; after the last, no next
        QCOMPARE(pick(markers, 1, 1, {}), -1);
        QCOMPARE(pick(markers, 45, 2, {}), -1);
        QCOMPARE(pick(markers, 12, 4, "Nowhere"), -1);
        QCOMPARE(pick({}, 12, 0, {}), -1);
        QCOMPARE(pick({}, 12, 3, {}), -1);
    }

    void aDuplicateHasIdsOfItsOwn()
    {
        Action action = proPresentersOwn();
        const auto newId = ids();
        QVERIFY(add(&action, 5, newId).isEmpty());
        QVERIFY(change(&action, "MARKER-1", {{"name", "Lights"}}).isEmpty());
        QVERIFY(addAction(&action, "MARKER-1", anAction("ACTION-1")).isEmpty());
        QString made;
        QVERIFY(duplicate(&action, "MARKER-1", newId, &made).isEmpty());
        QCOMPARE(made, QStringLiteral("MARKER-2"));
        QCOMPARE(action.media().markers_size(), 2);
        const auto &copy = action.media().markers(1);
        QCOMPARE(copy.name(), std::string("Lights"));
        QCOMPARE(copy.time(), 5.0);
        QCOMPARE(copy.actions(0).uuid().string(), std::string("MARKER-3"));
        QCOMPARE(action.media().markers(0).actions(0).uuid().string(), std::string("ACTION-1"));
    }
};

QTEST_GUILESS_MAIN(TestPlaybackMarkers)
#include "tst_playbackmarkers.moc"
