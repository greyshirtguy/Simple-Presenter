import QtQuick

// A pair of round buttons, − and +, that make the thumbnails of a grid smaller and
// larger. They only report the clicks; the sizes are the operator window's.
//
// Over the corner of a grid, as in the media bin, they are a little see-through until
// the pointer is on them. In a bar of their own, as under the slides, they are `solid`.
Row {
    id: buttons

    // Whether there is anything smaller, or larger, to go to
    property bool canShrink: true
    property bool canGrow: true
    // Not see-through: for where they are not over anything
    property bool solid: false
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
        color: zoomMouse.pressed ? "#6a6d75" : zoomMouse.containsMouse && buttons.solid ? "#45484e" : "#3a3c42"
        border.width: 1
        border.color: "#6c6f75"
        opacity: !available ? 0.3 : zoomMouse.containsMouse || buttons.solid ? 1 : 0.7

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
