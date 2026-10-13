// Clear groups (src/clearfile.h): the workspace's list of them, read and changed, tried
// on files made as ProPresenter writes them, and on a workspace that has no file.

#include "clearfile.h"

#include "clearGroups.pb.h"

#include <google/protobuf/unknown_field_set.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

using namespace clearfile;
using Document = rv::data::ClearGroupsDocument;
using FileGroup = rv::data::ClearGroupsDocument::ClearGroup;
using Clear = rv::data::Action::ClearType;
using rv::data::Action;

namespace {

const QStringList kEveryLayer {"audio", "audioEffects", "messages", "props", "announcements", "slide", "media", "videoInput"};

QString pathOf(const QString &workspace)
{
    return workspace + "/Configuration/ClearGroups";
}

Document fileOf(const QString &workspace)
{
    Document document;
    QFile file(pathOf(workspace));
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        document.ParseFromArray(bytes.constData(), int(bytes.size()));
    }
    return document;
}

QByteArray bytesOf(const QString &workspace)
{
    QFile file(pathOf(workspace));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

// Which file it is on the disk: a file that is written again, however the same, is
// another file under the same name (it is written beside the old one and put in its
// place), so this tells a file that was left alone from one that was written.
quint64 fileItself(const QString &workspace)
{
    struct stat info;
    return ::stat(pathOf(workspace).toLocal8Bit().constData(), &info) == 0 ? quint64(info.st_ino) : 0;
}

void target(FileGroup *group, Clear::ClearTargetLayer layer)
{
    group->add_layer_targets()->set_target_layer(layer);
}

// The group ProPresenter starts a workspace with, as the one file of its own that was
// looked at has it, field for field (the id apart, which is made up here).
void writeProPresentersOwn(FileGroup *group)
{
    group->mutable_uuid()->set_string("0A0A0A0A-1111-2222-3333-444444444444");
    group->set_name("Clear All");
    target(group, Clear::CLEAR_TARGET_LAYER_AUDIO);
    target(group, Clear::CLEAR_TARGET_LAYER_AUDIO_EFFECTS);
    target(group, Clear::CLEAR_TARGET_LAYER_MESSAGES);
    target(group, Clear::CLEAR_TARGET_LAYER_PROP);
    group->add_layer_targets()->set_content_destination(Action::CONTENT_DESTINATION_ANNOUNCEMENTS);
    target(group, Clear::CLEAR_TARGET_LAYER_SLIDE);
    target(group, Clear::CLEAR_TARGET_LAYER_BACKGROUND);
    target(group, Clear::CLEAR_TARGET_LAYER_LIVE_VIDEO);
    group->set_image_type(FileGroup::ImageTypeAll);
    rv::data::Color *tint = group->mutable_icon_tint_color();
    tint->set_red(1);
    tint->set_green(1);
    tint->set_blue(1);
    tint->set_alpha(1);
    group->add_timeline_targets(Action::CONTENT_DESTINATION_GLOBAL);
    group->add_timeline_targets(Action::CONTENT_DESTINATION_ANNOUNCEMENTS);
    group->set_clear_presentation_next_slide(true);
}

// A file as ProPresenter might have left it: its own group; a second, of the props and
// the slide and of clearing to the logo (which is none of the eight layers), with a
// picture of the user's own, a tint that is switched on, and its button hidden; and
// things in the file, and in a group, that this code knows nothing of.
void writeProPresenters(const QString &workspace)
{
    Document document;
    document.mutable_application_info()->set_platform(rv::data::ApplicationInfo::PLATFORM_MACOS);
    document.mutable_application_info()->mutable_application_version()->set_major_version(20);
    writeProPresentersOwn(document.add_groups());
    FileGroup *second = document.add_groups();
    second->mutable_uuid()->set_string("0B0B0B0B-1111-2222-3333-444444444444");
    second->set_name("Stage Only");
    target(second, Clear::CLEAR_TARGET_LAYER_PROP);
    target(second, Clear::CLEAR_TARGET_LAYER_LOGO);
    target(second, Clear::CLEAR_TARGET_LAYER_SLIDE);
    second->set_is_hidden_in_preview(true);
    second->set_image_data(std::string("\x89PNG\r\n\x1a\n and the rest of a picture", 34));
    second->set_is_icon_tinted(true);
    second->mutable_icon_tint_color()->set_red(1);
    second->mutable_icon_tint_color()->set_alpha(1);
    second->GetReflection()->MutableUnknownFields(second)->AddVarint(901, 7);
    document.GetReflection()->MutableUnknownFields(&document)->AddVarint(900, 42);
    const std::string bytes = encoded(document);
    QDir().mkpath(workspace + "/Configuration");
    QFile file(pathOf(workspace));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes.data(), qint64(bytes.size()));
}

