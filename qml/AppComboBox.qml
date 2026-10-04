import QtQuick
import QtQuick.Controls.Basic

// Drop-down in the app's dark style, for a model of plain strings. Never takes keyboard
// focus, so the arrow keys keep driving the slides.
ComboBox {
    id: control

    focusPolicy: Qt.NoFocus
    font.pixelSize: 14

    contentItem: Text {
        leftPadding: 12
        text: control.displayText
        font: control.font
        color: "#e6e6e6"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: 34
        radius: 6
        color: control.down ? "#50535a" : control.hovered ? "#45484e" : "#3a3c42"
    }

    delegate: ItemDelegate {
        id: option

        required property var modelData
        required property int index

        width: ListView.view.width
        height: 30
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: option.modelData
            font: control.font
            color: "#e6e6e6"
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 4
            color: option.highlighted ? "#45484e" : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 4

        contentItem: ListView {
            implicitHeight: contentHeight
            clip: true
            model: control.delegateModel
            currentIndex: control.highlightedIndex
        }

        background: Rectangle {
            radius: 6
            color: "#2b2d31"
            border.width: 1
            border.color: "#45484e"
        }
    }
}
