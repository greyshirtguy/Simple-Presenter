#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

// What is live, and the rules by which it changes.
//
// This is the show itself, with nothing of the app round it: no window, no file, no
// log, no clock. It is a handful of plain values (which slide the show is at, what is
// on the media layer, what audio is playing, which props are on, which messages are
// shown, which layout the stage has and what it is being told) and the things that can
// be done to them: a slide goes live, media is put on, a piece of audio is played, a
// layer is cleared, a prop is turned over, a message is shown, a macro is run. Each of
// those changes the values and says, as a list, what is then to happen outside: what the output is to be handed, which timer is
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
    // Its looks, { id, name } each
    QVariantList looks;
    // Its clear groups, { id, name, layers } each, the layers by the names
    // clearfile.h has for them ("slide", "media", "props" and ProPresenter's others)
    QVariantList clearGroups;
    // Its messages, { id, name, seconds } each: `seconds` is how long one is shown
    // before it goes by itself, 0 for one that stays until it is hidden
    QVariantList messages;
    // A playlist of the media bin (`bin` "media") or of the audio bin ("audio"), by its
    // id or failing that by its name, as { id, name, rows }, the rows as the bins have
    // them (see playlistfile.h); an empty map for none. It is a question and not a list
    // because it is asked only when an action names a playlist: the bins' rows are not
    // copied about for the sake of actions that may never come. Not set, every
    // playlist is none.
    std::function<QVariantMap(const QString &bin, const QString &id, const QString &name)> playlist;
    // Its copyright display (see ccli.h): { enabled, license, display, tokens }, as
    // ccli::describe has them; empty for none
    QVariantMap copyright;
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
    // Where the presentation is sent: "" for the slide layer and the media layer, which
    // is where a presentation goes unless it says otherwise, or "announcements" for
    // the announcements layer (see State::announcement)
    QString destination;
    // The presentation's Slide Show: the seconds each of its slides stays before the
    // next is triggered, 0 for none (see State::advance). A slide's own Go To Next
    // Timer is in its map, as "goToNext": {seconds, loop}.
    double slideShow = 0;
    // The presentation's copyright details (see ccli::describe), for the workspace's
    // copyright display: empty for none
    QVariantMap ccli;
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
        Announcement,          // show the slide the announcements are at on the announcements layer; the media there is left alone
        AnnouncementOverMedia, // show it over the media that is playing on that layer, which plays on
        AnnouncementWithMedia, // show it and start `media` under it, on that layer
        NoAnnouncements,       // take the announcements layer off: its slide, and the media under it
        Advance,        // in `seconds`, go on from the slide that is live to the one it leads to (see State::advance);
                        // `text` is "announcements" for the announcements' place, "" for the show's own
        NoAdvance,      // the going on that was waiting there (`text` as above) is off
        Audio,          // start the piece of audio `media` (music takes the place of the music that is playing)
        NoMusic,        // stop the music
        NoSounds,       // stop every sound effect
        Timer,          // work a timer as `action` says
        Look,           // make the saved look whose id is `text` the live one (see lookfile.h)
        Message,        // the message whose id is `text` has been shown, or shown again: as it is now is how it is to look,
                        // but for what `action` gives its tokens, by the token's id (words, for a text token), if an action showed it
        MessageTime,    // the message whose id is `text` goes by itself in `seconds`; 0 for no such time (any it had is off)
        Note,           // tell the log `text`, under `topic`
        Problem,        // tell the log, and whoever is running the show, that `text` went wrong
        Build,          // run the live slide's next On Click build: the step it is at now is State::buildStep
        Communicate,    // tell a communication device what `action` says (see communications.h)
        VideoInput,     // put the video input whose place is `action`'s "index" on the video input layer
        NoVideoInput,   // take the video input layer off
    };
    Kind kind = Note;
    QVariantMap media;
    QVariantMap action;
    QString topic;
    QString text;
    double seconds = 0;
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
    // The copyright line laid over the live slide on the audience screens, where the
    // workspace's copyright display says this slide shows it (see ccli.h), and "" where
    // it does not: decided as the slide goes live, and gone with it when it is cleared.
    // (A GUESS at ProPresenter: that it is the slide layer's, and that the
    // announcements' slides have none.)
    QString copyright;
    // Nothing is on the slide layer.
    bool cleared = true;
    // The slide the show is at took itself off: its cue has an action that clears the
    // slide, or everything. It is still the slide the show is at.
    bool clearedByCue = false;
    // How far the slide the show is at has got with its builds (ProPresenter's, see
    // builds.h): 0 as it goes live, which runs the builds that start with it, and one
    // more for each On Click build run since. Its map says how many On Click builds
    // it has, as "buildSteps" (none if it has no key).
    int buildStep = 0;

    // The video input layer, under the media layer: the input on it, by its place in the
    // workspace's list of them (which is how ProPresenter's files name one: see
    // videoinputfile.h), -1 for none. An input put on it stays, whatever slides and
    // media come and go over it, until it is cleared or another takes its place. (A
    // GUESS at ProPresenter, whose help says only that the slides are layered over it,
    // and whose looks switch it on and off apart from the media.)
    int videoInput = -1;

    // The media layer: what is on it, and the media playlist it was triggered from (""
    // if a slide triggered it).
    bool hasMedia = false;
    QVariantMap media;
    QString mediaPlaylistId;

    // The announcements layer: a second presentation, shown at the same time as the
    // one on the slide and media layers and over both. A presentation whose
    // destination is the announcements goes here, slide and media alike: the two are
    // one layer, the slide over the media, with a place in a presentation of its own
    // (which slide the announcements are at) that the show's own place knows nothing
    // of. So a loop of notices can run on the screens in the foyer, stepping by
    // itself, while the service is run from the slides as ever.
    //
    // It goes by the rules the slide and media layers go by, on its own: a slide that
    // brings no media leaves a background that is playing there and ends a foreground;
    // a background that is asked for again plays on.
    struct Place
    {
        bool atSlide = false;
        QString key;
        QString playlistId;
        QString presentation;
        int index = -1;
        int count = 0;
        QVariantMap slide;
        QVariantMap next;
        bool cleared = true;
        bool clearedByCue = false;
    };
    Place announcement;
    bool announcementHasMedia = false;
    QVariantMap announcementMedia;

    // The audio layer. Music is one piece at a time: what is playing, as a row of the
    // audio bin (see playlistfile.h), and the playlist of the bin it was played from
    // ("" if a slide or a macro played it). Sound effects are any number at once, over
    // the music and over each other, in the order they were started. Each piece that
    // is started has a number of its own, `play`, in its map: the same file played
    // twice is two, and whatever plays them tells them apart by it.
    bool hasMusic = false;
    QVariantMap music;
    QString musicPlaylistId;
    QVariantList sounds;
    int audioPlays = 0;

    // The props that are on, by id, in the order they were turned on: the last is in front.
    QStringList props;

    // The messages layer, which is over the props: the messages that are shown, by
    // id, in the order they were shown, the last in front. What a message says and
    // how it looks are not the show's business (see Messages, messages.h): only that
    // it is shown.
    QStringList messages;

    // What the stage is being told: the stage message, "" for none. A text box linked
    // to the stage message shows it, wherever the box is (a stage layout is where
    // such a box is to be found).
    QString stageMessage;

    // The saved look that was last made live, by id; "" for none. It is not what the
    // screens get: that is the live look, a look of its own which this one was copied
    // into and which may have been changed since (see lookfile.h). This is for marking
    // which saved look that was, and for an action to name one.
    QString lookId;

    // The layout each stage screen has, by the ids of both. A screen that is not here,
    // or has "", has the plain view.
    QMap<QString, QString> stageLayouts;

    // Whether the slide the show is at is to be marked as the live one: it is on the
    // output, or would be had it not taken itself off.
    bool cueLive() const { return !cleared || clearedByCue; }
    // Whether the show is at a slide of this presentation.
    bool at(const QString &key, const QString &playlistId) const;
    // The same two, for the announcements.
    bool announcementLive() const { return !announcement.cleared || announcement.clearedByCue; }
    bool announcementAt(const QString &key, const QString &playlistId) const;

    // Going on by itself: ProPresenter's Go To Next Timer, which a slide has, and its
    // Slide Show, which a presentation has. A slide that goes live starts one, on the
    // place it went live at (the show's own, or the announcements'), which are counted
    // apart:
    //
    //  - a slide with a Go To Next Timer goes on to the next slide when its seconds
    //    are up, or with "Loop to Beginning" to the first; from the last slide, a
    //    timer without that has nowhere to go and is not started;
    //  - any other slide of a presentation with a Slide Show goes on to the next when
    //    the presentation's seconds are up, and from the last to the first. So a
    //    slide's own timer wins over the presentation's, as it does in ProPresenter.
    //
    // Nothing starts but by a slide going live: a presentation given a Slide Show
    // while one of its slides is on waits for the next slide to be triggered. What is
    // waiting is off when its slide is cleared by whoever runs the show (not by the
    // slide's own action: that slide is still the one the show is at), when another
    // slide goes live there, and when its slide no longer asks for it.
    //
    // A time of nothing goes on at once. (That is how a file of ProPresenter's has a
    // slide that only leads back to the first: "Loop to Beginning" after 0 seconds. A
    // GUESS, from such files, that ProPresenter goes on at once too.) A presentation
    // all of whose slides did that would never stop, so one that has gone the whole
    // way round without any time passing is stopped, and said to be.
    struct Advance
    {
        bool waiting = false;
        double seconds = 0;
        // To the first slide, wherever it is from; and, for the others, whether the
        // last slide leads to the first
        bool toFirst = false;
        bool loops = false;
        bool operator==(const Advance &other) const = default;
    };
    Advance advance;
    Advance announcementAdvance;
    // The time of the one that was waiting is up: where it goes, as a place in the
    // presentation (-1 for nowhere, which is what there is if the presentation has
    // been left or has lost the slide). It is not waiting any more.
    int takeAdvance(bool announcements);

    // A slide goes live: it is shown, with the media it brings, and then its actions
    // are done, in their order. With `withoutMedia`, the slide and its actions and not
    // the media (what a click with Alt held does, as in ProPresenter). `byTimer` says
    // that it went live by the going on above and not by anyone's hand.
    Effects goLive(const Cue &cue, bool withoutMedia, const Workspace &workspace, bool byTimer = false);
    // Where a step of `delta` slides takes the show, for whoever is looking at the
    // presentation `key` of `playlistId`: from a cleared output, back to the slide it
    // was at (unless that slide cleared itself, which it would only do again); from
    // another presentation, to its first slide.
    // With `announcements`, the same for the announcements: from where they are.
    int stepTarget(const QString &key, const QString &playlistId, int delta, bool announcements = false) const;
    // Next, for whoever is looking at the presentation `key` of `playlistId`; or a click
    // on its slide `index` (-1 for Next, which means the slide the show is at). If that
    // is the slide on show, and it has an On Click build that has not been run, the
    // build is run and `*ran` is set; otherwise nothing is done, and Next goes to the
    // next slide (stepTarget) and the click triggers the slide again, as they always
    // have. As the maintainer has ProPresenter step through a slide's builds before
    // the next slide. (A GUESS: that a click on the live slide runs its next build as
    // Next does, rather than starting it again.) The announcements' slides are not
    // stepped so: see builds.h.
    Effects stepBuild(const QString &key, const QString &playlistId, int index, bool *ran);
    // The presentation the show is at was read again, or put in another order: this is
    // the slide it is at now. Nothing on the output changes. (A going on that is
    // waiting there is off if the slide no longer asks for one: see `advance`.)
    Effects follow(const Cue &cue);
    // A presentation's file was given another name. A row of a library goes by its
    // file, so a show that is at that row is at the row `to` now, and goes on from the
    // slide it was at: nothing on the output changes. (A row of a playlist has an id of
    // its own, which a rename leaves as it was, and a show at one is not touched.)
    void renamed(const QString &from, const QString &to);
    // The show is at no presentation any more (the workspace was changed).
    Effects leavePresentation();

    // Puts media on the media layer; `playlist` is the media playlist it was picked
    // from, if it was.
    Effects showMedia(const QVariantMap &media, const QString &playlist);
    // Whether this media is a background that is the one already playing, which is
    // then left to play on and not started again (unless it is set always to start
    // again). For a video that holds only while it is going round: one that plays to
    // its end and stops is started again, there being nothing of it to play on. And it
    // holds only for a video that is to play as the one playing does: one that has
    // been set to play another way since is started again, which is how a change to
    // the way a slide's video plays takes effect when the slide is next triggered. The
    // same goes for a picture or a video that its Media Inspector has set otherwise
    // since (its `settings`: cropped, turned, trimmed, given an effect).
    bool alreadyPlaying(const QVariantMap &media) const;

    // Plays a piece of audio: `audio` is a row of the audio bin, and `playlist` the
    // playlist of the bin it was picked from, if it was. Music takes the place of the
    // music that is playing; a sound effect is played over whatever is.
    Effects playAudio(const QVariantMap &audio, const QString &playlist);
    // A piece of audio has come to its end, by its number: it is playing no longer.
    // If it is the music, and it is one that is followed by the next of its playlist,
    // that is played: `rows` are the rows of that playlist as they are now, and the
    // next is the first after it that has a file to play. With none after it, or its
    // own row no longer there, the music has simply ended. (A GUESS: that ProPresenter
    // stops at a playlist's last row and does not start at its first again.)
    // What has ended already, or was never playing, changes nothing.
    Effects audioFinished(int play, const QVariantList &rows);
    // The video on the media layer has played to its end, by its row's id (one that
    // stops at its end; see MediaContent). If it was played from a media playlist (a
    // click in the bin, or an action that plays the playlist), and its row is followed
    // by another (ProPresenter's Next Cue: see mediacue.h), that row is put on the media
    // layer as a click on it in the bin would: `rows` are the playlist's rows as they
    // are now, which say what follows it, and `random(n)` gives a number from 0 to
    // below n, for Random. Otherwise it stays on its last frame, and nothing changes.
    // GUESSES, none of these having been seen in ProPresenter: Next stops at the
    // playlist's last row and does not go round to its first (as the audio bin's Next
    // is taken to); Random never picks the row that has just played, where there is
    // another; and a row with no file to play is passed over, and a row named to go to
    // that has none is nothing to go to.
    Effects mediaFinished(const QString &id, const QVariantList &rows, const std::function<int(int)> &random);
    // Which of `rows` follows the row `id` as its `next` and `nextCue` say (see
    // mediaFinished): its place, or -1 for none
    static int followingMedia(const QVariantList &rows, const QString &id, const std::function<int(int)> &random);

    Effects clearSlide();
    Effects clearMedia();
    // A video input put on its layer, by its place, as a click on it in the media bin's
    // Video Inputs does; one that is on it already is left as it is. And the layer
    // cleared.
    Effects showVideoInput(int index);
    Effects clearVideoInput();
    // The announcements layer: its slide and the media under it.
    Effects clearAnnouncements();
    Effects clearProps();
    // The audio layer: the music and every sound effect; and the sound effects alone.
    Effects clearAudio();
    Effects clearAudioEffects();
    Effects clearMessages();
    Effects clearAll();
    // Clears what a clear group clears, which is those of its layers there are here:
    // the slide, the media, the announcements, the props, the messages, the audio
    // (the music and the sound effects) and the audio effects (the sound effects
    // alone). The other it names (video inputs) is ProPresenter's, and nothing is
    // done for it. A group the workspace has not clears nothing.
    Effects clearGroup(const QString &id, const Workspace &workspace);

    // Turns a prop on, in front of the props that are on already, or off if it is on.
    // A collection set to show one prop at a time gives up whichever of its others is on.
    Effects toggleProp(const QString &id, const Workspace &workspace);
    // Puts a prop on or takes it off, whichever way it was.
    Effects setProp(const QString &id, bool on, const Workspace &workspace);
    // The workspace's props have changed: one that is no longer there is no longer on.
    // Says whether any went.
    bool dropMissingProps(const Workspace &workspace);

    // Shows a message, in front of those that are shown already; or, with `on` false,
    // hides it. A message that is shown and is shown again comes to the front, and
    // the time it has before it goes by itself starts again: that is how a message
    // whose words have been changed is shown as it now is. One the workspace has not
    // is not shown.
    Effects setMessage(const QString &id, bool on, const Workspace &workspace);
    // Shows it if it is not shown, and hides it if it is.
    Effects toggleMessage(const QString &id, const Workspace &workspace);
    // A message's time is up: it goes, if it is still shown.
    Effects messageTimeUp(const QString &id, const Workspace &workspace);
    // The workspace's messages have changed: one that is no longer there is no longer
    // shown. Says whether any went.
    bool dropMissingMessages(const Workspace &workspace);

    // Tells the stage something, or with no words stops telling it.
    Effects showStageMessage(const QString &text);
    Effects hideStageMessage();

    // Runs a macro by hand.
    Effects runMacro(const QString &id, const Workspace &workspace);
    // A playback marker has been reached by the media playing it, or picked from the
    // transport's Marker menu (see playbackmarkers.h): its actions are run as a macro's
    // are. `marker` is { name, time, actions } as playbackmarkers::describe has it, and
    // `media` what it is on, in a few words, for the log.
    Effects runMarker(const QVariantMap &marker, const QString &media, const Workspace &workspace);

    // Gives a stage screen a layout ("" for the plain view).
    void setStageLayout(const QString &screenId, const QString &layoutId);

    // Makes a saved look the live one. Asked for again while it is the one that was
    // last made live, it is made live again: that is how the live look is put back as
    // the saved look has it, after it has been changed. A look the workspace has not is
    // not made live.
    Effects setLook(const QString &id, const Workspace &workspace);
    // Says which saved look the live look came from, where that is known from the file
    // and nothing is to be made live ("" for none): what a workspace being opened does.
    void adoptLook(const QString &id);

