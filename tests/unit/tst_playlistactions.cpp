#include "playlistactions.h"

#include <QTest>

// The actions that play a playlist of the media bin or of the audio bin, as
// ProPresenter's file has them (see playlistactions.h): read as ProPresenter writes
// them, made new, and changed with what else they hold kept.
namespace {

using Action = rv::data::Action;

// An action as ProPresenter wrote one in a presentation of the maintainer's: the audio
// bin's playlist by id and name, and nothing else of it.
Action proAudioAction()
{
    Action action;
    action.mutable_uuid()->set_string("D2C17F20-7033-4538-8F49-80F4994478A5");
    action.set_isenabled(true);
    action.set_type(Action::ACTION_TYPE_AUDIO_BIN_PLAYLIST);
    action.mutable_playlist_item()->mutable_playlist_uuid()->set_string("9E98B135-A1FE-4A67-9450-8F86707697DC");
    action.mutable_playlist_item()->set_playlist_name("Royalty Free");
    return action;
}

// One that has all the file allows: an item, Select Playlist, Always Retrigger, and
// things of the action itself (a label, a delay), and a field this app knows nothing of.
Action fullMediaAction()
{
    Action action;
    action.mutable_uuid()->set_string("A1");
    action.set_isenabled(true);
    action.set_type(Action::ACTION_TYPE_MEDIA_BIN_PLAYLIST);
    action.set_delay_time(2.5);
    action.mutable_label()->set_text("Walk-in");
    Action::PlaylistItemType *playlist = action.mutable_playlist_item();
    playlist->mutable_playlist_uuid()->set_string("P1");
    playlist->set_playlist_name("Loops");
    playlist->mutable_item_uuid()->set_string("I1");
    playlist->set_item_name("Waves");
    playlist->set_select_playlist(true);
    playlist->set_always_retrigger(true);
    // Field 900 of the action, as a newer ProPresenter might have one
    std::string bytes = action.SerializeAsString();
    bytes += std::string("\xa0\x38\x07", 3);
    Action read;
    read.ParseFromString(bytes);
    return read;
}

} // namespace

class TestPlaylistActions : public QObject
{
    Q_OBJECT

private slots:
    void readsTheAudioBinsAsProPresenterWritesIt()
    {
        const Action action = proAudioAction();
        QVERIFY(playlistactions::isPlaylist(action));
        const QVariantMap map = playlistactions::describe(action);
        QCOMPARE(map.value("kind").toString(), "audioPlaylist");
        QCOMPARE(map.value("playlistId").toString(), "9E98B135-A1FE-4A67-9450-8F86707697DC");
        QCOMPARE(map.value("playlistName").toString(), "Royalty Free");
        QCOMPARE(map.value("itemId").toString(), QString());
        QVERIFY(!map.value("selects").toBool());
        QVERIFY(!map.value("retrigger").toBool());
        QCOMPARE(map.value("title").toString(), "Play the audio playlist “Royalty Free”");
    }

    void readsAllAMediaBinsCanHave()
    {
        const QVariantMap map = playlistactions::describe(fullMediaAction());
        QCOMPARE(map.value("kind").toString(), "mediaPlaylist");
        QCOMPARE(map.value("itemId").toString(), "I1");
        QCOMPARE(map.value("itemName").toString(), "Waves");
        QVERIFY(map.value("selects").toBool());
        QVERIFY(map.value("retrigger").toBool());
        QCOMPARE(map.value("title").toString(), "Play the media playlist “Loops”, again if it is playing");
    }

    void anotherKindWithAPlaylistInItIsNotOne()
    {
        // (ProPresenter's playlist of presentations has an action of its own, which uses
        // the same message: it is not this app's to play.)
        Action action = proAudioAction();
        action.set_type(Action::ACTION_TYPE_LIBRARY_PLAYLIST);
        QVERIFY(!playlistactions::isPlaylist(action));
        QVERIFY(playlistactions::describe(action).isEmpty());
    }

