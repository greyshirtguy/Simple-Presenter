// A presentation's slides, groups and arrangements, and what is on a slide, changed as
// the extended API asks (src/presentationedit.h, src/slideedit.h), on a presentation
// built as ProPresenter writes one, with fields it knows nothing of on the
// presentation, on a cue and on an element: slides added in each place, described by
// their words or by their elements; a slide's words, label, notes and group changed; a
// slide moved and removed; a group renamed, recoloured, moved and removed; an
// arrangement made and changed; a presentation made from a description; an element
// added, changed and taken off. And after each, that what the change was not about is as
// it was, to the byte.

#include "presentationedit.h"

#include "extendedapijson.h"
#include "labelfile.h"
#include "proconvert.h"
#include "slideedit.h"

#include <QGuiApplication>
#include <QTest>

#include <google/protobuf/unknown_field_set.h>

using namespace presentationedit;

namespace {

void addUnknown(google::protobuf::Message *message, int number, const std::string &value)
{
    message->GetReflection()->MutableUnknownFields(message)->AddLengthDelimited(number, value);
}

int unknownCount(const google::protobuf::Message &message)
{
    return message.GetReflection()->GetUnknownFields(message).field_count();
}

// The workspace's own groups, as GroupKeys gives them
const QVariantList kGroups {QVariantMap {{"id", "LIST-VERSE-1"}, {"name", "Verse 1"}, {"color", "#0000ff"}},
                            QVariantMap {{"id", "LIST-CHORUS"}, {"name", "Chorus"}, {"color", "#ff0000"}},
                            QVariantMap {{"id", "LIST-BRIDGE"}, {"name", "Bridge"}, {"color", "#00ff00"}}};
const Context kContext {kGroups, QSizeF(1920, 1080)};

rv::data::Presentation::CueGroup *group(rv::data::Presentation *presentation, const std::string &id, const std::string &name)
{
    auto *made = presentation->add_cue_groups();
    made->mutable_group()->mutable_uuid()->set_string(id);
    made->mutable_group()->set_name(name);
    made->mutable_group()->mutable_hotkey();
    return made;
}

rv::data::Cue *wordsCue(rv::data::Presentation *presentation, rv::data::Presentation::CueGroup *in, const std::string &id, const QString &words)
{
    rv::data::Cue *cue = proconvert::addBlankCue(presentation, std::string(), QSizeF(1920, 1080));
    cue->mutable_uuid()->set_string(id);
    in->add_cue_identifiers()->set_string(id);
    if (!words.isEmpty())
        slideedit::setWords(slideOf(cue)->mutable_base_slide(), words, nullptr);
    return cue;
}

// A song as ProPresenter might have left one: two verses' slides and a chorus, an
// arrangement that is selected, and things of a ProPresenter to come here and there
rv::data::Presentation song()
{
    rv::data::Presentation presentation = proconvert::newPresentation(QStringLiteral("Song"));
    addUnknown(&presentation, 900, "of a ProPresenter to come");
    auto *verse = group(&presentation, "G-VERSE", "Verse 1");
    rv::data::Cue *first = wordsCue(&presentation, verse, "CUE-1", QStringLiteral("Amazing grace\nhow sweet the sound"));
    addUnknown(first, 901, "on a cue");
    addUnknown(slideOf(first)->mutable_base_slide()->mutable_elements(0), 902, "on an element");
    // (The groups' entries are found again by index: adding to the list may move them)
    wordsCue(&presentation, presentation.mutable_cue_groups(0), "CUE-2", QStringLiteral("that saved a wretch\nlike me"));
    group(&presentation, "G-CHORUS", "Chorus");
    wordsCue(&presentation, presentation.mutable_cue_groups(1), "CUE-3", QStringLiteral("My chains are gone"));
    auto *arrangement = presentation.add_arrangements();
    arrangement->mutable_uuid()->set_string("ARR-SHORT");
    arrangement->set_name("Short");
    for (const char *id : {"G-VERSE", "G-CHORUS", "G-VERSE"})
        arrangement->add_group_identifiers()->set_string(id);
    presentation.mutable_selected_arrangement()->set_string("ARR-SHORT");
    return presentation;
}

std::string cueBytes(const rv::data::Presentation &presentation, const char *id)
{
    for (const rv::data::Cue &cue : presentation.cues()) {
        if (cue.uuid().string() == id)
            return cue.SerializeAsString();
    }
    return {};
}

QString wordsOf(const rv::data::Presentation &presentation, const QString &cueId)
{
    rv::data::Presentation copy = presentation;
    rv::data::Cue *found = cue(&copy, cueId);
    if (!found || !slideOf(found))
        return QStringLiteral("(no slide)");
    const rv::data::Slide &slide = slideOf(found)->base_slide();
    const int box = slideedit::wordsBox(slide);
    return box < 0 ? QString() : proconvert::readText(slide.elements(box).element().text()).plainText();
}

rv::data::Slide *firstSlide(rv::data::Presentation *presentation)
{
    return slideOf(cue(presentation, QStringLiteral("CUE-1")))->mutable_base_slide();
}

extendedapijson::ElementChanges asked(const QVariantMap &api, const rv::data::Slide *slide = nullptr, int index = -1)
{
    return extendedapijson::elementChanges(api, slide && index >= 0 ? proconvert::elementMap(slide->elements(index)) : QVariantMap());
}

}

class TestPresentationEdit : public QObject
{
    Q_OBJECT

private slots:
    // ---- finding

