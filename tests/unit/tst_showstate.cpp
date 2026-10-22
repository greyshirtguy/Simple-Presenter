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

// A row of a media playlist, as the bin has it: a video that stops at its end, with
// what follows it (mediacue.h), its cue's id being its name's
QVariantMap mediaRow(const QString &name, const QString &next = {}, const QString &nextCue = {}, bool missing = false)
{
    QVariantMap row = video(name, false, 0);
    row.insert("id", "row-" + name);
    row.insert("cueId", "cue-" + name);
    row.insert("missing", missing);
    if (missing)
        row.insert("path", QString());
    if (!next.isEmpty())
        row.insert("next", next);
    if (!nextCue.isEmpty())
        row.insert("nextCue", nextCue);
    return row;
}

// A piece of audio, as a row of the audio bin is: music that plays once, unless said
// otherwise.
QVariantMap tune(const QString &name, const QString &playback = QStringLiteral("stop"))
{
    return {{"id", "row-" + name}, {"name", name}, {"path", "/media/" + name + ".mp3"}, {"missing", false}, {"playback", playback},
            {"effect", false}, {"volume", 1.0}, {"duration", 60.0}, {"loopCount", 0}, {"loopSeconds", 0.0}};
}

QVariantMap soundEffect(const QString &name)
{
    QVariantMap sound = tune(name);
    sound.insert("effect", true);
    return sound;
}

QVariantMap audioAction(const QVariantMap &audio)
{
    return {{"kind", "audio"}, {"title", "Play the audio"}, {"done", true}, {"audio", audio}};
}

QStringList namesOf(const QVariantList &audio)
{
    QStringList names;
    for (const QVariant &one : audio)
        names << one.toMap().value("name").toString();
    return names;
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

QVariantMap clearGroupAction(const QString &id, const QString &name = {})
{
    return {{"kind", "clearGroup"}, {"title", "Clear the group"}, {"done", true}, {"groupId", id}, {"groupName", name}};
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
    // Where it is sent: "" or "announcements"
    QString destination;
    // Its Slide Show, in seconds; 0 for none
    double slideShow = 0;
    // Its copyright details
    QVariantMap ccli;
    // Its file
    QString file = "/library/song.pro";

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
        cue.destination = destination;
        cue.slideShow = slideShow;
        cue.ccli = ccli;
        cue.file = file;
        return cue;
    }
};

// A slide with a Go To Next Timer.
QVariantMap timed(QVariantMap slide, double seconds, bool loop = false)
{
    slide.insert("goToNext", QVariantMap {{"seconds", seconds}, {"loop", loop}});
    return slide;
}

// A presentation that is sent to the announcements: notices, of plain slides unless it
// is given others.
Presentation notices(int count = 3)
{
    Presentation made;
    made.key = "notices.pro";
    made.name = "Notices";
    made.destination = "announcements";
    for (int i = 0; i < count; ++i)
        made.slides << slide(QStringLiteral("n%1").arg(i));
    return made;
}

Presentation plainSong(int count = 4)
{
    Presentation song;
    for (int i = 0; i < count; ++i)
        song.slides << slide(QStringLiteral("s%1").arg(i));
    return song;
}

// A workspace with two collections of props (the second showing one at a time), a few
// macros, two stage layouts, and three clear groups: one as ProPresenter starts a
// workspace with, of all its layers; one of the props and layers this app has not; and
// one of nothing.
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
    w.clearGroups = {
        QVariantMap {{"id", "all"}, {"name", "Clear All"},
                     {"layers", QStringList {"audio", "audioEffects", "messages", "props", "announcements", "slide", "media", "videoInput"}}},
        QVariantMap {{"id", "extras"}, {"name", "Extras"}, {"layers", QStringList {"audio", "messages", "props"}}},
        QVariantMap {{"id", "none"}, {"name", "Nothing"}, {"layers", QStringList {}}},
    };
    // Two messages: one that stays until it is hidden, one that goes after ten seconds
    w.messages = {QVariantMap {{"id", "parent"}, {"name", "Parent Wanted"}, {"seconds", 0.0}},
                  QVariantMap {{"id", "lights"}, {"name", "Car Lights"}, {"seconds", 10.0}}};
    return w;
}

QVariantMap messageAction(const QString &id, const QString &name = {}, const QVariantList &values = {})
{
    return {{"kind", "message"}, {"title", "Show the message"}, {"done", true}, {"messageId", id}, {"messageName", name}, {"values", values}};
}

QVariantMap playlistAction(const QString &kind, const QString &id, const QString &name = {}, bool retrigger = false, const QString &item = {})
{
    return {{"kind", kind}, {"title", "Play the playlist"}, {"done", true}, {"playlistId", id}, {"playlistName", name},
            {"retrigger", retrigger}, {"itemId", item}, {"itemName", QString()}};
}

// The workspace above, with a playlist in each bin, which an action can find: of the
// media bin, "Loops", whose first row is not on this computer; and of the audio bin,
// "Before", of three pieces that are each followed by the next. And an empty one, of
// nothing that can be played.
Workspace withPlaylists()
{
    Workspace w = workspace();
    w.playlist = [](const QString &bin, const QString &id, const QString &name) -> QVariantMap {
        QVariantMap gone = video("Gone");
        gone.insert("id", "g");
        gone.insert("missing", true);
        gone.insert("path", QString());
        QVariantMap waves = video("Waves");
        waves.insert("id", "w");
        QVariantMap fire = video("Fire");
        fire.insert("id", "f");
        const QVariantList lists = bin == QLatin1String("media")
            ? QVariantList {QVariantMap {{"id", "loops"}, {"name", "Loops"}, {"rows", QVariantList {gone, waves, fire}}},
                            QVariantMap {{"id", "empty"}, {"name", "Empty"}, {"rows", QVariantList {gone}}}}
            : QVariantList {QVariantMap {{"id", "before"}, {"name", "Before"},
                                         {"rows", QVariantList {tune("A", "next"), tune("B", "next"), tune("C", "next")}}}};
        for (int pass = 0; pass < 2; ++pass) {
            for (const QVariant &entry : lists) {
                const QVariantMap list = entry.toMap();
                if (pass == 0 ? list.value("id") == id : list.value("name") == name)
                    return list;
            }
        }
        return {};
    };
    return w;
}

// The times that were set going or stopped, in order: "lights:10 parent:0"
QString times(const Effects &effects)
{
    QStringList said;
    for (const Effect &effect : effects) {
        if (effect.kind == Effect::MessageTime)
            said << effect.text + QLatin1Char(':') + QString::number(effect.seconds);
    }
    return said.join(u' ');
}

