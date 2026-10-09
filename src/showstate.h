#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// What is live, and the rules by which it changes.
//
// This is the show itself, with nothing of the app round it: no window, no file, no
// log, no clock. It is a handful of plain values (which slide the show is at, what is
// on the media layer, which props are on, which layout the stage has) and the things
// that can be done to them: a slide goes live, media is put on, a layer is cleared, a
// prop is turned over, a macro is run. Each of those changes the values and says, as a
// list, what is then to happen outside: what the output is to be handed, which timer is
// to be worked, what the log is to be told. Whoever owns one of these (the Show that
// QML talks to, in show.h) carries that list out.
//
// It is kept apart like this so that the rules can be tried by themselves, quickly and
// exactly, by tests/unit/tst_showstate.cpp, without the app being started: what a slide
// that clears itself leaves marked, when a background video is left to play on, what a
// macro that runs a macro does. Anything that decides what the audience sees belongs
// here and not in a window's script.
//
// The slides, the media and the actions are the maps the rest of the app passes round
// (proconvert.h for a slide, catalog.h for media, actions.h for an action). They are
// carried, and read for the few things the rules go by; nothing here makes or changes
// one.
namespace show {

// The things of a workspace an action can name: its props and macros by collection,
// its stage layouts and its stage screens ({ id, name } each, in their order), as the
// Props, Macros, StageLayouts and Screens objects list them.
struct Workspace
{
    QVariantList props;
    QVariantList macros;
    QVariantList stageLayouts;
    QVariantList stageScreens;
};

// A slide as it is triggered: where it is, and what it brings.
struct Cue
{
    // The row its presentation was opened from and the playlist that row is in ("" for
    // a library): together, which presentation this is
    QString key;
    QString playlistId;
    // The presentation's name, the slide's place in it and how many slides it has
    QString presentation;
    int index = -1;
    int count = 0;
    // The slide, and the one after it (empty if it is the last)
    QVariantMap slide;
    QVariantMap next;
};

// One thing to be done outside, as the outcome of a change.
struct Effect
{
    enum Kind {
        Slide,          // show the live slide on the slide layer; the media layer is left alone
        SlideOverMedia, // show the live slide over the media that is playing, which plays on
        SlideWithMedia, // show the live slide and start `media` under it
        NoSlide,        // take the slide layer off
        Media,          // start `media` on the media layer
        NoMedia,        // take the media layer off
        Timer,          // work a timer as `action` says
        Note,           // tell the log `text`, under `topic`
        Problem,        // tell the log, and whoever is running the show, that `text` went wrong
    };
    Kind kind = Note;
    QVariantMap media;
    QVariantMap action;
    QString topic;
    QString text;
};
using Effects = QList<Effect>;

class State
{
public:
    // The slide layer. The show stays at a slide while the layer is cleared, so that
    // stepping carries on from where it was.
    bool atSlide = false;
    QString key;
    QString playlistId;
    QString presentation;
    int index = -1;
    int count = 0;
    QVariantMap slide;
    QVariantMap next;
    // Nothing is on the slide layer.
    bool cleared = true;
    // The slide the show is at took itself off: its cue has an action that clears the
    // slide, or everything. It is still the slide the show is at.
    bool clearedByCue = false;

    // The media layer: what is on it, and the media playlist it was triggered from (""
    // if a slide triggered it).
    bool hasMedia = false;
    QVariantMap media;
    QString mediaPlaylistId;

    // The props that are on, by id, in the order they were turned on: the last is in front.
    QStringList props;

    // The layout each stage screen has, by the ids of both. A screen that is not here,
    // or has "", has the plain view.
    QMap<QString, QString> stageLayouts;

    // Whether the slide the show is at is to be marked as the live one: it is on the
    // output, or would be had it not taken itself off.
    bool cueLive() const { return !cleared || clearedByCue; }
    // Whether the show is at a slide of this presentation.
    bool at(const QString &key, const QString &playlistId) const;

    // A slide goes live: it is shown, with the media it brings, and then its actions
    // are done, in their order. With `withoutMedia`, the slide and its actions and not
    // the media (what a click with Alt held does, as in ProPresenter).
    Effects goLive(const Cue &cue, bool withoutMedia, const Workspace &workspace);
    // Where a step of `delta` slides takes the show, for whoever is looking at the
    // presentation `key` of `playlistId`: from a cleared output, back to the slide it
    // was at (unless that slide cleared itself, which it would only do again); from
    // another presentation, to its first slide.
    int stepTarget(const QString &key, const QString &playlistId, int delta) const;
    // The presentation the show is at was read again, or put in another order: this is
    // the slide it is at now. Nothing on the output changes.
    void follow(const Cue &cue);
    // The show is at no presentation any more (the workspace was changed).
    void leavePresentation();

