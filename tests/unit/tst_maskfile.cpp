// The masks of a workspace in ProPresenter's set-up file (src/maskfile.h), tried on
// folders made for the purpose.

#include "maskfile.h"

#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace maskfile;

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

QByteArray bytesOf(const QString &workspace)
{
    QFile file(workspace + "/Configuration/Workspace");
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void write(const QString &workspace, const rv::data::ProPresenterWorkspace &document)
{
    QDir().mkpath(workspace + "/Configuration");
    QFile file(workspace + "/Configuration/Workspace");
    QVERIFY(file.open(QIODevice::WriteOnly));
    const std::string bytes = document.SerializeAsString();
    file.write(bytes.data(), qint64(bytes.size()));
}

// A file as ProPresenter might have left it: two masks, each a slide with a shape on
// it and things about the slide that this app has no use for; a look whose lines give
// the room the first mask and the stream the second, and a live look that gives the
// room the first; a screen; and other things the file keeps (a library's name, how
// many channels of audio).
rv::data::ProPresenterWorkspace proPresenters()
{
    rv::data::ProPresenterWorkspace document;
    const auto mask = [&document](const char *id, const char *name, double width) {
        rv::data::ProMask *made = document.add_masks();
        made->set_name(name);
        rv::data::Slide *slide = made->mutable_base_slide();
        slide->mutable_uuid()->set_string(id);
        slide->mutable_size()->set_width(width);
        slide->mutable_size()->set_height(1080);
        rv::data::Graphics::Element *shape = slide->add_elements()->mutable_element();
        shape->mutable_uuid()->set_string(std::string("SHAPE-OF-") + id);
        shape->set_name("Left Edge");
        shape->mutable_bounds()->mutable_size()->set_width(200);
        shape->mutable_bounds()->mutable_size()->set_height(1080);
        shape->mutable_fill()->set_enable(true);
        shape->mutable_fill()->mutable_color()->set_alpha(1);
        return made;
    };
    mask("MASK-1", "Round Screen", 1920);
    mask("MASK-2", "Doorway", 3840);
    rv::data::ProAudienceLook *look = document.add_audience_looks();
    look->mutable_uuid()->set_string("LOOK-1");
    look->set_name("Sunday");
    const auto line = [](rv::data::ProAudienceLook *of, const char *screen, const char *maskId) {
        rv::data::ProAudienceLook::ProScreenLook *made = of->add_screen_looks();
        made->mutable_pro_screen_uuid()->set_string(screen);
        made->set_presentation_foreground_enabled(true);
        made->set_announcements_enabled(true);
        if (*maskId)
            made->mutable_mask_uuid()->set_string(maskId);
    };
    line(look, "ROOM", "MASK-1");
    line(look, "STREAM", "MASK-2");
    line(look, "FOYER", "");
    rv::data::ProAudienceLook *live = document.mutable_live_audience_look();
    live->mutable_uuid()->set_string("LIVE-COPY");
    live->mutable_original_look_uuid()->set_string("LOOK-1");
    line(live, "ROOM", "MASK-1");
    line(live, "STREAM", "MASK-2");
    document.add_pro_screens()->set_name("Room");
    document.set_selected_library_name("Songs");
    document.set_audio_channel_count(6);
    return document;
}

// The file with its masks left out: what no change to the masks may touch.
std::string rest(rv::data::ProPresenterWorkspace document)
{
    document.clear_masks();
    return document.SerializeAsString();
}

}

class TestMaskFile : public QObject
{
    Q_OBJECT

private slots:
    void aWorkspaceWithNoFileHasNoMasks()
    {
        QTemporaryDir folder;
        QString error = "unset";
        QVERIFY(read(folder.path(), &error).isEmpty());
        QCOMPARE(error, QString());
        QVERIFY(!QFile::exists(folder.path() + "/Configuration/Workspace"));
    }

