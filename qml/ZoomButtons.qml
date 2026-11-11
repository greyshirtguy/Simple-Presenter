import QtQuick

// A pair of round buttons, − and +, that make the thumbnails of a grid smaller and
// larger, at the right of the bar under the grid (the slides', the media bin's, the chord
// sheet's). They only report the clicks; the sizes are the operator window's. (The media
// bin's had floated over the corner of its thumbnails, a little see-through, until the
// maintainer asked for them in the bin's foot, 2026-11-11.)
Row {
    id: buttons

    // Whether there is anything smaller, or larger, to go to
    property bool canShrink: true
    property bool canGrow: true
    // How big each button is
    property real size: 26

    signal shrink
    signal grow

    spacing: 8

    component ZoomButton: Rectangle {
        id: zoomButton

        property alias text: zoomLabel.text
        property bool available: true

        signal clicked

        width: buttons.size
        height: buttons.size
        radius: buttons.size / 2
        color: zoomMouse.pressed ? "#6a6d75" : zoomMouse.containsMouse ? "#45484e" : "#3a3c42"
        border.width: 1
        border.color: "#6c6f75"
        opacity: available ? 1 : 0.3

        Text {
            id: zoomLabel

            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            color: "#e6e6e6"
            font.pixelSize: Math.round(buttons.size * 0.65)
        }

        MouseArea {
            id: zoomMouse

            anchors.fill: parent
            hoverEnabled: true
            enabled: zoomButton.available
            onClicked: zoomButton.clicked()
        }
    }

    ZoomButton {
        text: "−"
        available: buttons.canShrink
        onClicked: buttons.shrink()
    }

    ZoomButton {
        text: "+"
        available: buttons.canGrow
        onClicked: buttons.grow()
    }
}
