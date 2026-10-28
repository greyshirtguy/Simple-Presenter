// The list of a workspace's screens in ProPresenter's set-up file (src/screenfile.h),
// tried on folders made for the purpose: what is read from a workspace with no file,
// what a change writes, and that the rest of a file ProPresenter wrote goes back as it
// came. And a screen of outputs combined: made of two side by side and back, its first
// output's record kept as ProPresenter connected it, its second written as ProPresenter
// 7 wrote one (from a file it wrote), and its size read as the whole.

#include "screenfile.h"

#include "proworkspace.pb.h"

#include <google/protobuf/text_format.h>

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

// A workspace's set-up file written, and its bytes read
void writeFile(const QString &workspace, const rv::data::ProPresenterWorkspace &document)
{
    QDir().mkpath(workspace + "/Configuration");
    QFile file(workspace + "/Configuration/Workspace");
    QVERIFY(file.open(QIODevice::WriteOnly));
    const std::string bytes = document.SerializeAsString();
    file.write(bytes.data(), qint64(bytes.size()));
}

QByteArray contentsOf(const QString &workspace)
{
    QFile file(workspace + "/Configuration/Workspace");
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
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
        // (Its place, the whole screen, as ProPresenter writes it for a screen of one output)
        part->mutable_subscreen_unit_rect()->mutable_origin();
        part->mutable_subscreen_unit_rect()->mutable_size()->set_width(1);
        part->mutable_subscreen_unit_rect()->mutable_size()->set_height(1);
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
    writeFile(workspace, document);
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
        QCOMPARE(part.output_display().devicename(), "Choir");
        // (Its mode as ProPresenter names a placeholder's of this size, with no rate)
        QCOMPARE(part.output_display().mode().name(), "1080p0");
        QCOMPARE(part.output_display().mode().refresh_rate(), 0.0);
        QCOMPARE(part.subscreen_unit_rect().size().width(), 1.0);
    }

    void twoOutputsSideBySideAreOneScreenTwiceAsWide()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        const std::string before = fileOf(folder.path()).pro_screens(0).arrangement_single().screens(0).SerializeAsString();
        QCOMPARE(arrange(folder.path(), "AAAA", 2, 1), QString());
        rv::data::ProPresenterWorkspace document = fileOf(folder.path());
        const rv::data::ProPresenterScreen &screen = document.pro_screens(0);
        QVERIFY(screen.has_arrangement_combined());
        QCOMPARE(screen.arrangement_combined().columns(), 2u);
        QCOMPARE(screen.arrangement_combined().rows(), 1u);
        QCOMPARE(screen.arrangement_combined().screens_size(), 2);
        // The first output is the one ProPresenter connected, as it was, at the left
        const rv::data::Screen &left = screen.arrangement_combined().screens(0);
        QCOMPARE(left.SerializeAsString(), before);
        // and the second a new one, connected to nothing, as ProPresenter writes the
        // second it adds: at x 1920 in the whole, as big as the first, the whole of
        // itself, named by its place, naming no device, at 1080p60
        const rv::data::Screen &right = screen.arrangement_combined().screens(1);
        QCOMPARE(right.bounds().origin().x(), 1920.0);
        QCOMPARE(right.bounds().origin().y(), 0.0);
        QCOMPARE(right.bounds().size().width(), 1920.0);
        QCOMPARE(right.bounds().size().height(), 1080.0);
        QCOMPARE(right.subscreen_unit_rect().origin().x(), 0.0);
        QCOMPARE(right.subscreen_unit_rect().size().width(), 1.0);
        QCOMPARE(right.subscreen_unit_rect().size().height(), 1.0);
        QCOMPARE(right.output_display().type(), rv::data::OutputDisplay::TYPE_CUSTOM);
        QCOMPARE(QString::fromStdString(right.name()), "Placeholder - 2");
        QCOMPARE(QString::fromStdString(right.output_display().name()), "Placeholder - 2");
        QCOMPARE(right.output_display().devicename(), "");
        QCOMPARE(right.output_display().mode().name(), "1080p60");
        QCOMPARE(right.output_display().mode().refresh_rate(), 60.0);
        QVERIFY(QString::fromStdString(right.output_display().render_id()).startsWith("placeholder:"));
        QVERIFY(right.uuid().string() != left.uuid().string());
        // Read as one screen twice as wide, of two columns
        const Screens found = read(folder.path());
        QCOMPARE(found.screens.at(0).width, 3840);
        QCOMPARE(found.screens.at(0).height, 1080);
        QCOMPARE(found.screens.at(0).columns, 2);
        QCOMPARE(found.screens.at(0).rows, 1);
        // Nothing else of the file changes
        QCOMPARE(document.pro_screens(1).arrangement_single().screens_size(), 1);
        QCOMPARE(document.selected_library_name(), "Songs");
        QCOMPARE(document.audience_looks(0).screen_looks_size(), 2);
        // Asked again for the same, the file is not written
        const QByteArray bytes = contentsOf(folder.path());
        QCOMPARE(arrange(folder.path(), "AAAA", 2, 1), QString());
        QCOMPARE(contentsOf(folder.path()), bytes);
        // And one output again: the first, as it was, byte for byte
        QCOMPARE(arrange(folder.path(), "AAAA", 1, 1), QString());
        document = fileOf(folder.path());
        QVERIFY(document.pro_screens(0).has_arrangement_single());
        QCOMPARE(document.pro_screens(0).arrangement_single().screens_size(), 1);
        QCOMPARE(document.pro_screens(0).arrangement_single().screens(0).SerializeAsString(), before);
        QCOMPARE(read(folder.path()).screens.at(0).width, 1920);
    }

    // A screen ProPresenter 7 wrote, made Combined, two outputs side by side, with nothing
    // connected to either (as in a file it wrote, but for the ids, made up here)
    void aCombinedScreenProPresenterWroteIsReadAsOneAndWrittenAsItWas()
    {
        QTemporaryDir folder;
        rv::data::ProPresenterWorkspace document;
        QVERIFY(google::protobuf::TextFormat::ParseFromString(
            "  name: \"Double Wide\"\n"
            "  screen_type: SCREEN_TYPE_AUDIENCE\n"
            "  arrangement_combined {\n"
            "    screens {\n"
            "      uuid { string: \"00000001-0000-4000-8000-000000000000\" }\n"
            "      name: \"Placeholder - 1\"\n"
            "      color { red: 0.6667 green: 0.6667 blue: 0.6667 alpha: 1 }\n"
            "      bounds { origin { } size { width: 1920 height: 1080 } }\n"
            "      subscreen_unit_rect { origin { } size { width: 1 height: 1 } }\n"
            "      corner_values { top_left { } top_right { } bottom_left { } bottom_right { } }\n"
            "      output_display {\n"
            "        name: \"Placeholder - 1\"\n"
            "        deviceName: \"Placeholder - 1\"\n"
            "        type: TYPE_CUSTOM\n"
            "        mode { name: \"1080p0\" width: 1920 height: 1080 }\n"
            "        render_id: \"placeholder:00000001-1111-4000-8000-000000000000\"\n"
            "        hdr_max_nits_offset: 1000\n"
            "      }\n"
            "      color_enabled: true\n"
            "      color_adjustment { }\n"
            "      alpha_settings { mode: MODE_DISABLED }\n"
            "    }\n"
            "    screens {\n"
            "      uuid { string: \"00000002-0000-4000-8000-000000000000\" }\n"
            "      name: \"Placeholder - 2\"\n"
            "      color { red: 0.6667 green: 0.6667 blue: 0.6667 alpha: 1 }\n"
            "      bounds { origin { x: 1920 } size { width: 1920 height: 1080 } }\n"
            "      subscreen_unit_rect { origin { } size { width: 1 height: 1 } }\n"
            "      corner_values { top_left { } top_right { } bottom_left { } bottom_right { } }\n"
            "      output_display {\n"
            "        name: \"Placeholder - 2\"\n"
            "        type: TYPE_CUSTOM\n"
            "        mode { name: \"1080p60\" width: 1920 height: 1080 refresh_rate: 60 }\n"
            "        render_id: \"placeholder:00000002-1111-4000-8000-000000000000\"\n"
            "        hdr_max_nits_offset: 1000\n"
            "      }\n"
            "      color_enabled: true\n"
            "      color_adjustment { }\n"
            "      alpha_settings { mode: MODE_DISABLED }\n"
            "    }\n"
            "    rows: 1\n"
            "    columns: 2\n"
            "  }\n"
            "  background_color { alpha: 1 }\n"
            "  uuid { string: \"DDDDDDDD-0000-4000-8000-000000000000\" }\n"
            "  rendering_enabled: true\n",
            document.add_pro_screens()));
        writeFile(folder.path(), document);
        const Screens found = read(folder.path());
        QCOMPARE(found.screens.at(0).name, "Double Wide");
        QCOMPARE(found.screens.at(0).columns, 2);
        QCOMPARE(found.screens.at(0).rows, 1);
        QCOMPARE(found.screens.at(0).width, 3840);
        QCOMPARE(found.screens.at(0).height, 1080);
        // Asked for what it is, nothing is written
        const QByteArray bytes = contentsOf(folder.path());
        QCOMPARE(arrange(folder.path(), "DDDDDDDD-0000-4000-8000-000000000000", 2, 1), QString());
        QCOMPARE(contentsOf(folder.path()), bytes);
        // Made one output and two again, the second is written as ProPresenter wrote it,
        // but for its ids
        rv::data::Screen second = document.pro_screens(0).arrangement_combined().screens(1);
        QCOMPARE(arrange(folder.path(), "DDDDDDDD-0000-4000-8000-000000000000", 1, 1), QString());
        QCOMPARE(arrange(folder.path(), "DDDDDDDD-0000-4000-8000-000000000000", 2, 1), QString());
        rv::data::Screen made = fileOf(folder.path()).pro_screens(0).arrangement_combined().screens(1);
        QVERIFY(made.uuid().string() != second.uuid().string());
        made.clear_uuid();
        second.clear_uuid();
        made.mutable_output_display()->clear_render_id();
        second.mutable_output_display()->clear_render_id();
        QCOMPARE(made.SerializeAsString(), second.SerializeAsString());
    }

    // One of more rows and columns, each output's place in the origin of its bounds (that
    // ProPresenter writes a second row's at y 720 is a GUESS)
    void aCombinedScreenOfRowsAndColumnsIsReadByThem()
    {
        QTemporaryDir folder;
        rv::data::ProPresenterWorkspace document;
        rv::data::ProPresenterScreen *made = document.add_pro_screens();
        made->set_name("Wall");
        made->mutable_uuid()->set_string("WALL");
        auto *combined = made->mutable_arrangement_combined();
        combined->set_columns(2);
        combined->set_rows(2);
        for (int i = 0; i < 4; ++i) {
            rv::data::Screen *part = combined->add_screens();
            part->mutable_bounds()->mutable_origin()->set_x((i % 2) * 1280);
            part->mutable_bounds()->mutable_origin()->set_y((i / 2) * 720);
            part->mutable_bounds()->mutable_size()->set_width(1280);
            part->mutable_bounds()->mutable_size()->set_height(720);
        }
        writeFile(folder.path(), document);
        const Screens found = read(folder.path());
        QCOMPARE(found.screens.at(0).columns, 2);
        QCOMPARE(found.screens.at(0).rows, 2);
        QCOMPARE(found.screens.at(0).width, 2560);
        QCOMPARE(found.screens.at(0).height, 1440);
        // Made two side by side, the two in the second row go and the rest are as they were
        QCOMPARE(arrange(folder.path(), "WALL", 2, 1), QString());
        const rv::data::ProPresenterScreen screen = fileOf(folder.path()).pro_screens(0);
        QCOMPARE(screen.arrangement_combined().screens_size(), 2);
        QCOMPARE(screen.arrangement_combined().screens(1).SerializeAsString(), combined->screens(1).SerializeAsString());
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
