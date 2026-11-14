// The timeline (src/timeline.h): a presentation's timeline read as ProPresenter 22
// writes one, the rules of what plays when, and its changes, tried on such a
// presentation with things in it this app does not know of, to see they are kept.

#include "timeline.h"

#include "presentation.pb.h"

#include <google/protobuf/unknown_field_set.h>

#include <QTest>

using rv::data::Action;
using rv::data::Presentation;
using TimelineCue = rv::data::Presentation::Timeline::Cue;

namespace {

// Six slides in three groups, as the maintainer's example has them (two in Verse 1,
// three in Verse 2, and one in a group of no name, which is the sixth); the fifth has no
// colour
Presentation sample()
{
    Presentation presentation;
    const char *ids[] = {"A", "B", "C", "D", "E", "F"};
    for (const char *id : ids)
        presentation.add_cues()->mutable_uuid()->set_string(id);
    const auto group = [&](const char *name, std::initializer_list<const char *> cues, bool coloured) {
        auto *made = presentation.add_cue_groups();
        made->mutable_group()->set_name(name);
        if (coloured) {
            made->mutable_group()->mutable_color()->set_red(0.0f);
            made->mutable_group()->mutable_color()->set_green(0.5f);
            made->mutable_group()->mutable_color()->set_blue(1.0f);
            made->mutable_group()->mutable_color()->set_alpha(1.0f);
        }
        for (const char *cue : cues)
            made->add_cue_identifiers()->set_string(cue);
    };
    group("Verse 1", {"A", "B"}, true);
    group("Verse 2", {"C", "D", "E"}, true);
    group("", {"F"}, false);
    return presentation;
}

// A piece of audio as an action, as ProPresenter's timeline had it: its media element
// with an audio part, its length as the action's duration, or (with `duration` 0) only
// as the end point of its transport
Action audioAction(double duration, double endPoint)
{
    Action action;
    action.mutable_uuid()->set_string("AUDIO");
    action.set_type(Action::ACTION_TYPE_MEDIA);
    if (duration > 0)
        action.set_duration(duration);
    auto *element = action.mutable_media()->mutable_element();
    element->mutable_url()->set_absolute_string("file:///Media/Assets/New%20City.mp3");
    element->mutable_audio()->mutable_transport()->set_end_point(endPoint);
    return action;
}

TimelineCue *slideCue(Presentation::Timeline *timeline, double time, const char *id, const char *name, bool oldList = false)
{
    TimelineCue *cue = oldList ? timeline->add_cues() : timeline->add_cues_v2();
    if (time > 0)
        cue->set_trigger_time(time);
    cue->mutable_cue_id()->set_string(id);
    cue->set_name(name);
    return cue;
}

// The maintainer's example, in short: the sixth slide at 0, the first twice, the
// second, the third, the fourth; and the audio at 0, put on last
Presentation withTimeline()
{
    Presentation presentation = sample();
    auto *timeline = presentation.mutable_timeline();
    timeline->set_duration(300);
    slideCue(timeline, 0, "F", "Cue 6");
    slideCue(timeline, 4.9, "A", "Cue 1");
    slideCue(timeline, 12.3, "A", "Cue 1");
    slideCue(timeline, 19.5, "B", "Cue 2");
    slideCue(timeline, 25.1, "C", "Cue 3");
    slideCue(timeline, 34.0, "D", "Cue 4");
    auto *audio = timeline->add_cues_v2();
    audio->set_name("New City.mp3");
    *audio->mutable_action() = audioAction(229.0, 229.0);
    return presentation;
}

// Says what kind an action is, as actions::describe would for these: audio, media, or a
// macro for anything else
QVariantMap describeAction(const Action &action)
{
    if (action.has_media() && action.media().element().has_audio())
        return {{"kind", "audio"}, {"audio", QVariantMap {{"id", QString::fromStdString(action.uuid().string())}}}};
    if (action.has_media())
        return {{"kind", "media"}};
    return {{"kind", "macro"}, {"label", "Opening"}};
}

QVariantMap cue(double time, const char *track, double length = 0)
{
    return {{"time", time}, {"track", QString::fromLatin1(track)}, {"length", length}};
}

// Something ProPresenter wrote that this app knows nothing of: an unknown field, put in
// by hand, as a newer version's fields would be
void addUnknown(google::protobuf::Message *message, int number, const std::string &value)
{
    message->GetReflection()->MutableUnknownFields(message)->AddLengthDelimited(number, value);
}

bool hasUnknown(const google::protobuf::Message &message, int number)
{
    const auto &fields = message.GetReflection()->GetUnknownFields(message);
    for (int i = 0; i < fields.field_count(); ++i) {
        if (fields.field(i).number() == number)
            return true;
    }
    return false;
}

} // namespace

