#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// The two clocks a text box can show besides a timer: the time of day, and how long the
// video that is playing has left. Each is something a stage layout has a text box for
// (ProPresenter's own layouts have a clock at one corner and a video countdown at
// another), and each changes by the second while the box is on show, so the box asks
// for its words as they are at the moment (SlideElement.qml), as it does a timer's.
//
// Neither costs anything while nothing shows it, and a box that shows neither is put to
// no trouble over them: nothing is made for it and it says nothing. A box that shows one
// asks for its words (clockText, videoText), and is asked to ask again when they may
// have changed, by reading `second` or `videoRevision` where it asks. That it asked is
// how this knows that something is showing the clock:
//   - the clock here ticks, once a second, on the second (`second` goes up), from when
//     it is first asked for until a tick that nobody asks after;
//   - the operator window tells this how far the playing video has got (setVideo), which
//     it has from the output's own player, while `videoWatched` says so: from when a
//     video's time is first asked for until word of the video that nobody asks after.
// A clock that shows no seconds is asked every second all the same and answers the same
// words for sixty of them, which draws nothing. A video's time goes on only as its
// seconds do, unless a box that asked shows hundredths of them.
//
// How the time of day is written is clocktext.h's; how the video's time is, the timers'
// (Timers::linked): a video countdown has a timer's four parts.
//
// GUESSES at ProPresenter: what a video countdown shows when no video is playing (the
// time at nothing, as a timer that is not there shows, here), and that a video that
// goes round is counted down to the end of each time round unless the link says such
// videos are not counted.
//
// And a playback marker's name, or the time to it (see playbackmarkers.h), which a text
// box linked to one shows: of the media playing on one of three layers, which the
// operator window tells this of while `markersWatched` says a box is showing one, as it
// tells this of the video. The time to a marker that has been passed counts up from it,
// as a timer past its end would (A GUESS at ProPresenter); with no media on the layer,
// or no such marker, the name is nothing and the time is at nothing.
//
// `held` is for the self-test and the benchmark, whose pictures are to come out the same
// at any time of day: the clock then stands at a moment that is always the same.
//
// There is one of these, which QML reaches by its name.
class Clocks : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Goes up once a second while any box is showing the time of day: read it in the
    // binding that asks for the clock's words, to be asked again.
    Q_PROPERTY(int second READ second NOTIFY secondChanged)
    // Whether any box is showing a video's time: the operator window tells this about
    // the playing video only while one is.
    Q_PROPERTY(bool videoWatched READ videoWatched NOTIFY videoWatchedChanged)
    // Goes up whenever what is known of the playing video changes: read it likewise.
    Q_PROPERTY(int videoRevision READ videoRevision NOTIFY videoChanged)
    Q_PROPERTY(bool held READ held WRITE setHeld NOTIFY heldChanged)
    // Whether any box is showing a playback marker's name or time; and what goes up when
    // what is known of the markers changes, read in the binding that asks
    Q_PROPERTY(bool markersWatched READ markersWatched NOTIFY markersWatchedChanged)
    Q_PROPERTY(int markerRevision READ markerRevision NOTIFY markersChanged)

public:
    explicit Clocks(QObject *parent = nullptr);

    int second() const { return m_second; }
    bool videoWatched() const { return m_videoWatched; }
    int videoRevision() const { return m_videoRevision; }
    bool held() const { return m_held; }
    void setHeld(bool held);
    bool markersWatched() const { return m_markersWatched; }
    int markerRevision() const { return m_markerRevision; }

    // The time of day, written as a link to the clock says (see clocktext.h). Asking
    // is what keeps the clock going.
    Q_INVOKABLE QString clockText(int dateStyle, int timeStyle, bool twentyFourHour, const QString &pattern);
    // How long the playing video has left, written as a timer's time is and put into
    // `pattern`. With no video, or with one that goes round and `ignoreLooping`, the
    // time at nothing. Asking is what has the video watched.
    Q_INVOKABLE QString videoText(int hours, int minutes, int seconds, int hundredths, bool hundredthsUnderMinuteOnly,
                                  bool ignoreLooping, const QString &pattern);
    // The colour of that time by the link's Color Triggers, as a timer's (Timers::
    // linkedColor): the colour of the one in force, or nothing for the box's own
    Q_INVOKABLE QVariant videoColor(const QVariantList &triggers, bool ignoreLooping) const;

    // The video that is playing has this many seconds left, and goes round or does
    // not; or there is none.
    Q_INVOKABLE void setVideo(double secondsLeft, bool loops);
    Q_INVOKABLE void noVideo();

    // A playback marker's name, or the time to it written as a timer's time is (`time`),
    // of the media on a layer (`destination` 0 the presentation's, 1 the announcements',
    // 2 the audio's), the marker being `which` (see playbackmarkers::pick). Asking is
    // what has the markers watched. And that marker's colour, for a box drawn in it, or
    // nothing for none.
    Q_INVOKABLE QString markerText(int destination, int which, const QString &name, bool time, int hours, int minutes,
                                   int seconds, int hundredths, bool hundredthsUnderMinuteOnly);
    Q_INVOKABLE QVariant markerColor(int destination, int which, const QString &name);
    // The media on a layer is at `position` seconds and has these markers (as
    // playbackmarkers::describe gives them); an empty list for none, or no media.
    Q_INVOKABLE void setMarkers(int destination, double position, const QVariantList &markers);

signals:
    void secondChanged();
    void videoWatchedChanged();
    void videoChanged();
    void heldChanged();
    void markersWatchedChanged();
    void markersChanged();

private:
    void waitForNextSecond();
    void told();
    void settleWatched();

    int m_second = 0;
    int m_videoRevision = 0;
    // Whether the clock, or the video's time, has been asked for since those who show
    // it were last told that it had changed; and whether a video's time was asked for
    // to the hundredth of a second
    bool m_clockAsked = false;
    bool m_videoAsked = false;
    bool m_videoFine = false;
    bool m_videoWatched = false;
    // The clock's tick is being told of; and whether the video is watched is about to
    // be made to follow whether it was asked for
    bool m_ticking = false;
    bool m_settlePending = false;
    // Seconds left, or less than nothing for no video
    double m_videoLeft = -1;
    bool m_videoLoops = false;
    bool m_held = false;
    QTimer m_tick;
    // Each layer's media: where it is, in seconds, and its markers
    double m_markerAt[3] = {0, 0, 0};
    QVariantList m_markers[3];
    int m_markerRevision = 0;
    bool m_markerAsked = false;
    bool m_markersWatched = false;
    bool m_markerSettlePending = false;
    void settleMarkersWatched();
};
