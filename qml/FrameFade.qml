import QtQuick
import SimplePresenterApp

// A fade of something on a screen: how much of it is on show, from 0 to 1 (`value`), going
// from where it is to `to` over `duration` milliseconds, moved on at each frame of the
// screen it is in (FrameClock: on a screen playing a video, in the refreshes the video
// leaves free), and not by an animation of Qt's, which Qt steps by a timer of its own
// while two windows are showing, out of time with the screen, and which took a playing
// video's turn (see src/frameclock.h). For the layers of an audience screen as a look
// takes them off or puts them back (OutputScene), and for the props and messages as they
// come and go (PropsLayer): `opacity: fade.value`.
//
// It goes straight to `to` with no duration, and as it is made. A change of `to` half way
// through a fade goes on from where the fade has got to, as Qt's Behavior did.
Item {
    id: fade

    property real to: 1
    property int duration: 0
    // Where the fade is now
    property real value: 1

    // Where this fade began, and when, on the clock of the window's frames
    property real from: 1
    property double startedAt: 0
    property bool made: false

    width: 0
    height: 0

    onToChanged: begin()
    Component.onCompleted: {
        value = to
        made = true
    }

    function begin() {
        if (!made)
            return
        if (duration <= 0) {
            end()
            return
        }
        from = value
        // (Before the clock is started, by which the first frame is asked for)
        startedAt = clock.now()
        clock.running = true
        backstop.interval = duration + 1000
        backstop.restart()
    }

    // Where the fade is at `now`, on that clock: straight, as Qt's NumberAnimation went
    function step(now) {
        const t = Math.min(1, Math.max(0, (now - startedAt) / duration))
        value = from + (to - from) * t
        if (t >= 1)
            end()
    }

    // Over: at `to`, with no more frames asked for
    function end() {
        clock.running = false
        backstop.stop()
        value = to
    }

    FrameClock {
        id: clock

        onTicked: (now) => fade.step(now)
    }

    // A fade the window makes no frames for (its screen switched off) is not left waiting
    // on them: it is over a second after it should be, as a transition is (TransitionLayer).
    Timer {
        id: backstop

        onTriggered: fade.end()
    }
}
