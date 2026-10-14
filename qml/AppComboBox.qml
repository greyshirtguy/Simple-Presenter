import QtQuick
import QtQuick.Controls.Basic

// Drop-down in the app's dark style, for a model of plain strings. Never takes keyboard
// focus, so the arrow keys keep driving the slides.
ComboBox {
    id: control

    // How each string is shown, where that is not as it is: a function from the
    // string to what is drawn (the keys of the chord editors, with ♯ and ♭). Only
    // drawn: what is picked is the string itself.
    property var shownAs: null

    focusPolicy: Qt.NoFocus
    font.pixelSize: 14

    contentItem: Text {
        leftPadding: 12
        text: control.shownAs ? control.shownAs(control.displayText) : control.displayText
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
            text: control.shownAs ? control.shownAs(option.modelData) : option.modelData
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
        // Kept inside the window
        margins: 6
        padding: 4
        // Takes the keyboard while open, so that Esc closes it.
        focus: true

        // A long list scrolls instead of running down the window.
        contentItem: ListView {
            implicitHeight: Math.min(contentHeight, 420)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

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