    // Puts media on the media layer; `playlist` is the media playlist it was picked
    // from, if it was.
    Effects showMedia(const QVariantMap &media, const QString &playlist);
    // Whether this media is a background that is the one already playing, which is
    // then left to play on and not started again (unless it is set always to start
    // again). For a video that holds only while it is going round: one that plays to
    // its end and stops is started again, there being nothing of it to play on. And it
    // holds only for a video that is to play as the one playing does: one that has
    // been set to play another way since is started again, which is how a change to
    // the way a slide's video plays takes effect when the slide is next triggered.
    bool alreadyPlaying(const QVariantMap &media) const;

    Effects clearSlide();
    Effects clearMedia();
    Effects clearProps();
    Effects clearAll();

    // Turns a prop on, in front of the props that are on already, or off if it is on.
    // A collection set to show one prop at a time gives up whichever of its others is on.
    Effects toggleProp(const QString &id, const Workspace &workspace);
    // Puts a prop on or takes it off, whichever way it was.
    Effects setProp(const QString &id, bool on, const Workspace &workspace);
    // The workspace's props have changed: one that is no longer there is no longer on.
    // Says whether any went.
    bool dropMissingProps(const Workspace &workspace);

    // Runs a macro by hand.
    Effects runMacro(const QString &id, const Workspace &workspace);

    // Gives a stage screen a layout ("" for the plain view).
    void setStageLayout(const QString &screenId, const QString &layoutId);

private:
    // While the actions of the slide going live are being done
    bool m_runningCue = false;

    void runActions(const QVariantList &actions, int depth, const Workspace &workspace, Effects &effects);
    void runAction(const QVariantMap &action, int depth, const Workspace &workspace, Effects &effects);
    void clearSlide(Effects &effects);
    void clearMedia(Effects &effects);
    void clearProps(Effects &effects);
    void toggleProp(const QString &id, const Workspace &workspace, Effects &effects);
};

// What the rules look up in a workspace. Each answers with an empty map for nothing.

// A prop by its id, with its collection's id as "collection" and whether that
// collection shows one prop at a time as "single".
QVariantMap findProp(const QString &id, const Workspace &workspace);
// The prop an action is for: by its id, or failing that by its name.
QVariantMap propOf(const QVariantMap &action, const Workspace &workspace);
// A macro by its id, or failing that by its name ("" for by id alone).
QVariantMap findMacro(const QString &id, const QString &name, const Workspace &workspace);
// The workspace's stage screens, { id, name } each: those it lists, or for a workspace
// that lists none the one every workspace has, by the name it has here.
QVariantList stageScreens(const Workspace &workspace);
// The first of them: the one meant where only one is spoken of.
QVariantMap stageScreen(const Workspace &workspace);
// Which of a stage action's lines is for a stage screen: the one that names it by id,
// or failing that by name; -1 if none does.
int stageAssignmentFor(const QVariantMap &action, const QVariantMap &screen);
// The same for the first stage screen, which is also given, where the action names no
// screen of this workspace at all, the first line that gives a layout: the action was
// made somewhere the screens are others, and is taken to mean the one there is.
int stageAssignmentOf(const QVariantMap &action, const Workspace &workspace);
// The stage layout an action gives each stage screen, by the screen's id: an empty map
// for a screen it leaves as it is, or gives a layout that is not among the workspace's.
QMap<QString, QVariantMap> stageLayoutsOf(const QVariantMap &action, const Workspace &workspace);
// That of the first stage screen.
QVariantMap stageLayoutOf(const QVariantMap &action, const Workspace &workspace);
// A stage action's list of screens, with each stage screen that `layouts` has an entry
// for given that layout (an empty one meaning that it is left as it is). An action that
// is being changed keeps what it says of other screens; a new one (an empty `existing`)
// names every stage screen of the workspace, which is how ProPresenter writes one.
QVariantList stageAssignments(const QVariantMap &existing, const QMap<QString, QVariantMap> &layouts, const Workspace &workspace);
// The same with only the first stage screen given a layout.
QVariantList stageAssignments(const QVariantMap &existing, const QVariantMap &layout, const Workspace &workspace);

// Where the slide the show is at is, in a presentation that has been read again: the
// slide with the same id, and of those (a presentation can use a slide more than once)
// the one as far along as it was. `before` and `after` are the ids of the slides as
// they were and as they are; -1 if it is not there any more.
int placeAfterReload(const QStringList &before, int index, const QStringList &after);

// How media is spoken of in the log.
QString mediaWords(const QVariantMap &media);
QString quoted(const QString &name);

}
