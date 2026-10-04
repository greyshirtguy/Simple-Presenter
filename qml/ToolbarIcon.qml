import QtQuick

// A toolbar button drawn as a small icon with a caption under it. `kind` picks the icon:
// "dot" is a status light, green when `on`; "bin" is a window with its bottom pane filled
// when `on`; "settings" is a set of sliders.
Item {
    id: button

    property string kind: "dot"
    property string label
    property bool on: false

    signal clicked

    readonly property color ink: "#e6e6e6"

    width: 52
    height: 40

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: mouse.pressed ? "#50535a" : mouse.containsMouse ? "#3a3c42" : "transparent"
    }

    Item {
        id: icon

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 5
        width: 18
        height: 14

        Rectangle {
            anchors.centerIn: parent
            width: 12
            height: 12
            radius: 6
            visible: button.kind === "dot"
            color: button.on ? "#3ddc68" : "#5c5f66"
        }

        Rectangle {
            anchors.fill: parent
            visible: button.kind === "bin"
            radius: 2
            color: "transparent"
            border.width: 1.5
            border.color: button.ink

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 3
                height: 4
                color: button.on ? button.ink : "transparent"
                border.width: 1
                border.color: button.ink
            }
        }

        Repeater {
            model: button.kind === "settings" ? [0.25, 0.7, 0.4] : []

            delegate: Item {
                id: line

                required property real modelData
                required property int index

                y: 1 + index * 5
                width: icon.width
                height: 3

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 1.5
                    color: button.ink
                }

                Rectangle {
                    x: line.modelData * (parent.width - width)
                    anchors.verticalCenter: parent.verticalCenter
                    width: 5
                    height: 5
                    radius: 2.5
                    color: button.ink
                }
            }
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        color: "#c9cbd0"
        font.pixelSize: 10
        text: button.label
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.clicked()
    }
}
