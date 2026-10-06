import QtQuick

// A small square button drawn as a glyph, in the app's dark style. `kind` picks the
// glyph: "bold", "italic", "underline" and "strike" are letters; "alignLeft",
// "alignCenter", "alignRight" and "alignJustify" are lines of text; "alignTop",
// "alignMiddle" and "alignBottom" are a block against an edge; "eye" and "lock" are for
// the rows of a list; anything else shows `text`. `on` draws it as switched on. Never
// takes keyboard focus.
Rectangle {
    id: button

    property string kind
    property string text
    property bool on: false
    property bool available: true
    // Without a background until hovered, for use in a list row
    property bool flat: false
    // What the glyph is drawn in
    readonly property color ink: !available ? "#6c6f75" : on && !flat ? "black" : on || !flat ? "#e6e6e6" : "#7d8088"

    signal clicked

    width: 28
    height: 26
    radius: 5
    color: !available ? (flat ? "transparent" : "#2b2d31")
         : on && !flat ? "#ff8a1f"
         : mouse.pressed ? "#50535a" : mouse.containsMouse ? "#45484e" : flat ? "transparent" : "#3a3c42"

    // Letters, and anything given as text
    Text {
        anchors.centerIn: parent
        visible: text !== ""
        color: button.ink
        font.pixelSize: 14
        font.bold: button.kind === "bold"
        font.italic: button.kind === "italic"
        font.underline: button.kind === "underline"
        font.strikeout: button.kind === "strike"
        font.family: button.kind === "italic" ? "serif" : Qt.application.font.family
        text: button.kind === "bold" ? "B" : button.kind === "italic" ? "I" : button.kind === "underline" ? "U"
            : button.kind === "strike" ? "S" : button.text
    }

    // Lines of text, set left, centred, right or to both edges
    Column {
        anchors.centerIn: parent
        spacing: 2
        visible: button.kind === "alignLeft" || button.kind === "alignCenter" || button.kind === "alignRight"
                 || button.kind === "alignJustify"

        Repeater {
            model: button.kind === "alignJustify" ? [14, 14, 14, 14] : [14, 8, 12, 6]

            delegate: Item {
                required property int modelData

                width: 14
                height: 2

                Rectangle {
                    x: button.kind === "alignCenter" ? (14 - width) / 2 : button.kind === "alignRight" ? 14 - width : 0
                    width: parent.modelData
                    height: 2
                    color: button.ink
                }
            }
        }
    }

    // A block of text against the top, the middle or the bottom of its box
    Item {
        anchors.centerIn: parent
        width: 14
        height: 14
        visible: button.kind === "alignTop" || button.kind === "alignMiddle" || button.kind === "alignBottom"

        Rectangle {
            y: button.kind === "alignTop" ? 0 : button.kind === "alignMiddle" ? 6.25 : 12.5
            width: 14
            height: 1.5
            color: button.ink
        }

        Rectangle {
            x: 4
            y: button.kind === "alignTop" ? 3.5 : button.kind === "alignMiddle" ? 3 : 2.5
            width: 6
            height: 8
            radius: 1
            color: button.ink
            opacity: button.kind === "alignMiddle" ? 0.75 : 1
        }
    }

    // An eye, struck through when off
    Item {
        anchors.centerIn: parent
        width: 16
        height: 10
        visible: button.kind === "eye"

        Rectangle {
            anchors.fill: parent
            radius: 5
            color: "transparent"
            border.width: 1.5
            border.color: button.ink
        }

        Rectangle {
            anchors.centerIn: parent
            width: 4.5
            height: 4.5
            radius: 2.25
            color: button.ink
        }

        Rectangle {
            anchors.centerIn: parent
            width: 19
            height: 1.5
            rotation: -35
            visible: !button.on
            color: button.ink
        }
    }

    // A padlock, its shackle swung open when off
    Item {
        anchors.centerIn: parent
        width: 12
        height: 14
        visible: button.kind === "lock"

        Rectangle {
            x: button.on ? 2.5 : 6
            y: 0
            width: 7
            height: 10
            radius: 3.5
            color: "transparent"
            border.width: 1.5
            border.color: button.ink
        }

        Rectangle {
            x: 0
            y: 6
            width: 12
            height: 8
            radius: 2
            color: button.ink
        }
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        enabled: button.available
        onClicked: button.clicked()
    }
}
