#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQuickWindow;
class QVideoSink;

// The time of a screen's own frames, for what moves on it: a slide's transition, scrolling
// text, a build (TransitionLayer, ScrollingText, BuildRun).
//
// Why not Qt's own animations. Qt Quick moves its animations (NumberAnimation,
// FrameAnimation) by the screen's refreshes only while a single window is showing. With
// two, as this app always has (the operator window and the output), it moves them by an
// ordinary timer of sixteen milliseconds instead: sixty-two and a half steps a second
// against a screen's sixty (measured: 16.009 ms between the steps of a transition, on the
// laptop this is made on, 2026-10-24). So every 0.4 s one refresh had two steps of a
// transition or of scrolling words and showed a jump; and the second step's picture took
// the turn of the next frame of a video playing under it, which came a refresh late: a
// stutter in the video at nearly every transition (see framepacing.h).
//
// What this does instead. While any clock in a window is `running`, the window is kept
// making frames, one at each refresh of its screen: the next is asked for when the last
// has been handed to the screen, and Qt (on Wayland) hands that asking on to the window
// at the screen's next refresh, when the screen has taken the last. Each clock says
// `ticked` as each frame is asked for, with the time, and what moves works out where it
// is at that time: so it is in the frame that comes, and it moves on by the same amount
// at each refresh. (It is told as the frame is asked for and not as the frame is made,
// which is what Qt's afterAnimating would have been for: a change made while a frame is
// being made asks the window for another frame straight away, and Qt makes that one too,
// at once, whether or not anything in it is new, and the frame after it has to wait a
// whole refresh for the screen; so that every later frame was a refresh late, and the
// first of them a frame of the video that stayed on for three refreshes. Asked for
// first, the frame is already waiting when what moves changes, and the change asks for
// nothing more.) A screen sent out over NDI or to a DeckLink card is drawn by the app at
// the rate set for it (NdiScreen, DeckLinkScreen), and a frame asked for there is drawn
// at its next: its clock keeps that rate, which is the card's own when the card is what
// sets it.
//
// Where a video is playing on the screen (FramePace says which), the video's frames are
// the clock: each is drawn as it comes, as it always is, and what else moves is drawn as
// framepacing::framesBetween says, a refresh after each frame of the video, so that it is
// never in the way of the next. While the video gives no frames (paused, at its end) the
// screen keeps its own refreshes again.
//
// What it costs. Nothing while no clock runs: no frames are asked for. While one runs, a
// frame at each refresh, which is what a transition or scrolling text drew anyway; on a
// screen playing a thirty-frame video while a transition runs, the same sixty a second.
class FrameClock : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    // Whether what this clock is for is moving: while any clock in a window runs, the
    // window makes a frame at each refresh (or as its video says)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

public:
    explicit FrameClock(QQuickItem *parent = nullptr);
    ~FrameClock() override;

    bool running() const { return m_running; }
    void setRunning(bool running);
    // Milliseconds now, on the clock `ticked` is told: steady, from an arbitrary start, and
    // the same for everything that asks while the app's thread does one thing (and for a
    // couple of milliseconds after): so that the screens a slide is shown on in one click
    // begin its transition at the same moment, and two screens drawn in the same pass
    // (two outputs of a DeckLink card, a fill and its key) show the same frame of it, as
    // Qt's animations, which all go by one clock, had them. Each screen asking the time
    // for itself had two outputs of a card differ by a shade halfway through a dissolve.
    Q_INVOKABLE double now() const;
    static double frameTime();
    static double milliseconds();
    // Asks for `videoFrameCame` as the next frame of the video the window keeps to
    // (FramePace) arrives, so that a change made then is drawn in the frame the video's is
    // drawn in, and takes no frame of its own (TransitionLayer.paced): true if a video is
    // playing there; false, with nothing to follow, if none is (a still, a paused video,
    // none at all, a screen drawn for NDI or a card). A frame that does not come in half
    // again as long as one lasts (the video stopped meanwhile) is not waited for any
    // longer: `videoFrameCame` is said then. Asked again while waiting, it waits afresh.
    Q_INVOKABLE bool waitForVideoFrame();
    // Waits no longer, and says nothing
    Q_INVOKABLE void stopWaiting();
    // Has a ShaderEffectSource (`source`), used while this clock runs, ask for no frames of
    // its own: the frames are this clock's (see TransitionLayer's textures). Qt has a
    // source ask for one whenever its texture is to be drawn again, that is when what it
    // shows changes, and after it is drawn the first time, twice; but where a clock is
    // running the window is making a frame at every refresh anyway (or at each of a
    // video's frames and between them), in which the texture is drawn again if what it
    // shows has changed. Done as the window next takes what has changed, on its drawing
    // thread, where the source's texture is made, and for as long as that texture lasts.
    // (The asking is cut off by the name of Qt's own connection, signal to slot: if a
    // later Qt makes it otherwise, nothing is cut off, and its frames are asked as before.)
    Q_INVOKABLE void silence(QQuickItem *source);