    void slidesAreNumberedInThePresentationsOwnOrder()
    {
        const rv::data::Presentation presentation = song();
        QCOMPARE(cueOrder(presentation), QStringList({"CUE-1", "CUE-2", "CUE-3"}));
        QCOMPARE(groupCues(presentation), QList<QStringList>({{"CUE-1", "CUE-2"}, {"CUE-3"}}));
        QCOMPARE(cueId(presentation, QStringLiteral("CUE-2")), QStringLiteral("CUE-2"));
        QCOMPARE(cueId(presentation, QStringLiteral("2")), QStringLiteral("CUE-3"));
        QCOMPARE(cueId(presentation, QStringLiteral("3")), QString());
        QCOMPARE(cueId(presentation, QStringLiteral("no")), QString());
        QCOMPARE(groupIndex(presentation, QStringLiteral("G-CHORUS")), 1);
        QCOMPARE(groupIndex(presentation, QStringLiteral("chorus")), 1);
        QCOMPARE(groupIndex(presentation, QStringLiteral("0")), 0);
        QCOMPARE(groupIndex(presentation, QStringLiteral("Bridge")), -1);
        QCOMPARE(arrangementIndex(presentation, QStringLiteral("Short")), 0);
        QCOMPARE(arrangementIndex(presentation, QStringLiteral("ARR-SHORT")), 0);
        QCOMPARE(arrangementIndex(presentation, QStringLiteral("Long")), -1);
    }

    // ---- slides added

    void aSlideOfWordsGoesAfterASlideAndLooksAsItsNeighbourDoes()
    {
        rv::data::Presentation presentation = song();
        const std::string first = cueBytes(presentation, "CUE-1");
        const std::string third = cueBytes(presentation, "CUE-3");
        QStringList made;
        QVERIFY(addSlides(&presentation, {QStringLiteral("was blind\nbut now I see")}, {QStringLiteral("CUE-1"), {}, {}}, kContext, &made).isEmpty());
        QCOMPARE(made.size(), 1);
        // Straight after it, in its group
        QCOMPARE(cueOrder(presentation), QStringList({"CUE-1", made.first(), "CUE-2", "CUE-3"}));
        QCOMPARE(groupCues(presentation).at(0), QStringList({"CUE-1", made.first(), "CUE-2"}));
        QCOMPARE(wordsOf(presentation, made.first()), QStringLiteral("was blind\nbut now I see"));
        // Its text box is its neighbour's over again, as itself: the same name and place, an id of its own
        const rv::data::Slide &like = slideOf(cue(&presentation, QStringLiteral("CUE-1")))->base_slide();
        const rv::data::Slide &slide = slideOf(cue(&presentation, made.first()))->base_slide();
        QCOMPARE(slide.elements_size(), 1);
        QCOMPARE(slide.elements(0).element().name(), like.elements(0).element().name());
        QCOMPARE(slide.elements(0).element().bounds().SerializeAsString(), like.elements(0).element().bounds().SerializeAsString());
        QVERIFY(slide.elements(0).element().uuid().string() != like.elements(0).element().uuid().string());
        QCOMPARE(slide.size().width(), 1920.0);
        // The others are as they were, to the byte, and so is what this knows nothing of
        QCOMPARE(cueBytes(presentation, "CUE-1"), first);
        QCOMPARE(cueBytes(presentation, "CUE-3"), third);
        QCOMPARE(unknownCount(presentation), 1);
        // And the arrangement that was there goes through the same groups
        QCOMPARE(presentation.arrangements(0).group_identifiers_size(), 3);
    }

    void slidesGoBeforeASlideAtTheEndOfAGroupOrAtTheEnd()
    {
        rv::data::Presentation presentation = song();
        QStringList made;
        QVERIFY(addSlides(&presentation, {QStringLiteral("a"), QStringLiteral("b")}, {{}, QStringLiteral("CUE-2"), {}}, kContext, &made).isEmpty());
        QCOMPARE(cueOrder(presentation), QStringList({"CUE-1", made.at(0), made.at(1), "CUE-2", "CUE-3"}));

        presentation = song();
        QVERIFY(addSlides(&presentation, {QStringLiteral("a")}, {{}, {}, QStringLiteral("G-VERSE")}, kContext, &made).isEmpty());
        QCOMPARE(groupCues(presentation), QList<QStringList>({{"CUE-1", "CUE-2", made.first()}, {"CUE-3"}}));

        presentation = song();
        QVERIFY(addSlides(&presentation, {QStringLiteral("a")}, {}, kContext, &made).isEmpty());
        QCOMPARE(groupCues(presentation), QList<QStringList>({{"CUE-1", "CUE-2"}, {"CUE-3", made.first()}}));

        // A place that is not there: nothing is added
        presentation = song();
        const std::string before = presentation.SerializeAsString();
        QVERIFY(!addSlides(&presentation, {QStringLiteral("a")}, {QStringLiteral("NOT-THERE"), {}, {}}, kContext, &made).isEmpty());
        QVERIFY(!addSlides(&presentation, {QStringLiteral("a")}, {{}, {}, QStringLiteral("NO-GROUP")}, kContext, &made).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);
    }