const QString kOwn = QStringLiteral("0A0A0A0A-1111-2222-3333-444444444444");
const QString kSecond = QStringLiteral("0B0B0B0B-1111-2222-3333-444444444444");

}

class TestClearFile : public QObject
{
    Q_OBJECT

private slots:
    // ---- reading

    void aWorkspaceWithNoFileHasTheGroupEveryWorkspaceStartsWithAndNothingIsWritten()
    {
        QTemporaryDir workspace;
        QString error;
        const QList<Group> found = groups(workspace.path(), &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.at(0).name, "Clear All");
        QCOMPARE(found.at(0).layers, kEveryLayer);
        QCOMPARE(found.at(0).icon, 11);
        QVERIFY(!found.at(0).id.isEmpty() && !found.at(0).tinted && !found.at(0).hidden && found.at(0).image.isEmpty());
        QCOMPARE(found.at(0).tint, "#ffffff");
        // The same group every time, by its id.
        QCOMPARE(groups(workspace.path(), &error).at(0).id, found.at(0).id);
        QCOMPARE(standard().id, found.at(0).id);
        QVERIFY(!QFile::exists(pathOf(workspace.path())));
    }

    void theGroupMadeHereIsWrittenAsProPresentersOwnIs()
    {
        // What a workspace with no file is given, once it has to be written: the same as
        // ProPresenter's own group, field for field, but for its id.
        QTemporaryDir workspace;
        QVERIFY(keep(workspace.path()).isEmpty());
        Document made = fileOf(workspace.path());
        QCOMPARE(made.groups_size(), 1);
        FileGroup own;
        writeProPresentersOwn(&own);
        own.mutable_uuid()->set_string(made.groups(0).uuid().string());
        QCOMPARE(encoded(made.groups(0)), encoded(own));
        // And a workspace that has a file is left with the file it has.
        const quint64 was = fileItself(workspace.path());
        QVERIFY(keep(workspace.path()).isEmpty());
        QCOMPARE(fileItself(workspace.path()), was);
    }

