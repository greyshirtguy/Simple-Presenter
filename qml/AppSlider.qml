import QtQuick
import QtQuick.Controls.Basic

// Slider in the app's dark style. Never takes keyboard focus, so the arrow keys keep
// driving the slides.
Slider {
    id: control

    focusPolicy: Qt.NoFocus

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 120
        implicitHeight: 4
        width: control.availableWidth
        height: implicitHeight
        radius: 2
        color: "#3a3c42"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: control.enabled ? "#ff8a1f" : "#50535a"
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: 8
        color: !control.enabled ? "#6c6f75" : control.pressed ? "white" : "#e6e6e6"
    }
}
