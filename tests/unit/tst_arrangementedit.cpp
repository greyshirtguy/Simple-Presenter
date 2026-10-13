// Making and changing a presentation's arrangements (src/arrangementedit.h), tried on a
// presentation built as ProPresenter writes one: groups with their ids and colours, two
// arrangements, one of them selected, and things this code has no business with.

#include "arrangementedit.h"

#include <QTest>

using namespace arrangementedit;

namespace {

// A song as ProPresenter might have left it: three groups, the last with no name, and
// two arrangements, one of which has the first two groups twice. And things nothing
// here is to touch, to see that they are still there afterwards.
rv::data::Presentation song()
{
    rv::data::Presentation presentation;
    presentation.set_name("Song");
    presentation.mutable_uuid()->set_string("SONG");
    presentation.mutable_ccli()->set_song_title("Kept");
    const auto group = [&presentation](const char *id, const char *name, std::initializer_list<const char *> cues) {
        rv::data::Presentation::CueGroup *entry = presentation.add_cue_groups();
        entry->mutable_group()->mutable_uuid()->set_string(id);
        entry->mutable_group()->set_name(name);
        entry->mutable_group()->mutable_hotkey();
        entry->mutable_group()->mutable_color()->set_blue(1);
        entry->mutable_group()->mutable_color()->set_alpha(1);
        for (const char *cue : cues) {
            entry->add_cue_identifiers()->set_string(cue);
            presentation.add_cues()->mutable_uuid()->set_string(cue);
        }
    };
    group("VERSE", "Verse 1", {"v1", "v2", "v3"});
    group("CHORUS", "Chorus", {"c1", "c2"});
    group("END", "", {"e1"});
    const auto arrangement = [&presentation](const char *id, const char *name, std::initializer_list<const char *> groups) {
        rv::data::Presentation::Arrangement *made = presentation.add_arrangements();
        made->mutable_uuid()->set_string(id);
        made->set_name(name);
        for (const char *group : groups)
            made->add_group_identifiers()->set_string(group);
    };
    arrangement("A1", "Sunday", {"VERSE", "CHORUS", "END"});
    arrangement("A2", "Twice", {"VERSE", "CHORUS", "VERSE", "CHORUS"});
    presentation.mutable_selected_arrangement()->set_string("A1");
    return presentation;
}

// An arrangement's groups, by id, with commas between
QString groupsOf(const rv::data::Presentation &presentation, const char *id)
{
    QStringList ids;
    for (const auto &arrangement : presentation.arrangements()) {
        if (arrangement.uuid().string() != id)
            continue;
        for (const auto &group : arrangement.group_identifiers())
            ids << QString::fromStdString(group.string());
    }
    return ids.join(',');
}

// The arrangements, as "id:name", in the file's order
QString listed(const rv::data::Presentation &presentation)
{
    QStringList all;
    for (const auto &arrangement : presentation.arrangements())
        all << QString::fromStdString(arrangement.uuid().string() + ":" + arrangement.name());
    return all.join(' ');
}

// The presentation with its arrangements and which is selected left out: what none of
// this is to change
std::string rest(rv::data::Presentation presentation)
{
    presentation.clear_arrangements();
    presentation.clear_selected_arrangement();
    return presentation.SerializeAsString();
}

// One arrangement of it, as the file would hold it
std::string whole(const rv::data::Presentation &presentation, const char *id)
{
    for (const auto &arrangement : presentation.arrangements()) {
        if (arrangement.uuid().string() == id)
            return arrangement.SerializeAsString();
    }
    return {};
}

}

class TestArrangementEdit : public QObject
{
    Q_OBJECT

private slots:
    void aNameIsMadeFreeWithANumber()
    {
        rv::data::Presentation presentation = song();
        QCOMPARE(freeName(presentation, "New Arrangement"), "New Arrangement");
        QCOMPARE(freeName(presentation, "Sunday"), "Sunday 2");
        presentation.add_arrangements()->set_name("Sunday 2");
        QCOMPARE(freeName(presentation, "Sunday"), "Sunday 3");
    }

    void aNewArrangementHasEveryGroupOnceInMastersOrder()
    {
        rv::data::Presentation presentation = song();
        const rv::data::Presentation before = presentation;
        QCOMPARE(add(&presentation, "New Arrangement", "NEW"), "New Arrangement");
        QCOMPARE(listed(presentation), "A1:Sunday A2:Twice NEW:New Arrangement");
        QCOMPARE(groupsOf(presentation, "NEW"), "VERSE,CHORUS,END");
        // The one that was selected still is: selecting the new one is for whoever asked.
        QCOMPARE(presentation.selected_arrangement().string(), "A1");
        // And everything else is as it was: the groups, the other arrangements.
        QCOMPARE(rest(presentation), rest(before));
        QCOMPARE(whole(presentation, "A1"), whole(before, "A1"));
        QCOMPARE(whole(presentation, "A2"), whole(before, "A2"));
        // A second of the same name is told apart by a number.
        QCOMPARE(add(&presentation, "New Arrangement", "NEWER"), "New Arrangement 2");
    }

