import QtQuick

// The transition that every change on the output is made with, and how long it takes: the
// chosen transition (a click opens the menu of them all), a button for what can be
// adjusted about it, and its length as a slider and as a number of seconds.
//
// It sits over the bottom left corner of the slides, as the thumbnail size buttons sit
// over the bottom right, and like them is faint until the pointer is on it or it is in
// use. It is drawn at four fifths of the size its parts are made at; what opens from it
// (the menu, the panel of what can be adjusted) opens upwards, at full size.
Item {
    id: controls

    // The operator window: the transition is its state, and what is done here is done
    // by calling its functions.
    required property var win
    readonly property real shrink: 0.8
    readonly property bool busy: hover.hovered || options.opened || durationField.activeFocus || win.menuItem === controls

    width: panel.width * shrink
    height: panel.height * shrink
    opacity: busy ? 1 : 0.7

    HoverHandler {
        id: hover
    }

    Rectangle {
        id: panel

        transformOrigin: Item.TopLeft
        scale: controls.shrink
        width: row.width + 12
        height: 38
        radius: 8
        color: "#23252b"
        border.width: 1
        border.color: "#3a3c42"

        // Not through to the slide underneath
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Row {
            id: row

            anchors.centerIn: parent
            spacing: 6

            // The chosen transition. A click opens the menu of them all, by category.
            Rectangle {
                objectName: "transitionButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 150
                height: 28
                radius: 6
                color: transitionMouse.pressed ? "#50535a" : transitionMouse.containsMouse ? "#45484e" : "#3a3c42"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 22
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: controls.win.textColor
                    font.pixelSize: 13
                    text: controls.win.transition.name
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: controls.win.dimTextColor
                    font.pixelSize: 10
                    text: "▲"
                }

                MouseArea {
                    id: transitionMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: controls.win.showTransitionMenu(controls)
                }
            }

            // What can be adjusted about it, for the transitions that have anything
            IconButton {
                objectName: "transitionOptionsButton"
                anchors.verticalCenter: parent.verticalCenter
                height: 28
                kind: "sliders"
                on: options.opened
                available: controls.win.transition.options.length > 0
                onClicked: options.opened ? options.close() : options.open()
            }

            AppSlider {
                anchors.verticalCenter: parent.verticalCenter
                width: 100
                from: 0
                to: 3
                stepSize: 0.05
                enabled: controls.win.transitionIndex !== 0
                value: Math.min(controls.win.transitionDuration, to)
                onMoved: controls.win.transitionDuration = Math.round(value * 100) / 100
            }

            // Seconds; accepts any value from 0 up, beyond the slider's range.
            AppTextField {
                id: durationField

                function reset() {
                    text = Number(controls.win.transitionDuration.toFixed(2)).toString()
                }

                anchors.verticalCenter: parent.verticalCenter
                width: 42
                height: 28
                leftPadding: 4
                rightPadding: 6
                font.pixelSize: 13
                horizontalAlignment: TextInput.AlignRight
                enabled: controls.win.transitionIndex !== 0
                onEditingFinished: {
                    const seconds = Number(text.replace(",", "."))
                    if (text.trim() !== "" && isFinite(seconds) && seconds >= 0)
                        controls.win.transitionDuration = seconds
                    reset()
                    controls.win.takeFocus()
                }
                Keys.onEscapePressed: {
                    reset()
                    controls.win.takeFocus()
                }
                Component.onCompleted: reset()

                Connections {
                    target: controls.win

                    function onTransitionDurationChanged() {
                        durationField.reset()
                    }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                rightPadding: 2
                color: controls.win.dimTextColor
                font.pixelSize: 12
                text: "s"
            }
        }
    }

    // Over the controls, these being at the bottom of the window. It belongs to this
    // item and not to its button, which is drawn small: a panel placed by something
    // drawn small is placed by small measures, and is itself full size.
    TransitionOptions {
        id: options

        y: -height - 8
        win: controls.win
        onClosed: controls.win.takeFocus()
    }
}