    void proPresentersMasksAreReadInTheFilesOrder()
    {
        QTemporaryDir folder;
        write(folder.path(), proPresenters());
        const QByteArray before = bytesOf(folder.path());
        QString error;
        const QList<Mask> masks = read(folder.path(), &error);
        QCOMPARE(error, QString());
        QCOMPARE(masks.size(), 2);
        QCOMPARE(masks.at(0).id, "MASK-1");
        QCOMPARE(masks.at(0).name, "Round Screen");
        QCOMPARE(masks.at(1).id, "MASK-2");
        QCOMPARE(masks.at(1).name, "Doorway");
        // Its slide is handed on whole, as the file has it.
        rv::data::Slide slide;
        QVERIFY(slide.ParseFromArray(masks.at(1).slide.constData(), int(masks.at(1).slide.size())));
        QCOMPARE(slide.size().width(), 3840.0);
        QCOMPARE(slide.elements_size(), 1);
        QCOMPARE(slide.elements(0).element().name(), "Left Edge");
        // Reading writes nothing.
        QCOMPARE(bytesOf(folder.path()), before);
    }

    void aFileThatIsNotOneIsLeftAlone()
    {
        QTemporaryDir folder;
        QDir().mkpath(folder.path() + "/Configuration");
        QFile file(folder.path() + "/Configuration/Workspace");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xff\xff\xff\xff not a workspace");
        file.close();
        const QByteArray before = bytesOf(folder.path());
        QString error;
        QVERIFY(read(folder.path(), &error).isEmpty());
        QVERIFY(error.contains("not one this app can read"));
        QString id;
        QVERIFY(!add(folder.path(), "Mine", &id).isEmpty());
        QVERIFY(!rename(folder.path(), "MASK-1", "Mine").isEmpty());
        QVERIFY(!remove(folder.path(), "MASK-1").isEmpty());
        QCOMPARE(bytesOf(folder.path()), before);
    }

    void aNewMaskIsLastHasNothingOnItAndANameOfItsOwn()
    {
        QTemporaryDir folder;
        const rv::data::ProPresenterWorkspace was = proPresenters();
        write(folder.path(), was);
        QString id;
        QCOMPARE(add(folder.path(), "", &id), QString());
        const rv::data::ProPresenterWorkspace now = fileOf(folder.path());
        QCOMPARE(now.masks_size(), 3);
        const rv::data::ProMask &made = now.masks(2);
        QCOMPARE(made.name(), "Mask");
        QCOMPARE(QString::fromStdString(made.base_slide().uuid().string()), id);
        QCOMPARE(id.size(), 36);
        QVERIFY(id != "MASK-1" && id != "MASK-2");
        QCOMPARE(made.base_slide().elements_size(), 0);
        QVERIFY(!made.base_slide().draws_background_color());
        QCOMPARE(made.base_slide().size().width(), 1920.0);
        QCOMPARE(made.base_slide().size().height(), 1080.0);
        // The masks that were there, and the rest of the file, are as they were.
        QCOMPARE(now.masks(0).SerializeAsString(), was.masks(0).SerializeAsString());
        QCOMPARE(now.masks(1).SerializeAsString(), was.masks(1).SerializeAsString());
        QCOMPARE(rest(now), rest(was));
        // Another, and one asked for by a name that is taken: each gets a name no mask has.
        QString second;
        QCOMPARE(add(folder.path(), "", &second), QString());
        QString third;
        QCOMPARE(add(folder.path(), " Doorway ", &third), QString());
        const QList<Mask> masks = read(folder.path());
        QCOMPARE(masks.size(), 5);
        QCOMPARE(masks.at(3).name, "Mask 2");
        QCOMPARE(masks.at(4).name, "Doorway 2");
        QVERIFY(second != id && third != second);
    }

    void aWorkspaceWithNoFileIsGivenOneForItsFirstMask()
    {
        QTemporaryDir folder;
        QString id;
        QCOMPARE(add(folder.path(), "Stage Left", &id), QString());
        const QList<Mask> masks = read(folder.path());
        QCOMPARE(masks.size(), 1);
        QCOMPARE(masks.first().name, "Stage Left");
        QCOMPARE(masks.first().id, id);
        // The file has the mask and nothing else.
        QCOMPARE(rest(fileOf(folder.path())), std::string());
    }

