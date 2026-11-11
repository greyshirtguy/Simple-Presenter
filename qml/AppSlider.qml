import QtQuick
import QtQuick.Controls.Basic

// Slider in the app's dark style. Never takes keyboard focus, so the arrow keys keep
// driving the slides.
//
// Two things for a slider that is slid by eye to a round value, as the transitions'
// seconds are (the maintainer, 2026-11-11): marks under the track at given values, the
// major ones taller; and, for a slider that opts in with `fineWithShift`, a tenth of the
// pointer's movement while Shift is held, to land on a value between the marks (not
// discoverable, by their choice). Such a slider takes its pointer presses itself, as the
// control would take them: a press on the track moves the handle there, and a drag
// follows the pointer; the value goes out in `dragged`, for whoever owns it to set it
// (the control's own `moved` does not fire then), and `value` keeps its binding.
Slider {
    id: control

    // Values marked under the track, the major ones taller
    property var marks: []
    property var minorMarks: []
    // Whether Shift held while dragging makes the drag fine (see the top)
    property bool fineWithShift: false
    // Whether the pointer has it, either way
    readonly property bool held: pressed || fineArea.pressed

    // The value the pointer has dragged to, for a slider with `fineWithShift`
    signal dragged(real value)

    focusPolicy: Qt.NoFocus

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 120
        implicitHeight: 4
        width: control.availableWidth
        height: implicitHeight
        radius: 2
        color: "#3a3c42"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: control.enabled ? "#ff8a1f" : "#50535a"
        }

        // The marks, each under where the handle's middle is at its value. (Wider than a
        // unit: the transitions' slider is drawn at four fifths, and a line thinner than a
        // pixel there can be dropped by the renderer, as a minor mark was.)
        Repeater {
            model: control.marks.concat(control.minorMarks)

            Rectangle {
                required property real modelData
                required property int index

                readonly property bool major: index < control.marks.length

                x: (modelData - control.from) / (control.to - control.from) * (control.availableWidth - control.handle.width) + control.handle.width / 2 - width / 2
                y: parent.height + 2
                width: major ? 2.5 : 1.5
                height: major ? 7 : 4
                color: major ? "#9a9da3" : "#6c6f75"
            }
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: 8
        color: !control.enabled ? "#6c6f75" : control.held ? "white" : "#e6e6e6"
    }

    // The pointer, for a slider with `fineWithShift` (see the top)
    MouseArea {
        id: fineArea

        // Where the drag is reckoned from: the pointer's x and the value then, taken afresh
        // when Shift goes down or up in the middle of a drag, so that the pace changes from
        // there and the handle does not jump
        property real fromX: 0
        property real fromValue: 0
        property bool fine: false

        // The value whose handle would be centred at x, as the control places its handle
        function valueAt(x) {
            const extent = control.availableWidth - control.handle.width
            const position = extent > 0 ? Math.max(0, Math.min(1, (x - control.leftPadding - control.handle.width / 2) / extent)) : 0
            return control.from + position * (control.to - control.from)
        }

        function dragTo(value) {
            control.dragged(Math.max(control.from, Math.min(control.to, value)))
        }

        anchors.fill: parent
        enabled: control.fineWithShift && control.enabled
        preventStealing: true
        onPressed: (mouse) => {
            fine = (mouse.modifiers & Qt.ShiftModifier) !== 0
            if (!fine)
                dragTo(valueAt(mouse.x))
            fromX = mouse.x
            fromValue = control.value
        }
        onPositionChanged: (mouse) => {
            if (!pressed)
                return
            const nowFine = (mouse.modifiers & Qt.ShiftModifier) !== 0
            if (nowFine !== fine) {
                fine = nowFine
                fromX = mouse.x
                fromValue = control.value
            }
            const extent = control.availableWidth - control.handle.width
            if (extent > 0)
                dragTo(fromValue + (mouse.x - fromX) / extent * (control.to - control.from) * (fine ? 0.1 : 1))
        }
    }
}