class TimelineTest : public QObject
{
    Q_OBJECT

private slots:
    void readsTheNewList()
    {
        const QVariantMap described = timeline::describe(withTimeline(), describeAction);
        QCOMPARE(described.value("duration").toDouble(), 300.0);
        QCOMPARE(described.value("loop").toBool(), false);
        QCOMPARE(described.value("oldList").toBool(), false);
        const QVariantList cues = described.value("cues").toList();
        QCOMPARE(cues.size(), 7);
        const QVariantMap first = cues.at(0).toMap();
        QCOMPARE(first.value("place").toInt(), 0);
        QCOMPARE(first.value("track").toString(), QStringLiteral("slide"));
        QCOMPARE(first.value("slideId").toString(), QStringLiteral("F"));
        QCOMPARE(first.value("time").toDouble(), 0.0);
        // (A group of no name, and of no colour)
        QCOMPARE(first.value("group").toString(), QString());
        QCOMPARE(first.value("groupColor").toString(), QString());
        const QVariantMap second = cues.at(1).toMap();
        QCOMPARE(second.value("time").toDouble(), 4.9);
        QCOMPARE(second.value("name").toString(), QStringLiteral("Cue 1"));
        QCOMPARE(second.value("group").toString(), QStringLiteral("Verse 1"));
        QCOMPARE(second.value("groupColor").toString(), QStringLiteral("#0080ff"));
        const QVariantMap audio = cues.at(6).toMap();
        QCOMPARE(audio.value("track").toString(), QStringLiteral("audio"));
        QCOMPARE(audio.value("length").toDouble(), 229.0);
        QCOMPARE(audio.value("name").toString(), QStringLiteral("New City.mp3"));
        QCOMPARE(audio.value("action").toMap().value("kind").toString(), QStringLiteral("audio"));
    }

    // Every slide's name as ProPresenter names one: by its place through the groups as
    // they are stored, from 1
    void namesSlidesByTheirPlaceInTheGroups()
    {
        const QVariantMap slides = timeline::describe(sample(), describeAction).value("slides").toMap();
        QCOMPARE(slides.size(), 6);
        QCOMPARE(slides.value("A").toMap().value("name").toString(), QStringLiteral("Cue 1"));
        QCOMPARE(slides.value("E").toMap().value("name").toString(), QStringLiteral("Cue 5"));
        QCOMPARE(slides.value("F").toMap().value("name").toString(), QStringLiteral("Cue 6"));
        QCOMPARE(slides.value("C").toMap().value("group").toString(), QStringLiteral("Verse 2"));
    }

    void readsTheOldListWhereTheNewHasNothing()
    {
        Presentation presentation = sample();
        slideCue(presentation.mutable_timeline(), 2, "B", "Cue 2", true);
        QVariantMap described = timeline::describe(presentation, describeAction);
        QCOMPARE(described.value("oldList").toBool(), true);
        QCOMPARE(described.value("cues").toList().size(), 1);
        QCOMPARE(described.value("cues").toList().at(0).toMap().value("slideId").toString(), QStringLiteral("B"));
        // (A timeline with nothing in it, of no length, is ProPresenter's 300 seconds)
        QCOMPARE(described.value("duration").toDouble(), 300.0);

        // The new list, where it has anything, is the one read
        slideCue(presentation.mutable_timeline(), 3, "C", "Cue 3");
        described = timeline::describe(presentation, describeAction);
        QCOMPARE(described.value("oldList").toBool(), false);
        QCOMPARE(described.value("cues").toList().size(), 1);
        QCOMPARE(described.value("cues").toList().at(0).toMap().value("slideId").toString(), QStringLiteral("C"));
    }

    // Takes its length from what the media says, where the action says nothing
    void aPieceWithNoDurationIsAsLongAsItsMediaSays()
    {
        Presentation presentation = sample();
        auto *audio = presentation.mutable_timeline()->add_cues_v2();
        *audio->mutable_action() = audioAction(0, 61.5);
        QCOMPARE(timeline::describe(presentation, describeAction).value("cues").toList().at(0).toMap().value("length").toDouble(), 61.5);
    }

    // What a playhead passing from one time to another triggers: after the first, up to
    // and with the second, by time, and at one time in the file's order
    void passesByTimeAndThenByPlace()
    {
        const QVariantList cues {cue(5, "slide"), cue(1, "action"), cue(5, "action"), cue(3, "slide"), cue(1, "slide")};
        QCOMPARE(timeline::passed(cues, 0, 5), (QList<int> {1, 4, 3, 0, 2}));
        QCOMPARE(timeline::passed(cues, 1, 4), (QList<int> {3}));
        QCOMPARE(timeline::passed(cues, 3, 3), QList<int>());
        QCOMPARE(timeline::passed(cues, -1, 1), (QList<int> {1, 4}));
    }