    void aNameNoMaskHas()
    {
        QCOMPARE(freeName({}, ""), "Mask");
        QCOMPARE(freeName({{"a", "Mask", {}}}, ""), "Mask 2");
        QCOMPARE(freeName({{"a", "Mask", {}}, {"b", "Mask 2", {}}, {"c", "Mask 4", {}}}, ""), "Mask 3");
        QCOMPARE(freeName({{"a", "Mask", {}}}, "Edges"), "Edges");
        QCOMPARE(freeName({{"a", "Edges", {}}}, "  Edges "), "Edges 2");
    }

    void aMaskIsRenamedAndNothingElseIsChanged()
    {
        QTemporaryDir folder;
        const rv::data::ProPresenterWorkspace was = proPresenters();
        write(folder.path(), was);
        const QByteArray before = bytesOf(folder.path());
        QCOMPARE(rename(folder.path(), "MASK-2", "  Side Door "), QString());
        const rv::data::ProPresenterWorkspace now = fileOf(folder.path());
        QCOMPARE(now.masks(1).name(), "Side Door");
        // Its slide, with everything on it, is as it was; so are the other mask and the rest.
        QCOMPARE(now.masks(1).base_slide().SerializeAsString(), was.masks(1).base_slide().SerializeAsString());
        QCOMPARE(now.masks(0).SerializeAsString(), was.masks(0).SerializeAsString());
        QCOMPARE(rest(now), rest(was));
        // The looks still give it to the screens they gave it to: they name it by id.
        QCOMPARE(now.audience_looks(0).screen_looks(1).mask_uuid().string(), "MASK-2");
        // Named back, the file is what it was to the byte.
        QCOMPARE(rename(folder.path(), "MASK-2", "Doorway"), QString());
        QCOMPARE(bytesOf(folder.path()), before);
        // The name it has already writes nothing; no name, and a mask that is not there, are refused.
        QCOMPARE(rename(folder.path(), "MASK-2", "Doorway"), QString());
        QVERIFY(rename(folder.path(), "MASK-2", "   ").contains("needs a name"));
        QVERIFY(rename(folder.path(), "MASK-9", "Other").contains("not in the workspace"));
        QVERIFY(rename(folder.path(), "", "Other").contains("not in the workspace"));
        QCOMPARE(bytesOf(folder.path()), before);
    }

    void aMaskThatIsRemovedIsTakenFromEveryLookThatGaveIt()
    {
        QTemporaryDir folder;
        const rv::data::ProPresenterWorkspace was = proPresenters();
        write(folder.path(), was);
        QCOMPARE(remove(folder.path(), "MASK-1"), QString());
        const rv::data::ProPresenterWorkspace now = fileOf(folder.path());
        QCOMPARE(now.masks_size(), 1);
        QCOMPARE(now.masks(0).SerializeAsString(), was.masks(1).SerializeAsString());
        // The room, which had it in the saved look and in the live one, has none now.
        QVERIFY(!now.audience_looks(0).screen_looks(0).has_mask_uuid());
        QVERIFY(!now.live_audience_look().screen_looks(0).has_mask_uuid());
        // The stream keeps the other mask, in both; and each line's switches are as they were.
        QCOMPARE(now.audience_looks(0).screen_looks(1).mask_uuid().string(), "MASK-2");
        QCOMPARE(now.live_audience_look().screen_looks(1).mask_uuid().string(), "MASK-2");
        QVERIFY(now.audience_looks(0).screen_looks(0).presentation_foreground_enabled());
        QVERIFY(now.audience_looks(0).screen_looks(0).announcements_enabled());
        QCOMPARE(now.audience_looks(0).screen_looks_size(), 3);
        // The rest of the file is as it was.
        QCOMPARE(now.live_audience_look().uuid().string(), "LIVE-COPY");
        QCOMPARE(now.pro_screens(0).name(), "Room");
        QCOMPARE(now.selected_library_name(), "Songs");
        QCOMPARE(now.audio_channel_count(), 6u);
        // One that is not there is refused, and nothing is written.
        const QByteArray after = bytesOf(folder.path());
        QVERIFY(remove(folder.path(), "MASK-1").contains("not in the workspace"));
        QCOMPARE(bytesOf(folder.path()), after);
    }
};

QTEST_MAIN(TestMaskFile)
#include "tst_maskfile.moc"
