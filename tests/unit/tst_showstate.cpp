// The rules of the show, tried by themselves (src/showstate.h): no window, no workspace
// on disk, nothing drawn. Each test sets a show up from plain values, does one thing to
// it, and looks at what the show then holds and at what it said was to happen outside.

#include "showstate.h"

#include <QTest>

using namespace show;

namespace {

QVariantMap video(const QString &name, bool foreground = false, int playback = 1)
{
    return {{"name", name}, {"path", "/media/" + name}, {"video", true}, {"foreground", foreground}, {"retriggers", false},
            {"loops", playback != 0}, {"playback", playback}, {"loopCount", 2}, {"loopSeconds", 10.0}};
}

QVariantMap picture(const QString &name, bool foreground = false)
{
    return {{"name", name}, {"path", "/media/" + name}, {"video", false}, {"foreground", foreground}, {"retriggers", false},
            {"loops", false}, {"playback", 0}, {"loopCount", 0}, {"loopSeconds", 0.0}};
}

QVariantMap slide(const QString &id, const QVariantMap &media = {}, const QVariantList &actions = {})
{
    QVariantMap map {{"id", id}, {"label", QString()}, {"mediaName", media.value("name").toString()}, {"actions", actions}};
    if (!media.isEmpty())
        map.insert("media", media);
    return map;
}

QVariantMap clearAction(int layer, bool done = true)
{
    return {{"id", "c" + QString::number(layer)}, {"kind", "clear"}, {"title", "Clear"}, {"done", done}, {"layer", layer}};
}

QVariantMap propAction(const QString &id, const QString &name, bool clear = false)
{
    return {{"kind", "prop"}, {"title", "Prop"}, {"done", true}, {"propId", id}, {"propName", name}, {"clear", clear}};
}

QVariantMap macroAction(const QString &id, const QString &name = {})
{
    return {{"kind", "macro"}, {"title", "Macro"}, {"done", true}, {"macroId", id}, {"macroName", name}};
}

QVariantMap stageAction(const QVariantList &assignments)
{
    return {{"kind", "stage"}, {"title", "Stage"}, {"done", true}, {"assignments", assignments}};
}

QVariantMap assignment(const QString &screenId, const QString &screenName, const QString &layoutId, const QString &layoutName)
{
    return {{"screenId", screenId}, {"screenName", screenName}, {"layoutId", layoutId}, {"layoutName", layoutName}};
}

// A presentation of plain slides, by which a cue is made for any of them.
struct Presentation
{
    QString key = "song.pro";
    QString playlistId;
    QString name = "Song";
    QVariantList slides;