signals:
    void runningChanged();
    // The window is making a frame, at `now` (on the clock above): what moves is to be
    // where it is at that time
    void ticked(double now);
    // The frame of the video waited for has arrived (see waitForVideoFrame)
    void videoFrameCame();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    friend class WindowFrames;
    void attach(QQuickWindow *window);
    void videoFrameArrived();

    bool m_running = false;
    QPointer<class WindowFrames> m_frames;
    // Waiting for the video's next frame, and for how long at most: made when first asked
    bool m_awaiting = false;
    QTimer *m_awaitLimit = nullptr;
};

// The video whose frames pace the window this is in: the one on the media layer of an
// audience screen (OutputScene). Null while none is playing.
//
// A change of slide waits for the video's next frame, to be drawn in the same frame as it
// (FrameClock::waitForVideoFrame, TransitionLayer.paced). (Tried by itself on 2026-10-24
// on the Demo workspace's songs, that did not help: the slide's words were painted in the
// frame it was drawn in, which took longer than a refresh then; and words with a shadow,
// drawn through layers of Qt's, had Qt draw two frames more after the change, wherever
// it was drawn, two that do not fit in the one refresh a video of thirty frames a second
// leaves between its own. On 2026-10-31 the words came to be painted before the change,
// their shadows with them (StrokedText), and a transition's textures to ask for no frames
// of their own (FrameClock::silence): with those, over a 1080p video on the laptop this is
// made on, with a Screen Preview on the stage, ten cuts held a frame of the video too long
// twice at most, where they had held five to nine, and eight dissolves of 0.3 s three
// times at most, where they had held four to seven. The desktop that was measured in holds
// one now and then with nothing being done.)
class FramePace : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    // A QVideoSink (a VideoOutput's videoSink), or null
    Q_PROPERTY(QObject *video READ video WRITE setVideo NOTIFY videoChanged)

public:
    explicit FramePace(QQuickItem *parent = nullptr);
    ~FramePace() override;

    QObject *video() const;
    void setVideo(QObject *video);

signals:
    void videoChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void attach(QQuickWindow *window);

    QPointer<QVideoSink> m_video;
    QPointer<class WindowFrames> m_frames;
};

// What a window's clocks share: one for each window that has one, made with its first and
// a child of the window.
class WindowFrames : public QObject
{
    Q_OBJECT

public:
    static WindowFrames *of(QQuickWindow *window);

    void add(FrameClock *clock);
    void remove(FrameClock *clock);
    void clockStarted();
    void setPace(QVideoSink *video);
    bool videoPlaying() const;
    // How long a frame of the video lasts, in milliseconds (framepacing::videoInterval)
    double videoInterval() const;

private:
    explicit WindowFrames(QQuickWindow *window);
    bool anyRunning() const;
    double refresh() const;
    void tick();
    void frameShown();
    void videoFrame();
    void drawAtNext();
    void askForFrame();

    QQuickWindow *m_window;
    QList<QPointer<FrameClock>> m_clocks;
    QPointer<QVideoSink> m_pace;
    QMetaObject::Connection m_paceConnection;
    // When the video's last frames came, on FrameClock's clock
    QList<double> m_arrivals;
    // The moments still to come at which to draw between this frame of the video and the
    // next (on FrameClock's clock), and the timer for the first of them
    QList<double> m_between;
    QTimer m_timer;
    // How many frames of the window have been begun since the video's last frame came:
    // its own, and any other
    int m_framesSinceVideo = 0;
    // For the video's having stopped while something moves: no frame of it for half again
    // as long as one lasts, and the window keeps its own refreshes again
    QTimer m_stopped;
};