    void aFileOfProPresentersIsRead()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        QString error;
        const QList<Group> found = groups(workspace.path(), &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).id, kOwn);
        QCOMPARE(found.at(0).layers, kEveryLayer);
        QCOMPARE(found.at(1).name, "Stage Only");
        // (Clearing to the logo is not one of the layers, and is not among them.)
        QCOMPARE(found.at(1).layers, (QStringList {"props", "slide"}));
        QCOMPARE(found.at(1).icon, 0);
        QVERIFY(found.at(1).image.startsWith("\x89PNG") && found.at(1).image.size() == 34);
        QVERIFY(found.at(1).tinted && found.at(1).hidden);
        QCOMPARE(found.at(1).tint, "#ff0000");
    }

    void aFileThatCannotBeReadIsSaidToBe()
    {
        QTemporaryDir workspace;
        QDir().mkpath(workspace.path() + "/Configuration");
        QFile file(pathOf(workspace.path()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xff\xff\xff\xff not a list of clear groups");
        file.close();
        QString error;
        QVERIFY(groups(workspace.path(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        // And is not written over by a change.
        const QByteArray was = bytesOf(workspace.path());
        QVERIFY(!add(workspace.path(), "NEW", "Another").isEmpty());
        QCOMPARE(bytesOf(workspace.path()), was);
    }

    // ---- the layers

    void aLayerIsTakenOutAndNothingElseIsTouched()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        Document expected = fileOf(workspace.path());
        QVERIFY(setLayer(workspace.path(), kOwn, "slide", false).isEmpty());
        // The sixth of its eight targets is the slide.
        expected.mutable_groups(0)->mutable_layer_targets()->DeleteSubrange(5, 1);
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));
        QString error;
        QCOMPARE(groups(workspace.path(), &error).at(0).layers,
                 (QStringList {"audio", "audioEffects", "messages", "props", "announcements", "media", "videoInput"}));
    }

    void aLayerPutBackIsWhereItWas()
    {
        // A change of mind leaves the file byte for byte as it was: each of the three
        // layers that can be changed, out and in again.
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const QByteArray was = bytesOf(workspace.path());
        for (const QString &layer : {QStringLiteral("props"), QStringLiteral("slide"), QStringLiteral("media")}) {
            QVERIFY(setLayer(workspace.path(), kOwn, layer, false).isEmpty());
            QVERIFY(bytesOf(workspace.path()) != was);
            QVERIFY(setLayer(workspace.path(), kOwn, layer, true).isEmpty());
            QCOMPARE(bytesOf(workspace.path()), was);
        }
    }

    void aLayerIsPutInProPresentersOrderAmongThoseTheGroupHas()
    {
        // The second group has the props, clearing to the logo, and the slide. The media
        // come after the slide; taken out and put back, the props come first again, and
        // the target that is none of the layers stays where it is.
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        QVERIFY(setLayer(workspace.path(), kSecond, "media", true).isEmpty());
        const auto targets = [&] {
            // (The file is held here for as long as its targets are gone through.)
            const Document now = fileOf(workspace.path());
            QStringList found;
            for (const Clear &target : now.groups(1).layer_targets())
                found << QString::number(int(target.target_layer()));
            return found.join(u' ');
        };
        // (As the file numbers them: 4 the props, 6 the logo, 5 the slide, 2 the media.)
        QCOMPARE(targets(), "4 6 5 2");
        QVERIFY(setLayer(workspace.path(), kSecond, "props", false).isEmpty());
        QCOMPARE(targets(), "6 5 2");
        QVERIFY(setLayer(workspace.path(), kSecond, "props", true).isEmpty());
        QCOMPARE(targets(), "6 4 5 2");
        // What the group has that this knows nothing of is still there.
        QCOMPARE(fileOf(workspace.path()).groups(1).GetReflection()->GetUnknownFields(fileOf(workspace.path()).groups(1)).field_count(), 1);
    }

    void aLayerTheGroupHasAlreadyOrHasNotWritesNothing()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const quint64 was = fileItself(workspace.path());
        QVERIFY(setLayer(workspace.path(), kOwn, "slide", true).isEmpty());
        QVERIFY(setLayer(workspace.path(), kSecond, "media", false).isEmpty());
        QCOMPARE(fileItself(workspace.path()), was);
    }

    void onlyTheLayersThisAppHasCanBeChanged()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const quint64 was = fileItself(workspace.path());
        QCOMPARE(ourLayers(), (QStringList {"audio", "audioEffects", "messages", "props", "announcements", "slide", "media"}));
        QCOMPARE(layers(), kEveryLayer);
        for (const QString &layer : {QStringLiteral("videoInput"), QStringLiteral("logo")})
            QVERIFY(!setLayer(workspace.path(), kOwn, layer, false).isEmpty());
        QCOMPARE(fileItself(workspace.path()), was);
    }

    void theMessagesCanBeTakenFromAGroupAndGivenBack()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const Document before = fileOf(workspace.path());
        QString error;
        QCOMPARE(setLayer(workspace.path(), kOwn, "messages", false), QString());
        QCOMPARE(groups(workspace.path(), &error).first().layers,
                 (QStringList {"audio", "audioEffects", "props", "announcements", "slide", "media", "videoInput"}));
        // The second group, which never had them, is given them: and is otherwise as it was
        QCOMPARE(setLayer(workspace.path(), kSecond, "messages", true), QString());
        QVERIFY(groups(workspace.path(), &error).at(1).layers.contains("messages"));
        QCOMPARE(setLayer(workspace.path(), kSecond, "messages", false), QString());
        QCOMPARE(encoded(fileOf(workspace.path()).groups(1)), encoded(before.groups(1)));
    }

    void theTwoHalvesOfTheAudioCanBeChangedAndNothingElseIsWith()
    {
        // ProPresenter's own group has all eight layers. The audio taken from it, and
        // then the audio effects, and both put back: each time only that layer of
        // that group is other than it was.
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const Document before = fileOf(workspace.path());
        QString error;
        QVERIFY(setLayer(workspace.path(), kOwn, "audio", false).isEmpty());
        QCOMPARE(groups(workspace.path(), &error).first().layers,
                 (QStringList {"audioEffects", "messages", "props", "announcements", "slide", "media", "videoInput"}));
        QVERIFY(setLayer(workspace.path(), kOwn, "audioEffects", false).isEmpty());
        QCOMPARE(groups(workspace.path(), &error).first().layers, (QStringList {"messages", "props", "announcements", "slide", "media", "videoInput"}));
        // The second group, and the rest of the first, are as they were.
        const Document without = fileOf(workspace.path());
        QCOMPARE(encoded(without.groups(1)), encoded(before.groups(1)));
        QCOMPARE(without.groups(0).name(), before.groups(0).name());
        QCOMPARE(without.groups(0).layer_targets_size(), before.groups(0).layer_targets_size() - 2);
        QVERIFY(setLayer(workspace.path(), kOwn, "audio", true).isEmpty());
        QVERIFY(setLayer(workspace.path(), kOwn, "audioEffects", true).isEmpty());
        const QStringList again = groups(workspace.path(), &error).first().layers;
        QCOMPARE(again.size(), 8);
        QVERIFY(again.contains("audio") && again.contains("audioEffects"));
    }

    // ---- the groups

    void aNewGroupIsLastAndClearsNothing()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        Document expected = fileOf(workspace.path());
        QVERIFY(add(workspace.path(), "NEW-1", "  Offering  ").isEmpty());
        const Document now = fileOf(workspace.path());
        QCOMPARE(now.groups_size(), 3);
        const FileGroup &made = now.groups(2);
        QCOMPARE(made.uuid().string(), "NEW-1");
        QCOMPARE(made.name(), "Offering");
        QCOMPARE(made.layer_targets_size(), 0);
        // The first of the digits that no group has for its picture; and the white tint,
        // not switched on, that ProPresenter's own has.
        QCOMPARE(int(made.image_type()), int(FileGroup::ImageTypeOne));
        QVERIFY(!made.is_icon_tinted() && made.icon_tint_color().red() == 1 && made.icon_tint_color().alpha() == 1);
        // The groups there were, and the rest of the file, are as they were.
        *expected.add_groups() = made;
        QCOMPARE(encoded(now), encoded(expected));

        QVERIFY(add(workspace.path(), "NEW-2", "Welcome").isEmpty());
        QCOMPARE(int(fileOf(workspace.path()).groups(3).image_type()), int(FileGroup::ImageTypeTwo));
        // A name is wanted, and an id that no group has.
        QVERIFY(!add(workspace.path(), "NEW-3", "  ").isEmpty());
        QVERIFY(!add(workspace.path(), "NEW-1", "Again").isEmpty());
        QCOMPARE(fileOf(workspace.path()).groups_size(), 4);
    }

    void theFirstChangeToAWorkspaceWithNoFileWritesItsGroupWithTheChange()
    {
        QTemporaryDir workspace;
        const QString own = standard().id;
        QVERIFY(setLayer(workspace.path(), own, "props", false).isEmpty());
        const Document now = fileOf(workspace.path());
        QCOMPARE(now.groups_size(), 1);
        QCOMPARE(now.groups(0).uuid().string(), own.toStdString());
        QCOMPARE(now.groups(0).layer_targets_size(), 7);
        QString error;
        QCOMPARE(groups(workspace.path(), &error).at(0).layers,
                 (QStringList {"audio", "audioEffects", "messages", "announcements", "slide", "media", "videoInput"}));
    }

    void aGroupIsRenamedMovedAndRemoved()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        Document expected = fileOf(workspace.path());

        QVERIFY(rename(workspace.path(), kSecond, " Band ").isEmpty());
        expected.mutable_groups(1)->set_name("Band");
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));
        QVERIFY(!rename(workspace.path(), kSecond, "").isEmpty());
        // (The name it has already writes nothing.)
        const quint64 was = fileItself(workspace.path());
        QVERIFY(rename(workspace.path(), kSecond, "Band").isEmpty());
        QCOMPARE(fileItself(workspace.path()), was);

        QVERIFY(move(workspace.path(), kSecond, -1).isEmpty());
        expected.mutable_groups()->SwapElements(0, 1);
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));
        // (At the top already: nothing to do, and nothing written.)
        const quint64 moved = fileItself(workspace.path());
        QVERIFY(move(workspace.path(), kSecond, -1).isEmpty());
        QCOMPARE(fileItself(workspace.path()), moved);
        QVERIFY(move(workspace.path(), kSecond, 1).isEmpty());
        expected.mutable_groups()->SwapElements(0, 1);
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));

        QVERIFY(remove(workspace.path(), kOwn).isEmpty());
        expected.mutable_groups()->DeleteSubrange(0, 1);
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));
        QVERIFY(!remove(workspace.path(), kOwn).isEmpty());
        // The last can go too, and the list is then empty: no group is made up in its place.
        QVERIFY(remove(workspace.path(), kSecond).isEmpty());
        QString error;
        QVERIFY(groups(workspace.path(), &error).isEmpty() && error.isEmpty());
        // What the file has that this knows nothing of is still there.
        QCOMPARE(fileOf(workspace.path()).GetReflection()->GetUnknownFields(fileOf(workspace.path())).field_count(), 1);
    }

    // ---- the picture, its tint, and whether the button is shown

    void aPictureIsChosenAndOneOfTheUsersOwnStaysInTheFile()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        Document expected = fileOf(workspace.path());
        QVERIFY(setIcon(workspace.path(), kSecond, 20).isEmpty());
        expected.mutable_groups(1)->set_image_type(FileGroup::ImageTypeBell);
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));
        QCOMPARE(fileOf(workspace.path()).groups(1).image_data().size(), size_t(34));
        // Only the pictures there are, by number.
        QVERIFY(!setIcon(workspace.path(), kSecond, 0).isEmpty());
        QVERIFY(!setIcon(workspace.path(), kSecond, 33).isEmpty());
        QCOMPARE(pictures(), 32);
        QCOMPARE(pictureName(1), "1");
        QCOMPARE(pictureName(10), "0");
        QCOMPARE(pictureName(11), "All");
        QCOMPARE(pictureName(20), "Bell");
        QCOMPARE(pictureName(32), "Exclamation");
        QCOMPARE(pictureName(0), "");
    }

    void aTintIsSwitchedOnGivenAColourAndSwitchedOff()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const QByteArray was = bytesOf(workspace.path());
        Document expected = fileOf(workspace.path());

        // Switched on, with the colour the group has; and off again, which leaves the
        // file byte for byte as it was.
        QVERIFY(setTint(workspace.path(), kOwn, true, "").isEmpty());
        expected.mutable_groups(0)->set_is_icon_tinted(true);
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));
        QVERIFY(setTint(workspace.path(), kOwn, false, "").isEmpty());
        QCOMPARE(bytesOf(workspace.path()), was);

        // Given a colour.
        QVERIFY(setTint(workspace.path(), kOwn, true, "#1E88E5").isEmpty());
        QString error;
        Group now = groups(workspace.path(), &error).at(0);
        QVERIFY(now.tinted);
        QCOMPARE(now.tint, "#1e88e5");
        // Switched off, the colour is kept, for when it is switched on again.
        QVERIFY(setTint(workspace.path(), kOwn, false, "").isEmpty());
        now = groups(workspace.path(), &error).at(0);
        QVERIFY(!now.tinted);
        QCOMPARE(now.tint, "#1e88e5");
        // What is not a colour is not taken, and what it has already writes nothing.
        QVERIFY(!setTint(workspace.path(), kOwn, true, "blue").isEmpty());
        const quint64 file = fileItself(workspace.path());
        QVERIFY(setTint(workspace.path(), kOwn, false, "#1e88e5").isEmpty());
        QCOMPARE(fileItself(workspace.path()), file);
    }

    void aButtonIsKeptOutOfSightAndBroughtBack()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const QByteArray was = bytesOf(workspace.path());
        QVERIFY(setHidden(workspace.path(), kOwn, true).isEmpty());
        QString error;
        QVERIFY(groups(workspace.path(), &error).at(0).hidden);
        QVERIFY(setHidden(workspace.path(), kOwn, false).isEmpty());
        QCOMPARE(bytesOf(workspace.path()), was);
    }

    void aGroupThatIsNotThereIsSaidNotToBe()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const quint64 was = fileItself(workspace.path());
        QVERIFY(!rename(workspace.path(), "GONE", "X").isEmpty());
        QVERIFY(!setLayer(workspace.path(), "GONE", "slide", true).isEmpty());
        QVERIFY(!setIcon(workspace.path(), "GONE", 3).isEmpty());
        QVERIFY(!setTint(workspace.path(), "GONE", true, "").isEmpty());
        QVERIFY(!setHidden(workspace.path(), "GONE", true).isEmpty());
        QVERIFY(!move(workspace.path(), "GONE", 1).isEmpty());
        QVERIFY(!remove(workspace.path(), "GONE").isEmpty());
        QCOMPARE(fileItself(workspace.path()), was);
    }
};

QTEST_APPLESS_MAIN(TestClearFile)
#include "tst_clearfile.moc"
