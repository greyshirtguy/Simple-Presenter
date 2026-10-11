// Putting slides into another group as ProPresenter does it from a slide's menu
// (src/groupedit.h), tried on a presentation built as ProPresenter writes one: groups
// given from the workspace's list, with their ids and colours, and arrangements.

#include "groupedit.h"

#include <QTest>

using namespace groupedit;

namespace {

// A song as ProPresenter might have left it: a verse of three slides and a chorus of
// two, each given from the workspace's list of groups, an ending in a group with no
// name, and two arrangements, one of which has the verse twice. And things this code
// has no business with, to see that they are still there afterwards.
rv::data::Presentation song()
{
    rv::data::Presentation presentation;
    presentation.set_name("Song");
    presentation.mutable_uuid()->set_string("SONG");
    presentation.mutable_ccli()->set_song_title("Kept");
    const auto group = [&presentation](const char *id, const char *name, const char *listId, std::initializer_list<const char *> cues) {
        rv::data::Presentation::CueGroup *entry = presentation.add_cue_groups();
        entry->mutable_group()->mutable_uuid()->set_string(id);
        entry->mutable_group()->set_name(name);
        entry->mutable_group()->mutable_hotkey();
        if (*listId) {
            entry->mutable_group()->mutable_application_group_identifier()->set_string(listId);
            entry->mutable_group()->mutable_color()->set_blue(1);
            entry->mutable_group()->mutable_color()->set_alpha(1);
        }
        for (const char *cue : cues) {
            entry->add_cue_identifiers()->set_string(cue);
            presentation.add_cues()->mutable_uuid()->set_string(cue);
        }
        return entry;
    };
    group("VERSE", "Verse 1", "LIST-VERSE", {"v1", "v2", "v3"});
    rv::data::Presentation::CueGroup *chorus = group("CHORUS", "Chorus", "LIST-CHORUS", {"c1", "c2"});
    // A hotkey of the presentation's own on a group: nothing here is to lose it.
    chorus->mutable_group()->mutable_hotkey()->set_code(rv::data::KEY_CODE_ANSI_C);
    group("END", "", "", {"e1"});
    const auto arrangement = [&presentation](const char *id, const char *name, std::initializer_list<const char *> groups) {
        rv::data::Presentation::Arrangement *made = presentation.add_arrangements();
        made->mutable_uuid()->set_string(id);
        made->set_name(name);
        for (const char *group : groups)
            made->add_group_identifiers()->set_string(group);
    };
    arrangement("A1", "Default", {"VERSE", "CHORUS", "END"});
    arrangement("A2", "Twice", {"VERSE", "CHORUS", "VERSE", "CHORUS"});
    presentation.mutable_selected_arrangement()->set_string("A1");
    return presentation;
}

const Choice bridge {QStringLiteral("Bridge"), QStringLiteral("#8000ff"), QStringLiteral("LIST-BRIDGE")};

std::function<std::string()> ids()
{
    auto next = std::make_shared<int>(0);
    return [next] { return "NEW-" + std::to_string(++*next); };
}

// The groups in order, each as "name[cues]"
QString groupsOf(const rv::data::Presentation &presentation)
{
    QStringList groups;
    for (const rv::data::Presentation::CueGroup &entry : presentation.cue_groups()) {
        QStringList cues;
        for (const rv::data::UUID &cue : entry.cue_identifiers())
            cues << QString::fromStdString(cue.string());
        groups << QString::fromStdString(entry.group().name()) + "[" + cues.join(",") + "]";
    }
    return groups.join(" ");
}

QString arrangementOf(const rv::data::Presentation &presentation, int index)
{
    QStringList groups;
    for (const rv::data::UUID &group : presentation.arrangements(index).group_identifiers())
        groups << QString::fromStdString(group.string());
    return groups.join(" ");
}

} // namespace

class TestGroupEdit : public QObject
{
    Q_OBJECT

private slots:
    void onTheFirstSlideOfAGroupTheWholeGroupBecomesTheChosenOne()
    {
        rv::data::Presentation presentation = song();
        const std::string before = presentation.SerializeAsString();
        QCOMPARE(setGroupFrom(&presentation, "v1", bridge, ids()), Outcome::Whole);
        QCOMPARE(groupsOf(presentation), "Bridge[v1,v2,v3] Chorus[c1,c2] [e1]");
        // It is the group it was: its id, and so its place in every arrangement.
        const rv::data::Group &group = presentation.cue_groups(0).group();
        QCOMPARE(group.uuid().string(), std::string("VERSE"));
        QCOMPARE(arrangementOf(presentation, 0), "VERSE CHORUS END");
        QCOMPARE(arrangementOf(presentation, 1), "VERSE CHORUS VERSE CHORUS");
        // Written as ProPresenter writes a group given from its list.
        QCOMPARE(group.application_group_identifier().string(), std::string("LIST-BRIDGE"));
        QVERIFY(qAbs(group.color().red() - 128 / 255.0f) < 0.001f && group.color().green() == 0 && group.color().blue() == 1 && group.color().alpha() == 1);
        QVERIFY(group.has_hotkey());
        // Nothing else is other than it was: put back, the presentation is byte for byte the one it was.
        rv::data::Presentation back = presentation;
        *back.mutable_cue_groups(0)->mutable_group() = song().cue_groups(0).group();
        QCOMPARE(back.SerializeAsString(), before);
    }

