import QtQuick

// A tick box with a label, in the app's dark style. It does not change itself: it reports
// what it was asked to become, and shows `checked`. Never takes keyboard focus.
Item {
    id: check

    property bool checked: false
    property string text
    property bool available: true

    signal toggled(bool checked)

    implicitWidth: box.width + (text !== "" ? 8 + label.implicitWidth : 0)
    implicitHeight: 24
    opacity: available ? 1 : 0.45

    Rectangle {
        id: box

        anchors.verticalCenter: parent.verticalCenter
        width: 16
        height: 16
        radius: 4
        color: check.checked ? "#ff8a1f" : "#15161a"
        border.width: 1
        border.color: check.checked ? "#ff8a1f" : mouse.containsMouse ? "#8b8f98" : "#5c5f66"

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 0.5
            visible: check.checked
            color: "black"
            font.pixelSize: 12
            font.bold: true
            text: "✓"
        }
    }

    Text {
        id: label

        anchors.left: box.right
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight
        color: "#e6e6e6"
        font.pixelSize: 13
        text: check.text
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        enabled: check.available
        onClicked: check.toggled(!check.checked)
    }
}
