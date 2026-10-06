import QtQuick

// A pair of round buttons, − and +, that sit over the corner of a grid of thumbnails and
// make them smaller and larger. They only report the clicks; the sizes are the operator
// window's.
Row {
    id: buttons

    // Whether there is anything smaller, or larger, to go to
    property bool canShrink: true
    property bool canGrow: true

    signal shrink
    signal grow

    spacing: 8

    component ZoomButton: Rectangle {
        id: zoomButton

        property alias text: zoomLabel.text
        property bool available: true

        signal clicked

        width: 26
        height: 26
        radius: 13
        color: zoomMouse.pressed ? "#6a6d75" : "#3a3c42"
        border.width: 1
        border.color: "#6c6f75"
        opacity: !available ? 0.3 : zoomMouse.containsMouse ? 1 : 0.7

        Text {
            id: zoomLabel

            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            color: "#e6e6e6"
            font.pixelSize: 17
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