    void aPresentationWithNoGroupsCanBeGivenOneAllTheSame()
    {
        rv::data::Presentation presentation;
        QCOMPARE(add(&presentation, "New Arrangement", "NEW"), "New Arrangement");
        QCOMPARE(presentation.arrangements_size(), 1);
        QCOMPARE(presentation.arrangements(0).group_identifiers_size(), 0);
    }

    void aCopyGoesStraightAfterWhatItIsACopyOf()
    {
        rv::data::Presentation presentation = song();
        const rv::data::Presentation before = presentation;
        QString made;
        QCOMPARE(duplicate(&presentation, "A1", "COPY", &made), Outcome::Done);
        QCOMPARE(made, "Sunday 2");
        QCOMPARE(listed(presentation), "A1:Sunday COPY:Sunday 2 A2:Twice");
        QCOMPARE(groupsOf(presentation, "COPY"), "VERSE,CHORUS,END");
        QCOMPARE(rest(presentation), rest(before));
        QCOMPARE(whole(presentation, "A1"), whole(before, "A1"));
        QCOMPARE(presentation.selected_arrangement().string(), "A1");
        // The last one's copy is the last.
        QCOMPARE(duplicate(&presentation, "A2", "COPY2", &made), Outcome::Done);
        QCOMPARE(listed(presentation), "A1:Sunday COPY:Sunday 2 A2:Twice COPY2:Twice 2");
        QCOMPARE(groupsOf(presentation, "COPY2"), "VERSE,CHORUS,VERSE,CHORUS");
        QCOMPARE(duplicate(&presentation, "NOT-THERE", "X", &made), Outcome::NoSuchArrangement);
    }

    void anArrangementIsRenamed()
    {
        rv::data::Presentation presentation = song();
        const rv::data::Presentation before = presentation;
        QCOMPARE(rename(&presentation, "A2", "  Evening  "), Outcome::Done);
        QCOMPARE(listed(presentation), "A1:Sunday A2:Evening");
        QCOMPARE(groupsOf(presentation, "A2"), "VERSE,CHORUS,VERSE,CHORUS");
        QCOMPARE(rest(presentation), rest(before));
        // The name it has, a name of nothing, and the name another has
        QCOMPARE(rename(&presentation, "A2", "Evening"), Outcome::Unchanged);
        QCOMPARE(rename(&presentation, "A2", "   "), Outcome::NoName);
        QCOMPARE(rename(&presentation, "A2", "Sunday"), Outcome::NameTaken);
        QCOMPARE(rename(&presentation, "NOT-THERE", "X"), Outcome::NoSuchArrangement);
        QCOMPARE(listed(presentation), "A1:Sunday A2:Evening");
    }

    void removingTheSelectedArrangementLeavesNoneSelected()
    {
        rv::data::Presentation presentation = song();
        const rv::data::Presentation before = presentation;
        QCOMPARE(remove(&presentation, "A1"), Outcome::Done);
        QCOMPARE(listed(presentation), "A2:Twice");
        QVERIFY(!presentation.has_selected_arrangement());
        QCOMPARE(rest(presentation), rest(before));
        QCOMPARE(whole(presentation, "A2"), whole(before, "A2"));
        QCOMPARE(remove(&presentation, "A1"), Outcome::NoSuchArrangement);
    }

    void removingAnotherLeavesTheSelectionAsItWas()
    {
        rv::data::Presentation presentation = song();
        QCOMPARE(remove(&presentation, "A2"), Outcome::Done);
        QCOMPARE(listed(presentation), "A1:Sunday");
        QCOMPARE(presentation.selected_arrangement().string(), "A1");
    }