// The kinds of what was to happen outside, in order, the log left out: "slide nomedia".
QString outside(const Effects &effects)
{
    static const char *names[] = {"slide", "slideovermedia", "slidewithmedia", "noslide", "media", "nomedia",
                                  "announcement", "announcementovermedia", "announcementwithmedia", "noannouncements",
                                  "advance", "noadvance", "audio", "nomusic", "nosounds", "timer", "look", "message", "messagetime", "note", "problem", "build", "communicate",
                                  "videoinput", "novideoinput"};
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

    // A slide with On Click builds: Next runs them, one at a time, and then goes on; a
    // click on the slide does the same; going live again starts them again; and a
    // cleared slide, or another presentation, is not stepped so.
    void nextRunsOnClickBuildsFirst()
    {
        State show;
        Presentation song = plainSong();
        QVariantMap built = song.slides.at(1).toMap();
        built.insert("buildSteps", 2);
        song.slides[1] = built;
        bool ran = false;
        show.goLive(song.cue(1), false, workspace());
        QCOMPARE(show.buildStep, 0);
        Effects effects = show.stepBuild("song.pro", "", -1, &ran);
        QVERIFY(ran);
        QCOMPARE(outside(effects), "build");
        QVERIFY(noted(effects, "build 1 of 2 of slide 2"));
        QCOMPARE(show.buildStep, 1);
        // Another presentation's Next, or a click on another slide, is not this one's
        QVERIFY(show.stepBuild("other.pro", "", -1, &ran).isEmpty());
        QVERIFY(!ran);
        QVERIFY(show.stepBuild("song.pro", "", 3, &ran).isEmpty());
        QVERIFY(!ran);
        // A click on the slide itself is
        QCOMPARE(outside(show.stepBuild("song.pro", "", 1, &ran)), "build");
        QVERIFY(ran);
        QCOMPARE(show.buildStep, 2);
        // None left: Next goes to the next slide
        QVERIFY(show.stepBuild("song.pro", "", -1, &ran).isEmpty());
        QVERIFY(!ran);
        QCOMPARE(show.stepTarget("song.pro", "", 1), 2);
        // Triggered again, it starts again
        show.goLive(song.cue(1), false, workspace());
        QCOMPARE(show.buildStep, 0);
        // Cleared, it is not stepped, and Next brings it back
        show.clearSlide();
        QVERIFY(show.stepBuild("song.pro", "", -1, &ran).isEmpty());
        QVERIFY(!ran);
        QCOMPARE(show.stepTarget("song.pro", "", 1), 1);
        // A slide without builds never is
        show.goLive(song.cue(2), false, workspace());
        QVERIFY(show.stepBuild("song.pro", "", -1, &ran).isEmpty());
        QVERIFY(!ran);
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

    // ---- the announcements layer

    void aPresentationSentToTheAnnouncementsGoesThereAndTheShowStaysWhereItWas()
    {
        State show;
        show.goLive(plainSong().cue(1), false, workspace());
        const Effects effects = show.goLive(notices().cue(0), false, workspace());
        QCOMPARE(outside(effects), "announcement");
        QVERIFY(noted(effects, "to the announcements, slide 1 of 3 of \"Notices\""));
        // The show is at the slide it was at, which is still on its layer.
        QVERIFY(show.at("song.pro", "") && show.index == 1 && !show.cleared && show.cueLive());
        // And the announcements are at theirs.
        QVERIFY(show.announcementAt("notices.pro", "") && show.announcement.index == 0 && show.announcement.count == 3);
        QVERIFY(!show.announcement.cleared && show.announcementLive());
        QCOMPARE(show.announcement.slide.value("id").toString(), "n0");
        QCOMPARE(show.announcement.next.value("id").toString(), "n1");
        // A slide of the song after that leaves the announcements where they are.
        QCOMPARE(outside(show.goLive(plainSong().cue(2), false, workspace())), "slide");
        QVERIFY(show.index == 2 && show.announcement.index == 0 && !show.announcement.cleared);
    }

    void theAnnouncementsHaveMediaOfTheirOwnUnderTheirSlide()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        Presentation board = notices(0);
        board.slides = {slide("n0", video("loop.mp4")), slide("n1", video("loop.mp4")), slide("n2"), slide("n3", video("sting.mp4", true, 0)),
                        slide("n4")};
        QCOMPARE(outside(show.goLive(song.cue(0), false, workspace())), "slidewithmedia");
        // Their first slide starts its video under it, on their layer: the show's own plays on.
        Effects effects = show.goLive(board.cue(0), false, workspace());
        QCOMPARE(outside(effects), "announcementwithmedia");
        QCOMPARE(effects.last().media.value("name").toString(), "loop.mp4");
        QVERIFY(show.announcementHasMedia && show.announcementMedia.value("name") == "loop.mp4");
        QVERIFY(show.hasMedia && show.media.value("name") == "waves.mp4");
        // The same background asked for again is left to play.
        QCOMPARE(outside(show.goLive(board.cue(1), false, workspace())), "announcementovermedia");
        // A slide that brings none leaves a background playing under it.
        QCOMPARE(outside(show.goLive(board.cue(2), false, workspace())), "announcement");
        QVERIFY(show.announcementHasMedia);
        // A foreground takes its place, and is ended by the next slide that brings none:
        // the layer is given that slide with no media under it.
        QCOMPARE(outside(show.goLive(board.cue(3), false, workspace())), "announcementwithmedia");
        QVERIFY(show.announcementMedia.value("foreground").toBool());
        effects = show.goLive(board.cue(4), false, workspace());
        QCOMPARE(outside(effects), "announcementwithmedia");
        QVERIFY(effects.last().media.isEmpty() && !show.announcementHasMedia);
        // None of which touched what the show itself has on.
        QVERIFY(show.hasMedia && show.media.value("name") == "waves.mp4" && show.index == 0 && !show.cleared);
        // And media triggered by itself goes to the media layer, not theirs.
        QCOMPARE(outside(show.showMedia(video("other.mp4"), "")), "media");
        QVERIFY(!show.announcementHasMedia);
    }

    void withoutItsMediaAnAnnouncementSlideIsShownAlone()
    {
        State show;
        Presentation board = notices(0);
        board.slides = {slide("n0", video("loop.mp4"))};
        const Effects effects = show.goLive(board.cue(0), true, workspace());
        QCOMPARE(outside(effects), "announcement");
        QVERIFY(!show.announcementHasMedia && noted(effects, "without its media, as asked"));
    }

    void clearingTheAnnouncementsTakesTheirSlideAndTheirMediaAndNothingElse()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        Presentation board = notices(0);
        board.slides = {slide("n0", video("loop.mp4")), slide("n1")};
        show.goLive(song.cue(0), false, workspace());
        show.goLive(board.cue(0), false, workspace());
        show.toggleProp("p1", workspace());
        const Effects effects = show.clearAnnouncements();
        QCOMPARE(outside(effects), "noannouncements");
        QVERIFY(noted(effects, "the announcements"));
        QVERIFY(show.announcement.cleared && !show.announcement.clearedByCue && !show.announcementLive() && !show.announcementHasMedia);
        // They are still at their slide, for stepping; and the rest of the show is as it was.
        QVERIFY(show.announcement.atSlide && show.announcement.index == 0);
        QVERIFY(!show.cleared && show.hasMedia && show.props.size() == 1);
        // Cleared again, there is nothing to clear.
        QCOMPARE(outside(show.clearAnnouncements()), "");
        // And the other clears leave them: the slide, the media and the props cleared, they stay.
        show.goLive(board.cue(1), false, workspace());
        show.clearSlide();
        show.clearMedia();
        show.clearProps();
        QVERIFY(!show.announcement.cleared && show.announcementLive());
    }

    void mediaLeftOnTheAnnouncementsWithNoSlideIsStillSomethingToClear()
    {
        // A slide of theirs that clears nothing but was shown with media, then their
        // layer cleared and media left: there is none, the layer goes whole. But a
        // layer with media and a cleared slide cannot come about any other way than
        // by its being asked for, so this holds the one rule: either is enough.
        State show;
        Presentation board = notices(0);
        board.slides = {slide("n0", video("loop.mp4"))};
        show.goLive(board.cue(0), false, workspace());
        QCOMPARE(outside(show.clearAnnouncements()), "noannouncements");
        QVERIFY(!show.announcementHasMedia && show.announcement.cleared);
    }

    void clearingEverythingAndAGroupThatHasThemClearTheAnnouncements()
    {
        State show;
        show.goLive(plainSong().cue(0), false, workspace());
        show.goLive(notices().cue(1), false, workspace());
        QCOMPARE(outside(show.clearAll()), "noslide noannouncements");
        QVERIFY(show.cleared && show.announcement.cleared);
        // The group of every layer does the same; one that has not the announcements leaves them.
        show.goLive(plainSong().cue(0), false, workspace());
        show.goLive(notices().cue(1), false, workspace());
        QCOMPARE(outside(show.clearGroup("extras", workspace())), "");
        QVERIFY(!show.announcement.cleared);
        QCOMPARE(outside(show.clearGroup("all", workspace())), "noslide noannouncements");
        QVERIFY(show.announcement.cleared && show.announcement.index == 1);
    }

    void aClearActionCanNameTheAnnouncements()
    {
        // As actions.cpp describes one: every layer, of the announcements' destination.
        const QVariantMap clearThem {{"kind", "clear"}, {"title", "Clear the announcements"}, {"done", true}, {"layer", 0}, {"announcements", true}};
        State show;
        Presentation song;
        song.slides = {slide("a"), slide("b", {}, {clearThem})};
        show.goLive(song.cue(0), false, workspace());
        show.goLive(notices().cue(0), false, workspace());
        // On a slide of the song: the announcements go, and the song's slide is shown.
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slide noannouncements");
        QVERIFY(!show.cleared && show.announcement.cleared && !show.announcement.clearedByCue);
        // On a slide of theirs: that slide takes itself off, and is still the one they are at.
        Presentation board = notices(0);
        board.slides = {slide("n0"), slide("n1", {}, {clearThem})};
        QCOMPARE(outside(show.goLive(board.cue(1), false, workspace())), "announcement noannouncements");
        QVERIFY(show.announcement.cleared && show.announcement.clearedByCue && show.announcementLive() && show.announcement.index == 1);
        // An action of theirs that clears the slide clears the show's slide, as it says, and not theirs.
        board.slides = {slide("n0", {}, {clearAction(5)})};
        QCOMPARE(outside(show.goLive(board.cue(0), false, workspace())), "announcement noslide");
        QVERIFY(show.cleared && !show.clearedByCue && !show.announcement.cleared);
    }

    void theAnnouncementsAreSteppedFromWhereTheyAre()
    {
        State show;
        show.goLive(plainSong().cue(2), false, workspace());
        show.goLive(notices().cue(0), false, workspace());
        QCOMPARE(show.stepTarget("notices.pro", "", 1, true), 1);
        QCOMPARE(show.stepTarget("notices.pro", "", -1, true), -1);
        // Another presentation looked at as announcements starts at its top.
        QCOMPARE(show.stepTarget("other.pro", "", 1, true), 0);
        // Cleared, a step brings back the slide they were at.
        show.clearAnnouncements();
        QCOMPARE(show.stepTarget("notices.pro", "", 1, true), 0);
        // And the show's own steps go from the show's own slide throughout.
        QCOMPARE(show.stepTarget("song.pro", "", 1), 3);
        QCOMPARE(show.stepTarget("notices.pro", "", 1), 0);
    }

    void theAnnouncementsFollowTheirPresentationWhenItIsReadAgain()
    {
        State show;
        show.goLive(plainSong().cue(1), false, workspace());
        show.goLive(notices().cue(0), false, workspace());
        Presentation longer = notices(5);
        show.follow(longer.cue(2));
        QVERIFY(show.announcement.index == 2 && show.announcement.count == 5 && show.announcement.slide.value("id") == "n2");
        // The show's own place is not theirs to move, nor theirs the show's.
        QVERIFY(show.index == 1 && show.count == 4);
        show.follow(plainSong(6).cue(3));
        QVERIFY(show.index == 3 && show.count == 6 && show.announcement.index == 2);
        // A library's file renamed: whichever is at it goes by the new name.
        show.renamed("notices.pro", "foyer.pro");
        QVERIFY(show.announcementAt("foyer.pro", "") && show.at("song.pro", ""));
    }

    // ---- going on by itself: Go To Next Timers and the Slide Show

    void aSlideWithAGoToNextTimerLeadsToTheNextWhenItsTimeIsUp()
    {
        State show;
        Presentation song;
        song.slides = {timed(slide("a"), 5), slide("b"), slide("c")};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide advance");
        QCOMPARE(effects.last().seconds, 5.0);
        QVERIFY(effects.last().text.isEmpty());
        QVERIFY(noted(effects, "the show goes on to the next slide in 5 s, by the slide's Go To Next Timer"));
        QVERIFY(show.advance.waiting && show.advance.seconds == 5.0 && !show.announcementAdvance.waiting);
        // The time is up: the next slide, and nothing is waiting until that slide is live.
        QCOMPARE(show.takeAdvance(false), 1);
        QVERIFY(!show.advance.waiting);
        QCOMPARE(show.takeAdvance(false), -1);
        // The slide it led to has no timer: it stays.
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace(), true)), "slide");
        QVERIFY(!show.advance.waiting);
    }

    void loopToBeginningLeadsToTheFirstSlideFromWhereverItIs()
    {
        State show;
        Presentation song;
        song.slides = {slide("a"), timed(slide("b"), 3, true), timed(slide("c"), 2.5, true)};
        Effects effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(outside(effects), "slide advance");
        QVERIFY(noted(effects, "on to the first slide in 3 s"));
        QCOMPARE(show.takeAdvance(false), 0);
        effects = show.goLive(song.cue(2), false, workspace());
        QVERIFY(noted(effects, "in 2.5 s"));
        QCOMPARE(show.takeAdvance(false), 0);
    }

    void aTimerOnTheLastSlideThatDoesNotLoopHasNowhereToGo()
    {
        State show;
        Presentation song;
        song.slides = {slide("a"), timed(slide("b"), 5)};
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slide");
        QVERIFY(!show.advance.waiting);
    }

    void aSlideShowLeadsEverySlideToTheNextAndTheLastToTheFirst()
    {
        State show;
        Presentation song = plainSong(3);
        song.slideShow = 10;
        Effects effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(outside(effects), "slide advance");
        QCOMPARE(effects.last().seconds, 10.0);
        QVERIFY(noted(effects, "on to the next slide in 10 s, by the presentation's Slide Show"));
        QCOMPARE(show.takeAdvance(false), 2);
        // That slide, live by the timer, starts the time again; and being the last leads to the first.
        QCOMPARE(outside(show.goLive(song.cue(2), false, workspace(), true)), "slide advance");
        QCOMPARE(show.takeAdvance(false), 0);
        // A presentation without one leads nowhere.
        QCOMPARE(outside(show.goLive(plainSong(3).cue(0), false, workspace())), "slide");
    }

    void aSlidesOwnTimerWinsOverThePresentationsSlideShow()
    {
        State show;
        Presentation song;
        song.slideShow = 10;
        song.slides = {slide("a"), timed(slide("b"), 2), timed(slide("c"), 4)};
        Effects effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(effects.last().seconds, 2.0);
        QVERIFY(noted(effects, "by the slide's Go To Next Timer"));
        // The last slide's own timer does not loop, so it stays there: its own wins, even to stop at.
        effects = show.goLive(song.cue(2), false, workspace());
        QCOMPARE(outside(effects), "slide noadvance");
        QVERIFY(!show.advance.waiting);
        // And a slide with none of its own goes by the presentation's.
        effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(effects.last().seconds, 10.0);
    }

    void whatIsWaitingIsOffWhenAnotherSlideGoesLiveOrTheSlideIsCleared()
    {
        State show;
        Presentation song;
        song.slides = {timed(slide("a"), 5), slide("b"), slide("c")};
        show.goLive(song.cue(0), false, workspace());
        // Another slide, which has no timer
        QCOMPARE(outside(show.goLive(song.cue(2), false, workspace())), "slide noadvance");
        QVERIFY(!show.advance.waiting);
        // The slide cleared
        show.goLive(song.cue(0), false, workspace());
        Effects effects = show.clearSlide();
        QCOMPARE(outside(effects), "noadvance noslide");
        QVERIFY(!show.advance.waiting && noted(effects, "no longer goes on by itself"));
        // Everything cleared
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(show.clearAll()), "noadvance noslide");
        // The media cleared, or the props: the slide still leads on.
        show.goLive(song.cue(0), false, workspace());
        show.clearMedia();
        show.clearProps();
        QVERIFY(show.advance.waiting);
        // The workspace changed
        QCOMPARE(outside(show.leavePresentation()), "noadvance");
        QVERIFY(!show.advance.waiting);
        QCOMPARE(show.takeAdvance(false), -1);
    }

    void aSlideThatTakesItselfOffStillLeadsOnAndIsStoppedByClearingIt()
    {
        State show;
        Presentation song;
        song.slides = {timed(slide("a", {}, {clearAction(5)}), 5), slide("b")};
        // The slide's own action clears it: it is still the slide the show is at.
        QCOMPARE(outside(show.goLive(song.cue(0), false, workspace())), "slide noslide advance");
        QVERIFY(show.cleared && show.clearedByCue && show.advance.waiting);
        // Cleared by hand, though nothing is on the layer, it leads nowhere.
        QCOMPARE(outside(show.clearSlide()), "noadvance");
        QVERIFY(!show.advance.waiting);
    }

    void theShowAndTheAnnouncementsGoOnApart()
    {
        State show;
        Presentation song = plainSong(3);
        song.slideShow = 10;
        Presentation board = notices(3);
        board.slideShow = 5;
        show.goLive(song.cue(0), false, workspace());
        const Effects effects = show.goLive(board.cue(2), false, workspace());
        QCOMPARE(outside(effects), "announcement advance");
        QCOMPARE(effects.last().text, "announcements");
        QCOMPARE(effects.last().seconds, 5.0);
        QVERIFY(noted(effects, "the announcements go on to the next slide in 5 s"));
        QVERIFY(show.advance.waiting && show.advance.seconds == 10.0 && show.announcementAdvance.waiting && show.announcementAdvance.seconds == 5.0);
        // Theirs is the last of three: round to the first. The show's is the first: on to the second.
        QCOMPARE(show.takeAdvance(true), 0);
        QVERIFY(show.advance.waiting);
        QCOMPARE(show.takeAdvance(false), 1);
        // Clearing the one leaves the other's waiting.
        show.goLive(song.cue(0), false, workspace());
        show.goLive(board.cue(0), false, workspace());
        QCOMPARE(outside(show.clearAnnouncements()), "noadvance noannouncements");
        QVERIFY(show.advance.waiting && !show.announcementAdvance.waiting);
        show.goLive(board.cue(0), false, workspace());
        QCOMPARE(outside(show.clearSlide()), "noadvance noslide");
        QVERIFY(!show.advance.waiting && show.announcementAdvance.waiting);
        // A slide of the song that clears the announcements stops theirs.
        const QVariantMap clearThem {{"kind", "clear"}, {"title", "Clear the announcements"}, {"done", true}, {"layer", 0}, {"announcements", true}};
        song.slides[1] = slide("b", {}, {clearThem});
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace())), "slide noadvance noannouncements advance");
        QVERIFY(show.advance.waiting && !show.announcementAdvance.waiting);
    }

    void aTimeOfNothingGoesOnAtOnceButNotRoundForEver()
    {
        State show;
        Presentation song;
        song.slides = {timed(slide("a"), 0), timed(slide("b"), 0), timed(slide("c"), 0, true)};
        Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide advance");
        QCOMPARE(effects.last().seconds, 0.0);
        QCOMPARE(show.takeAdvance(false), 1);
        QCOMPARE(outside(show.goLive(song.cue(1), false, workspace(), true)), "slide advance");
        QCOMPARE(show.takeAdvance(false), 2);
        QCOMPARE(outside(show.goLive(song.cue(2), false, workspace(), true)), "slide advance");
        QCOMPARE(show.takeAdvance(false), 0);
        // All the way round with no time passing: it is stopped where it began, and said to be.
        effects = show.goLive(song.cue(0), false, workspace(), true);
        QCOMPARE(outside(effects), "slide problem");
        QVERIFY(noted(effects, "would never stop") && !show.advance.waiting);
        // Triggered by hand it goes round once more.
        QCOMPARE(outside(show.goLive(song.cue(0), false, workspace())), "slide advance");
        // A slide that takes time between makes it a loop like any other.
        song.slides[1] = timed(slide("b"), 1);
        for (int round = 0; round < 3; ++round) {
            for (int i = 0; i < 3; ++i) {
                QCOMPARE(outside(show.goLive(song.cue(i), false, workspace(), round + i > 0)), "slide advance");
                QCOMPARE(show.takeAdvance(false), (i + 1) % 3);
            }
        }
    }

    void whatIsWaitingFollowsTheSlideWhenItsPresentationIsReadAgain()
    {
        State show;
        Presentation song;
        song.slides = {timed(slide("a"), 5), slide("b"), slide("c")};
        show.goLive(song.cue(0), false, workspace());
        // Read again with the timer still on the slide, wherever the slide now is: it runs on.
        Presentation reordered;
        reordered.slides = {slide("c"), timed(slide("a"), 8), slide("b")};
        QCOMPARE(outside(show.follow(reordered.cue(1))), "");
        QVERIFY(show.advance.waiting && show.advance.seconds == 5.0);
        // When its time is up it leads on from where the slide is now.
        QCOMPARE(show.takeAdvance(false), 2);
        // Read again with the timer taken off: it is off.
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(show.follow(plainSong(3).cue(0))), "noadvance");
        QVERIFY(!show.advance.waiting);
        // And a timer or a Slide Show given while its slide is on starts nothing: a slide has to be triggered.
        Presentation shown = plainSong(3);
        shown.slideShow = 10;
        QCOMPARE(outside(show.follow(shown.cue(0))), "");
        QVERIFY(!show.advance.waiting);
        QCOMPARE(outside(show.goLive(shown.cue(0), false, workspace())), "slide advance");
        // The Slide Show taken off again while it is waiting: off.
        QCOMPARE(outside(show.follow(plainSong(3).cue(0))), "noadvance");
    }

    // ---- clear groups

    void aClearGroupClearsThoseOfItsLayersThatAreHere()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        show.goLive(song.cue(0), false, workspace());
        show.toggleProp("p1", workspace());
        // The props, and the audio and the messages, of which there are none: the slide and its media stay.
        const Effects effects = show.clearGroup("extras", workspace());
        QCOMPARE(outside(effects), "");
        QVERIFY(show.props.isEmpty() && !show.cleared && show.hasMedia);
        QVERIFY(noted(effects, "the clear group \"Extras\""));
    }

    void theGroupOfEveryLayerClearsAllThereIsHere()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        show.goLive(song.cue(0), false, workspace());
        show.toggleProp("p1", workspace());
        QCOMPARE(outside(show.clearGroup("all", workspace())), "noslide nomedia");
        QVERIFY(show.cleared && !show.clearedByCue && !show.hasMedia && show.props.isEmpty());
        // It is still at the slide, for stepping, as when everything is cleared.
        QCOMPARE(show.index, 0);
        QVERIFY(show.atSlide);
    }

    void aGroupOfNoLayersAndOneTheWorkspaceHasNotClearNothing()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"))};
        show.goLive(song.cue(0), false, workspace());
        show.toggleProp("p1", workspace());
        QCOMPARE(outside(show.clearGroup("none", workspace())), "");
        QVERIFY(show.clearGroup("gone", workspace()).isEmpty());
        // (By hand a group is asked for by its id alone: there is no name to go by.)
        QVERIFY(show.clearGroup("", workspace()).isEmpty());
        QVERIFY(!show.cleared && show.hasMedia && show.props.size() == 1);
    }

    void anActionClearsItsGroupFoundByIdOrFailingThatByName()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4")), slide("b", {}, {clearGroupAction("extras", "Whatever")}),
                       slide("c", {}, {clearGroupAction("an id from elsewhere", "Extras")}),
                       slide("d", {}, {clearGroupAction("an id from elsewhere", "No Such Group")})};
        show.goLive(song.cue(0), false, workspace());
        show.toggleProp("p1", workspace());
        show.goLive(song.cue(1), false, workspace());
        QVERIFY(show.props.isEmpty() && !show.cleared && show.hasMedia);
        show.toggleProp("p2", workspace());
        show.goLive(song.cue(2), false, workspace());
        QVERIFY(show.props.isEmpty());
        show.toggleProp("p2", workspace());
        const Effects effects = show.goLive(song.cue(3), false, workspace());
        QVERIFY(show.props.size() == 1 && noted(effects, "there is no such clear group here"));
    }

    void aSlideWhoseActionClearsAGroupWithTheSlideInItHasClearedItself()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", video("waves.mp4"), {clearGroupAction("all")}), slide("b")};
        QCOMPARE(outside(show.goLive(song.cue(0), false, workspace())), "slidewithmedia noslide nomedia");
        // As with an action that clears the slide: it is still the slide the show is at,
        // and the next step goes on from it.
        QVERIFY(show.cleared && show.clearedByCue && show.cueLive() && !show.hasMedia);
        QCOMPARE(show.stepTarget(song.key, song.playlistId, 1), 1);
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

    // The same file, set otherwise in its Media Inspector since it was started, is
    // started again as it is now set: a picture as much as a video.
    void mediaSetOtherwiseIsStartedAgain()
    {
        State show;
        QVariantMap waves = video("waves.mp4");
        waves.insert("settings", QVariantMap {{"flipH", false}});
        QCOMPARE(outside(show.showMedia(waves, "bin1")), "media");
        QCOMPARE(outside(show.showMedia(waves, "bin1")), "");
        waves.insert("settings", QVariantMap {{"flipH", true}});
        QCOMPARE(outside(show.showMedia(waves, "bin1")), "media");
        // The same file in another place, set to look the same, but with another moment
        // for its thumbnail, its length said in its file and a crop that is switched
        // off: the same media, playing on
        QVariantMap there = waves;
        there.insert("settings", QVariantMap {{"flipH", true}, {"thumbnail", 3.0}, {"endPoint", 30.0}, {"cropLeft", 40.0}});
        QCOMPARE(outside(show.showMedia(there, "bin2")), "");
        QVariantMap still = picture("sky.jpg");
        QCOMPARE(outside(show.showMedia(still, "bin1")), "media");
        QCOMPARE(outside(show.showMedia(still, "bin1")), "");
        still.insert("settings", QVariantMap {{"rotation", 90}});
        QCOMPARE(outside(show.showMedia(still, "bin1")), "media");
        // Media with no settings is media set to nothing.
        QVariantMap plain = video("fire.mp4");
        QCOMPARE(outside(show.showMedia(plain, "bin1")), "media");
        plain.insert("settings", QVariantMap {{"scale", 0}, {"flipH", false}, {"effects", QVariantList()}});
        QCOMPARE(outside(show.showMedia(plain, "bin1")), "");
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

    // ---- the copyright display

    void theCopyrightLineIsOnTheSlidesTheDisplaySays()
    {
        State show;
        Workspace w = workspace();
        const QVariantList tokens {QVariantMap {{"kind", "title"}}, QVariantMap {{"kind", "text"}, {"text", " / "}},
                                   QVariantMap {{"kind", "license"}}};
        w.copyright = {{"enabled", true}, {"license", "998877"}, {"display", "firstAndLast"}, {"tokens", tokens}};
        Presentation song;
        song.slides = {slide("a"), slide("b"), slide("c")};
        song.ccli = {{"title", "A Made Up Song"}, {"display", true}};
        show.goLive(song.cue(0), false, w);
        QCOMPARE(show.copyright, QStringLiteral("A Made Up Song / 998877"));
        show.goLive(song.cue(1), false, w);
        QVERIFY(show.copyright.isEmpty());
        show.goLive(song.cue(2), false, w);
        QCOMPARE(show.copyright, QStringLiteral("A Made Up Song / 998877"));
        // Gone with the slide when it is cleared
        show.clearSlide();
        QVERIFY(show.copyright.isEmpty());
        // None where the display is off, or the presentation does not show its details
        w.copyright.insert("enabled", false);
        show.goLive(song.cue(0), false, w);
        QVERIFY(show.copyright.isEmpty());
        w.copyright.insert("enabled", true);
        song.ccli.insert("display", false);
        show.goLive(song.cue(0), false, w);
        QVERIFY(show.copyright.isEmpty());
        // A workspace with no display at all has none
        song.ccli.insert("display", true);
        show.goLive(song.cue(0), false, workspace());
        QVERIFY(show.copyright.isEmpty());
    }

    void aPlaybackMarkersActionsAreRunAsAMacrosAre()
    {
        State show;
        const Workspace w = workspace();
        show.showMedia(video("waves.mp4"), "");
        const QVariantMap marker {{"name", "Lights"}, {"time", 12.5}, {"actions", QVariantList {clearAction(2), macroAction("tidy")}}};
        const Effects effects = show.runMarker(marker, "the video \"waves.mp4\"", w);
        QCOMPARE(outside(effects), "nomedia");
        QVERIFY(!show.hasMedia);
        QVERIFY(noted(effects, "the marker \"Lights\" of the video \"waves.mp4\": 2 actions"));
        // One with no name is told by its time; one with no actions does nothing.
        QVERIFY(noted(show.runMarker({{"time", 3.0}}, "the music", w), "the marker at 3.00 s of the music: 0 actions"));
        QCOMPARE(outside(show.runMarker({{"time", 3.0}}, "the music", w)), "");
    }

    // Where the markers of each layer's media are kept: a slide's, a row of the media bin,
    // a piece of the audio bin, or nowhere; gone with the media
    void theMediaOfEachLayerSaysWhereItsMarkersAreKept()
    {
        State show;
        const Workspace w = workspace();
        Presentation song = plainSong();
        song.slides[1] = slide("s1", video("waves.mp4"));
        show.goLive(song.cue(1), false, w);
        const QVariantMap atSlide {{"kind", "slide"}, {"path", "/library/song.pro"}, {"slideId", "s1"}};
        QCOMPARE(show.placeOf("media"), atSlide);
        // A slide that leaves the media playing leaves it where it was kept
        show.goLive(song.cue(2), false, w);
        QCOMPARE(show.placeOf("media"), atSlide);
        // A row of the media bin, and media that is neither
        QVariantMap row = video("sea.mp4");
        row.insert("id", "row-7");
        show.showMedia(row, "bin");
        QCOMPARE(show.placeOf("media"), (QVariantMap {{"kind", "mediaRow"}, {"item", "row-7"}}));
        show.showMedia(video("other.mp4"), "");
        QVERIFY(show.placeOf("media").isEmpty());
        show.showMedia(row, "bin");
        show.clearMedia();
        QVERIFY(show.placeOf("media").isEmpty());
        // The announcements' media, by its slide
        Presentation foyer = notices();
        foyer.file = "/library/notices.pro";
        foyer.slides[0] = slide("n0", video("loop.mp4"));
        show.goLive(foyer.cue(0), false, w);
        QCOMPARE(show.placeOf("announcements"), (QVariantMap {{"kind", "slide"}, {"path", "/library/notices.pro"}, {"slideId", "n0"}}));
        QVERIFY(show.placeOf("media").isEmpty());
        // The music, by its row of the audio bin; none for music no playlist played
        QVariantMap piece = tune("Prelude");
        piece.insert("id", "tune-1");
        show.playAudio(piece, "list");
        QCOMPARE(show.placeOf("audio"), (QVariantMap {{"kind", "audioRow"}, {"item", "tune-1"}}));
        show.playAudio(piece, "");
        QVERIFY(show.placeOf("audio").isEmpty());
        QVERIFY(show.placeOf("no such layer").isEmpty());
    }

    // Markers changed where the live media's are kept are what that media goes by from
    // then on; a change anywhere else changes nothing that is live
    void changedMarkersAreTheLiveMediasWhereItIsKept()
    {
        State show;
        const Workspace w = workspace();
        Presentation song = plainSong();
        QVariantMap waves = video("waves.mp4");
        waves.insert("markers", QVariantList {QVariantMap {{"id", "m1"}, {"time", 2.0}}});
        song.slides[0] = slide("s0", waves);
        show.goLive(song.cue(0), false, w);
        const QVariantList moved {QVariantMap {{"id", "m1"}, {"time", 5.0}}};
        QVERIFY(show.setMarkers({{"kind", "slide"}, {"path", "/library/song.pro"}, {"slideId", "s3"}}, moved).isEmpty());
        QVERIFY(show.setMarkers({}, moved).isEmpty());
        QCOMPARE(show.media.value("markers").toList().first().toMap().value("time").toDouble(), 2.0);
        QCOMPARE(show.setMarkers(show.placeOf("media"), moved), QStringList {"media"});
        QCOMPARE(show.media.value("markers").toList(), moved);
        QCOMPARE(show.media.value("name").toString(), QStringLiteral("waves.mp4"));
        // None left takes the list away
        QCOMPARE(show.setMarkers(show.placeOf("media"), {}), QStringList {"media"});
        QVERIFY(!show.media.contains("markers"));
        // The music's
        QVariantMap piece = tune("Prelude");
        piece.insert("id", "tune-1");
        show.playAudio(piece, "list");
        QCOMPARE(show.setMarkers({{"kind", "audioRow"}, {"item", "tune-1"}}, moved), QStringList {"audio"});
        QCOMPARE(show.music.value("markers").toList(), moved);
        QVERIFY(show.music.value("play").toInt() > 0);
    }

    // A slide's communication actions are handed out to be sent, in their order with
    // its other actions; one of a kind not sent here is only said in the log
    void communicationActionsAreHandedOut()
    {
        State show;
        Presentation song;
        const QVariantMap note {{"kind", "communication"}, {"command", "midi"}, {"title", "Send MIDI note on 6"}, {"done", true},
                                {"deviceName", "MIDI"}, {"note", 6}};
        const QVariantMap other {{"kind", "communication"}, {"command", "other"}, {"title", "Fade To Black, to “Sony”"}, {"done", false}};
        song.slides = {slide("a", {}, {note, clearAction(4), other})};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide communicate");
        for (const Effect &effect : effects) {
            if (effect.kind == Effect::Communicate)
                QCOMPARE(effect.action.value("note").toInt(), 6);
        }
        QVERIFY(noted(effects, "Fade To Black, to “Sony”: not done here"));
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

    void aRenamedPresentationIsStillTheOneTheShowIsAt()
    {
        // In a library a row goes by its file: the show is at the row of the new name,
        // at the slide it was at, and stepping goes on from there.
        State show;
        Presentation song = plainSong();
        show.goLive(song.cue(1), false, workspace());
        show.renamed("song.pro", "hymn.pro");
        QCOMPARE(show.key, "hymn.pro");
        QCOMPARE(show.index, 1);
        QVERIFY(show.atSlide && !show.cleared);
        QCOMPARE(show.stepTarget("hymn.pro", "", 1), 2);
        QCOMPARE(show.stepTarget("song.pro", "", 1), 0);
        // Another presentation's rename is nothing to it.
        show.renamed("other.pro", "another.pro");
        QCOMPARE(show.key, "hymn.pro");
        // A playlist's row has an id of its own, whatever its file is called, and an id
        // that happened to read as the old name would not be a library's row.
        State listed;
        Presentation row = plainSong();
        row.key = "song.pro";
        row.playlistId = "playlist";
        listed.goLive(row.cue(0), false, workspace());
        listed.renamed("song.pro", "hymn.pro");
        QCOMPARE(listed.key, "song.pro");
        // A show that is nowhere stays nowhere.
        State nowhere;
        nowhere.renamed("song.pro", "hymn.pro");
        QVERIFY(!nowhere.atSlide && nowhere.key.isEmpty());
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

    // ---- The audio layer

    void musicIsOneAtATime()
    {
        State show;
        Effects effects = show.playAudio(tune("Prelude"), "list");
        QCOMPARE(outside(effects), "audio");
        QVERIFY(show.hasMusic);
        QCOMPARE(show.music.value("name").toString(), "Prelude");
        QCOMPARE(show.musicPlaylistId, "list");
        const int first = show.music.value("play").toInt();
        QVERIFY(first > 0);
        // Another takes its place: one thing to start, and nothing told to stop (what
        // plays them takes the one off as it brings the other on).
        effects = show.playAudio(tune("Offertory"), "other");
        QCOMPARE(outside(effects), "audio");
        QCOMPARE(effects.last().media.value("name").toString(), "Offertory");
        QCOMPARE(show.music.value("name").toString(), "Offertory");
        QCOMPARE(show.musicPlaylistId, "other");
        QVERIFY(show.music.value("play").toInt() != first);
        QVERIFY(noted(effects, "in place of \"Prelude\""));
        QVERIFY(show.sounds.isEmpty());
    }

    void soundEffectsPlayOverTheMusicAndOverEachOther()
    {
        State show;
        show.playAudio(tune("Prelude"), "list");
        Effects effects = show.playAudio(soundEffect("Thunder"), "list");
        QCOMPARE(outside(effects), "audio");
        effects = show.playAudio(soundEffect("Bell"), "");
        QCOMPARE(outside(effects), "audio");
        // The same one again is another playing of it.
        show.playAudio(soundEffect("Bell"), "");
        QCOMPARE(namesOf(show.sounds), (QStringList {"Thunder", "Bell", "Bell"}));
        QVERIFY(show.sounds.at(1).toMap().value("play") != show.sounds.at(2).toMap().value("play"));
        // And the music plays on, from the playlist it was played from.
        QVERIFY(show.hasMusic);
        QCOMPARE(show.music.value("name").toString(), "Prelude");
        QCOMPARE(show.musicPlaylistId, "list");
        // Music started after them leaves them playing.
        show.playAudio(tune("Offertory"), "list");
        QCOMPARE(show.sounds.size(), 3);
    }

    // ---- the video input layer

    void aVideoInputActionPutsTheInputOnItsLayerAndItStays()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {QVariantMap {{"kind", "videoInput"}, {"title", "Show video input 2"}, {"done", true}, {"inputIndex", 1}}}),
                       slide("b", video("Waves"))};
        Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(show.videoInput, 1);
        QVERIFY(outside(effects).contains("videoinput"));
        bool told = false;
        for (const Effect &effect : effects) {
            if (effect.kind == Effect::VideoInput)
                told = effect.action.value("index").toInt() == 1;
        }
        QVERIFY(told);
        // A slide that brings media, and none: the input stays under them
        effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(show.videoInput, 1);
        QVERIFY(!outside(effects).contains("novideoinput"));
        show.clearSlide();
        show.clearMedia();
        QCOMPARE(show.videoInput, 1);
        // The same one again is left as it is; another takes its place
        QVERIFY(!outside(show.showVideoInput(1)).contains("videoinput"));
        QCOMPARE(outside(show.showVideoInput(0)), "videoinput");
        QCOMPARE(show.videoInput, 0);
    }

    void theVideoInputIsClearedByItsClearAndByEverything()
    {
        State show;
        show.showVideoInput(0);
        QCOMPARE(outside(show.clearVideoInput()), "novideoinput");
        QCOMPARE(show.videoInput, -1);
        QVERIFY(show.clearVideoInput().isEmpty());
        show.showVideoInput(2);
        QVERIFY(outside(show.clearAll()).contains("novideoinput"));
        QCOMPARE(show.videoInput, -1);
        // By an action that clears its layer (the file's 3), and by Clear Everything (0)
        Presentation song;
        song.slides = {slide("a", {}, {clearAction(3)}), slide("b", {}, {clearAction(0)})};
        show.showVideoInput(1);
        show.goLive(song.cue(0), false, workspace());
        QCOMPARE(show.videoInput, -1);
        show.showVideoInput(1);
        show.goLive(song.cue(1), false, workspace());
        QCOMPARE(show.videoInput, -1);
        // And by a clear group that has it (Clear All has every layer), and not one that has not
        show.showVideoInput(1);
        show.clearGroup("extras", workspace());
        QCOMPARE(show.videoInput, 1);
        show.clearGroup("all", workspace());
        QCOMPARE(show.videoInput, -1);
    }

    // ---- actions that play a playlist of either bin

    void aMediaPlaylistActionPlaysItsFirstRowThatIsHere()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {playlistAction("mediaPlaylist", "loops", "Loops")})};
        const Effects effects = show.goLive(song.cue(0), false, withPlaylists());
        QVERIFY(outside(effects).contains("media"));
        // (The first row is not on this computer, and is passed over.)
        QCOMPARE(show.media.value("name").toString(), "Waves");
        QCOMPARE(show.mediaPlaylistId, "loops");
    }

    void aPlaylistThatIsPlayingIsLeftToPlayOnUnlessItIsToRetrigger()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {playlistAction("mediaPlaylist", "loops", "Loops")}),
                       slide("b", {}, {playlistAction("mediaPlaylist", "loops", "Loops")}),
                       slide("c", {}, {playlistAction("mediaPlaylist", "loops", "Loops", true)})};
        show.goLive(song.cue(0), false, withPlaylists());
        // Another of its rows, clicked in the bin, is the playlist playing still.
        QVariantMap fire = video("Fire");
        fire.insert("id", "f");
        show.showMedia(fire, "loops");
        Effects effects = show.goLive(song.cue(1), false, withPlaylists());
        QVERIFY(!outside(effects).contains("media"));
        QVERIFY(noted(effects, "it is playing already, and is left to play on"));
        QCOMPARE(show.media.value("name").toString(), "Fire");
        // Always Retrigger: from its first row again, started again even where that
        // is a background playing already.
        effects = show.goLive(song.cue(2), false, withPlaylists());
        QVERIFY(outside(effects).contains("media"));
        QVERIFY(noted(effects, "from its start again"));
        QCOMPARE(show.media.value("name").toString(), "Waves");
        QVERIFY(show.media.value("retriggers").toBool());
        effects = show.goLive(song.cue(2), false, withPlaylists());
        QVERIFY(outside(effects).contains("media"));
    }

    void aPlaylistIsFoundByItsNameAndAtTheRowTheActionNames()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {playlistAction("mediaPlaylist", "an old id", "Loops", false, "f")}),
                       slide("b", {}, {playlistAction("mediaPlaylist", "nowhere", "Nothing")}),
                       slide("c", {}, {playlistAction("mediaPlaylist", "empty", "Empty")})};
        show.goLive(song.cue(0), false, withPlaylists());
        QCOMPARE(show.media.value("name").toString(), "Fire");
        QCOMPARE(show.mediaPlaylistId, "loops");
        show.clearMedia();
        Effects effects = show.goLive(song.cue(1), false, withPlaylists());
        QVERIFY(noted(effects, "there is no such playlist here"));
        QVERIFY(!show.hasMedia);
        effects = show.goLive(song.cue(2), false, withPlaylists());
        QVERIFY(noted(effects, "it has nothing on this computer to play"));
        QVERIFY(!show.hasMedia);
        // A workspace that has no way to look a playlist up has none.
        effects = show.goLive(song.cue(0), false, workspace());
        QVERIFY(noted(effects, "there is no such playlist here"));
    }

    void anAudioPlaylistActionPlaysItsFirstPieceAndTheNextFollows()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {playlistAction("audioPlaylist", "before", "Before")}),
                       slide("b", {}, {playlistAction("audioPlaylist", "before", "Before", true)})};
        Effects effects = show.goLive(song.cue(0), false, withPlaylists());
        QVERIFY(outside(effects).contains("audio"));
        QCOMPARE(show.music.value("name").toString(), "A");
        QCOMPARE(show.musicPlaylistId, "before");
        // Again: left to play on
        effects = show.goLive(song.cue(0), false, withPlaylists());
        QVERIFY(!outside(effects).contains("audio"));
        // At its end the next follows, as it does for music clicked in the bin
        const QVariantList rows {tune("A", "next"), tune("B", "next"), tune("C", "next")};
        effects = show.audioFinished(show.music.value("play").toInt(), rows);
        QCOMPARE(show.music.value("name").toString(), "B");
        // Always Retrigger: from its first piece again
        effects = show.goLive(song.cue(1), false, withPlaylists());
        QCOMPARE(show.music.value("name").toString(), "A");
        // Another playlist's music playing is not this one playing.
        show.playAudio(tune("Other"), "elsewhere");
        effects = show.goLive(song.cue(0), false, withPlaylists());
        QCOMPARE(show.music.value("name").toString(), "A");
    }

    void audioThatIsNotOnThisComputerIsAProblemAndNothingGivesWay()
    {
        State show;
        show.playAudio(tune("Prelude"), "list");
        QVariantMap gone = tune("Lost");
        gone.insert("missing", true);
        gone.insert("path", QString());
        const Effects effects = show.playAudio(gone, "list");
        QCOMPARE(outside(effects), "problem");
        QCOMPARE(show.music.value("name").toString(), "Prelude");
    }

    void aMediaRowThatEndsGoesOnAsItsNextCueSays()
    {
        State show;
        QVariantList rows {mediaRow("A"), mediaRow("B", "next"), mediaRow("C", {}, {}, true), mediaRow("D")};
        show.showMedia(rows.at(1).toMap(), "list");
        Effects effects = show.mediaFinished("row-B", rows, nullptr);
        // Next: the next with a file, C having none
        QCOMPARE(outside(effects), "media");
        QCOMPARE(show.media.value("name").toString(), "D");
        QCOMPARE(show.mediaPlaylistId, "list");
        QVERIFY(noted(effects, "goes on to the next"));
        // D is followed by nothing: it stays where it is
        effects = show.mediaFinished("row-D", rows, nullptr);
        QCOMPARE(outside(effects), "");
        QCOMPARE(show.media.value("name").toString(), "D");
        QVERIFY(show.hasMedia);
    }

    void firstACueAndRandomAreFoundAmongTheRows()
    {
        const QVariantList rows {mediaRow("A"), mediaRow("B"), mediaRow("C", {}, {}, true), mediaRow("D")};
        const auto with = [&rows](const QString &next, const QString &cue = {}) {
            QVariantList changed = rows;
            changed[1] = mediaRow("B", next, cue);
            return changed;
        };
        QCOMPARE(State::followingMedia(with("first"), "row-B", nullptr), 0);
        QCOMPARE(State::followingMedia(with("cue", "cue-D"), "row-B", nullptr), 3);
        QCOMPARE(State::followingMedia(with("cue", "cue-A"), "row-B", nullptr), 0);
        // A row named that has no file, or is gone, is nothing to go to
        QCOMPARE(State::followingMedia(with("cue", "cue-C"), "row-B", nullptr), -1);
        QCOMPARE(State::followingMedia(with("cue", "cue-Z"), "row-B", nullptr), -1);
        // Random: one of the others with a file (A and D), as the number given picks
        int asked = 0;
        QCOMPARE(State::followingMedia(with("random"), "row-B", [&asked](int n) { asked = n; return 1; }), 3);
        QCOMPARE(asked, 2);
        QCOMPARE(State::followingMedia(with("random"), "row-B", [](int) { return 0; }), 0);
        // Alone in its playlist, it is the one there is
        QCOMPARE(State::followingMedia({mediaRow("B", "random")}, "row-B", [](int) { return 0; }), 0);
        // First with the first having no file: the first that has one
        QVariantList noFirst = with("first");
        noFirst[0] = mediaRow("A", {}, {}, true);
        QCOMPARE(State::followingMedia(noFirst, "row-B", nullptr), 1);
        // Its own row gone, nothing follows
        QCOMPARE(State::followingMedia(with("next"), "row-gone", nullptr), -1);
    }

    void onlyMediaPlayedFromItsPlaylistGoesOn()
    {
        const QVariantList rows {mediaRow("A", "next"), mediaRow("B")};
        // Triggered by a slide: no playlist to go on in
        State bySlide;
        bySlide.showMedia(rows.at(0).toMap(), QString());
        QCOMPARE(outside(bySlide.mediaFinished("row-A", rows, nullptr)), "");
        QCOMPARE(bySlide.media.value("name").toString(), "A");
        // Media that has taken its place since is not what ended
        State replaced;
        replaced.showMedia(rows.at(0).toMap(), "list");
        replaced.showMedia(video("Other"), "list");
        QCOMPARE(outside(replaced.mediaFinished("row-A", rows, nullptr)), "");
        QCOMPARE(replaced.media.value("name").toString(), "Other");
        // Nor is cleared media
        State cleared;
        cleared.showMedia(rows.at(0).toMap(), "list");
        cleared.clearMedia();
        QCOMPARE(outside(cleared.mediaFinished("row-A", rows, nullptr)), "");
        QVERIFY(!cleared.hasMedia);
    }

    void musicThatPlaysOnceHasEndedWhenItEnds()
    {
        State show;
        show.playAudio(tune("Prelude"), "list");
        const QVariantList rows {tune("Prelude"), tune("Offertory")};
        const Effects effects = show.audioFinished(show.music.value("play").toInt(), rows);
        QCOMPARE(outside(effects), "");
        QVERIFY(!show.hasMusic);
        QVERIFY(show.music.isEmpty() && show.musicPlaylistId.isEmpty());
    }

    void musicThatGoesOnIsFollowedByTheNextOfItsPlaylist()
    {
        State show;
        const QVariantList rows {tune("Prelude", "next"), tune("Offertory", "next"), tune("Postlude", "next")};
        show.playAudio(rows.at(0).toMap(), "list");
        Effects effects = show.audioFinished(show.music.value("play").toInt(), rows);
        QCOMPARE(outside(effects), "audio");
        QCOMPARE(show.music.value("name").toString(), "Offertory");
        QCOMPARE(show.musicPlaylistId, "list");
        effects = show.audioFinished(show.music.value("play").toInt(), rows);
        QCOMPARE(show.music.value("name").toString(), "Postlude");
        // The last has none after it: the music has ended. (It does not start at the
        // first again.)
        effects = show.audioFinished(show.music.value("play").toInt(), rows);
        QCOMPARE(outside(effects), "");
        QVERIFY(!show.hasMusic);
        QVERIFY(noted(effects, "none after it"));
    }

    void theNextIsTheFirstAfterItThatHasAFile()
    {
        State show;
        QVariantMap gone = tune("Lost", "next");
        gone.insert("missing", true);
        const QVariantList rows {tune("Prelude", "next"), gone, tune("Postlude")};
        show.playAudio(rows.at(0).toMap(), "list");
        show.audioFinished(show.music.value("play").toInt(), rows);
        QCOMPARE(show.music.value("name").toString(), "Postlude");
    }

    void theNextGoesByThePlaylistAsItIsNow()
    {
        // Rows moved while it played: what follows it is what follows it now. And a
        // row that has been taken out of its playlist is followed by nothing.
        State show;
        show.playAudio(tune("Prelude", "next"), "list");
        show.audioFinished(show.music.value("play").toInt(), {tune("Offertory"), tune("Prelude", "next"), tune("Postlude")});
        QCOMPARE(show.music.value("name").toString(), "Postlude");
        State other;
        other.playAudio(tune("Prelude", "next"), "list");
        other.audioFinished(other.music.value("play").toInt(), {tune("Offertory"), tune("Postlude")});
        QVERIFY(!other.hasMusic);
    }

    void musicPlayedByASlideOrAMacroIsFollowedByNothing()
    {
        State show;
        show.playAudio(tune("Prelude", "next"), "");
        const Effects effects = show.audioFinished(show.music.value("play").toInt(), {tune("Prelude", "next"), tune("Offertory")});
        QCOMPARE(outside(effects), "");
        QVERIFY(!show.hasMusic);
    }

    void aSoundEffectThatEndsIsNoLongerPlayingAndNothingFollowsIt()
    {
        State show;
        show.playAudio(tune("Prelude"), "list");
        show.playAudio(soundEffect("Thunder"), "list");
        show.playAudio(soundEffect("Bell"), "list");
        const int thunder = show.sounds.at(0).toMap().value("play").toInt();
        const Effects effects = show.audioFinished(thunder, {soundEffect("Thunder"), soundEffect("Bell")});
        QCOMPARE(outside(effects), "");
        QCOMPARE(namesOf(show.sounds), QStringList {"Bell"});
        QVERIFY(show.hasMusic);
    }

    void whatEndedAlreadyOrWasReplacedChangesNothing()
    {
        State show;
        show.playAudio(tune("Prelude", "next"), "list");
        const int first = show.music.value("play").toInt();
        show.playAudio(tune("Offertory"), "list");
        // The first, which gave way, is heard of late: nothing follows it now.
        const Effects effects = show.audioFinished(first, {tune("Prelude", "next"), tune("Postlude")});
        QVERIFY(effects.isEmpty());
        QCOMPARE(show.music.value("name").toString(), "Offertory");
        QVERIFY(show.audioFinished(9999, {}).isEmpty());
    }

    void clearingTheAudioStopsTheMusicAndTheSoundEffects()
    {
        State show;
        show.playAudio(tune("Prelude"), "list");
        show.playAudio(soundEffect("Thunder"), "list");
        const Effects effects = show.clearAudio();
        QCOMPARE(outside(effects), "nomusic nosounds");
        QVERIFY(!show.hasMusic && show.sounds.isEmpty() && show.musicPlaylistId.isEmpty());
        // With nothing playing there is nothing to clear.
        QVERIFY(show.clearAudio().isEmpty());
    }

    void clearingTheAudioEffectsLeavesTheMusic()
    {
        State show;
        show.playAudio(tune("Prelude"), "list");
        show.playAudio(soundEffect("Thunder"), "list");
        const Effects effects = show.clearAudioEffects();
        QCOMPARE(outside(effects), "nosounds");
        QVERIFY(show.hasMusic && show.sounds.isEmpty());
    }

    void clearingEverythingClearsTheAudioToo()
    {
        State show;
        show.goLive(plainSong().cue(0), false, workspace());
        show.playAudio(tune("Prelude"), "list");
        show.playAudio(soundEffect("Thunder"), "list");
        const Effects effects = show.clearAll();
        QCOMPARE(outside(effects), "noslide nomusic nosounds");
        QVERIFY(!show.hasMusic && show.sounds.isEmpty());
    }

    void theOtherClearsLeaveTheAudio()
    {
        State show;
        show.goLive(plainSong().cue(0), false, workspace());
        show.showMedia(video("waves.mp4"), "");
        show.playAudio(tune("Prelude"), "list");
        show.playAudio(soundEffect("Thunder"), "list");
        show.clearSlide();
        show.clearMedia();
        show.clearProps();
        QVERIFY(show.hasMusic);
        QCOMPARE(show.sounds.size(), 1);
    }

    void aClearGroupClearsTheHalvesOfTheAudioItNames()
    {
        Workspace w = workspace();
        w.clearGroups << QVariantMap {{"id", "fx"}, {"name", "Effects"}, {"layers", QStringList {"audioEffects"}}};
        State show;
        show.playAudio(tune("Prelude"), "list");
        show.playAudio(soundEffect("Thunder"), "list");
        // The audio effects alone: the music plays on.
        Effects effects = show.clearGroup("fx", w);
        QCOMPARE(outside(effects), "nosounds");
        QVERIFY(show.hasMusic);
        // The audio: the music, and the sound effects with it.
        show.playAudio(soundEffect("Bell"), "list");
        effects = show.clearGroup("extras", w);
        QCOMPARE(outside(effects), "nomusic nosounds");
        QVERIFY(!show.hasMusic && show.sounds.isEmpty());
        // A group of nothing clears nothing.
        show.playAudio(tune("Prelude"), "list");
        show.clearGroup("none", w);
        QVERIFY(show.hasMusic);
    }

    void anActionPlaysAPieceOfAudioFromNoPlaylist()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {audioAction(tune("Prelude", "next")), audioAction(soundEffect("Bell"))}), slide("b")};
        const Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide audio audio");
        QCOMPARE(show.music.value("name").toString(), "Prelude");
        QVERIFY(show.musicPlaylistId.isEmpty());
        QCOMPARE(namesOf(show.sounds), QStringList {"Bell"});
        // The next slide has no audio of its own: what is playing plays on.
        show.goLive(song.cue(1), false, workspace());
        QVERIFY(show.hasMusic);
        QCOMPARE(show.sounds.size(), 1);
    }

    void anActionThatClearsTheAudioOrItsEffectsDoesSo()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {clearAction(8)}), slide("b", {}, {clearAction(1)}), slide("c", {}, {clearAction(0)})};
        show.playAudio(tune("Prelude"), "list");
        show.playAudio(soundEffect("Thunder"), "list");
        Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(outside(effects), "slide nosounds");
        QVERIFY(show.hasMusic);
        show.playAudio(soundEffect("Thunder"), "list");
        effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(outside(effects), "slide nomusic nosounds");
        show.playAudio(tune("Prelude"), "list");
        effects = show.goLive(song.cue(2), false, workspace());
        QVERIFY(!show.hasMusic && show.cleared);
    }

    void aMacroPlaysAudioAsASlideDoes()
    {
        Workspace w = workspace();
        w.macros = {QVariantMap {{"id", "m"}, {"name", "Macros"}, {"macros", QVariantList {
            QVariantMap {{"id", "walkin"}, {"name", "Walk In"}, {"actions", QVariantList {audioAction(tune("Prelude", "loop"))}}}}}}};
        State show;
        const Effects effects = show.runMacro("walkin", w);
        QCOMPARE(outside(effects), "audio");
        QCOMPARE(show.music.value("playback").toString(), "loop");
    }

    // ---- messages

    void messagesStackInTheOrderTheyWereShown()
    {
        State show;
        Effects effects = show.toggleMessage("parent", workspace());
        QCOMPARE(show.messages, QStringList({"parent"}));
        QVERIFY(noted(effects, "\"Parent Wanted\" shown"));
        // (Whoever draws it is told it was shown, to take it as it is now.)
        QCOMPARE(outside(effects), "message messagetime");
        // (One that stays until it is hidden has no time: any that was being counted is stopped.)
        QCOMPARE(times(effects), "parent:0");
        show.toggleMessage("lights", workspace());
        QCOMPARE(show.messages, QStringList({"parent", "lights"}));
        // Hidden, and the other is left
        effects = show.toggleMessage("parent", workspace());
        QCOMPARE(show.messages, QStringList({"lights"}));
        QVERIFY(noted(effects, "hidden"));
        // One the workspace has not is not shown
        QVERIFY(show.toggleMessage("nothing", workspace()).isEmpty());
        QCOMPARE(show.messages, QStringList({"lights"}));
    }

    void aMessageThatGoesByItselfIsTimedAndShownAgainIsTimedAgain()
    {
        State show;
        Effects effects = show.setMessage("lights", true, workspace());
        QCOMPARE(times(effects), "lights:10");
        QVERIFY(noted(effects, "to go by itself after 10 seconds"));
        show.setMessage("parent", true, workspace());
        // Shown while it is shown: it comes to the front, and its time starts again
        effects = show.setMessage("lights", true, workspace());
        QCOMPARE(show.messages, QStringList({"parent", "lights"}));
        QCOMPARE(times(effects), "lights:10");
        QVERIFY(noted(effects, "shown again"));
        // Its time is up: it goes, and the other stays
        effects = show.messageTimeUp("lights", workspace());
        QCOMPARE(show.messages, QStringList({"parent"}));
        QVERIFY(noted(effects, "its time being up"));
        // A time that is up for a message that was hidden meanwhile does nothing
        QVERIFY(show.messageTimeUp("lights", workspace()).isEmpty());
        // Hidden by hand, its time is stopped
        show.setMessage("lights", true, workspace());
        QCOMPARE(times(show.setMessage("lights", false, workspace())), "lights:0");
        QVERIFY(show.setMessage("lights", false, workspace()).isEmpty());
    }

    void clearingTheMessagesTakesThemAllAndStopsTheirTimes()
    {
        State show;
        QVERIFY(show.clearMessages().isEmpty());
        show.setMessage("parent", true, workspace());
        show.setMessage("lights", true, workspace());
        show.toggleProp("p1", workspace());
        const Effects effects = show.clearMessages();
        QVERIFY(show.messages.isEmpty());
        QCOMPARE(times(effects), "parent:0 lights:0");
        QVERIFY(noted(effects, "the messages, 2 of them"));
        // The props are another layer
        QCOMPARE(show.props, QStringList({"p1"}));
    }

    void clearingEverythingAndAGroupThatHasThemClearTheMessages()
    {
        State show;
        show.setMessage("parent", true, workspace());
        show.clearAll();
        QVERIFY(show.messages.isEmpty());
        show.setMessage("parent", true, workspace());
        show.clearGroup("none", workspace());
        QCOMPARE(show.messages, QStringList({"parent"}));
        // ("Extras" is the audio, the messages and the props.)
        show.clearGroup("extras", workspace());
        QVERIFY(show.messages.isEmpty());
    }

    void anActionClearsTheMessagesAndAnActionShowsOneFoundByIdOrFailingThatByName()
    {
        State show;
        Presentation song;
        song.slides = {slide("a", {}, {messageAction("parent")}), slide("b", {}, {messageAction("not here", "Car Lights")}),
                       slide("c", {}, {clearAction(7)}), slide("d", {}, {messageAction("not here", "Nor This")}), slide("e", {}, {clearAction(0)})};
        Effects effects = show.goLive(song.cue(0), false, workspace());
        QCOMPARE(show.messages, QStringList({"parent"}));
        effects = show.goLive(song.cue(1), false, workspace());
        QCOMPARE(show.messages, QStringList({"parent", "lights"}));
        QCOMPARE(times(effects), "lights:10");
        // The layer of messages, as an action's file numbers it: the slide stays
        effects = show.goLive(song.cue(2), false, workspace());
        QVERIFY(show.messages.isEmpty() && !show.cleared);
        effects = show.goLive(song.cue(3), false, workspace());
        QVERIFY(show.messages.isEmpty());
        QVERIFY(noted(effects, "there is no such message here"));
        show.setMessage("parent", true, workspace());
        show.goLive(song.cue(4), false, workspace());
        QVERIFY(show.messages.isEmpty() && show.cleared);
    }

    void aMessageActionGivesTheMessagesTokensItsWords()
    {
        State show;
        Presentation song;
        const QVariantList values {QVariantMap {{"tokenId", "t1"}, {"name", "Name"}, {"kind", "text"}, {"value", "Sam"}},
                                   QVariantMap {{"tokenId", "t2"}, {"name", "Countdown"}, {"kind", "timer"}, {"value", QString()}}};
        song.slides = {slide("a", {}, {messageAction("parent", {}, values)}), slide("b", {}, {messageAction("lights")})};
        Effects effects = show.goLive(song.cue(0), false, workspace());
        Effect shown;
        for (const Effect &effect : effects) {
            if (effect.kind == Effect::Message)
                shown = effect;
        }
        QCOMPARE(shown.text, "parent");
        // The words of its text tokens, by the token's id: a timer's is not words
        QCOMPARE(shown.action, (QVariantMap {{"t1", "Sam"}}));
        // An action that gives nothing, and the messages' own tab, give nothing
        effects = show.goLive(song.cue(1), false, workspace());
        for (const Effect &effect : effects) {
            if (effect.kind == Effect::Message)
                QVERIFY(effect.action.isEmpty());
        }
        show.setMessage("parent", false, workspace());
        effects = show.setMessage("parent", true, workspace());
        int told = 0;
        for (const Effect &effect : effects) {
            if (effect.kind == Effect::Message) {
                ++told;
                QVERIFY(effect.action.isEmpty());
            }
        }
        QCOMPARE(told, 1);
    }

    void aMessageThatIsNoLongerThereIsNoLongerShown()
    {
        State show;
        show.setMessage("parent", true, workspace());
        show.setMessage("lights", true, workspace());
        Workspace fewer = workspace();
        fewer.messages.removeFirst();
        QVERIFY(show.dropMissingMessages(fewer));
        QCOMPARE(show.messages, QStringList({"lights"}));
        QVERIFY(!show.dropMissingMessages(fewer));
    }

    // ---- the stage message

    void theStageIsToldSomethingUntilItIsToldNoMore()
    {
        State show;
        QVERIFY(show.hideStageMessage().isEmpty());
        Effects effects = show.showStageMessage("Wrap it up");
        QCOMPARE(show.stageMessage, "Wrap it up");
        // (The log has that there was one, and not what it said.)
        QVERIFY(noted(effects, "a stage message shown") && !noted(effects, "Wrap"));
        show.showStageMessage("Two minutes");
        QCOMPARE(show.stageMessage, "Two minutes");
        // Nothing clears it but being hidden: it is no layer of the output
        show.clearAll();
        QCOMPARE(show.stageMessage, "Two minutes");
        effects = show.hideStageMessage();
        QCOMPARE(show.stageMessage, "");
        QVERIFY(noted(effects, "hidden"));
        // No words is no message
        show.showStageMessage("Something");
        show.showStageMessage("   ");
        QCOMPARE(show.stageMessage, "");
    }
};

QTEST_APPLESS_MAIN(TestShowState)
#include "tst_showstate.moc"
