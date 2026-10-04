import QtQuick
import QtQuick.Controls.Basic

// Text box in the app's dark style.
TextField {
    id: control

    leftPadding: 8
    rightPadding: 8
    font.pixelSize: 14
    color: enabled ? "#e6e6e6" : "#6c6f75"
    selectionColor: "#ff8a1f"
    selectedTextColor: "black"
    selectByMouse: true

    background: Rectangle {
        implicitHeight: 34
        radius: 6
        color: "#15161a"
        border.width: 1
        border.color: control.activeFocus ? "#ff8a1f" : "#45484e"
    }
}
