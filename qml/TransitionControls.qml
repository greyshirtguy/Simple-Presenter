import QtQuick

// The transition that changes on the output are made with, and how long it takes: the
// chosen transition (a click opens the menu of them all), a button for what can be
// adjusted about it, and its length as a slider and as a number of seconds.
//
// It is at the left of the thin bar under the slides, as the thumbnail size buttons are
// at its right, for the slides' transition; and again at the foot of the media bin, for
// the media bin's own (`media`), as ProPresenter's media bin has at its bottom left. It
// is drawn at four fifths of the size its parts are made at, which keeps the bar thin;
// what opens from it (the menu, the panel of what can be adjusted) opens upwards, at
// full size.
Item {
    id: controls

    // The operator window: the transition is its state, and what is done here is done
    // by calling its functions.
    required property var win
    // Which of its two: the media bin's, or the slides'
    property bool media: false
    readonly property var transition: media ? win.mediaTransition : win.transition
    readonly property real duration: media ? win.mediaTransitionDuration : win.transitionDuration
    readonly property bool cut: (media ? win.mediaTransitionIndex : win.transitionIndex) === 0
    readonly property real shrink: 0.8

    function setDuration(seconds) {
        if (media)
            win.mediaTransitionDuration = seconds
        else
            win.transitionDuration = seconds
    }

    width: panel.width * shrink
    height: panel.height * shrink

    Item {
        id: panel

        transformOrigin: Item.TopLeft
        scale: controls.shrink
        width: row.width
        height: 28

        Row {
            id: row

            anchors.centerIn: parent
            spacing: 6

            // The chosen transition. A click opens the menu of them all, by category.
            Rectangle {
                objectName: controls.media ? "mediaTransitionButton" : "transitionButton"
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
                    text: controls.transition.name
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
                    onClicked: controls.win.showTransitionMenu(controls, controls.media)
                }
            }

            // What can be adjusted about it, for the transitions that have anything
            IconButton {
                objectName: controls.media ? "mediaTransitionOptionsButton" : "transitionOptionsButton"
                anchors.verticalCenter: parent.verticalCenter
                height: 28
                kind: "sliders"
                on: options.opened
                available: controls.transition.options.length > 0
                onClicked: options.opened ? options.close() : options.open()
            }

            AppSlider {
                anchors.verticalCenter: parent.verticalCenter
                width: 100
                from: 0
                to: 3
                stepSize: 0.05
                enabled: !controls.cut
                value: Math.min(controls.duration, to)
                onMoved: controls.setDuration(Math.round(value * 100) / 100)
            }

            // Seconds; accepts any value from 0 up, beyond the slider's range.
            AppTextField {
                id: durationField

                function reset() {
                    text = Number(controls.duration.toFixed(2)).toString()
                }

                anchors.verticalCenter: parent.verticalCenter
                width: 42
                height: 28
                leftPadding: 4
                rightPadding: 6
                font.pixelSize: 13
                horizontalAlignment: TextInput.AlignRight
                enabled: !controls.cut
                onEditingFinished: {
                    const seconds = Number(text.replace(",", "."))
                    if (text.trim() !== "" && isFinite(seconds) && seconds >= 0)
                        controls.setDuration(seconds)
                    reset()
                    controls.win.takeFocus()
                }
                Keys.onEscapePressed: {
                    reset()
                    controls.win.takeFocus()
                }
                Component.onCompleted: reset()

                Connections {
                    target: controls

                    function onDurationChanged() {
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
        media: controls.media
        onClosed: controls.win.takeFocus()
    }
}
