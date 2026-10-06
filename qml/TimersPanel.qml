import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes
import SimplePresenterApp

// The workspace's timers, a row each: its name, what it shows, and the buttons that run
// it (back to its start, and start or stop). A click on a row opens it, one at a time,
// on how its timer is set up: its name, what kind of timer it is, the time it runs for
// or to, and whether it runs on past its end.
//
// The timers themselves are `Timers` (src/timers.h), which also keeps them running; this
// only shows them and passes on what is asked. A timer is put on a slide by linking a
// text box to it, in the editor.
Item {
    id: panel

    // The operator window, for its menu and for handing the keyboard back
    required property var win
    readonly property var kinds: ["countdown", "countdownTo", "elapsed"]
    readonly property var kindNames: ["Countdown", "Countdown to Time", "Elapsed Time"]
    // The id of the timer whose row is open, or "" when none is
    property string opened: ""
    // Stands for a timer in a row that is on its way out, so that nothing it shows is
    // looked for in nothing
    readonly property var noTimer: ({ id: "", name: "", kind: "countdown", duration: 0, timeOfDay: 0, startTime: 0,
                                      endTime: 0, hasEndTime: false, overrun: false })

    // Whatever is still being typed into a box here is taken as it stands, and the
    // keyboard goes back to the show. Everything a click does here starts with this:
    // a length typed and then Start pressed is the length that is started.
    function settle() {
        win.takeFocus()
    }

    // Adds a timer, and opens its row so that it can be named and set.
    function add() {
        settle()
        const added = Timers.add()
        if (win.report(added.error))
            opened = added.id
    }

    // Brings the open row into view, all of it if there is room.
    function reveal() {
        const index = Timers.timers.findIndex(timer => timer.id === opened)
        if (index >= 0)
            list.positionViewAtIndex(index, ListView.Contain)
    }

    // Once the row has taken the height of what it now shows
    onOpenedChanged: Qt.callLater(reveal)

    function remove(id) {
        settle()
        win.report(Timers.remove(id))
    }

    function open(id) {
        settle()
        opened = opened === id ? "" : id
    }

    EmptyNote {
        anchors.centerIn: parent
        width: parent.width - 24
        visible: list.count === 0
        font.pixelSize: 13
        text: "No timers. Add one with the + above."
    }

    ListView {
        id: list

        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 4
        // The rows are numbered and each looks its timer up. Given the timers themselves,
        // the list would make every row again whenever anything about any timer changed,
        // and whatever was being typed into one would go with it.
        model: Timers.timers.length

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        delegate: Rectangle {
            id: row

            required property int index
            readonly property var timer: Timers.timers[index] ?? panel.noTimer
            // How it stands: followed by reading the tick that says the timers have moved.
            readonly property var state: Timers.tick >= 0 ? Timers.state(timer.id) : null
            readonly property bool open: timer.id !== "" && panel.opened === timer.id

            function configure(changes) {
                panel.win.report(Timers.configure(timer.id, changes))
            }

            // What a click on one of the row's own controls does, after whatever was
            // being typed has been taken.
            function settled(changes) {
                panel.settle()
                configure(changes)
            }

            width: list.width
            height: rows.height + 8
            radius: 6
            color: rowMouse.containsMouse && !row.open ? "#33353a" : "#2b2d31"

            // A click on the row opens or shuts it; a right click offers to remove it.
            MouseArea {
                id: rowMouse

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    const id = row.timer.id
                    if (mouse.button === Qt.LeftButton) {
                        panel.open(id)
                    } else {
                        panel.settle()
                        panel.win.showMenu([
                            { label: "Remove Timer", danger: true, run: () => panel.remove(id) }
                        ], row, mouse.x, mouse.y)
                    }
                }
            }

            Column {
                id: rows

                x: 8
                y: 4
                width: parent.width - 16
                spacing: 6

                // The name takes what room the rest leaves, on two lines if it needs
                // them: the panel may be narrow, and the time and the buttons that run
                // the timer are what must not give way.
                Item {
                    width: parent.width
                    height: Math.max(28, name.implicitHeight)

                    // Says that the row opens, and whether it is open
                    Shape {
                        id: chevron

                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        rotation: row.open ? 90 : 0
                        preferredRendererType: Shape.CurveRenderer

                        ShapePath {
                            strokeColor: "#9a9da3"
                            strokeWidth: 1.5
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            joinStyle: ShapePath.RoundJoin

                            PathPolyline {
                                path: [Qt.point(2.5, 1), Qt.point(6, 4), Qt.point(2.5, 7)]
                            }
                        }
                    }

                    Text {
                        id: name

                        anchors.left: chevron.right
                        anchors.leftMargin: 6
                        anchors.right: time.left
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        // Between words; a word too long for the room is cut short.
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: "#e6e6e6"
                        font.pixelSize: 13
                        text: row.timer.name
                    }

                    // Bright while it runs, red once it is past its end
                    Text {
                        id: time

                        anchors.right: reset.left
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(62, implicitWidth)
                        horizontalAlignment: Text.AlignRight
                        color: row.state.seconds < 0 ? "#ff6b6b" : row.state.running ? "#e6e6e6" : "#9a9da3"
                        font.pixelSize: 17
                        font.features: { "tnum": 1 }
                        text: row.state.text
                    }

                    IconButton {
                        id: reset

                        anchors.right: run.left
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        kind: "restart"
                        onClicked: {
                            panel.settle()
                            Timers.reset(row.timer.id)
                        }
                    }

                    IconButton {
                        id: run

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        kind: row.state.running ? "stop" : "play"
                        on: row.state.running
                        onClicked: {
                            panel.settle()
                            // As it stands after that, which may have set it up anew
                            if (Timers.state(row.timer.id).running)
                                Timers.stop(row.timer.id)
                            else
                                Timers.start(row.timer.id)
                        }
                    }
                }

                // How it is set up. Only made for the row that is open.
                Loader {
                    width: parent.width
                    active: row.open
                    visible: active

                    sourceComponent: Column {
                        spacing: 6
                        bottomPadding: 4

                        AppTextField {
                            width: parent.width
                            height: 26
                            leftPadding: 6
                            rightPadding: 6
                            font.pixelSize: 13
                            text: row.timer.name
                            // The keyboard goes back first: what the change sets off
                            // may be the end of this box.
                            onEditingFinished: {
                                const name = text.trim()
                                panel.win.takeFocus()
                                if (name !== "" && name !== row.timer.name)
                                    row.configure({ name: name })
                                else
                                    text = Qt.binding(() => row.timer.name)
                            }
                            Keys.onEscapePressed: {
                                text = Qt.binding(() => row.timer.name)
                                panel.win.takeFocus()
                            }
                        }

                        // The kind, and beside it the time that goes with it; under it
                        // where the panel is too narrow for the kind's name to be read
                        // with the time beside it.
                        Flow {
                            width: parent.width
                            spacing: 6

                            AppComboBox {
                                width: parent.width - 84 - parent.spacing >= 150 ? parent.width - 84 - parent.spacing : parent.width
                                height: 26
                                font.pixelSize: 12
                                model: panel.kindNames
                                currentIndex: panel.kinds.indexOf(row.timer.kind)
                                onActivated: (index) => row.settled({ kind: panel.kinds[index] })
                            }

                            // A countdown's length, the time of day one runs to, or
                            // where an elapsed time starts
                            TimeField {
                                width: 84
                                timeOfDay: row.timer.kind === "countdownTo"
                                seconds: row.timer.kind === "countdown" ? row.timer.duration
                                       : row.timer.kind === "countdownTo" ? row.timer.timeOfDay : row.timer.startTime
                                onEdited: (seconds) => row.configure(row.timer.kind === "countdown" ? { duration: seconds }
                                                                   : row.timer.kind === "countdownTo" ? { timeOfDay: seconds }
                                                                   : { startTime: seconds })
                                onFinished: panel.win.takeFocus()
                            }
                        }

                        // An elapsed time may have an end to stop at.
                        Row {
                            width: parent.width
                            height: 26
                            spacing: 6
                            visible: row.timer.kind === "elapsed"

                            AppCheck {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 84 - parent.spacing
                                text: "Ends at"
                                checked: row.timer.hasEndTime
                                onToggled: (checked) => row.settled({ hasEndTime: checked })
                            }

                            TimeField {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 84
                                enabled: row.timer.hasEndTime
                                seconds: row.timer.endTime
                                onEdited: (seconds) => row.configure({ endTime: seconds })
                                onFinished: panel.win.takeFocus()
                            }
                        }

                        Row {
                            width: parent.width
                            height: 26
                            spacing: 6

                            AppCheck {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - remove.width - parent.spacing
                                // ProPresenter's word for running on past the end
                                text: "Overrun"
                                checked: row.timer.overrun
                                onToggled: (checked) => row.settled({ overrun: checked })
                            }

                            AppButton {
                                id: remove

                                anchors.verticalCenter: parent.verticalCenter
                                height: 24
                                leftPadding: 10
                                rightPadding: 10
                                font.pixelSize: 12
                                text: "Remove"
                                onClicked: panel.remove(row.timer.id)
                            }
                        }
                    }
                }
            }
        }
    }
}
