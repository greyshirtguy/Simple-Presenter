import QtQuick

// The show controls: what is worked during a show besides the slides and the media, a
// tab for each kind: the timers, the props, the stage screens, and the macros.
//
// Whatever a tab lists can also be dragged onto a slide, which gives the slide the
// action that goes with it: do this to that timer, show that prop, give the stage
// that layout, run that macro (see DragSource, and dropActionOnSlide in Main.qml).
//
// The tabs are a row of buttons across the whole width, pictures and not words, as
// ProPresenter's are, and the pictures are ProPresenter's own: its timer, its pile of
// layers for the props, its speaker at a lectern for the stage, its M in brackets for
// the macros. The one whose tab is showing is blue. Under them, at the right, is the
// button that adds to what the tab holds.
Item {
    id: control

    // The operator window
    required property var win
    // Which tab is showing: "timers", "props", "stage" or "macros"
    property string tab: "timers"
    // Whether something here is being renamed in place, and so has the keyboard
    readonly property bool renaming: props.renaming !== "" || macros.renaming !== ""
    readonly property var tabs: [
        { id: "timers", name: "Timers", picture: "Countdown" },
        { id: "props", name: "Props", picture: "Prop" },
        { id: "stage", name: "Stage", picture: "Stage" },
        { id: "macros", name: "Macros", picture: "Macro" }
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

                // ProPresenter's own picture for the tab (see ProIcon)
                ProIcon {
                    anchors.centerIn: parent
                    name: button.modelData.picture
                    ink: button.ink
                    size: 25
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
    // layout, a macro or a collection of them. Small, and out of the way of the tabs.
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
                else if (control.tab === "macros")
                    macros.showAddMenu(add)
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

        MacrosPanel {
            id: macros

            anchors.fill: parent
            visible: control.tab === "macros"
            win: control.win
        }
    }
}