    void aNewOneIsThePlaylistAlone()
    {
        Action action;
        QCOMPARE(playlistactions::build({{"kind", "mediaPlaylist"}, {"playlistId", "P2"}, {"playlistName", "Backgrounds"}}, &action), QString());
        QCOMPARE(action.type(), Action::ACTION_TYPE_MEDIA_BIN_PLAYLIST);
        QVERIFY(action.isenabled());
        QCOMPARE(QString::fromStdString(action.uuid().string()).size(), 36);
        QCOMPARE(action.playlist_item().playlist_uuid().string(), "P2");
        QCOMPARE(action.playlist_item().playlist_name(), "Backgrounds");
        QVERIFY(!action.playlist_item().has_item_uuid());
        QVERIFY(!action.playlist_item().select_playlist());
        QVERIFY(!action.playlist_item().always_retrigger());
        // Made as ProPresenter makes its own of the audio bin, but for the type
        Action audio;
        QCOMPARE(playlistactions::build({{"kind", "audioPlaylist"}, {"playlistId", "9E98B135-A1FE-4A67-9450-8F86707697DC"}, {"playlistName", "Royalty Free"}}, &audio),
                 QString());
        Action pro = proAudioAction();
        pro.mutable_uuid()->set_string(audio.uuid().string());
        QCOMPARE(audio.SerializeAsString(), pro.SerializeAsString());
    }

    void alwaysRetriggerIsChangedAndAllElseKept()
    {
        Action action = fullMediaAction();
        QCOMPARE(playlistactions::build({{"kind", "mediaPlaylist"}, {"playlistId", "P1"}, {"playlistName", "Loops"}, {"retrigger", false}}, &action), QString());
        QVERIFY(!action.playlist_item().always_retrigger());
        QCOMPARE(action.playlist_item().item_uuid().string(), "I1");
        QCOMPARE(action.playlist_item().item_name(), "Waves");
        QVERIFY(action.playlist_item().select_playlist());
        QCOMPARE(action.delay_time(), 2.5);
        QCOMPARE(action.label().text(), "Walk-in");
        QCOMPARE(action.uuid().string(), "A1");
        // The field this app does not know is still there, as it came
        QVERIFY(action.SerializeAsString().find(std::string("\xa0\x38\x07", 3)) != std::string::npos);
        // And back on
        QCOMPARE(playlistactions::build({{"kind", "mediaPlaylist"}, {"playlistId", "P1"}, {"playlistName", "Loops"}, {"retrigger", true}}, &action), QString());
        QVERIFY(action.playlist_item().always_retrigger());
        // Nothing said of it, nothing changed of it
        QCOMPARE(playlistactions::build({{"kind", "mediaPlaylist"}, {"playlistId", "P1"}, {"playlistName", "Loops"}}, &action), QString());
        QVERIFY(action.playlist_item().always_retrigger());
    }

    void anotherPlaylistLetsTheItemGo()
    {
        Action action = fullMediaAction();
        QCOMPARE(playlistactions::build({{"kind", "mediaPlaylist"}, {"playlistId", "P9"}, {"playlistName", "Other"}}, &action), QString());
        QCOMPARE(action.playlist_item().playlist_uuid().string(), "P9");
        QVERIFY(!action.playlist_item().has_item_uuid());
        QVERIFY(action.playlist_item().item_name().empty());
        // (What else it says, it says still.)
        QVERIFY(action.playlist_item().select_playlist());
        QVERIFY(action.playlist_item().always_retrigger());
    }

    void oneWithNoPlaylistIsNotMade()
    {
        Action action;
        QVERIFY(!playlistactions::build({{"kind", "audioPlaylist"}}, &action).isEmpty());
        QVERIFY(!playlistactions::build({{"kind", "macro"}, {"playlistId", "P1"}}, &action).isEmpty());
        QCOMPARE(action.type(), Action::ACTION_TYPE_UNKNOWN);
    }
};

QTEST_GUILESS_MAIN(TestPlaylistActions)
#include "tst_playlistactions.moc"