    // Started part way: the slide before, the audio and media under the playhead (and
    // how far into each), and the actions at exactly that time
    void startsPartWayAsItWouldHaveBeen()
    {
        const QVariantList cues {cue(0, "slide"), cue(0, "audio", 229), cue(10, "slide"), cue(12, "action"), cue(15, "media", 30),
                                 cue(20, "action"), cue(20, "slide"), cue(40, "audio", 5)};
        using timeline::Entry;
        QCOMPARE(timeline::entering(cues, 0), (QList<Entry> {{0, 0}, {1, 0}}));
        QCOMPARE(timeline::entering(cues, 14), (QList<Entry> {{1, 14}, {2, 0}}));
        QCOMPARE(timeline::entering(cues, 20), (QList<Entry> {{1, 20}, {4, 5}, {5, 0}, {6, 0}}));
        // (The audio at 40 lasts five seconds: at 46 it has ended, and the media too)
        QCOMPARE(timeline::entering(cues, 46), (QList<Entry> {{1, 46}, {6, 0}}));
        // Before anything: nothing
        QCOMPARE(timeline::entering(QVariantList {cue(3, "slide")}, 1), QList<Entry>());
        // Of two slides at one time, the later in the file
        QCOMPARE(timeline::entering(QVariantList {cue(2, "slide"), cue(2, "slide")}, 5), (QList<Entry> {{1, 0}}));
        // A piece of no known length only where it begins
        QCOMPARE(timeline::entering(QVariantList {cue(2, "audio", 0)}, 2), (QList<Entry> {{0, 0}}));
        QCOMPARE(timeline::entering(QVariantList {cue(2, "audio", 0)}, 3), QList<Entry>());
    }

    void findsTheNextTime()
    {
        const QVariantList cues {cue(5, "slide"), cue(1, "action"), cue(3, "slide")};
        QCOMPARE(timeline::nextTime(cues, -1), 1.0);
        QCOMPARE(timeline::nextTime(cues, 1), 3.0);
        QCOMPARE(timeline::nextTime(cues, 4.5), 5.0);
        QCOMPARE(timeline::nextTime(cues, 5), -1.0);
    }

    // A slide that comes up more than once is taken at the first place after the slide
    // the show is at, round to the first
    void takesASlideAtItsNextPlace()
    {
        const QStringList ids {"F", "A", "B", "C", "A", "B"};
        QCOMPARE(timeline::slidePlace(ids, "A", -1), 1);
        QCOMPARE(timeline::slidePlace(ids, "A", 1), 4);
        QCOMPARE(timeline::slidePlace(ids, "A", 3), 4);
        QCOMPARE(timeline::slidePlace(ids, "A", 4), 1);
        QCOMPARE(timeline::slidePlace(ids, "F", 5), 0);
        QCOMPARE(timeline::slidePlace(ids, "Z", -1), -1);
    }

    // A slide put on goes after the cues there are, named as ProPresenter names it, in
    // the new list; and nothing else is touched
    void addsASlideAfterTheRest()
    {
        Presentation presentation = withTimeline();
        addUnknown(presentation.mutable_timeline()->mutable_cues_v2(6)->mutable_action()->mutable_media()->mutable_element(), 10, "asset");
        addUnknown(presentation.mutable_timeline(), 40, "newer");
        const Presentation before = presentation;
        QVERIFY(timeline::addSlide(&presentation, 40.5, "E"));
        const auto &cues = presentation.timeline().cues_v2();
        QCOMPARE(cues.size(), 8);
        QCOMPARE(cues.at(7).trigger_time(), 40.5);
        QCOMPARE(cues.at(7).cue_id().string(), std::string("E"));
        QCOMPARE(cues.at(7).name(), std::string("Cue 5"));
        // (What was there is as it was, the unknown fields too)
        for (int i = 0; i < 7; ++i)
            QCOMPARE(cues.at(i).SerializeAsString(), before.timeline().cues_v2(i).SerializeAsString());
        QVERIFY(hasUnknown(presentation.timeline(), 40));
        QVERIFY(hasUnknown(cues.at(6).action().media().element(), 10));
        QCOMPARE(presentation.timeline().duration(), 300.0);
        // A slide the presentation has not is not put on
        QVERIFY(!timeline::addSlide(&presentation, 1, "Z"));
        QCOMPARE(presentation.timeline().cues_v2_size(), 8);
        // A slide at 0 has no time in the file, as ProPresenter writes one
        QVERIFY(timeline::addSlide(&presentation, 0, "A"));
        QVERIFY(presentation.timeline().cues_v2(8).trigger_time() == 0);
    }

