import QtQuick

// A box for typing a length of time, or a time of day. It shows `seconds` as hours,
// minutes and seconds (as hours and minutes when it is a time of day), and reports what
// is typed with edited() when Enter is pressed or the box is left; it does not change
// `seconds` itself. What is typed is read from the right: seconds, then minutes, then
// hours, so "90", "1:30" and "0:01:30" are all a minute and a half; a time of day is
// hours and minutes, with seconds if a third part is given. `finished` follows every
// edit, and Esc, so that whoever owns the keyboard can take it back.
AppTextField {
    id: field

    property real seconds: 0
    // A time of day: hours and minutes on the 24-hour clock
    property bool timeOfDay: false

    signal edited(real seconds)
    signal finished

    function written(seconds) {
        const whole = Math.max(0, Math.round(seconds))
        const two = n => String(n).padStart(2, "0")
        const hours = Math.floor(whole / 3600)
        const minutes = Math.floor(whole / 60) % 60
        return timeOfDay ? two(hours) + ":" + two(minutes) + (whole % 60 ? ":" + two(whole % 60) : "")
                         : hours + ":" + two(minutes) + ":" + two(whole % 60)
    }

    // The seconds that what is typed means, or -1 if it means nothing.
    function read(typed) {
        const parts = typed.trim().replace(/[.,;]/g, ":").split(":")
        if (parts.length === 0 || parts.length > 3 || parts.some(p => !/^\d+$/.test(p.trim())))
            return -1
        const numbers = parts.map(p => Number(p))
        if (timeOfDay) {
            // Hours first: "10" is ten o'clock and "10:30" half past.
            const seconds = numbers[0] * 3600 + (numbers[1] ?? 0) * 60 + (numbers[2] ?? 0)
            return seconds < 24 * 3600 ? seconds : -1
        }
        return numbers.reduce((total, n) => total * 60 + n, 0)
    }

    function reset() {
        text = written(seconds)
    }

    height: 26
    leftPadding: 6
    rightPadding: 6
    font.pixelSize: 13
    horizontalAlignment: TextInput.AlignHCenter
    onSecondsChanged: reset()
    onTimeOfDayChanged: reset()
    Component.onCompleted: reset()
    onEditingFinished: {
        const typed = read(text)
        if (typed >= 0 && typed !== Math.round(seconds))
            edited(typed)
        reset()
        finished()
    }
    Keys.onEscapePressed: {
        reset()
        finished()
    }
}