    Cue cue(int index) const
    {
        Cue cue;
        cue.key = key;
        cue.playlistId = playlistId;
        cue.presentation = name;
        cue.index = index;
        cue.count = int(slides.size());
        cue.slide = slides.value(index).toMap();
        cue.next = slides.value(index + 1).toMap();
        return cue;
    }
};

Presentation plainSong(int count = 4)
{
    Presentation song;
    for (int i = 0; i < count; ++i)
        song.slides << slide(QStringLiteral("s%1").arg(i));
    return song;
}

// A workspace with two collections of props (the second showing one at a time), a few
// macros and two stage layouts.
Workspace workspace()
{
    Workspace w;
    const auto prop = [](const QString &id, const QString &name) { return QVariantMap {{"id", id}, {"name", name}}; };
    w.props = {
        QVariantMap {{"id", "free"}, {"name", "Free"}, {"single", false}, {"props", QVariantList {prop("p1", "Logo"), prop("p2", "Clock")}}},
        QVariantMap {{"id", "one"}, {"name", "One"}, {"single", true}, {"props", QVariantList {prop("p3", "Lower"), prop("p4", "Upper")}}},
    };
    w.macros = {
        QVariantMap {{"id", "m"}, {"name", "Macros"}, {"macros", QVariantList {
            QVariantMap {{"id", "tidy"}, {"name", "Tidy"}, {"actions", QVariantList {clearAction(4), clearAction(2)}}},
            QVariantMap {{"id", "blank"}, {"name", "Blank"}, {"actions", QVariantList {clearAction(5)}}},
            QVariantMap {{"id", "outer"}, {"name", "Outer"}, {"actions", QVariantList {propAction("p1", "Logo"), macroAction("tidy")}}},
            QVariantMap {{"id", "loop"}, {"name", "Loop"}, {"actions", QVariantList {macroAction("loop")}}},
        }}},
    };
    w.stageLayouts = {QVariantMap {{"id", "l1"}, {"name", "Words"}}, QVariantMap {{"id", "l2"}, {"name", "Clock"}}};
    w.stageScreens = {QVariantMap {{"id", "screen1"}, {"name", "Stage"}}, QVariantMap {{"id", "screen2"}, {"name", "Lobby"}}};
    w.looks = {QVariantMap {{"id", "full"}, {"name", "Everything"}}, QVariantMap {{"id", "third"}, {"name", "Lower Third"}}};
    return w;
}

// The kinds of what was to happen outside, in order, the log left out: "slide nomedia".
QString outside(const Effects &effects)
{
    static const char *names[] = {"slide", "slideovermedia", "slidewithmedia", "noslide", "media", "nomedia", "timer", "look", "note", "problem"};
    QStringList said;
    for (const Effect &effect : effects) {
        if (effect.kind != Effect::Note)
            said << names[effect.kind];
    }
    return said.join(u' ');
}

bool noted(const Effects &effects, const QString &part)
{
    for (const Effect &effect : effects) {
        if ((effect.kind == Effect::Note || effect.kind == Effect::Problem) && effect.text.contains(part))
            return true;
    }
    return false;
}

}

class TestShowState : public QObject
{
    Q_OBJECT

private slots:
    // ---- a slide goes live

    void aSlideGoesLive()
    {
        State show;
        QVERIFY(show.cleared);
        QVERIFY(!show.cueLive());
        const Presentation song = plainSong();
        const Effects effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(outside(effects), "slide");
        QVERIFY(show.atSlide);
        QCOMPARE(show.index, 1);
        QCOMPARE(show.count, 4);
        QVERIFY(!show.cleared);
        QVERIFY(show.cueLive());
        QCOMPARE(show.slide.value("id").toString(), "s1");
        QCOMPARE(show.next.value("id").toString(), "s2");
        QVERIFY(show.at("song.pro", ""));
        QVERIFY(!show.at("song.pro", "sunday"));
        QVERIFY(noted(effects, "slide 2 of 4 of \"Song\""));
    }

    void aSlideThatIsNotThereDoesNothing()
    {
        State show;
        const Presentation song = plainSong();
        QVERIFY(show.goLive(song.cue(-1), false, workspace()).isEmpty());
        QVERIFY(show.goLive(song.cue(4), false, workspace()).isEmpty());
        QVERIFY(!show.atSlide);
        QVERIFY(show.cleared);
    }

    // ---- the media a slide brings

