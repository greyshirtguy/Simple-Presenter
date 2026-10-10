// The looks of a workspace in ProPresenter's set-up file (src/lookfile.h), tried on
// folders made for the purpose.

#include "lookfile.h"

#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace lookfile;

namespace {

rv::data::ProPresenterWorkspace fileOf(const QString &workspace)
{
    rv::data::ProPresenterWorkspace document;
    QFile file(workspace + "/Configuration/Workspace");
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        document.ParseFromArray(bytes.constData(), int(bytes.size()));
    }
    return document;
}

// The file itself, byte for byte: to see that nothing was written
QByteArray bytesOf(const QString &workspace)
{
    QFile file(workspace + "/Configuration/Workspace");
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A file as ProPresenter might have left it: two looks for a room and a stream, the
// first of them live (the live look being a copy of it under an id of its own, as
// ProPresenter keeps it), with a theme for the stream named by a path on a Mac.
void writeProPresenters(const QString &workspace)
{
    rv::data::ProPresenterWorkspace document;
    const auto look = [&document](const char *id, const char *name) {
        rv::data::ProAudienceLook *made = document.add_audience_looks();
        made->mutable_uuid()->set_string(id);
        made->set_name(name);
        made->set_transition_duration(1);
        return made;
    };
    const auto line = [](rv::data::ProAudienceLook *look, const char *screen, bool slide, bool media, bool props) {
        rv::data::ProAudienceLook::ProScreenLook *made = look->add_screen_looks();
        made->mutable_pro_screen_uuid()->set_string(screen);
        made->set_presentation_foreground_enabled(slide);
        made->set_presentation_background_enabled(media);
        made->set_props_layer_enabled(props);
        made->set_announcements_enabled(true);
        made->set_messages_layer_enabled(true);
        return made;
    };
    rv::data::ProAudienceLook *lyrics = look("LOOK-1", "Lyrics L3rd");
    line(lyrics, "ROOM", true, true, true);
    rv::data::ProAudienceLook::ProScreenLook *stream = line(lyrics, "STREAM", true, false, true);
    stream->mutable_template_document_file_path()->set_absolute_string("file:///Users/someone/Desktop/ProPresenter%20MR/Themes/New%20Life%20Chapel/Theme");
    stream->mutable_template_slide_uuid()->set_string("THEME-SLIDE");
    rv::data::ProAudienceLook *clear = look("LOOK-2", "Stream Clear");
    line(clear, "ROOM", true, true, true);
    line(clear, "STREAM", false, false, true);
    *document.mutable_live_audience_look() = *lyrics;
    document.mutable_live_audience_look()->mutable_uuid()->set_string("LIVE-COPY");
    document.mutable_live_audience_look()->mutable_original_look_uuid()->set_string("LOOK-1");
    document.set_selected_library_name("Songs");
    QDir().mkpath(workspace + "/Configuration");
    QFile file(workspace + "/Configuration/Workspace");
    QVERIFY(file.open(QIODevice::WriteOnly));
    const std::string bytes = document.SerializeAsString();
    file.write(bytes.data(), qint64(bytes.size()));
}

}

class TestLookFile : public QObject
{
    Q_OBJECT

private slots:
    void aWorkspaceWithNoFileHasNoLooks()
    {
        QTemporaryDir folder;
        const Looks found = read(folder.path());
        QVERIFY(found.error.isEmpty());
        QVERIFY(found.looks.isEmpty());
        // And no live look: every screen gets everything
        QVERIFY(found.live.id.isEmpty() && found.live.screens.isEmpty());
        QCOMPARE(found.live.screens.value("ANY"), ScreenLook());
        QVERIFY(!QFile::exists(folder.path() + "/Configuration/Workspace"));
    }

