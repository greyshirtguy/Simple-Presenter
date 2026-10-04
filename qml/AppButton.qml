import QtQuick
import QtQuick.Controls.Basic

// Button in the app's dark style. Never takes keyboard focus, so the arrow keys
// keep driving the slides.
Button {
    id: control

    // Draws the button in red while enabled: for actions that take something off air.
    property bool alert: false

    focusPolicy: Qt.NoFocus
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: 14

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? "#6c6f75" : control.alert ? "white" : "#e6e6e6"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: 34
        radius: 6
        color: !control.enabled ? "#2b2d31"
             : control.alert ? (control.down ? "#e25555" : control.hovered ? "#d84343" : "#c62828")
             : control.down ? "#50535a" : control.hovered ? "#45484e" : "#3a3c42"
    }
}
