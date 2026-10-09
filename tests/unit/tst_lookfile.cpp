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

// A file as ProPresenter might have left it: two looks for a room and a stream, the
// first of them live, with a theme for the stream named by a path on a Mac.
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
        QVERIFY(found.live.isEmpty());
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
        QCOMPARE(found.live, "LOOK-1");
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
        QCOMPARE(add(folder.path(), "Everything", {"A", "B"}, &id), QString());
        const Looks found = read(folder.path());
        QCOMPARE(found.looks.size(), 1);
        QCOMPARE(found.looks.at(0).id, id);
        QCOMPARE(found.looks.at(0).screens.size(), 2);
        QCOMPARE(found.looks.at(0).screens.value("B"), ScreenLook());
        // With the layers this app has not on as well, as ProPresenter's own new look has them
        const rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        const rv::data::ProAudienceLook::ProScreenLook &line = document.audience_looks(0).screen_looks(0);
        QVERIFY(line.announcements_enabled() && line.messages_layer_enabled() && line.live_video_enabled());
        QVERIFY(!add(folder.path(), "  ", {"A"}, nullptr).isEmpty());
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
        QCOMPARE(found.looks.at(1).screens.value("ROOM"), wanted);
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
        QCOMPARE(read(folder.path()).looks.at(1).screens.value("ROOM"), wanted);
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
        // The one that was live is gone: none is, to go by
        QVERIFY(found.live.isEmpty());
        QVERIFY(!remove(folder.path(), "LOOK-1").isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestLookFile)
#include "tst_lookfile.moc"
