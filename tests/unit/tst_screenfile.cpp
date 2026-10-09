// The list of a workspace's screens in ProPresenter's set-up file (src/screenfile.h),
// tried on folders made for the purpose: what is read from a workspace with no file,
// what a change writes, and that the rest of a file ProPresenter wrote goes back as it
// came.

#include "screenfile.h"

#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace screenfile;

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

// A file as ProPresenter might have left it: two audience screens and a stage screen
// connected to things of its own, a stage layout for the stage screen, a look that
// names the screens, and settings this app knows nothing of.
void writeProPresenters(const QString &workspace)
{
    rv::data::ProPresenterWorkspace document;
    const auto screen = [&document](const char *id, const char *name, bool stage) {
        rv::data::ProPresenterScreen *made = document.add_pro_screens();
        made->set_name(name);
        made->set_screen_type(stage ? rv::data::ProPresenterScreen::SCREEN_TYPE_STAGE : rv::data::ProPresenterScreen::SCREEN_TYPE_AUDIENCE);
        made->mutable_uuid()->set_string(id);
        rv::data::Screen *part = made->mutable_arrangement_single()->add_screens();
        part->mutable_bounds()->mutable_size()->set_width(stage ? 960 : 1920);
        part->mutable_bounds()->mutable_size()->set_height(stage ? 1740 : 1080);
        part->mutable_output_display()->set_type(rv::data::OutputDisplay::TYPE_CARD);
        part->mutable_output_display()->set_model("DeckLink Duo 2");
    };
    screen("AAAA", "Main Hall", false);
    screen("BBBB", "Stream", false);
    screen("CCCC", "Rear", true);
    auto *mapping = document.add_stage_layout_mappings();
    mapping->mutable_screen()->mutable_parameter_uuid()->set_string("CCCC");
    mapping->mutable_layout()->mutable_parameter_uuid()->set_string("LAYOUT");
    auto *look = document.add_audience_looks();
    look->set_name("Lower Third");
    look->add_screen_looks()->mutable_pro_screen_uuid()->set_string("AAAA");
    look->add_screen_looks()->mutable_pro_screen_uuid()->set_string("BBBB");
    document.set_selected_library_name("Songs");
    document.set_audio_channel_count(8);
    QDir().mkpath(workspace + "/Configuration");
    QFile file(workspace + "/Configuration/Workspace");
    QVERIFY(file.open(QIODevice::WriteOnly));
    const std::string bytes = document.SerializeAsString();
    file.write(bytes.data(), qint64(bytes.size()));
}

}

class TestScreenFile : public QObject
{
    Q_OBJECT

private slots:
    void aWorkspaceWithNoFileHasAnAudienceScreenAndAStageScreen()
    {
        QTemporaryDir folder;
        const Screens found = read(folder.path());
        QVERIFY(found.error.isEmpty());
        QVERIFY(!found.fromFile);
        QCOMPARE(found.screens, defaults());
        QCOMPARE(found.screens.size(), 2);
        QVERIFY(!found.screens.at(0).stage);
        QVERIFY(found.screens.at(1).stage);
        // Reading makes no file.
        QVERIFY(!QFile::exists(folder.path() + "/Configuration/Workspace"));
    }

    void theFirstChangeWritesTheTwoItHadAsWell()
    {
        QTemporaryDir folder;
        QString id;
        QCOMPARE(add(folder.path(), false, "Overflow", &id), QString());
        const Screens found = read(folder.path());
        QVERIFY(found.fromFile);
        QCOMPARE(found.screens.size(), 3);
        // With the ids they had, so that what they are connected to here still is.
        QCOMPARE(found.screens.at(0), defaults().at(0));
        QCOMPARE(found.screens.at(1), defaults().at(1));
        QCOMPARE(found.screens.at(2).id, id);
        QCOMPARE(found.screens.at(2).name, "Overflow");
        QVERIFY(!found.screens.at(2).stage);
    }

    void aNewScreenIsWrittenAsOneConnectedToNothing()
    {
        QTemporaryDir folder;
        QString id;
        QCOMPARE(add(folder.path(), true, "Choir", &id), QString());
        const rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        const rv::data::ProPresenterScreen &made = document.pro_screens(2);
        QCOMPARE(made.screen_type(), rv::data::ProPresenterScreen::SCREEN_TYPE_STAGE);
        QVERIFY(made.rendering_enabled());
        QCOMPARE(made.arrangement_single().screens_size(), 1);
        const rv::data::Screen &part = made.arrangement_single().screens(0);
        QCOMPARE(part.bounds().size().width(), 1920.0);
        QCOMPARE(part.output_display().type(), rv::data::OutputDisplay::TYPE_CUSTOM);
        QVERIFY(QString::fromStdString(part.output_display().render_id()).startsWith("placeholder:"));
        QCOMPARE(part.subscreen_unit_rect().size().width(), 1.0);
    }