    void onALaterSlideTheGroupIsSplitThere()
    {
        rv::data::Presentation presentation = song();
        QCOMPARE(setGroupFrom(&presentation, "v2", bridge, ids()), Outcome::Split);
        // The slide and those after it in its group, and no others.
        QCOMPARE(groupsOf(presentation), "Verse 1[v1] Bridge[v2,v3] Chorus[c1,c2] [e1]");
        const rv::data::Group &old = presentation.cue_groups(0).group();
        const rv::data::Group &made = presentation.cue_groups(1).group();
        QCOMPARE(old.uuid().string(), std::string("VERSE"));
        QCOMPARE(old.application_group_identifier().string(), std::string("LIST-VERSE"));
        QCOMPARE(made.uuid().string(), std::string("NEW-1"));
        QCOMPARE(made.application_group_identifier().string(), std::string("LIST-BRIDGE"));
        QVERIFY(made.has_hotkey() && made.has_color() && made.color().alpha() == 1);
        // Every arrangement shows the slides it did, in the order it did: the new
        // group is after the old one wherever that comes up.
        QCOMPARE(arrangementOf(presentation, 0), "VERSE NEW-1 CHORUS END");
        QCOMPARE(arrangementOf(presentation, 1), "VERSE NEW-1 CHORUS VERSE NEW-1 CHORUS");
        // The rest is as it was.
        QCOMPARE(presentation.cues_size(), 6);
        QCOMPARE(presentation.ccli().song_title(), std::string("Kept"));
        QCOMPARE(presentation.selected_arrangement().string(), std::string("A1"));
        QCOMPARE(presentation.cue_groups(2).group().hotkey().code(), rv::data::KEY_CODE_ANSI_C);
    }

    void onTheLastSlideOfAGroupOnlyThatSlideGoes()
    {
        rv::data::Presentation presentation = song();
        QCOMPARE(setGroupFrom(&presentation, "c2", bridge, ids()), Outcome::Split);
        QCOMPARE(groupsOf(presentation), "Verse 1[v1,v2,v3] Chorus[c1] Bridge[c2] [e1]");
        QCOMPARE(arrangementOf(presentation, 1), "VERSE CHORUS NEW-1 VERSE CHORUS NEW-1");
    }

    void aSlideInAGroupOfThatNameAlreadyIsLeftAlone()
    {
        rv::data::Presentation presentation = song();
        const std::string before = presentation.SerializeAsString();
        // In the middle of its group, and at its start; whatever the list says the group is.
        QCOMPARE(setGroupFrom(&presentation, "v2", {QStringLiteral("Verse 1"), QStringLiteral("#ff0000"), QStringLiteral("ANOTHER")}, ids()), Outcome::Unchanged);
        QCOMPARE(setGroupFrom(&presentation, "c1", {QStringLiteral(" Chorus "), QString(), QString()}, ids()), Outcome::Unchanged);
        QCOMPARE(presentation.SerializeAsString(), before);
    }

    void aSlideInAGroupWithNoNameIsGivenOneTheSameWay()
    {
        rv::data::Presentation presentation = song();
        QCOMPARE(setGroupFrom(&presentation, "e1", bridge, ids()), Outcome::Whole);
        QCOMPARE(groupsOf(presentation), "Verse 1[v1,v2,v3] Chorus[c1,c2] Bridge[e1]");
        QCOMPARE(presentation.cue_groups(2).group().uuid().string(), std::string("END"));
    }

    void aGroupThatIsNotFromTheListHasNoIdOfTheListBesideIt()
    {
        // One of the app's own, where the workspace has no list: a name and a colour.
        rv::data::Presentation presentation = song();
        QCOMPARE(setGroupFrom(&presentation, "v1", {QStringLiteral("Tag"), QStringLiteral("#00ff00"), QString()}, ids()), Outcome::Whole);
        const rv::data::Group &group = presentation.cue_groups(0).group();
        QVERIFY(!group.has_application_group_identifier());
        QVERIFY(group.color().red() == 0 && group.color().green() == 1 && group.color().blue() == 0);
        // And one with no colour to give leaves none behind from the group that was.
        QCOMPARE(setGroupFrom(&presentation, "c1", {QStringLiteral("Plain"), QString(), QString()}, ids()), Outcome::Whole);
        QVERIFY(!presentation.cue_groups(1).group().has_color());
        QCOMPARE(presentation.cue_groups(1).group().hotkey().code(), rv::data::KEY_CODE_ANSI_C);
    }

    void aSlideThatIsInNoGroupIsNotFound()
    {
        rv::data::Presentation presentation = song();
        const std::string before = presentation.SerializeAsString();
        QCOMPARE(setGroupFrom(&presentation, "nowhere", bridge, ids()), Outcome::NoSuchSlide);
        QCOMPARE(presentation.SerializeAsString(), before);
    }
};

QTEST_APPLESS_MAIN(TestGroupEdit)
#include "tst_groupedit.moc"
