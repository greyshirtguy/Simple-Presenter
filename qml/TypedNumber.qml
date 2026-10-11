import QtQuick

// The number being typed for a slide, shown over the slides while it is typed (see
// src/slidenumber.h, which says how the whole of it goes; Main.qml has the keys).
//
// A dark panel with the digits in it: large, to be read at a glance from where the
// operator sits, and three-quarters solid, so that the slides under it are not lost.
// It comes at once, with the first digit, and goes with a quick fade: when the number
// is taken, when it is given up, and by itself when a second has gone by without a
// key. The digits are kept in it while it fades, the number itself having gone by then.
//
// It is made when a number is first typed and not with the window (see Main.qml). It
// draws nothing while no number is up, and takes no clicks at any time.
Item {
    id: panel

    // What is typed so far: "" when no number is being typed
    property string typed: ""
    // How solid it is while a number is up, and how long its going takes, in
    // milliseconds
    readonly property real solid: 0.75
    readonly property int fade: 150
    // The digits it shows, which outlast `typed` by the fade
    property string shown: ""

    onTypedChanged: {
        if (typed !== "")
            shown = typed
    }
    Component.onCompleted: shown = typed

    implicitWidth: sheet.width
    implicitHeight: sheet.height
    opacity: 0
    visible: opacity > 0

    states: State {
        name: "up"
        when: panel.typed !== ""

        PropertyChanges {
            panel.opacity: panel.solid
        }
    }

    // (Only on the way out: it comes at once.)
    transitions: Transition {
        from: "up"

        NumberAnimation {
            property: "opacity"
            duration: panel.fade
        }
    }

    Rectangle {
        id: sheet

        objectName: "typedNumber"
        width: Math.max(150, digits.implicitWidth + 64)
        height: 124
        radius: 18
        color: "#101114"
        border.width: 1
        border.color: "#5c5f66"

        Text {
            id: digits

            objectName: "typedNumberDigits"
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: 76
            font.bold: true
            text: panel.shown
        }
    }
}
