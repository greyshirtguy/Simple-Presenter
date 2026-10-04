import QtQuick
import QtQuick.Controls.Basic

// Toolbar button in the app's dark style. Never takes keyboard focus, so the arrow keys
// keep driving the slides.
Button {
    id: control

    focusPolicy: Qt.NoFocus
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: 14

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "#e6e6e6" : "#6c6f75"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitHeight: 34
        radius: 6
        color: !control.enabled ? "#2b2d31" : control.down ? "#50535a" : control.hovered ? "#45484e" : "#3a3c42"
    }
}
