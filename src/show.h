#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include "showstate.h"

#include <QVariantMap>
#include <functional>
#include <QtQml/qqmlregistration.h>

// What is live: the one place the app keeps it, and the only way it changes.
//
// The operator window asks for things here (this slide is to go live, that prop is to
// be turned over, clear the media), and reads back from here what is then live, to mark
// it in its lists. The rules themselves are not here but in showstate.h, as plain
// values and functions with nothing of Qt Quick about them, so that they can be tested
// by themselves; this is that, given properties QML can bind to and signals for what a
// change asks of the world outside:
//
//   - the output is told what to show by the signals slideShown, slideCleared,
//     mediaShown and mediaCleared (the operator window passes them on to the output
//     window, which knows nothing of presentations);
//   - a timer is worked by timerAction;
//   - the log is told directly.
//
// What the rules need to know of the workspace (its props, macros and stage layouts,
// which a slide's actions name) the operator window keeps handed in, as `props`,
// `macros`, `stageLayouts` and `stageScreens`.
//
// The slides themselves stay with the operator window, which has the presentation as
// it was read: a slide is handed in as it goes live (with the one after it), and that
// is all of the presentation that is kept here.
//
// It is also where a text box linked to the words of the live slide, or of the next,
// gets them. That is what a stage layout is made of: the people on stage read the
// words from boxes like that, laid out as a slide is (see StageLayouts). Such a box is
// drawn by asking here what the words are now (SlideElement does, as it asks Timers for
// a timer's time). The words of a slide are those of its text boxes that show, one
// after another, in the order the slide has them, with no formatting: the box that
// shows them has its own.
//
// There is one of these, which QML reaches by its name.
class Show : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The slide layer: whether the show is at a slide, and of which presentation (the
    // row it was opened from and the playlist that row is in, "" for a library). It
    // stays at its slide while the layer is cleared, so that stepping carries on.
    Q_PROPERTY(bool atSlide READ atSlide NOTIFY slideChanged)
    Q_PROPERTY(int liveIndex READ liveIndex NOTIFY slideChanged)
    Q_PROPERTY(QString liveKey READ liveKey NOTIFY slideChanged)
    Q_PROPERTY(QString livePlaylistId READ livePlaylistId NOTIFY slideChanged)
    // Nothing is on the slide layer; and that is because the slide the show is at took
    // itself off, by an action of its own that clears the slide or everything. Such a
    // slide is still the one to mark as live, which is what cueLive says: the slide
    // the show is at is on the output, or would be had it not taken itself off.
    Q_PROPERTY(bool cleared READ cleared NOTIFY slideChanged)
    Q_PROPERTY(bool clearedByCue READ clearedByCue NOTIFY slideChanged)
    Q_PROPERTY(bool cueLive READ cueLive NOTIFY slideChanged)
    // What is on the media layer, as a map (see catalog.h), or null; and the media
    // playlist it was triggered from, "" if a slide triggered it.
    Q_PROPERTY(QVariant liveMedia READ liveMedia NOTIFY mediaChanged)
    Q_PROPERTY(QString liveMediaPlaylistId READ liveMediaPlaylistId NOTIFY mediaChanged)
    // The props that are on, by id, in the order they were turned on: the last is in front.
    Q_PROPERTY(QStringList liveProps READ liveProps NOTIFY propsChanged)
    // The stage layout each stage screen has, by the ids of both; a screen that is not
    // in it has the plain view. And that of the first stage screen, "" for the plain
    // view: the one meant where only one stage screen is spoken of.
    Q_PROPERTY(QVariantMap screenLayouts READ screenLayouts NOTIFY screenLayoutsChanged)
    Q_PROPERTY(QString stageLayoutId READ stageLayoutId WRITE setStageLayoutId NOTIFY screenLayoutsChanged)

    // The look that is live, by id; "" for none, which is every screen getting everything
    // The saved look that was last made live, by id. Setting it makes that look live
    // (see lookAsked); adoptLook() only says which it was.
    Q_PROPERTY(QString lookId READ lookId WRITE setLookId NOTIFY lookChanged)

    // The workspace's things an action can name, as Props, Macros and StageLayouts list them
    Q_PROPERTY(QVariantList props READ props WRITE setProps NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList macros READ macros WRITE setMacros NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList stageLayouts READ stageLayouts WRITE setStageLayouts NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList stageScreens READ stageScreens WRITE setStageScreens NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList looks READ looks WRITE setLooks NOTIFY workspaceChanged)

    // The slide that is live and the one that would come next, as maps (see
    // proconvert), or empty maps for none
    Q_PROPERTY(QVariantMap currentSlide READ currentSlide NOTIFY changed)
    Q_PROPERTY(QVariantMap nextSlide READ nextSlide NOTIFY changed)
    // Goes up whenever either changes: read it in a binding to have the binding follow.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    // The key the chords of the presentation the show is at are written in, and the
    // key they are to be shown in ("" for as written): see chords.h. The window sets
    // both, from the presentation and from the key picked for it, which is a matter
    // of this sitting and is never written to the file.
    Q_PROPERTY(QString originalKey READ originalKey WRITE setOriginalKey NOTIFY changed)
    Q_PROPERTY(QString chordKey READ chordKey WRITE setChordKey NOTIFY changed)

public:
    // Which of a slide's text a link asks for, as ProPresenter's files have it: all of
    // its words; its notes; or the words of its elements of a given name
    enum Source { Words = 0, Notes = 1, ElementNamed = 2 };
    Q_ENUM(Source)
    // How a slide is shown, as slideShown says it: by itself, the media layer left
    // alone; over the media that is playing, which plays on; with media started under it
    enum Showing { Alone = 0, OverMedia = 1, WithMedia = 2 };
    Q_ENUM(Showing)

    using QObject::QObject;

    bool atSlide() const { return m_state.atSlide; }
    int liveIndex() const { return m_state.index; }
    QString liveKey() const { return m_state.key; }
    QString livePlaylistId() const { return m_state.playlistId; }
    bool cleared() const { return m_state.cleared; }
    bool clearedByCue() const { return m_state.clearedByCue; }
    bool cueLive() const { return m_state.cueLive(); }
    QVariant liveMedia() const;
    QString liveMediaPlaylistId() const { return m_state.mediaPlaylistId; }
    QStringList liveProps() const { return m_state.props; }
    QVariantMap screenLayouts() const;
    QString stageLayoutId() const;
    void setStageLayoutId(const QString &id);

    QVariantList props() const { return m_workspace.props; }
    void setProps(const QVariantList &props);
    QVariantList macros() const { return m_workspace.macros; }
    void setMacros(const QVariantList &macros);
    QVariantList stageLayouts() const { return m_workspace.stageLayouts; }
    void setStageLayouts(const QVariantList &layouts);
    QVariantList stageScreens() const { return m_workspace.stageScreens; }
    void setStageScreens(const QVariantList &screens);
    QVariantList looks() const { return m_workspace.looks; }
    void setLooks(const QVariantList &looks);
    QString lookId() const { return m_state.lookId; }
    void setLookId(const QString &id);

    QVariantMap currentSlide() const { return m_state.cleared ? QVariantMap() : m_state.slide; }
    QVariantMap nextSlide() const { return m_state.next; }
    int revision() const { return m_revision; }
    QString originalKey() const { return m_originalKey; }
    void setOriginalKey(const QString &key);
    QString chordKey() const { return m_chordKey; }
    void setChordKey(const QString &key);

    // A slide goes live: it is shown, with the media it brings, and then its actions
    // are done, in their order. `cue` says which slide and what it is:
    // { key, playlistId, presentation, index, count, slide, next } (see show::Cue).
    // With `withoutMedia`, the slide and its actions and not its media.
    Q_INVOKABLE void goLive(const QVariantMap &cue, bool withoutMedia = false);
    // Where a step of `delta` slides takes the show, for whoever is looking at that
    // presentation (see show::State::stepTarget).
    Q_INVOKABLE int stepTarget(const QString &key, const QString &playlistId, int delta) const;
    // The presentation the show is at was read again or put in another order: `cue` is
    // the slide it is at now. Nothing on the output changes.
    Q_INVOKABLE void follow(const QVariantMap &cue);
    // Where the slide at `index` is in a presentation that has been read again, going
    // by the ids of its slides before and after; -1 if it is not there any more.
    Q_INVOKABLE int placeAfterReload(const QStringList &before, int index, const QStringList &after) const;
    // The show is at no presentation any more (the workspace was changed).
    Q_INVOKABLE void leavePresentation();

    // Puts media on the media layer; `playlist` is the media playlist it was picked
    // from, if it was. A background that is playing already is left to play on.
    Q_INVOKABLE void showMedia(const QVariantMap &media, const QString &playlist = QString());
    Q_INVOKABLE bool alreadyPlaying(const QVariantMap &media) const;

    Q_INVOKABLE void clearSlide();
    Q_INVOKABLE void clearMedia();
    Q_INVOKABLE void clearProps();
    Q_INVOKABLE void clearAll();

    // Turns a prop on, in front of the others, or off if it is on; or puts it one way.
    Q_INVOKABLE void toggleProp(const QString &id);
    Q_INVOKABLE void setProp(const QString &id, bool on);

    // Runs a macro by hand.
    Q_INVOKABLE void runMacro(const QString &id);

    // Says which saved look the live look came from, without making anything live: what
    // is known from the workspace's file when it is opened (see show::State::adoptLook).
    Q_INVOKABLE void adoptLook(const QString &id);

    // Gives a stage screen a layout, by the ids of both ("" for the plain view).
    Q_INVOKABLE void setStageLayout(const QString &screenId, const QString &layoutId);

    // What an action names, in this workspace (see showstate.h): the prop a prop action
    // is for and the layout a stage action gives this app's stage screen, each as a map
    // or null; that screen; which of a stage action's screens it is; and a stage
    // action's list of screens with this app's given `layout` (null for none), keeping
    // what `existing` (an action, or null for a new one) says of the others.
    Q_INVOKABLE QVariant propOf(const QVariantMap &action) const;
    Q_INVOKABLE QVariant stageLayoutOf(const QVariantMap &action) const;
    Q_INVOKABLE QVariantMap stageScreen() const;
    Q_INVOKABLE int stageAssignmentOf(const QVariantMap &action) const;
    Q_INVOKABLE QVariantList stageAssignments(const QVariant &existing, const QVariant &layout) const;
    // The same for every stage screen at once: the layout an action gives each, as a
    // map from a screen's id to the layout or to null; and an action's list of screens
    // with each screen `layouts` has an entry for given that layout (null to leave it
    // as it is).
    Q_INVOKABLE QVariantMap stageLayoutsOf(const QVariantMap &action) const;
    Q_INVOKABLE QVariantList stageAssignmentsFor(const QVariant &existing, const QVariantMap &layouts) const;

    // What a text box linked to a slide's text shows: of the live slide, or with `next`
    // of the one after it; `source` is a Source and `name` the element name it goes by,
    // if it goes by one; `transform` is what is done to the text on the way, as for
    // text linked from another element (see proconvert). Nothing for a slide's notes,
    // which this app does not read.
    Q_INVOKABLE QString slideText(bool next, int source, const QString &name, int transform) const;
    // The same words line by line with their chords, for a text box set to draw them:
    // a list of { text, chords: [{ at, name }] }, each chord's place counted along its
    // line and its name already in the key being shown and in `notation` (a
    // chords::Notation). Text that has been transformed on the way has no chords:
    // their places are places in the words as they were written.
    Q_INVOKABLE QVariantList chordLines(bool next, int source, const QString &name, int transform, int notation) const;

signals:
    void slideChanged();
    void mediaChanged();
    void propsChanged();
    void screenLayoutsChanged();
    void lookChanged();
    // A saved look is to be made the live one, by hand or by an action: whoever keeps
    // the looks (Looks, by way of the operator window) copies it into the live look.
    void lookAsked(const QString &id);
    void workspaceChanged();
    void changed();

    // What a change asks of the world outside, in the order it is to happen. The slide
    // to show is the one the show is at; `showing` is a Showing, and `media` what is to
    // be started under the slide, for WithMedia.
    void slideShown(int showing, const QVariantMap &media);
    void slideCleared();
    void mediaShown(const QVariantMap &media);
    void mediaCleared();
    void timerAction(const QVariantMap &action);

private:
    // Does one thing to the show: says what of it has changed, and then carries out
    // what the change asks of the world outside.
    void change(const std::function<show::Effects()> &what);

    show::State m_state;
    show::Workspace m_workspace;
    int m_revision = 0;
    QString m_originalKey;
    QString m_chordKey;
};