    void aSlideIsMadeAsItIsDescribed()
    {
        rv::data::Presentation presentation = song();
        QStringList made;
        const QVariantMap described {{"label", "Bridge 1"}, {"label_color", "#ff0000"}, {"notes", "Slow down"}, {"enabled", false}, {"background", "#102030"},
                                     {"elements", QVariantList {QVariantMap {{"kind", "rectangle"}, {"name", "Bar"}, {"x", 0}, {"y", 900}, {"width", 1920}, {"height", 180}},
                                                                QVariantMap {{"name", "Lyrics"}, {"text", "Words"}, {"format", QVariantMap {{"size", 60}}}}}}};
        QVERIFY(addSlides(&presentation, {described}, {}, kContext, &made).isEmpty());
        rv::data::Cue *added = cue(&presentation, made.first());
        QCOMPARE(labelfile::of(*added).text, QStringLiteral("Bridge 1"));
        QCOMPARE(labelfile::of(*added).color, QStringLiteral("#ff0000"));
        QCOMPARE(added->isenabled(), false);
        QCOMPARE(proconvert::readNotes(*slideOf(added)).plainText(), QStringLiteral("Slow down"));
        const rv::data::Slide &slide = slideOf(added)->base_slide();
        QVERIFY(slide.draws_background_color());
        QCOMPARE(proconvert::toColor(slide.background_color()), QColor(0x10, 0x20, 0x30));
        // Its elements, in the order given, the first furthest back
        QCOMPARE(slide.elements_size(), 2);
        QCOMPARE(slide.elements(0).element().name(), std::string("Bar"));
        QCOMPARE(slide.elements(0).element().bounds().origin().y(), 900.0);
        QCOMPARE(slide.elements(1).element().name(), std::string("Lyrics"));
        const RichText text = proconvert::readText(slide.elements(1).element().text());
        QCOMPARE(text.plainText(), QStringLiteral("Words"));
        QCOMPARE(text.firstRun().size, 60.0);

        // What cannot be made is not half made
        const std::string before = presentation.SerializeAsString();
        QVERIFY(!addSlides(&presentation, {QVariantMap {{"elements", QVariantList {QVariantMap {{"kind", "hexagon"}}}}}}, {}, kContext, &made).isEmpty());
        QVERIFY(!addSlides(&presentation, {QStringLiteral("fine"), QVariantMap {{"background", "nearly black"}}}, {}, kContext, &made).isEmpty());
        QVERIFY(!addSlides(&presentation, {QVariant(12)}, {}, kContext, &made).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);
    }

    void theFirstWordsOfAPresentationGetABoxOverTheSlide()
    {
        // Nothing to look like: a box named for a song's words, over the slide but for a
        // margin, its words centred in white, as the song imports make
        rv::data::Presentation presentation = proconvert::newPresentation(QStringLiteral("New"));
        QStringList made;
        QVERIFY(addSlides(&presentation, {QStringLiteral("First words")}, {}, kContext, &made).isEmpty());
        QCOMPARE(presentation.cue_groups_size(), 1);
        const rv::data::Slide &slide = slideOf(cue(&presentation, made.first()))->base_slide();
        QCOMPARE(slide.elements(0).element().name(), std::string("Lyrics"));
        QCOMPARE(slide.elements(0).element().bounds().origin().x(), 96.0);
        QCOMPARE(slide.elements(0).element().bounds().size().width(), 1728.0);
        const RichText text = proconvert::readText(slide.elements(0).element().text());
        QCOMPARE(text.plainText(), QStringLiteral("First words"));
        QCOMPARE(text.paragraphs.first().alignment, Qt::AlignHCenter);
        QCOMPARE(text.firstRun().size, 84.0);
        QCOMPARE(text.firstRun().fill, QColor(Qt::white));
    }

    // ---- a slide changed

    void aSlidesWordsAreTypedOverKeepingHowTheyAreSet()
    {
        rv::data::Presentation presentation = song();
        // (Its words made bold first, to see that they stay so)
        rv::data::Slide *slide = firstSlide(&presentation);
        QVERIFY(slideedit::change(slide, 0, asked({{"format", QVariantMap {{"bold", true}, {"size", 72}}}}, slide, 0)).isEmpty());
        const std::string second = cueBytes(presentation, "CUE-2");

        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-1"), {{"text", "Amazing grace\nhow sweet the song"}}, kContext).isEmpty());
        QCOMPARE(wordsOf(presentation, QStringLiteral("CUE-1")), QStringLiteral("Amazing grace\nhow sweet the song"));
        const RichText text = proconvert::readText(firstSlide(&presentation)->elements(0).element().text());
        QCOMPARE(text.firstRun().bold, true);
        QCOMPARE(text.firstRun().size, 72.0);
        // What was on the element and the cue from a ProPresenter to come is still there
        QCOMPARE(unknownCount(firstSlide(&presentation)->elements(0)), 1);
        QCOMPARE(unknownCount(*cue(&presentation, QStringLiteral("CUE-1"))), 1);
        QCOMPARE(cueBytes(presentation, "CUE-2"), second);

        // The same words: nothing is written
        const std::string before = presentation.SerializeAsString();
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-1"), {{"text", "Amazing grace\nhow sweet the song"}}, kContext).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);
    }

