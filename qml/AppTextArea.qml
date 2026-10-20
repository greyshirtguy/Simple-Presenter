import QtQuick
import QtQuick.Controls.Basic

// Text box of several lines in the app's dark style, as AppTextField is of one: for an
// HTTP request's body and headers. It grows with what is typed, to `maximumLines`, and
// scrolls beyond that.
ScrollView {
    id: view

    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property int maximumLines: 6
    readonly property alias activeFocusInside: area.activeFocus

    signal editingFinished

    implicitHeight: Math.min(area.implicitHeight, area.font.pixelSize * 1.45 * maximumLines + area.topPadding + area.bottomPadding)
    clip: true

    background: Rectangle {
        radius: 6
        color: "#15161a"
        border.width: 1
        border.color: area.activeFocus ? "#ff8a1f" : "#45484e"
    }

    TextArea {
        id: area

        leftPadding: 8
        rightPadding: 8
        topPadding: 6
        bottomPadding: 6
        font.pixelSize: 13
        color: enabled ? "#e6e6e6" : "#6c6f75"
        placeholderTextColor: "#6c6f75"
        selectionColor: "#ff8a1f"
        selectedTextColor: "black"
        selectByMouse: true
        wrapMode: TextEdit.Wrap
        background: null
        onEditingFinished: view.editingFinished()
    }
}
