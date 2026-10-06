import QtQuick

// The show controls: what is worked during a show besides the slides and the media, a
// tab for each kind: the timers, the props, and the stage screens.
//
// The tabs are a row of buttons across the whole width, pictures and not words, as
// ProPresenter's are, and the one whose tab is showing is blue. Under them, at the
// right, is the button that adds to what the tab holds.
Item {
    id: control

    // The operator window
    required property var win
    // Which tab is showing: "timers", "props" or "stage"
    property string tab: "timers"
    // Whether something here is being renamed in place, and so has the keyboard
    readonly property bool renaming: props.renaming !== ""
    readonly property var tabs: [
        { id: "timers", name: "Timers" },
        { id: "props", name: "Props" },
        { id: "stage", name: "Stage" }
    ]

    Row {
        id: buttons

        width: parent.width
        spacing: 4

        Repeater {
            model: control.tabs

            delegate: Rectangle {
                id: button

                required property var modelData
                readonly property bool chosen: control.tab === modelData.id
                readonly property color ink: chosen ? "white" : "#c9cdd6"

                width: (buttons.width - (control.tabs.length - 1) * buttons.spacing) / control.tabs.length
                height: 28
                radius: 6
                // The tab that is showing is blue.
                color: chosen ? "#1e88e5" : mouse.pressed ? "#50535a" : mouse.containsMouse ? "#45484e" : "#3a3c42"

                // Timers: a stopwatch
                Item {
                    anchors.centerIn: parent
                    width: 16
                    height: 18
                    visible: button.modelData.id === "timers"

                    Rectangle {
                        x: 6
                        width: 4
                        height: 2.5
                        color: button.ink
                    }

                    Rectangle {
                        y: 3
                        width: 16
                        height: 15
                        radius: 8
                        color: "transparent"
                        border.width: 1.5
                        border.color: button.ink
                    }

                    Rectangle {
                        x: 7.25
                        y: 6
                        width: 1.5
                        height: 5
                        color: button.ink
                    }

                    Rectangle {
                        x: 7.25
                        y: 10
                        width: 4.5
                        height: 1.5
                        color: button.ink
                    }
                }

                // Props: something laid over the corner of the picture
                Item {
                    anchors.centerIn: parent
                    width: 18
                    height: 13
                    visible: button.modelData.id === "props"

                    Rectangle {
                        anchors.fill: parent
                        radius: 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: button.ink
                    }

                    Rectangle {
                        x: 9
                        y: 6.5
                        width: 6
                        height: 3.5
                        radius: 1
                        color: button.ink
                    }
                }

                // Stage: a screen on its stand
                Item {
                    anchors.centerIn: parent
                    width: 18
                    height: 16
                    visible: button.modelData.id === "stage"

                    Rectangle {
                        width: 18
                        height: 11.5
                        radius: 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: button.ink
                    }

                    Rectangle {
                        x: 8.25
                        y: 11.5
                        width: 1.5
                        height: 3
                        color: button.ink
                    }

                    Rectangle {
                        x: 5
                        y: 14.5
                        width: 8
                        height: 1.5
                        color: button.ink
                    }
                }

                MouseArea {
                    id: mouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        // Out of whatever box of the tab being left was being typed in
                        control.win.takeFocus()
                        control.tab = button.modelData.id
                    }
                }
            }
        }
    }

    // Adds to what the tab holds: a timer, a prop or a collection of them, a stage
    // layout. Small, and out of the way of the tabs.
    Rectangle {
        id: add

        objectName: "showControlAdd"
        anchors.right: parent.right
        anchors.top: buttons.bottom
        anchors.topMargin: 6
        width: 24
        height: 20
        radius: 5
        color: addMouse.pressed ? "#50535a" : addMouse.containsMouse ? "#45484e" : "#3a3c42"

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            color: "#e6e6e6"
            font.pixelSize: 15
            text: "+"
        }

        MouseArea {
            id: addMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                control.win.takeFocus()
                if (control.tab === "timers")
                    timers.add()
                else if (control.tab === "props")
                    props.showAddMenu(add)
                else
                    stage.add()
            }
        }
    }

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: add.bottom
        anchors.topMargin: 6
        anchors.bottom: parent.bottom

        TimersPanel {
            id: timers

            anchors.fill: parent
            visible: control.tab === "timers"
            win: control.win
        }

        PropsPanel {
            id: props

            anchors.fill: parent
            visible: control.tab === "props"
            win: control.win
        }

        StagePanel {
            id: stage

            anchors.fill: parent
            visible: control.tab === "stage"
            win: control.win
        }
    }
}