    void aSlidesLabelNotesBackgroundAndWhetherItIsEnabledAreChanged()
    {
        rv::data::Presentation presentation = song();
        const std::string third = cueBytes(presentation, "CUE-3");
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-2"), {{"label", "Repeat"}, {"notes", "Twice"}, {"enabled", false}, {"background", "black"}},
                            kContext).isEmpty());
        rv::data::Cue *changed = cue(&presentation, QStringLiteral("CUE-2"));
        QCOMPARE(labelfile::of(*changed).text, QStringLiteral("Repeat"));
        QCOMPARE(labelfile::of(*changed).color, QString());
        QCOMPARE(changed->isenabled(), false);
        QCOMPARE(proconvert::readNotes(*slideOf(changed)).plainText(), QStringLiteral("Twice"));
        QVERIFY(slideOf(changed)->base_slide().draws_background_color());
        QCOMPARE(cueBytes(presentation, "CUE-3"), third);

        // A colour for the label, its words as they were; and the same again changing nothing
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-2"), {{"label_color", "#00ff00"}}, kContext).isEmpty());
        QCOMPARE(labelfile::of(*cue(&presentation, QStringLiteral("CUE-2"))).text, QStringLiteral("Repeat"));
        QCOMPARE(labelfile::of(*cue(&presentation, QStringLiteral("CUE-2"))).color, QStringLiteral("#00ff00"));
        const std::string before = presentation.SerializeAsString();
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-2"), {{"label", "Repeat"}, {"label_color", "#00ff00"}, {"notes", "Twice"}, {"enabled", false},
                                                                    {"background", "#000000"}}, kContext).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);

        // Notes typed over, and emptied; no label; no background
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-2"), {{"notes", ""}, {"label", ""}, {"label_color", ""}, {"background", ""}, {"enabled", true}},
                            kContext).isEmpty());
        changed = cue(&presentation, QStringLiteral("CUE-2"));
        QCOMPARE(proconvert::readNotes(*slideOf(changed)).plainText(), QString());
        QCOMPARE(labelfile::of(*changed).text, QString());
        QVERIFY(!slideOf(changed)->base_slide().draws_background_color());
        QCOMPARE(changed->isenabled(), true);
        // Nothing typed into no notes is nothing to keep
        const std::string firstBefore = cueBytes(presentation, "CUE-1");
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-1"), {{"notes", ""}}, kContext).isEmpty());
        QCOMPARE(cueBytes(presentation, "CUE-1"), firstBefore);

        // What cannot be leaves the presentation as it was
        const std::string whole = presentation.SerializeAsString();
        QVERIFY(!changeSlide(&presentation, QStringLiteral("CUE-1"), {{"label", "Kept?"}, {"background", "nearly black"}}, kContext).isEmpty());
        QVERIFY(!changeSlide(&presentation, QStringLiteral("NOT-THERE"), {{"label", "x"}}, kContext).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), whole);
    }

    void aGroupIsBegunAtASlide()
    {
        rv::data::Presentation presentation = song();
        // From its second slide: the group is split there, and the new group is the
        // workspace's of that name, with its colour and its id in the list
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-2"), {{"group", "bridge"}}, kContext).isEmpty());
        QCOMPARE(groupCues(presentation), QList<QStringList>({{"CUE-1"}, {"CUE-2"}, {"CUE-3"}}));
        const rv::data::Group &made = presentation.cue_groups(1).group();
        QCOMPARE(made.name(), std::string("Bridge"));
        QCOMPARE(proconvert::toColor(made.color()), QColor(0, 255, 0));
        QCOMPARE(made.application_group_identifier().string(), std::string("LIST-BRIDGE"));
        // And it comes after the old group wherever an arrangement has that
        QCOMPARE(presentation.arrangements(0).group_identifiers_size(), 5);

        // A group the workspace has none of, with a colour of its own
        QVERIFY(changeSlide(&presentation, QStringLiteral("CUE-3"), {{"group", QVariantMap {{"name", "Outro"}, {"color", "#123456"}}}}, kContext).isEmpty());
        QCOMPARE(presentation.cue_groups(2).group().name(), std::string("Outro"));
        QCOMPARE(proconvert::toColor(presentation.cue_groups(2).group().color()), QColor(0x12, 0x34, 0x56));
        QVERIFY(!presentation.cue_groups(2).group().has_application_group_identifier());
        // (The same group of the presentation, with the id it had)
        QCOMPARE(presentation.cue_groups(2).group().uuid().string(), std::string("G-CHORUS"));
    }

    void aSlideIsMovedAndRemoved()
    {
        rv::data::Presentation presentation = song();
        const std::string moved = cueBytes(presentation, "CUE-1");
        // Beside a slide of another group: into that group
        QVERIFY(moveCue(&presentation, QStringLiteral("CUE-1"), {QStringLiteral("CUE-3"), {}, {}}).isEmpty());
        QCOMPARE(groupCues(presentation), QList<QStringList>({{"CUE-2"}, {"CUE-3", "CUE-1"}}));
        QCOMPARE(cueBytes(presentation, "CUE-1"), moved);
        QVERIFY(moveCue(&presentation, QStringLiteral("CUE-1"), {{}, QStringLiteral("CUE-2"), {}}).isEmpty());
        QCOMPARE(groupCues(presentation), QList<QStringList>({{"CUE-1", "CUE-2"}, {"CUE-3"}}));
        // To the end of a group
        QVERIFY(moveCue(&presentation, QStringLiteral("CUE-1"), {{}, {}, QStringLiteral("G-VERSE")}).isEmpty());
        QCOMPARE(groupCues(presentation), QList<QStringList>({{"CUE-2", "CUE-1"}, {"CUE-3"}}));
        // Beside itself, or beside what is not there: nothing
        const std::string before = presentation.SerializeAsString();
        QVERIFY(moveCue(&presentation, QStringLiteral("CUE-1"), {QStringLiteral("CUE-1"), {}, {}}).isEmpty());
        QVERIFY(!moveCue(&presentation, QStringLiteral("CUE-1"), {QStringLiteral("NOT-THERE"), {}, {}}).isEmpty());
        QVERIFY(!moveCue(&presentation, QStringLiteral("NOT-THERE"), {QStringLiteral("CUE-1"), {}, {}}).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);

        QVERIFY(removeCue(&presentation, QStringLiteral("CUE-2")).isEmpty());
        QCOMPARE(cueOrder(presentation), QStringList({"CUE-1", "CUE-3"}));
        QCOMPARE(presentation.cues_size(), 2);
        QVERIFY(!removeCue(&presentation, QStringLiteral("CUE-2")).isEmpty());
        QCOMPARE(unknownCount(presentation), 1);
    }

    // ---- groups

    void aGroupIsRenamedRecolouredMovedAndRemoved()
    {
        rv::data::Presentation presentation = song();
        const std::string cues = cueBytes(presentation, "CUE-1") + cueBytes(presentation, "CUE-2") + cueBytes(presentation, "CUE-3");
        // Renamed to one of the workspace's: its colour and its id in the list come with it
        QVERIFY(changeGroup(&presentation, 0, {{"name", "Chorus"}}, kContext).isEmpty());
        QCOMPARE(presentation.cue_groups(0).group().name(), std::string("Chorus"));
        QCOMPARE(proconvert::toColor(presentation.cue_groups(0).group().color()), QColor(255, 0, 0));
        QCOMPARE(presentation.cue_groups(0).group().application_group_identifier().string(), std::string("LIST-CHORUS"));
        // (It is the same group of the presentation, where every arrangement has it)
        QCOMPARE(presentation.cue_groups(0).group().uuid().string(), std::string("G-VERSE"));
        // A colour alone; and none
        QVERIFY(changeGroup(&presentation, 0, {{"color", "#abcdef"}}, kContext).isEmpty());
        QCOMPARE(proconvert::toColor(presentation.cue_groups(0).group().color()), QColor(0xab, 0xcd, 0xef));
        QCOMPARE(presentation.cue_groups(0).group().name(), std::string("Chorus"));
        const std::string before = presentation.SerializeAsString();
        QVERIFY(changeGroup(&presentation, 0, {{"color", "#abcdef"}}, kContext).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);
        QVERIFY(changeGroup(&presentation, 0, {{"color", ""}}, kContext).isEmpty());
        QVERIFY(!presentation.cue_groups(0).group().has_color());
        QVERIFY(!changeGroup(&presentation, 0, {{"color", "nearly red"}}, kContext).isEmpty());
        QVERIFY(!changeGroup(&presentation, 7, {{"name", "x"}}, kContext).isEmpty());

        // Moved after the other, and back before it
        QVERIFY(moveGroup(&presentation, 0, 1, true).isEmpty());
        QCOMPARE(presentation.cue_groups(0).group().uuid().string(), std::string("G-CHORUS"));
        QCOMPARE(cueOrder(presentation), QStringList({"CUE-3", "CUE-1", "CUE-2"}));
        QVERIFY(moveGroup(&presentation, 1, 0, false).isEmpty());
        QCOMPARE(cueOrder(presentation), QStringList({"CUE-1", "CUE-2", "CUE-3"}));
        QVERIFY(!moveGroup(&presentation, 0, 5, true).isEmpty());
        // Through all of which the slides are as they were
        QCOMPARE(cueBytes(presentation, "CUE-1") + cueBytes(presentation, "CUE-2") + cueBytes(presentation, "CUE-3"), cues);

        // Removed: its slides go with it, and its places in the arrangement
        removeGroup(&presentation, 0);
        QCOMPARE(cueOrder(presentation), QStringList({"CUE-3"}));
        QCOMPARE(presentation.cues_size(), 1);
        QCOMPARE(presentation.arrangements(0).group_identifiers_size(), 1);
        QCOMPARE(presentation.arrangements(0).group_identifiers(0).string(), std::string("G-CHORUS"));
    }

    void aGroupIsMadeOfTheWorkspacesOrOfItsOwn()
    {
        rv::data::Presentation presentation = song();
        const int bridge = addGroup(&presentation, QStringLiteral("Bridge"), QString(), kContext);
        QCOMPARE(bridge, 2);
        QCOMPARE(proconvert::toColor(presentation.cue_groups(2).group().color()), QColor(0, 255, 0));
        QVERIFY(!presentation.cue_groups(2).group().uuid().string().empty());
        const int tag = addGroup(&presentation, QStringLiteral("Tag"), QStringLiteral("#ffff00"), kContext);
        QCOMPARE(presentation.cue_groups(tag).group().name(), std::string("Tag"));
        QCOMPARE(proconvert::toColor(presentation.cue_groups(tag).group().color()), QColor(255, 255, 0));
        // One with no name, which is how ProPresenter keeps slides that are in no group
        const int plain = addGroup(&presentation, QString(), QString(), kContext);
        QVERIFY(presentation.cue_groups(plain).group().name().empty());
        QVERIFY(presentation.cue_groups(plain).group().has_hotkey());
    }

    // ---- arrangements

    void anArrangementIsMadeAndChanged()
    {
        rv::data::Presentation presentation = song();
        const std::string cues = cueBytes(presentation, "CUE-1") + cueBytes(presentation, "CUE-2") + cueBytes(presentation, "CUE-3");
        QString id;
        // A new one, of the groups named, each by its name or its id
        QVERIFY(setArrangement(&presentation, QStringLiteral("Long"), {{"groups", QVariantList {"Verse 1", "G-CHORUS", "verse 1", "Chorus", "1"}}}, &id).isEmpty());
        QCOMPARE(presentation.arrangements_size(), 2);
        QCOMPARE(presentation.arrangements(1).name(), std::string("Long"));
        QCOMPARE(presentation.arrangements(1).uuid().string(), id.toStdString());
        QStringList groups;
        for (const rv::data::UUID &each : presentation.arrangements(1).group_identifiers())
            groups.append(QString::fromStdString(each.string()));
        QCOMPARE(groups, QStringList({"G-VERSE", "G-CHORUS", "G-VERSE", "G-CHORUS", "G-CHORUS"}));
        // A new one with no groups said has every group once
        QVERIFY(setArrangement(&presentation, QStringLiteral("All"), {}, &id).isEmpty());
        QCOMPARE(presentation.arrangements(2).group_identifiers_size(), 2);

        // One there is, changed: its groups, by its name; and renamed, by its id
        const std::string shortBefore = presentation.arrangements(0).SerializeAsString();
        QVERIFY(setArrangement(&presentation, QStringLiteral("long"), {{"groups", QVariantList {"Chorus"}}}, &id).isEmpty());
        QCOMPARE(presentation.arrangements_size(), 3);
        QCOMPARE(presentation.arrangements(1).group_identifiers_size(), 1);
        QVERIFY(setArrangement(&presentation, id, {{"name", "Chorus Only"}}, &id).isEmpty());
        QCOMPARE(presentation.arrangements(1).name(), std::string("Chorus Only"));
        // The others, and the slides, are as they were
        QCOMPARE(presentation.arrangements(0).SerializeAsString(), shortBefore);
        QCOMPARE(cueBytes(presentation, "CUE-1") + cueBytes(presentation, "CUE-2") + cueBytes(presentation, "CUE-3"), cues);
        // As it is already: nothing
        const std::string before = presentation.SerializeAsString();
        QVERIFY(setArrangement(&presentation, QStringLiteral("Short"), {{"groups", QVariantList {"G-VERSE", "G-CHORUS", "G-VERSE"}}}, &id).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);

        // What cannot be: a group that is not there, a name another has, no name at all
        QVERIFY(!setArrangement(&presentation, QStringLiteral("Short"), {{"groups", QVariantList {"Verse 9"}}}, &id).isEmpty());
        QVERIFY(!setArrangement(&presentation, QStringLiteral("Short"), {{"name", "All"}}, &id).isEmpty());
        QVERIFY(!setArrangement(&presentation, QStringLiteral("  "), {}, &id).isEmpty());
        QVERIFY(!setArrangement(&presentation, QStringLiteral("Short"), {{"groups", "Chorus"}}, &id).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);
    }

    void anArrangementIsSelectedOrNone()
    {
        rv::data::Presentation presentation = song();
        const std::string before = presentation.SerializeAsString();
        QVERIFY(selectArrangement(&presentation, QStringLiteral("Short")).isEmpty());
        QCOMPARE(presentation.SerializeAsString(), before);
        QVERIFY(selectArrangement(&presentation, QString()).isEmpty());
        QVERIFY(!presentation.has_selected_arrangement());
        QVERIFY(selectArrangement(&presentation, QStringLiteral("ARR-SHORT")).isEmpty());
        QCOMPARE(presentation.selected_arrangement().string(), std::string("ARR-SHORT"));
        QVERIFY(!selectArrangement(&presentation, QStringLiteral("Long")).isEmpty());
    }

    // ---- a presentation made

    void aPresentationIsMadeOfItsGroupsAndSlides()
    {
        rv::data::Presentation presentation = proconvert::newPresentation(QStringLiteral("Made"));
        QStringList made;
        QVariantList descriptions;
        const QVariantMap wanted {
            {"groups", QVariantList {QVariantMap {{"name", "Verse 1"}, {"slides", QVariantList {"one\ntwo", QVariantMap {{"text", "three"}, {"label", "L"}}}}},
                                     QVariantMap {{"name", "Chorus"}, {"color", "#010203"}, {"slides", QVariantList {"chorus"}}}}},
            {"arrangements", QVariantList {QVariantMap {{"name", "Sunday"}, {"groups", QVariantList {"Verse 1", "Chorus", "Chorus"}}}}},
            {"arrangement", "Sunday"},
            {"destination", "announcements"},
            {"copyright", QVariantMap {{"title", "Made"}, {"author", "Nobody"}, {"song_number", 123}, {"display", true}}},
            {"key", "F#m"},
        };
        QVERIFY(build(&presentation, wanted, kContext, &made, &descriptions).isEmpty());
        QCOMPARE(made.size(), 3);
        QCOMPARE(descriptions.size(), 3);
        QCOMPARE(descriptions.at(1).toMap().value("label").toString(), QStringLiteral("L"));
        QCOMPARE(groupCues(presentation), QList<QStringList>({{made.at(0), made.at(1)}, {made.at(2)}}));
        // The first group in the workspace's colour for its name, the second in its own
        QCOMPARE(proconvert::toColor(presentation.cue_groups(0).group().color()), QColor(0, 0, 255));
        QCOMPARE(proconvert::toColor(presentation.cue_groups(1).group().color()), QColor(1, 2, 3));
        QCOMPARE(wordsOf(presentation, made.at(0)), QStringLiteral("one\ntwo"));
        QCOMPARE(wordsOf(presentation, made.at(2)), QStringLiteral("chorus"));
        // (Every slide's box as the first's is)
        QCOMPARE(slideOf(cue(&presentation, made.at(2)))->base_slide().elements(0).element().name(), std::string("Lyrics"));
        QCOMPARE(presentation.arrangements_size(), 1);
        QCOMPARE(presentation.arrangements(0).group_identifiers_size(), 3);
        QCOMPARE(presentation.selected_arrangement().string(), presentation.arrangements(0).uuid().string());
        QCOMPARE(presentation.content_destination(), rv::data::Action::CONTENT_DESTINATION_ANNOUNCEMENTS);
        QCOMPARE(presentation.ccli().author(), std::string("Nobody"));
        QCOMPARE(presentation.ccli().song_number(), 123u);
        QVERIFY(presentation.ccli().display());
        QVERIFY(presentation.has_music());
        QCOMPARE(presentation.music().original().music_scale(), rv::data::MusicKeyScale::MUSIC_SCALE_MINOR);
        // And what every presentation of ProPresenter's own starts with is still there
        QVERIFY(presentation.has_timeline());
    }

    void aPresentationWithNothingSaidHasOneSlideAndOneOfSlidesHasThemInNoGroup()
    {
        rv::data::Presentation presentation = proconvert::newPresentation(QStringLiteral("Empty"));
        QStringList made;
        QVariantList descriptions;
        QVERIFY(build(&presentation, {}, kContext, &made, &descriptions).isEmpty());
        QCOMPARE(made.size(), 1);
        QCOMPARE(presentation.cue_groups_size(), 1);
        QVERIFY(presentation.cue_groups(0).group().name().empty());
        QCOMPARE(slideOf(cue(&presentation, made.first()))->base_slide().elements_size(), 0);

        presentation = proconvert::newPresentation(QStringLiteral("Slides"));
        QVERIFY(build(&presentation, {{"slides", QVariantList {"a", "b"}}}, {kGroups, QSizeF(1280, 720)}, &made, &descriptions).isEmpty());
        QCOMPARE(made.size(), 2);
        QCOMPARE(presentation.cue_groups_size(), 1);
        // (Of the size asked for)
        QCOMPARE(slideOf(cue(&presentation, made.first()))->base_slide().size().width(), 1280.0);
        QCOMPARE(slideOf(cue(&presentation, made.first()))->base_slide().elements(0).element().bounds().size().width(), 1152.0);

        // What cannot be made
        for (const QVariantMap &wrong : {QVariantMap {{"groups", "Verse"}}, QVariantMap {{"groups", QVariantList {QVariantMap {{"name", "V"}}}}},
                                         QVariantMap {{"slides", "words"}}, QVariantMap {{"key", "H"}}, QVariantMap {{"destination", "the moon"}},
                                         QVariantMap {{"arrangement", "None Such"}}, QVariantMap {{"copyright", "mine"}},
                                         QVariantMap {{"arrangements", QVariantList {QVariantMap {{"name", "A"}, {"groups", QVariantList {"Nope"}}}}}}}) {
            rv::data::Presentation fresh = proconvert::newPresentation(QStringLiteral("x"));
            QVERIFY2(!build(&fresh, wrong, kContext, &made, &descriptions).isEmpty(), qPrintable(wrong.firstKey()));
        }
    }

    // ---- what is on a slide

    void anElementIsFoundByItsIdItsNameOrItsNumber()
    {
        rv::data::Presentation presentation = song();
        rv::data::Slide *slide = firstSlide(&presentation);
        const QString id = QString::fromStdString(slide->elements(0).element().uuid().string());
        QCOMPARE(slideedit::indexOf(*slide, id), 0);
        QCOMPARE(slideedit::indexOf(*slide, QStringLiteral("Lyrics")), 0);
        QCOMPARE(slideedit::indexOf(*slide, QStringLiteral("0")), 0);
        QCOMPARE(slideedit::indexOf(*slide, QStringLiteral("1")), -1);
        QCOMPARE(slideedit::indexOf(*slide, QStringLiteral("Title")), -1);
        QCOMPARE(slideedit::wordsBox(*slide), 0);
    }

    void anElementIsAddedInFrontOfWhatIsThere()
    {
        rv::data::Presentation presentation = song();
        rv::data::Slide *slide = firstSlide(&presentation);
        const std::string lyrics = slide->elements(0).SerializeAsString();
        QString id;
        // A text box: set like the slide's first text, with the words and the place given
        QVERIFY(slideedit::add(slide, asked({{"name", "Reference"}, {"text", "John 3:16"}, {"x", 100}, {"y", 900}, {"width", 800}, {"height", 100}}), &id).isEmpty());
        QCOMPARE(slide->elements_size(), 2);
        QCOMPARE(slide->elements(1).element().uuid().string(), id.toStdString());
        QCOMPARE(slide->elements(1).element().name(), std::string("Reference"));
        QCOMPARE(slide->elements(1).element().bounds().origin().y(), 900.0);
        QCOMPARE(proconvert::readText(slide->elements(1).element().text()).plainText(), QStringLiteral("John 3:16"));
        QCOMPARE(proconvert::readText(slide->elements(1).element().text()).firstRun().size,
                 proconvert::readText(slide->elements(0).element().text()).firstRun().size);
        // With no words given it says "Text", as the editor's does
        QVERIFY(slideedit::add(slide, asked({}), &id).isEmpty());
        QCOMPARE(proconvert::readText(slide->elements(2).element().text()).plainText(), QStringLiteral("Text"));
        // A shape, with a colour
        QVERIFY(slideedit::add(slide, asked({{"kind", "ellipse"}, {"fill_color", "#ff0000"}}), &id).isEmpty());
        QCOMPARE(proconvert::elementMap(slide->elements(3)).value("shape").toString(), QStringLiteral("ellipse"));
        QCOMPARE(proconvert::elementMap(slide->elements(3)).value("fillColor").value<QColor>(), QColor(Qt::red));
        // What was there is as it was
        QCOMPARE(slide->elements(0).SerializeAsString(), lyrics);

        // What cannot be made leaves the slide as it was
        const std::string before = slide->SerializeAsString();
        QVERIFY(!slideedit::add(slide, asked({{"kind", "hexagon"}}), &id).isEmpty());
        QVERIFY(!slideedit::add(slide, asked({{"kind", "media"}}), &id).isEmpty());
        QVERIFY(!slideedit::add(slide, asked({{"kind", "media"}, {"file", "/nowhere/at/all.png"}}), &id).isEmpty());
        QVERIFY(!slideedit::add(slide, asked({{"chords", QVariantList {QVariantMap {{"name", "G"}}}}}), &id).isEmpty());
        QCOMPARE(slide->SerializeAsString(), before);
    }

    void anElementIsChangedAndNothingElseOnTheSlideIs()
    {
        rv::data::Presentation presentation = song();
        rv::data::Slide *slide = firstSlide(&presentation);
        QString id;
        QVERIFY(slideedit::add(slide, asked({{"kind", "rectangle"}, {"name", "Bar"}}), &id).isEmpty());
        const std::string bar = slide->elements(1).SerializeAsString();

        // Its place, its fill, how its words sit, and its words in another look
        QVERIFY(slideedit::change(slide, 0, asked({{"x", 10}, {"y", 20}, {"width", 500}, {"height", 300}, {"fill_on", true}, {"fill_color", "#00000080"},
                                                   {"vertical_alignment", "bottom"}, {"format", QVariantMap {{"color", "#ffff00"}, {"alignment", "left"}}}},
                                                  slide, 0)).isEmpty());
        const QVariantMap now = proconvert::elementMap(slide->elements(0));
        QCOMPARE(now.value("x").toDouble(), 10.0);
        QCOMPARE(now.value("width").toDouble(), 500.0);
        QCOMPARE(now.value("fillOn").toBool(), true);
        QCOMPARE(now.value("fillColor").value<QColor>(), QColor(0, 0, 0, 128));
        QCOMPARE(now.value("verticalAlignment").toInt(), int(Qt::AlignBottom));
        const RichText text = proconvert::readText(slide->elements(0).element().text());
        QCOMPARE(text.firstRun().fill, QColor(255, 255, 0));
        QCOMPARE(text.paragraphs.first().alignment, Qt::AlignLeft);
        QCOMPARE(text.plainText(), QStringLiteral("Amazing grace\nhow sweet the sound"));
        // The other element, and what a ProPresenter to come put on this one, as they were
        QCOMPARE(slide->elements(1).SerializeAsString(), bar);
        QCOMPARE(unknownCount(slide->elements(0)), 1);

        // Sent again, it changes nothing
        const std::string before = slide->SerializeAsString();
        QVERIFY(slideedit::change(slide, 0, asked({{"x", 10}, {"fill_color", "#00000080"}, {"vertical_alignment", "bottom"},
                                                   {"text", "Amazing grace\nhow sweet the sound"}}, slide, 0)).isEmpty());
        QCOMPARE(slide->SerializeAsString(), before);

        // Its text anew, run by run; and chords over its words
        QVERIFY(slideedit::change(slide, 0, asked({{"paragraphs", QVariantList {QVariantMap {{"runs", QVariantList {QVariantMap {{"text", "Grace "}},
                                                                                                                       QVariantMap {{"text", "alone"}, {"bold", true}}}}}}},
                                                   {"chords", QVariantList {QVariantMap {{"at", 0}, {"name", "G"}}, QVariantMap {{"at", 6}, {"name", "D/F#"}}}}},
                                                  slide, 0)).isEmpty());
        const RichText made = proconvert::readText(slide->elements(0).element().text());
        QCOMPARE(made.plainText(), QStringLiteral("Grace alone"));
        QCOMPARE(made.paragraphs.first().runs.size(), 2);
        QCOMPARE(made.paragraphs.first().runs.at(1).bold, true);
        // (A run takes what it does not say from the look the box had)
        QCOMPARE(made.paragraphs.first().runs.at(0).fill, QColor(255, 255, 0));
        const QList<chords::Chord> chords = proconvert::readChords(slide->elements(0).element().text());
        QCOMPARE(chords.size(), 2);
        QCOMPARE(chords.at(1).name, QStringLiteral("D/F#"));

        // What cannot be leaves the slide as it was
        const std::string whole = slide->SerializeAsString();
        QVERIFY(!slideedit::change(slide, 0, asked({{"x", 55}, {"fill_media_path", "/nowhere/at/all.png"}}, slide, 0)).isEmpty());
        QVERIFY(!slideedit::change(slide, 0, asked({{"x", 55}, {"chords", QVariantList {QVariantMap {{"at", -1}, {"name", "G"}}}}}, slide, 0)).isEmpty());
        QVERIFY(!slideedit::change(slide, 9, asked({{"x", 55}})).isEmpty());
        QCOMPARE(slide->SerializeAsString(), whole);
    }

    void anElementIsPutElsewhereInTheOrderAndTakenOff()
    {
        rv::data::Presentation presentation = song();
        rv::data::Slide *slide = firstSlide(&presentation);
        QString bar;
        QString dot;
        QVERIFY(slideedit::add(slide, asked({{"kind", "rectangle"}, {"name", "Bar"}}), &bar).isEmpty());
        QVERIFY(slideedit::add(slide, asked({{"kind", "ellipse"}, {"name", "Dot"}}), &dot).isEmpty());
        // The last added is in front; sent to the back, it is first
        extendedapijson::ElementChanges toBack;
        toBack.order = 0;
        QVERIFY(slideedit::change(slide, 2, toBack).isEmpty());
        QCOMPARE(slide->elements(0).element().name(), std::string("Dot"));
        QCOMPARE(slide->elements(1).element().name(), std::string("Lyrics"));
        QCOMPARE(slide->elements(2).element().name(), std::string("Bar"));
        // (Past the front is the front)
        extendedapijson::ElementChanges toFront;
        toFront.order = 99;
        QVERIFY(slideedit::change(slide, 0, toFront).isEmpty());
        QCOMPARE(slide->elements(2).element().name(), std::string("Dot"));

        slideedit::remove(slide, slideedit::indexOf(*slide, bar));
        QCOMPARE(slide->elements_size(), 2);
        QCOMPARE(slideedit::indexOf(*slide, bar), -1);
        QCOMPARE(slideedit::indexOf(*slide, QStringLiteral("Lyrics")), 0);
        // (What is not there is not taken off)
        slideedit::remove(slide, 7);
        QCOMPARE(slide->elements_size(), 2);
    }

    void aSlidesBackgroundIsSetAndTakenAway()
    {
        rv::data::Presentation presentation = song();
        rv::data::Slide *slide = firstSlide(&presentation);
        const std::string before = slide->SerializeAsString();
        // None, where there is none: nothing
        QVERIFY(slideedit::setBackground(slide, QString()));
        QCOMPARE(slide->SerializeAsString(), before);
        QVERIFY(slideedit::setBackground(slide, QStringLiteral("#336699")));
        QVERIFY(slide->draws_background_color());
        QCOMPARE(proconvert::toColor(slide->background_color()), QColor(0x33, 0x66, 0x99));
        const std::string coloured = slide->SerializeAsString();
        QVERIFY(slideedit::setBackground(slide, QStringLiteral("#336699")));
        QCOMPARE(slide->SerializeAsString(), coloured);
        QVERIFY(!slideedit::setBackground(slide, QStringLiteral("nearly blue")));
        QVERIFY(slideedit::setBackground(slide, QString()));
        QVERIFY(!slide->draws_background_color());
    }
};

// Fonts are looked up for the text (a new text box is set in the system's own font), which
// Qt does only for an application that could draw: one with windows, on Qt's offscreen
// platform, so that the test runs where there is no desktop.
int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestPresentationEdit test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_presentationedit.moc"