    void proPresentersLooksAreReadWithWhatEachGivesEachScreen()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        const Looks found = read(folder.path());
        QCOMPARE(found.looks.size(), 2);
        QCOMPARE(found.looks.at(0).name, "Lyrics L3rd");
        QCOMPARE(found.looks.at(0).transition, 1.0);
        const ScreenLook room = found.looks.at(0).screens.value("ROOM");
        QVERIFY(room.slide && room.media && room.props && room.theme.isEmpty());
        const ScreenLook stream = found.looks.at(0).screens.value("STREAM");
        QVERIFY(stream.slide && !stream.media && stream.props);
        // The theme, by where it is under Themes, whatever computer the path was on
        QCOMPARE(stream.theme, "New Life Chapel");
        QCOMPARE(stream.themeSlide, "THEME-SLIDE");
        const ScreenLook cleared = found.looks.at(1).screens.value("STREAM");
        QVERIFY(!cleared.slide && !cleared.media && cleared.props);
        // A screen a look says nothing of gets everything
        const ScreenLook other = found.looks.at(1).screens.value("NOT-THERE");
        QVERIFY(other.slide && other.media && other.props);
        // The layers this app has not are read too, to be shown: here, no video input
        QVERIFY(room.messages && room.announcements && !room.videoInput && room.mask.isEmpty());
    }

    void theLiveLookIsALookOfItsOwnThatKnowsWhereItCameFrom()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        const Looks found = read(folder.path());
        // Not one of the saved looks: its own id, the saved look's name, and that look's id as its origin
        QCOMPARE(found.live.id, "LIVE-COPY");
        QCOMPARE(found.live.name, "Lyrics L3rd");
        QCOMPARE(found.live.origin, "LOOK-1");
        QVERIFY(std::none_of(found.looks.cbegin(), found.looks.cend(), [](const Look &look) { return look.id == "LIVE-COPY"; }));
        QCOMPARE(found.live.screens, found.looks.at(0).screens);
    }

    void makingASavedLookLiveCopiesItIntoTheLiveLook()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QCOMPARE(makeLive(folder.path(), "LOOK-2"), QString());
        const Looks found = read(folder.path());
        QCOMPARE(found.live.id, "LIVE-COPY");
        QCOMPARE(found.live.name, "Stream Clear");
        QCOMPARE(found.live.origin, "LOOK-2");
        QCOMPARE(found.live.screens, found.looks.at(1).screens);
        // The saved looks are as they were, and so is the rest of the file
        QCOMPARE(found.looks.size(), 2);
        QCOMPARE(found.looks.at(0).screens.value("STREAM").theme, "New Life Chapel");
        QCOMPARE(fileOf(folder.path()).selected_library_name(), "Songs");
        QVERIFY(!makeLive(folder.path(), "no such").isEmpty());
        QCOMPARE(read(folder.path()).live.origin, "LOOK-2");
    }

    void theLiveLookIsChangedByItselfAndASavedLookByItself()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        ScreenLook wanted = read(folder.path()).live.screens.value("ROOM");
        wanted.media = false;
        wanted.theme = "Samples/Black Box";
        wanted.themeSlide = "TWO-LINES";
        QCOMPARE(setLiveScreen(folder.path(), "ROOM", wanted), QString());
        Looks found = read(folder.path());
        QCOMPARE(found.live.screens.value("ROOM"), wanted);
        // The look it came from knows nothing of it, and it still says where it came from
        QVERIFY(found.looks.at(0).screens.value("ROOM").media && found.looks.at(0).screens.value("ROOM").theme.isEmpty());
        QCOMPARE(found.live.origin, "LOOK-1");
        QCOMPARE(found.live.id, "LIVE-COPY");
        // And the other way about: the saved look changed, the screens not
        ScreenLook saved = found.looks.at(0).screens.value("STREAM");
        saved.slide = false;
        QCOMPARE(setScreen(folder.path(), "LOOK-1", "STREAM", saved), QString());
        found = read(folder.path());
        QVERIFY(!found.looks.at(0).screens.value("STREAM").slide && found.live.screens.value("STREAM").slide);
        // Made live again, the saved look is what the screens get, the change to the live look gone
        QCOMPARE(makeLive(folder.path(), "LOOK-1"), QString());
        found = read(folder.path());
        QCOMPARE(found.live.screens, found.looks.at(0).screens);
        QVERIFY(found.live.screens.value("ROOM").media && !found.live.screens.value("STREAM").slide);
    }

    void theLiveLookIsSavedAsTheLookItWasMadeFrom()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        // Something of a line that this app knows nothing of, in the live look only
        rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        document.mutable_live_audience_look()->mutable_screen_looks(0)->mutable_mask_uuid()->set_string("A-MASK");
        {
            QFile file(folder.path() + "/Configuration/Workspace");
            QVERIFY(file.open(QIODevice::WriteOnly));
            const std::string bytes = document.SerializeAsString();
            file.write(bytes.data(), qint64(bytes.size()));
        }
        ScreenLook wanted = read(folder.path()).live.screens.value("ROOM");
        wanted.slide = false;
        QCOMPARE(setLiveScreen(folder.path(), "ROOM", wanted), QString());
        QVERIFY(read(folder.path()).looks.at(0).screens.value("ROOM").slide);

        QCOMPARE(saveLive(folder.path()), QString());
        const Looks found = read(folder.path());
        // The saved look now gives each screen what the live look does, the whole of each line
        QCOMPARE(found.looks.at(0).screens, found.live.screens);
        QVERIFY(!found.looks.at(0).screens.value("ROOM").slide);
        QCOMPARE(fileOf(folder.path()).audience_looks(0).screen_looks(0).mask_uuid().string(), "A-MASK");
        // It is still itself: its own id, name and transition, and its place in the list
        QCOMPARE(found.looks.at(0).id, "LOOK-1");
        QCOMPARE(found.looks.at(0).name, "Lyrics L3rd");
        QCOMPARE(found.looks.at(0).transition, 1.0);
        // The live look is as it was, and still says where it came from; the other look and the rest of the file too
        QCOMPARE(found.live.id, "LIVE-COPY");
        QCOMPARE(found.live.origin, "LOOK-1");
        QVERIFY(found.looks.at(1).screens.value("ROOM").slide);
        QCOMPARE(fileOf(folder.path()).selected_library_name(), "Songs");

        // A live look whose saved look has gone has nowhere to be saved to, and nothing is written
        QCOMPARE(remove(folder.path(), "LOOK-1"), QString());
        const QByteArray before = bytesOf(folder.path());
        QVERIFY(!before.isEmpty());
        QVERIFY(!saveLive(folder.path()).isEmpty());
        QCOMPARE(bytesOf(folder.path()), before);
    }

    void aWorkspaceWithNoLiveLookIsGivenOneWhenItIsNeeded()
    {
        // No file at all: a layer switched off for a screen makes the file, with a live look in it
        QTemporaryDir folder;
        ScreenLook wanted;
        wanted.props = false;
        QCOMPARE(setLiveScreen(folder.path(), "A", wanted), QString());
        Looks found = read(folder.path());
        QVERIFY(!found.live.id.isEmpty() && found.live.id != liveLook() && found.live.origin.isEmpty() && found.looks.isEmpty());
        QCOMPARE(found.live.screens.value("A"), wanted);
        // A saved look made live where there was no live look
        QTemporaryDir other;
        QString id;
        QCOMPARE(add(other.path(), "Everything", {"A"}, QString(), &id), QString());
        QVERIFY(read(other.path()).live.id.isEmpty());
        QCOMPARE(makeLive(other.path(), id), QString());
        found = read(other.path());
        QVERIFY(!found.live.id.isEmpty() && found.live.id != id);
        QCOMPARE(found.live.origin, id);
        QCOMPARE(found.live.name, "Everything");
    }

    void aNewLookCanBeACopyOfAnotherOrOfTheLiveLook()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QString copy;
        QCOMPARE(add(folder.path(), "Clear Too", {"IGNORED"}, "LOOK-2", &copy), QString());
        Looks found = read(folder.path());
        QCOMPARE(found.looks.size(), 3);
        QCOMPARE(found.looks.at(2).id, copy);
        QCOMPARE(found.looks.at(2).name, "Clear Too");
        QCOMPARE(found.looks.at(2).screens, found.looks.at(1).screens);
        // The live look, as it has been changed, kept as a saved look of its own
        ScreenLook wanted = found.live.screens.value("ROOM");
        wanted.props = false;
        QCOMPARE(setLiveScreen(folder.path(), "ROOM", wanted), QString());
        QString kept;
        QCOMPARE(add(folder.path(), "As It Is Now", {}, liveLook(), &kept), QString());
        found = read(folder.path());
        QCOMPARE(found.looks.at(3).screens, found.live.screens);
        QVERIFY(!found.looks.at(3).screens.value("ROOM").props);
        // A saved look names no other look as where it came from
        QVERIFY(!fileOf(folder.path()).audience_looks(3).has_original_look_uuid());
        QVERIFY(!add(folder.path(), "Of Nothing", {}, "no such", nullptr).isEmpty());
    }

    void aThemeIsKnownByItsPlaceUnderThemes()
    {
        QCOMPARE(themePlace("file:///Users/x/Documents/ProPresenter/Themes/Samples/Black Box/Theme"), "Samples/Black Box");
        QCOMPARE(themePlace("/home/someone/WorkSpaces/Demo/Themes/Plain/Theme"), "Plain");
        QCOMPARE(themePlace("C:/Users/x/Documents/ProPresenter/Themes/Plain/"), "Plain");
        QCOMPARE(themePlace("/somewhere/else/Theme"), "");
        QCOMPARE(themePlace(""), "");
    }

    void aNewLookGivesEveryScreenEverything()
    {
        QTemporaryDir folder;
        QString id;
        QCOMPARE(add(folder.path(), "Everything", {"A", "B"}, QString(), &id), QString());
        const Looks found = read(folder.path());
        QCOMPARE(found.looks.size(), 1);
        QCOMPARE(found.looks.at(0).id, id);
        QCOMPARE(found.looks.at(0).screens.size(), 2);
        QCOMPARE(found.looks.at(0).screens.value("B"), ScreenLook());
        // With the layers this app has not on as well, as ProPresenter's own new look has them
        const rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        const rv::data::ProAudienceLook::ProScreenLook &line = document.audience_looks(0).screen_looks(0);
        QVERIFY(line.announcements_enabled() && line.messages_layer_enabled() && line.live_video_enabled());
        QVERIFY(!add(folder.path(), "  ", {"A"}, QString(), nullptr).isEmpty());
    }

    void whatALookGivesAScreenIsChangedAndTheRestLeft()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        ScreenLook wanted;
        wanted.media = false;
        wanted.props = false;
        QCOMPARE(setScreen(folder.path(), "LOOK-2", "ROOM", wanted), QString());
        const Looks found = read(folder.path());
        const ScreenLook now = found.looks.at(1).screens.value("ROOM");
        QVERIFY(now.slide && !now.media && !now.props && now.theme.isEmpty());
        // The other look, the other screen, the live look and the rest of the file are as they were
        QVERIFY(found.looks.at(0).screens.value("ROOM").media);
        QVERIFY(!found.looks.at(1).screens.value("STREAM").slide);
        const rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        QCOMPARE(document.live_audience_look().uuid().string(), "LIVE-COPY");
        QCOMPARE(document.selected_library_name(), "Songs");
        // And so are the switches for the layers this app has not
        QVERIFY(document.audience_looks(1).screen_looks(0).announcements_enabled());
        QVERIFY(!document.audience_looks(1).screen_looks(0).live_video_enabled());
    }

    void aThemeThatIsNotChangedIsLeftNamedAsItWas()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        ScreenLook wanted = read(folder.path()).looks.at(0).screens.value("STREAM");
        wanted.props = false;
        QCOMPARE(setScreen(folder.path(), "LOOK-1", "STREAM", wanted), QString());
        const rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        const rv::data::ProAudienceLook::ProScreenLook &line = document.audience_looks(0).screen_looks(1);
        QCOMPARE(line.template_document_file_path().absolute_string(), "file:///Users/someone/Desktop/ProPresenter%20MR/Themes/New%20Life%20Chapel/Theme");
        QVERIFY(!line.props_layer_enabled());
    }

    void aScreenIsGivenAThemeAndHasItTakenAway()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        ScreenLook wanted;
        wanted.theme = "Samples/Black Box";
        wanted.themeSlide = "TWO-LINES";
        QCOMPARE(setScreen(folder.path(), "LOOK-2", "ROOM", wanted), QString());
        const ScreenLook now = read(folder.path()).looks.at(1).screens.value("ROOM");
        QVERIFY(now.theme == "Samples/Black Box" && now.themeSlide == "TWO-LINES" && now.slide && now.media && now.props);
        const QString written = QString::fromStdString(fileOf(folder.path()).audience_looks(1).screen_looks(0).template_document_file_path().absolute_string());
        QVERIFY2(written.startsWith("file:///") && written.endsWith("/Themes/Samples/Black%20Box/Theme"), qPrintable(written));
        QCOMPARE(setScreen(folder.path(), "LOOK-2", "ROOM", ScreenLook()), QString());
        QVERIFY(read(folder.path()).looks.at(1).screens.value("ROOM").theme.isEmpty());
        QVERIFY(!fileOf(folder.path()).audience_looks(1).screen_looks(0).has_template_document_file_path());
    }

    void aLookIsGivenALineForAScreenItHadNone()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        ScreenLook wanted;
        wanted.slide = false;
        QCOMPARE(setScreen(folder.path(), "LOOK-1", "LOBBY", wanted), QString());
        const Looks found = read(folder.path());
        QCOMPARE(found.looks.at(0).screens.size(), 3);
        QCOMPARE(found.looks.at(0).screens.value("LOBBY"), wanted);
    }

    void looksAreRenamedAndRemoved()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QCOMPARE(rename(folder.path(), "LOOK-2", " Stream Off "), QString());
        QCOMPARE(read(folder.path()).looks.at(1).name, "Stream Off");
        QVERIFY(!rename(folder.path(), "LOOK-2", "").isEmpty());
        QVERIFY(!rename(folder.path(), "no such", "Name").isEmpty());
        QCOMPARE(remove(folder.path(), "LOOK-1"), QString());
        const Looks found = read(folder.path());
        QCOMPARE(found.looks.size(), 1);
        QCOMPARE(found.looks.at(0).id, "LOOK-2");
        // The look the live look came from is gone; the live look is still what it was
        QCOMPARE(found.live.origin, "LOOK-1");
        QCOMPARE(found.live.name, "Lyrics L3rd");
        QVERIFY(found.live.screens.value("STREAM").slide);
        QVERIFY(!remove(folder.path(), "LOOK-1").isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestLookFile)
#include "tst_lookfile.moc"