    void aGroupIsPutInAtALanding()
    {
        rv::data::Presentation presentation = song();
        const rv::data::Presentation before = presentation;
        // Before the first, between two, and after the last
        QCOMPARE(insertGroup(&presentation, "A1", "END", 0), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A1"), "END,VERSE,CHORUS,END");
        QCOMPARE(insertGroup(&presentation, "A1", "CHORUS", 2), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A1"), "END,VERSE,CHORUS,CHORUS,END");
        QCOMPARE(insertGroup(&presentation, "A1", "VERSE", 5), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A1"), "END,VERSE,CHORUS,CHORUS,END,VERSE");
        // Nothing else is touched: the groups, the other arrangement, what is selected.
        QCOMPARE(rest(presentation), rest(before));
        QCOMPARE(whole(presentation, "A2"), whole(before, "A2"));
        QCOMPARE(presentation.selected_arrangement().string(), "A1");
        // What is not there
        QCOMPARE(insertGroup(&presentation, "A1", "NO-GROUP", 0), Outcome::NoSuchGroup);
        QCOMPARE(insertGroup(&presentation, "A1", "VERSE", 7), Outcome::NoSuchPlace);
        QCOMPARE(insertGroup(&presentation, "A1", "VERSE", -1), Outcome::NoSuchPlace);
        QCOMPARE(insertGroup(&presentation, "NOT-THERE", "VERSE", 0), Outcome::NoSuchArrangement);
        QCOMPARE(groupsOf(presentation, "A1"), "END,VERSE,CHORUS,CHORUS,END,VERSE");
    }

    void anArrangementWithNothingInItTakesItsFirstGroup()
    {
        rv::data::Presentation presentation = song();
        presentation.mutable_arrangements(0)->clear_group_identifiers();
        QCOMPARE(insertGroup(&presentation, "A1", "CHORUS", 0), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A1"), "CHORUS");
    }

    void aGroupIsMovedToALanding()
    {
        rv::data::Presentation presentation = song();
        const rv::data::Presentation before = presentation;
        // The first of "Twice" to after the last
        QCOMPARE(moveGroup(&presentation, "A2", 0, 4), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A2"), "CHORUS,VERSE,CHORUS,VERSE");
        // The last to before the first
        QCOMPARE(moveGroup(&presentation, "A2", 3, 0), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A2"), "VERSE,CHORUS,VERSE,CHORUS");
        // Back where it began, it is what it was to the byte.
        QCOMPARE(whole(presentation, "A2"), whole(before, "A2"));
        QCOMPARE(rest(presentation), rest(before));
        // Let go just before itself or just after: nothing changes.
        QCOMPARE(moveGroup(&presentation, "A2", 1, 1), Outcome::Unchanged);
        QCOMPARE(moveGroup(&presentation, "A2", 1, 2), Outcome::Unchanged);
        QCOMPARE(moveGroup(&presentation, "A2", 4, 0), Outcome::NoSuchPlace);
        QCOMPARE(moveGroup(&presentation, "A2", 0, 5), Outcome::NoSuchPlace);
        QCOMPARE(moveGroup(&presentation, "NOT-THERE", 0, 1), Outcome::NoSuchArrangement);
        QCOMPARE(whole(presentation, "A2"), whole(before, "A2"));
    }

    void everyMoveIsWhatTakingItOutAndPuttingItInWouldGive()
    {
        const QStringList ids = {"VERSE", "CHORUS", "END"};
        for (int count = 1; count <= 6; ++count) {
            for (int from = 0; from < count; ++from) {
                for (int landing = 0; landing <= count; ++landing) {
                    rv::data::Presentation presentation = song();
                    presentation.mutable_arrangements(0)->clear_group_identifiers();
                    QStringList wanted;
                    for (int i = 0; i < count; ++i) {
                        // (Told apart by where they began: the same group may come up again.)
                        wanted << ids.at(i % 3);
                        presentation.mutable_arrangements(0)->add_group_identifiers()->set_string(ids.at(i % 3).toStdString());
                    }
                    const QString was = wanted.join(',');
                    const QString moved = wanted.takeAt(from);
                    wanted.insert(landing > from ? landing - 1 : landing, moved);
                    const Outcome outcome = moveGroup(&presentation, "A1", from, landing);
                    QCOMPARE(groupsOf(presentation, "A1"), wanted.join(','));
                    QCOMPARE(outcome == Outcome::Unchanged, placed(count, from, landing) < 0);
                    if (outcome == Outcome::Unchanged)
                        QCOMPARE(groupsOf(presentation, "A1"), was);
                }
            }
        }
    }

    void oneComingUpOfAGroupIsTakenOut()
    {
        rv::data::Presentation presentation = song();
        const rv::data::Presentation before = presentation;
        // "Twice" has the verse at 0 and at 2: the second is taken, and the first stays.
        QCOMPARE(removeGroup(&presentation, "A2", 2), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A2"), "VERSE,CHORUS,CHORUS");
        // The group is still the presentation's, with its slides.
        QCOMPARE(rest(presentation), rest(before));
        QCOMPARE(presentation.cue_groups(0).cue_identifiers_size(), 3);
        QCOMPARE(removeGroup(&presentation, "A2", 3), Outcome::NoSuchPlace);
        QCOMPARE(removeGroup(&presentation, "A2", -1), Outcome::NoSuchPlace);
        QCOMPARE(removeGroup(&presentation, "NOT-THERE", 0), Outcome::NoSuchArrangement);
        // Down to nothing: an arrangement may have none.
        for (int i = 0; i < 3; ++i)
            QCOMPARE(removeGroup(&presentation, "A2", 0), Outcome::Done);
        QCOMPARE(groupsOf(presentation, "A2"), "");
        QCOMPARE(listed(presentation), "A1:Sunday A2:Twice");
    }

    void whereAThingLetGoAtALandingIs()
    {
        QCOMPARE(placed(4, 0, 4), 3);
        QCOMPARE(placed(4, 3, 0), 0);
        QCOMPARE(placed(4, 1, 3), 2);
        QCOMPARE(placed(4, 2, 1), 1);
        QCOMPARE(placed(4, 1, 1), -1);
        QCOMPARE(placed(4, 1, 2), -1);
        QCOMPARE(placed(1, 0, 0), -1);
        QCOMPARE(placed(1, 0, 1), -1);
        QCOMPARE(placed(0, 0, 0), -1);
        QCOMPARE(placed(4, 4, 0), -1);
        QCOMPARE(placed(4, 0, 5), -1);
    }
};

QTEST_APPLESS_MAIN(TestArrangementEdit)
#include "tst_arrangementedit.moc"