    void aSlideStartsTheMediaItBrings()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slidewithmedia");
        QCOMPARE(effects.last().media.value("name").toString(), "waves.mp4");
        QVERIFY(show.hasMedia);
        QCOMPARE(show.mediaPlaylistId, "");
    }

    void aLoopingBackgroundIsLeftToPlayOn()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4")), slide("b", video("waves.mp4")), slide("c")};
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slideovermedia");
        // A slide that brings nothing leaves a background where it is.
        QCOMPARE(outside(show.goLive(song.cue(2), false, workspace())), "slide");
        QVERIFY(show.hasMedia);
    }

    void aBackgroundThatStopsAtItsEndIsStartedAgain()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("sting.mp4", false, 0)), slide("b", video("sting.mp4", false, 0))};
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slidewithmedia");
    }

    void aBackgroundSetToPlayAnotherWayIsStartedAgain()
    {
        State show;
        Presentation song;
        QVariantMap twice = video("waves.mp4", false, 2);
        QVariantMap thrice = twice;
        thrice.insert("loopCount", 3);
        QVariantMap otherSeconds = video("waves.mp4");
        otherSeconds.insert("loopSeconds", 99.0);
        song.slides = {slide("a", video("waves.mp4")), slide("b", twice), slide("c", twice), slide("d", thrice), slide("e", video("waves.mp4")),
                       slide("f", otherSeconds)};
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slidewithmedia"); // loop, then loop twice
        QCOMPARE(outside(show.goLive(song.cue(2), false, workspace())), "slideovermedia"); // the same again
        QCOMPARE(outside(show.goLive(song.cue(3), false, workspace())), "slidewithmedia"); // three times, not twice
        QCOMPARE(outside(show.goLive(song.cue(4), false, workspace())), "slidewithmedia"); // plain looping again
        // The length of time only matters to the way of playing that goes by it.
        QCOMPARE(outside(show.goLive(song.cue(5), false, workspace())), "slideovermedia");
    }

    void aPictureBackgroundStaysAndOneSetToRetriggerDoesNot()
    {
        State show;
        Presentation song;
        QVariantMap again = picture("hill.jpg");
        again.insert("retriggers", true);
        song.slides = {slide("a", picture("hill.jpg")), slide("b", picture("hill.jpg")), slide("c", again)};
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slideovermedia");
        QCOMPARE(outside(show.goLive(song.cue(2), false, workspace())), "slidewithmedia");
    }

    void aForegroundIsAlwaysStartedAndEndsWithTheNextSlide()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("intro.mp4", true)), slide("b", video("intro.mp4", true)), slide("c")};
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slidewithmedia");
        const Effects effects = show.goLive(song.cue(2), false, workspace());
        QCOMPARE(outside(effects), "nomedia slide");
        QVERIFY(!show.hasMedia);
        QVERIFY(noted(effects, "takes off the foreground media"));
    }

    void withAltASlideComesWithoutItsMedia()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"), {clearAction(4)})};
        show.toggleProp("p1", workspace());
        const Effects effects = show.goLive(song.cue(0), true, workspace());
        QCOMPARE(outside(effects), "slide");
        QVERIFY(!show.hasMedia);
        // Its actions are done all the same.
        QVERIFY(show.props.isEmpty());
        QVERIFY(noted(effects, "without its media, as asked"));
    }

    void mediaThatIsNotInTheWorkspaceIsAProblemAndTheSlideIsShown()
    {
        State show;
        Presentation song;
        QVariantMap lost = slide("a");
        lost.insert("mediaName", "gone.mp4");
        song.slides = {lost};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "problem slide");
        QVERIFY(!show.cleared);
    }

    // ---- a slide's actions

    void theSlideIsShownAndThenItsActionsAreDone()
    {
        State show;
        Presentation song;
        const QVariantMap timer {{"kind", "timer"}, {"title", "Start"}, {"done", true}, {"timerId", "t1"}};
        song.slides = {slide("a", {}, {timer, propAction("p1", "Logo")})};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide timer");
        QCOMPARE(effects.at(2).action.value("timerId").toString(), "t1");
        QCOMPARE(show.props, QStringList {"p1"});
    }

    void aSlideThatClearsItselfIsStillTheSlideTheShowIsAt()
    {
        State show;
        Presentation song = plainSong();
        song.slides[1] = slide("s1", {}, {clearAction(5)});
        show.goLive(song.cue(0), false, workspace());
        const Effects effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(outside(effects), "slide noslide");
        QVERIFY(show.cleared);
        QVERIFY(show.clearedByCue);
        QVERIFY(show.cueLive());
        QCOMPARE(show.index, 1);
        QVERIFY(noted(effects, "the slide, by its own action"));
        // The arrow key goes on from it: it would only clear itself again.
        QCOMPARE(show.stepTarget("song.pro", "", 1), 2);
        QCOMPARE(show.stepTarget("song.pro", "", -1), 0);
        // And the next slide is an ordinary one again.
        show.goLive(song.cue(2), false, workspace());
        QVERIFY(!show.cleared);
        QVERIFY(!show.clearedByCue);
    }

    void aSlideThatClearsEverythingTakesItsOwnMediaAndThePropsToo()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"), {clearAction(0)})};
        show.toggleProp("p1", workspace());
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slidewithmedia noslide nomedia");
        QVERIFY(show.cleared && show.clearedByCue && !show.hasMedia && show.props.isEmpty());
    }

    void aClearMediaActionClearsTheMediaTheSlideBrought()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"), {clearAction(2)})};
        QCOMPARE(outside(show.goLive(song.cue(0), false, workspace())), "slidewithmedia nomedia");
        QVERIFY(!show.hasMedia);
        QVERIFY(!show.cleared);
    }

    void aClearOfALayerThisAppHasNotIsNotedAndNotDone()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {clearAction(1, false)})};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide");
        QVERIFY(!show.cleared);
        QVERIFY(noted(effects, "not done here"));
    }

    void anActionOfAKindNotKnownIsNotedAndNotDone()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {QVariantMap {{"kind", "other"}, {"title", "Audience Look"}, {"done", false}}})};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide");
        QVERIFY(noted(effects, "Audience Look: not done here"));
    }

    // ---- stepping

    void steppingGoesOnFromTheLiveSlide()
    {
        State show;
        const Presentation song = plainSong();
        // Looking at a presentation the show is not at: its first slide.
        QCOMPARE(show.stepTarget("song.pro", "", 1), 0);
        show.goLive(song.cue(2), false, workspace());
        QCOMPARE(show.stepTarget("song.pro", "", 1), 3);
        QCOMPARE(show.stepTarget("song.pro", "", -1), 1);
        QCOMPARE(show.stepTarget("other.pro", "", 1), 0);
        QCOMPARE(show.stepTarget("song.pro", "sunday", 1), 0);
    }

    void steppingFromAClearedOutputBringsTheSlideBack()
    {
        State show;
        const Presentation song = plainSong();
        show.goLive(song.cue(2), false, workspace());
        QCOMPARE(outside(show.clearSlide()), "noslide");
        QVERIFY(show.cleared);
        QVERIFY(!show.clearedByCue);
        QVERIFY(!show.cueLive());
        QCOMPARE(show.stepTarget("song.pro", "", 1), 2);
        QCOMPARE(show.stepTarget("song.pro", "", -1), 2);
    }

    // ---- clearing

    void clearingWhatIsNotThereDoesNothing()
    {
        State show;
        QVERIFY(show.clearSlide().isEmpty());
        QVERIFY(show.clearMedia().isEmpty());
        QVERIFY(show.clearProps().isEmpty());
        QCOMPARE(outside(show.clearAll()), "");
    }

    void clearingEverythingTakesTheSlideTheMediaAndTheProps()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        show.goLive(song.cue(0), false, workspace());
        show.toggleProp("p1", workspace());
        QCOMPARE(outside(show.clearAll()), "noslide nomedia");
        QVERIFY(show.cleared && !show.clearedByCue && !show.hasMedia && show.props.isEmpty());
        // It is still at the slide, for stepping.
        QCOMPARE(show.index, 0);
        QVERIFY(show.atSlide);
    }

    // ---- media from the media bin

    void mediaFromTheBinIsStartedUnlessItIsPlayingAlready()
    {
        State show;
        QCOMPARE(outside(show.showMedia(video("waves.mp4"), "bin1")), "media");
        QCOMPARE(show.mediaPlaylistId, "bin1");
        const Effects again = show.showMedia(video("waves.mp4"), "bin2");
        QCOMPARE(outside(again), "");
        QVERIFY(noted(again, "is playing already and is left to"));
        // It is the one picked all the same: the playlist it was picked from follows.
        QCOMPARE(show.mediaPlaylistId, "bin2");
        QCOMPARE(outside(show.showMedia(video("fire.mp4"), "bin2")), "media");
        QCOMPARE(show.media.value("name").toString(), "fire.mp4");
    }

    void mediaFromTheBinLeavesTheSlideLayerAlone()
    {
        State show;
        const Presentation song = plainSong();
        show.goLive(song.cue(1), false, workspace());
        show.showMedia(video("waves.mp4"), "");
        QVERIFY(!show.cleared);
        QCOMPARE(show.index, 1);
    }

    // ---- props

    void propsStackInTheOrderTheyWereTurnedOn()
    {
        State show;
        const Workspace w = workspace();
        show.toggleProp("p2", w);
        show.toggleProp("p1", w);
        QCOMPARE(show.props, (QStringList {"p2", "p1"}));
        show.toggleProp("p2", w);
        QCOMPARE(show.props, QStringList {"p1"});
        // A prop that is not in the workspace is not turned on.
        QVERIFY(show.toggleProp("nope", w).isEmpty());
        QCOMPARE(show.props, QStringList {"p1"});
    }

    void aCollectionThatShowsOneAtATimeGivesUpTheOther()
    {
        State show;
        const Workspace w = workspace();
        show.toggleProp("p1", w);
        show.toggleProp("p3", w);
        const Effects effects = show.toggleProp("p4", w);
        QCOMPARE(show.props, (QStringList {"p1", "p4"}));
        QVERIFY(noted(effects, "in place of another of its collection"));
    }

    void settingAPropOnOrOffIsTheSameDoneTwice()
    {
        State show;
        const Workspace w = workspace();
        show.setProp("p1", true, w);
        QVERIFY(show.setProp("p1", true, w).isEmpty());
        QCOMPARE(show.props, QStringList {"p1"});
        show.setProp("p1", false, w);
        QVERIFY(show.setProp("p1", false, w).isEmpty());
        QVERIFY(show.props.isEmpty());
    }

    void aPropThatIsNoLongerThereIsNoLongerOn()
    {
        State show;
        Workspace w = workspace();
        show.toggleProp("p1", w);
        show.toggleProp("p3", w);
        QVERIFY(!show.dropMissingProps(w));
        w.props.removeLast();
        QVERIFY(show.dropMissingProps(w));
        QCOMPARE(show.props, QStringList {"p1"});
    }

    void aPropActionFindsItsPropByIdOrFailingThatByName()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {propAction("elsewhere", "Clock")}), slide("b", {}, {propAction("elsewhere", "Clock", true)}),
                       slide("c", {}, {propAction("x", "No such")})};
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(show.props, QStringList {"p2"});
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(show.props, QStringList {"p2"});
        show.goLive(song.cue(1), false, workspace());
        QVERIFY(show.props.isEmpty());
        QVERIFY(noted(show.goLive(song.cue(2), false, workspace()), "there is no such prop here"));
    }

    // ---- macros

    void aMacroRunByHandDoesItsActions()
    {
        State show;
        const Workspace w = workspace();
        show.showMedia(video("waves.mp4"), "");
        show.toggleProp("p1", w);
        const Effects effects = show.runMacro("tidy", w);
        QCOMPARE(outside(effects), "nomedia");
        QVERIFY(show.props.isEmpty() && !show.hasMedia);
        QVERIFY(noted(effects, "\"Tidy\" run by hand: 2 actions"));
        QVERIFY(show.runMacro("no such", w).isEmpty());
    }

    void aSlideClearedByAMacroRunByHandIsNotMarked()
    {
        State show;
        const Presentation song = plainSong();
        show.goLive(song.cue(1), false, workspace());
        show.runMacro("blank", workspace());
        QVERIFY(show.cleared);
        QVERIFY(!show.clearedByCue);
        QVERIFY(!show.cueLive());
    }

    void aSlideClearedByAMacroOfItsOwnCueIsMarked()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {macroAction("blank")})};
        show.goLive(song.cue(0), false, workspace());
        QVERIFY(show.cleared && show.clearedByCue && show.cueLive());
    }

    void aMacroCanRunAMacroAndIsFoundByNameFailingItsId()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"), {macroAction("made elsewhere", "Outer")}), slide("b", {}, {macroAction("x", "No such")})};
        // Outer puts the logo on and runs Tidy, which takes the props and the media off.
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slidewithmedia nomedia");
        QVERIFY(show.props.isEmpty() && !show.hasMedia && !show.cleared);
        QVERIFY(noted(show.goLive(song.cue(1), false, workspace()), "there is no such macro here"));
    }

    void macrosThatRunEachOtherAreStopped()
    {
        State show;
        const Effects effects = show.runMacro("loop", workspace());
        QCOMPARE(outside(effects), "problem");
        QVERIFY(noted(effects, "gone round eight times"));
    }

    // ---- the stage

    void aStageActionGivesTheStageItsLayout()
    {
        State show;
        Presentation song;
        song.slides = {
            slide("a", {}, {stageAction({assignment("screen1", "Stage", "l2", "Clock")})}),
            // Made where the layouts have other ids: found by name.
            slide("b", {}, {stageAction({assignment("screen1", "Stage", "other", "Words")})}),
            // A layout this workspace has not: nothing changes.
            slide("c", {}, {stageAction({assignment("screen1", "Stage", "other", "No such")})}),
            // "No change" for this screen.
            slide("d", {}, {stageAction({assignment("screen1", "Stage", "", "")})}),
        };
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(show.stageLayouts.value("screen1"), "l2");
        show.goLive(song.cue(1), false, workspace());
        QCOMPARE(show.stageLayouts.value("screen1"), "l1");
        QVERIFY(noted(show.goLive(song.cue(2), false, workspace()), "nothing to change here"));
        QCOMPARE(show.stageLayouts.value("screen1"), "l1");
        show.goLive(song.cue(3), false, workspace());
        QCOMPARE(show.stageLayouts.value("screen1"), "l1");
    }

    void aStageActionGivesEachStageScreenItsOwnLayout()
    {
        State show;
        const Workspace w = workspace();
        Presentation song;
        song.slides = {
            // Both screens, the second named only by its name
            slide("a", {}, {stageAction({assignment("screen1", "Stage", "l1", "Words"), assignment("elsewhere", "Lobby", "l2", "Clock")})}),
            // The lobby alone: the stage is left as it is
            slide("b", {}, {stageAction({assignment("screen2", "Lobby", "l1", "Words")})}),
            // The lobby left as it is, the stage changed
            slide("c", {}, {stageAction({assignment("screen1", "Stage", "l2", "Clock"), assignment("screen2", "Lobby", "", "")})}),
        };
        show.goLive(song.cue(0), false, w);
        QCOMPARE(show.stageLayouts.value("screen1"), "l1");
        QCOMPARE(show.stageLayouts.value("screen2"), "l2");
        show.goLive(song.cue(1), false, w);
        QCOMPARE(show.stageLayouts.value("screen1"), "l1");
        QCOMPARE(show.stageLayouts.value("screen2"), "l1");
        show.goLive(song.cue(2), false, w);
        QCOMPARE(show.stageLayouts.value("screen1"), "l2");
        QCOMPARE(show.stageLayouts.value("screen2"), "l1");
        // By hand, and back to the plain view
        show.setStageLayout("screen2", "");
        QVERIFY(!show.stageLayouts.contains("screen2"));
    }

    void anActionMadeWhereTheScreensAreOthersGoesToTheFirstStageScreen()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {stageAction({assignment("x", "Left", "", ""), assignment("y", "Right", "l2", "Clock")})})};
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(show.stageLayouts.value("screen1"), "l2");
        QVERIFY(!show.stageLayouts.contains("screen2"));
    }

    void whichOfAStageActionsScreensIsThisApps()
    {
        const Workspace w = workspace();
        // By id, whatever it is called there
        QCOMPARE(stageAssignmentOf(stageAction({assignment("screen2", "Lobby", "l1", ""), assignment("screen1", "Platform", "l2", "")}), w), 1);
        // By name, where the ids are others
        QCOMPARE(stageAssignmentOf(stageAction({assignment("a", "Lobby", "l1", ""), assignment("b", "Stage", "", "")}), w), 1);
        // Failing both, the first that is given a layout
        QCOMPARE(stageAssignmentOf(stageAction({assignment("a", "Left", "", ""), assignment("b", "Right", "l2", "")}), w), 1);
        QCOMPARE(stageAssignmentOf(stageAction({assignment("a", "Left", "", "")}), w), -1);
        QCOMPARE(stageAssignmentOf(stageAction({}), w), -1);
        QVERIFY(stageLayoutOf(stageAction({}), w).isEmpty());
        // A workspace ProPresenter has set no screens up for has the one, called Stage.
        Workspace bare = w;
        bare.stageScreens.clear();
        QCOMPARE(stageScreen(bare).value("name").toString(), "Stage");
        QCOMPARE(stageAssignmentOf(stageAction({assignment("anything", "Stage", "l1", "")}), bare), 0);
    }

    void aNewStageActionNamesEveryScreenAndAChangedOneKeepsTheOthers()
    {
        const Workspace w = workspace();
        const QVariantMap clock = w.stageLayouts.at(1).toMap();
        const QVariantList made = stageAssignments({}, clock, w);
        QCOMPARE(made.size(), 2);
        QCOMPARE(made.at(0).toMap(), assignment("screen1", "Stage", "l2", "Clock"));
        QCOMPARE(made.at(1).toMap(), assignment("screen2", "Lobby", "", ""));
        const QVariantMap existing = stageAction({assignment("screen2", "Lobby", "l1", "Words"), assignment("screen1", "Stage", "l1", "Words")});
        const QVariantList changed = stageAssignments(existing, clock, w);
        QCOMPARE(changed.at(0).toMap(), assignment("screen2", "Lobby", "l1", "Words"));
        QCOMPARE(changed.at(1).toMap(), assignment("screen1", "Stage", "l2", "Clock"));
        // With no layout: left as it is.
        QCOMPARE(stageAssignments(existing, QVariantMap(), w).at(1).toMap(), assignment("screen1", "Stage", "", ""));
    }

    void aStageActionCanGiveSeveralScreensLayoutsAtOnce()
    {
        const Workspace w = workspace();
        const QVariantMap words = w.stageLayouts.at(0).toMap();
        const QVariantMap clock = w.stageLayouts.at(1).toMap();
        const QVariantList made = stageAssignments({}, QMap<QString, QVariantMap> {{"screen1", words}, {"screen2", clock}}, w);
        QCOMPARE(made.at(0).toMap(), assignment("screen1", "Stage", "l1", "Words"));
        QCOMPARE(made.at(1).toMap(), assignment("screen2", "Lobby", "l2", "Clock"));
        // Changing one that names only the stage: the lobby gets a line when it is given a layout.
        const QVariantMap existing = stageAction({assignment("screen1", "Stage", "l1", "Words")});
        QCOMPARE(stageAssignments(existing, QMap<QString, QVariantMap> {{"screen2", QVariantMap()}}, w).size(), 1);
        const QVariantList changed = stageAssignments(existing, QMap<QString, QVariantMap> {{"screen2", clock}}, w);
        QCOMPARE(changed.size(), 2);
        QCOMPARE(changed.at(0).toMap(), assignment("screen1", "Stage", "l1", "Words"));
        QCOMPARE(changed.at(1).toMap(), assignment("screen2", "Lobby", "l2", "Clock"));
        const QMap<QString, QVariantMap> read = stageLayoutsOf(stageAction(changed), w);
        QCOMPARE(read.value("screen1").value("id").toString(), "l1");
        QCOMPARE(read.value("screen2").value("id").toString(), "l2");
    }

    // ---- looks

    void aLookIsMadeLiveByHandOrByAnAction()
    {
        State show;
        const Workspace w = workspace();
        QVERIFY(show.lookId.isEmpty());
        const auto asked = [](const show::Effects &effects) {
            QStringList ids;
            for (const show::Effect &effect : effects) {
                if (effect.kind == show::Effect::Look)
                    ids << effect.text;
            }
            return ids;
        };
        show::Effects effects = show.setLook("third", w);
        QVERIFY(noted(effects, "\"Lower Third\" is live"));
        QCOMPARE(asked(effects), QStringList {"third"});
        QCOMPARE(show.lookId, "third");
        // Asked for again, it is made live again: that is what puts the live look back
        // as the saved look has it. One that is not there is not gone over to.
        QCOMPARE(asked(show.setLook("third", w)), QStringList {"third"});
        QVERIFY(show.setLook("no such", w).isEmpty());
        QVERIFY(show.setLook("", w).isEmpty());
        QCOMPARE(show.lookId, "third");
        // An action names a look by its id, or failing that by its name
        Presentation song;
        const auto lookAction = [](const QString &id, const QString &name) {
            return QVariantMap {{"kind", "look"}, {"title", "Look"}, {"done", true}, {"lookId", id}, {"lookName", name}};
        };
        song.slides = {slide("a", {}, {lookAction("full", "")}), slide("b", {}, {lookAction("made elsewhere", "Lower Third")}),
                       slide("c", {}, {lookAction("x", "No such")})};
        QCOMPARE(asked(show.goLive(song.cue(0), false, w)), QStringList {"full"});
        QCOMPARE(show.lookId, "full");
        QCOMPARE(asked(show.goLive(song.cue(1), false, w)), QStringList {"third"});
        QCOMPARE(show.lookId, "third");
        effects = show.goLive(song.cue(2), false, w);
        QVERIFY(noted(effects, "there is no such look here"));
        QVERIFY(asked(effects).isEmpty());
        QCOMPARE(show.lookId, "third");
        // What a workspace being opened knows of the look that was live: said, with nothing made live
        show.adoptLook("full");
        QCOMPARE(show.lookId, "full");
        show.adoptLook("");
        QVERIFY(show.lookId.isEmpty());
    }

    // ---- a presentation that changes under the show

    void theShowFollowsItsSlideThroughAPresentationReadAgain()
    {
        // A slide used twice (as an arrangement does): the one as far along as it was.
        QCOMPARE(placeAfterReload({"a", "b", "a", "c"}, 2, {"x", "a", "b", "a", "c"}), 3);
        QCOMPARE(placeAfterReload({"a", "b", "a", "c"}, 0, {"x", "a", "b", "a", "c"}), 1);
        QCOMPARE(placeAfterReload({"a", "b"}, 1, {"b", "a"}), 0);
        // Gone, or never anywhere
        QCOMPARE(placeAfterReload({"a", "b"}, 1, {"a"}), -1);
        QCOMPARE(placeAfterReload({"a", "b", "a"}, 2, {"a", "b"}), -1);
        QCOMPARE(placeAfterReload({"a"}, 5, {"a"}), -1);
        QCOMPARE(placeAfterReload({}, -1, {"a"}), -1);
    }

    void followingChangesWhereTheShowIsAndNothingOnTheOutput()
    {
        State show;
        Presentation song = plainSong();
        show.goLive(song.cue(1), false, workspace());
        show.clearSlide();
        song.slides.prepend(slide("new"));
        show.follow(song.cue(2));
        QCOMPARE(show.index, 2);
        QCOMPARE(show.count, 5);
        QCOMPARE(show.slide.value("id").toString(), "s1");
        QVERIFY(show.cleared);
        // A show that is nowhere has nothing to follow.
        State nowhere;
        nowhere.follow(song.cue(2));
        QVERIFY(!nowhere.atSlide);
        QCOMPARE(nowhere.index, -1);
    }

    void leavingAPresentationLeavesTheMediaAndThePropsAlone()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        show.goLive(song.cue(0), false, workspace());
        show.toggleProp("p1", workspace());
        show.leavePresentation();
        QVERIFY(!show.atSlide);
        QCOMPARE(show.index, -1);
        QVERIFY(!show.at("song.pro", ""));
        QVERIFY(show.hasMedia);
        QCOMPARE(show.props, QStringList {"p1"});
    }
};

QTEST_APPLESS_MAIN(TestShowState)
#include "tst_showstate.moc"