    void theScreensOfAFileProPresenterWroteAreReadWithTheirSizesAndLayouts()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        const Screens found = read(folder.path());
        QVERIFY(found.fromFile);
        QCOMPARE(found.screens.size(), 3);
        QCOMPARE(found.screens.at(0).name, "Main Hall");
        QCOMPARE(found.screens.at(2).id, "CCCC");
        QVERIFY(found.screens.at(2).stage);
        QCOMPARE(found.screens.at(2).width, 960);
        QCOMPARE(found.screens.at(2).height, 1740);
        QCOMPARE(found.layouts.value("CCCC"), "LAYOUT");
    }

    void aChangeLeavesTheRestOfProPresentersFileAsItWas()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QString id;
        QCOMPARE(add(folder.path(), false, "Lobby", &id), QString());
        QCOMPARE(rename(folder.path(), "BBBB", "Live Stream"), QString());
        const rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        QCOMPARE(document.pro_screens_size(), 4);
        QCOMPARE(document.pro_screens(1).name(), "Live Stream");
        // What ProPresenter connected its screens to is not touched.
        QCOMPARE(document.pro_screens(0).arrangement_single().screens(0).output_display().model(), "DeckLink Duo 2");
        QCOMPARE(document.pro_screens(1).arrangement_single().screens(0).output_display().type(), rv::data::OutputDisplay::TYPE_CARD);
        QCOMPARE(document.selected_library_name(), "Songs");
        QCOMPARE(document.audio_channel_count(), 8u);
        QCOMPARE(document.audience_looks(0).screen_looks_size(), 2);
        QCOMPARE(document.stage_layout_mappings_size(), 1);
    }

    void aScreenThatIsRemovedTakesItsLayoutAndItsPlaceInTheLooksWithIt()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QCOMPARE(remove(folder.path(), "CCCC"), QString());
        QCOMPARE(remove(folder.path(), "BBBB"), QString());
        const rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        QCOMPARE(document.pro_screens_size(), 1);
        QCOMPARE(document.stage_layout_mappings_size(), 0);
        QCOMPARE(document.audience_looks(0).screen_looks_size(), 1);
        QCOMPARE(document.audience_looks(0).screen_looks(0).pro_screen_uuid().string(), "AAAA");
        QVERIFY(read(folder.path()).layouts.isEmpty());
    }

    void theLastAudienceScreenStays()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QCOMPARE(remove(folder.path(), "AAAA"), QString());
        QVERIFY(!remove(folder.path(), "BBBB").isEmpty());
        QVERIFY(!remove(folder.path(), "no such").isEmpty());
        QCOMPARE(read(folder.path()).screens.size(), 2);
    }

    void thereIsRoomForSixteen()
    {
        QTemporaryDir folder;
        for (int i = 2; i < limit; ++i)
            QCOMPARE(add(folder.path(), i % 2 == 0, QStringLiteral("Screen %1").arg(i + 1), nullptr), QString());
        QCOMPARE(read(folder.path()).screens.size(), limit);
        QVERIFY(!add(folder.path(), false, "One too many", nullptr).isEmpty());
        QCOMPARE(read(folder.path()).screens.size(), limit);
    }

    void aScreenNeedsAName()
    {
        QTemporaryDir folder;
        QVERIFY(!rename(folder.path(), defaults().at(0).id, "  ").isEmpty());
        QCOMPARE(rename(folder.path(), defaults().at(0).id, " Sanctuary "), QString());
        QCOMPARE(read(folder.path()).screens.at(0).name, "Sanctuary");
    }

    void aFileThatCannotBeReadIsLeftAlone()
    {
        QTemporaryDir folder;
        QDir().mkpath(folder.path() + "/Configuration");
        QFile file(folder.path() + "/Configuration/Workspace");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xff\xff\xff\xff not a file of ProPresenter's");
        file.close();
        const Screens found = read(folder.path());
        QVERIFY(!found.error.isEmpty());
        QCOMPARE(found.screens, defaults());
        QVERIFY(!add(folder.path(), false, "New", nullptr).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().startsWith("\xff\xff\xff\xff not"));
    }
};

QTEST_APPLESS_MAIN(TestScreenFile)
#include "tst_screenfile.moc"