private:
    // While the actions of the slide going live are being done; and of a slide going
    // live on the announcements layer
    bool m_runningCue = false;
    bool m_runningAnnouncement = false;
    // How many slides in a row have gone on with no time between, at each place
    int m_run = 0;
    int m_announcementRun = 0;

    void arm(bool announcements, const Cue &cue, bool byTimer, Effects &effects);
    void stopAdvance(bool announcements, Effects &effects);

    void runActions(const QVariantList &actions, int depth, const Workspace &workspace, Effects &effects);
    void runAction(const QVariantMap &action, int depth, const Workspace &workspace, Effects &effects);
    void clearSlide(Effects &effects);
    void clearMedia(Effects &effects);
    void showVideoInput(int index, Effects &effects);
    void clearVideoInput(Effects &effects);
    void clearAnnouncements(Effects &effects);
    Effects goLiveAnnouncement(const Cue &cue, bool withoutMedia, const Workspace &workspace, bool byTimer);
    void clearProps(Effects &effects);
    void clearMessages(Effects &effects);
    void showMessage(const QVariantMap &message, Effects &effects, const QVariantMap &values = {});
    void hideMessage(const QString &id, const QString &name, const QString &how, Effects &effects);
    void clearMusic(Effects &effects);
    void clearSounds(Effects &effects);
    void startAudio(const QVariantMap &audio, const QString &playlist, Effects &effects);
    void playPlaylist(const QVariantMap &action, const Workspace &workspace, Effects &effects);
    void clearGroup(const QVariantMap &group, Effects &effects);
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
// A look by its id, or failing that by its name.
QVariantMap findLook(const QString &id, const QString &name, const Workspace &workspace);
// A clear group by its id, or failing that by its name ("" for by id alone).
QVariantMap findClearGroup(const QString &id, const QString &name, const Workspace &workspace);
// A message by its id, or failing that by its name ("" for by id alone).
QVariantMap findMessage(const QString &id, const QString &name, const Workspace &workspace);
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

// How media is spoken of in the log, and a piece of audio.
QString mediaWords(const QVariantMap &media);
QString audioWords(const QVariantMap &audio);
QString quoted(const QString &name);

}