    // A timeline in the old list is copied to the new as it is first changed, and the
    // old one is left as it was
    void changesAnOldListInTheNew()
    {
        Presentation presentation = sample();
        slideCue(presentation.mutable_timeline(), 2, "B", "Cue 2", true);
        slideCue(presentation.mutable_timeline(), 6, "C", "Cue 3", true);
        const std::string old = presentation.timeline().cues(0).SerializeAsString() + presentation.timeline().cues(1).SerializeAsString();
        QVERIFY(timeline::move(&presentation, 1, 7.5));
        QCOMPARE(presentation.timeline().cues_size(), 2);
        QCOMPARE(presentation.timeline().cues(0).SerializeAsString() + presentation.timeline().cues(1).SerializeAsString(), old);
        QCOMPARE(presentation.timeline().cues_v2_size(), 2);
        QCOMPARE(presentation.timeline().cues_v2(0).cue_id().string(), std::string("B"));
        QCOMPARE(presentation.timeline().cues_v2(1).trigger_time(), 7.5);
        QCOMPARE(timeline::describe(presentation, describeAction).value("oldList").toBool(), false);

        // Taking off the last of the new list empties the old one too, which would
        // otherwise be read again
        QVERIFY(timeline::remove(&presentation, 1));
        QCOMPARE(presentation.timeline().cues_size(), 2);
        QVERIFY(timeline::remove(&presentation, 0));
        QCOMPARE(presentation.timeline().cues_size(), 0);
        QCOMPARE(presentation.timeline().cues_v2_size(), 0);
        QCOMPARE(timeline::describe(presentation, describeAction).value("cues").toList().size(), 0);
    }

    void movesAndTakesOff()
    {
        Presentation presentation = withTimeline();
        QVERIFY(!timeline::move(&presentation, 1, 4.9));
        QVERIFY(timeline::move(&presentation, 1, 6));
        QCOMPARE(presentation.timeline().cues_v2(1).trigger_time(), 6.0);
        // To before the start is to the start, and leaves no time in the file
        QVERIFY(timeline::move(&presentation, 1, -3));
        QVERIFY(presentation.timeline().cues_v2(1).trigger_time() == 0);
        QVERIFY(!timeline::move(&presentation, 9, 1));
        QVERIFY(timeline::remove(&presentation, 0));
        QCOMPARE(presentation.timeline().cues_v2_size(), 6);
        QCOMPARE(presentation.timeline().cues_v2(0).cue_id().string(), std::string("A"));
        QVERIFY(!timeline::remove(&presentation, 6));
        // A presentation with no timeline has nothing to move or take off
        Presentation none = sample();
        QVERIFY(!timeline::move(&none, 0, 1));
        QVERIFY(!timeline::remove(&none, 0));
        QVERIFY(!none.has_timeline());
    }

    // An action put on is as made, with its length from its media where it has none
    void addsAnAction()
    {
        Presentation presentation = sample();
        timeline::addAction(&presentation, 2.25, audioAction(0, 42), "New City.mp3");
        QVERIFY(presentation.has_timeline());
        // (A presentation that had none is given ProPresenter's length)
        QCOMPARE(presentation.timeline().duration(), 300.0);
        const TimelineCue &cue = presentation.timeline().cues_v2(0);
        QCOMPARE(cue.trigger_time(), 2.25);
        QCOMPARE(cue.name(), std::string("New City.mp3"));
        QCOMPARE(cue.action().uuid().string(), std::string("AUDIO"));
        QCOMPARE(cue.action().duration(), 42.0);
    }

    void setsItsLengthAndLoop()
    {
        Presentation presentation = sample();
        // (300 is what a presentation with none has: nothing to change, nothing made)
        QVERIFY(!timeline::setLoop(&presentation, false));
        QVERIFY(!presentation.has_timeline());
        QVERIFY(timeline::setDuration(&presentation, 300));
        QCOMPARE(presentation.timeline().duration(), 300.0);
        QVERIFY(!timeline::setDuration(&presentation, 300));
        QVERIFY(timeline::setDuration(&presentation, 0.2));
        QCOMPARE(presentation.timeline().duration(), 1.0);
        QVERIFY(timeline::setLoop(&presentation, true));
        QVERIFY(presentation.timeline().loop());
        QVERIFY(!timeline::setLoop(&presentation, true));
    }
};

QTEST_GUILESS_MAIN(TimelineTest)
#include "tst_timeline.moc"
