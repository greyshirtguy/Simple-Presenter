import QtQuick

// What can be adjusted about a transition, a row for each of its options (see
// TransitionCatalogue for what an option is): a slider and a box for a number, a swatch
// for a colour, and a pad of nine for a direction. The panel of the transition in use
// has them (TransitionOptions), and so does the editor's Build tab (EditorBuilds).
//
// Nothing here keeps anything: `valueOf` says what an option is set to, and `choose` is
// told what it is set to. With `live`, a slider says so at every step it is dragged, as
// the transition in use takes it, which costs nothing; without, only when it is let go
// (the box beside it follows it meanwhile), as a build takes it, which is a change to a
// file and a step to undo each time.
// (Not `rows` for its id: a Grid has a property of that name, which the pad of directions
// would find first.)
Column {
    id: optionRows

    // The transition's options, and what each is set to and is to be set to
    property var options: []
    property var valueOf: (option) => option.value
    property var choose: (option, value) => {}
    property bool live: true

    spacing: 12

    Repeater {
        model: optionRows.options

        delegate: Column {
            id: row

            required property var modelData
            // What it is set to
            readonly property var chosen: optionRows.valueOf(modelData)

            width: optionRows.width
            spacing: 5

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                textFormat: Text.StyledText
                color: "#e6e6e6"
                font.pixelSize: 13
                text: row.modelData.label
                    + (row.modelData.note ? " <font color=\"#9a9da3\">· " + row.modelData.note + "</font>" : "")
            }

            Loader {
                width: parent.width
                sourceComponent: row.modelData.kind === "number" ? number
                               : row.modelData.kind === "color" ? colour : direction
            }

            Component {
                id: number

                Row {
                    spacing: 8

                    AppSlider {
                        id: slider

                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - field.width - parent.spacing
                        from: row.modelData.from
                        to: row.modelData.to
                        value: row.chosen
                        onMoved: {
                            if (optionRows.live)
                                optionRows.choose(row.modelData, Math.round(value * 100) / 100)
                        }
                        onPressedChanged: {
                            if (!pressed && !optionRows.live && Math.round(value * 100) / 100 !== row.chosen)
                                optionRows.choose(row.modelData, Math.round(value * 100) / 100)
                        }
                    }

                    NumberField {
                        id: field

                        anchors.verticalCenter: parent.verticalCenter
                        width: 62
                        decimals: 2
                        from: row.modelData.from
                        to: row.modelData.to
                        step: (row.modelData.to - row.modelData.from) / 100
                        value: slider.pressed ? Math.round(slider.value * 100) / 100 : row.chosen
                        onEdited: (value) => optionRows.choose(row.modelData, value)
                    }
                }
            }

            // In a row of its own, or the swatch would be stretched to the panel's width.
            Component {
                id: colour

                Row {
                    ColorButton {
                        value: row.chosen
                        onChanging: (value) => {
                            if (optionRows.live)
                                optionRows.choose(row.modelData, value.toString())
                        }
                        onPicked: (value) => optionRows.choose(row.modelData, value.toString())
                    }
                }
            }

            // Nine places, set out as the sides and corners of the picture they stand
            // for: the one chosen is where what is coming comes from, and its arrow the
            // way it travels.
            Component {
                id: direction

                Grid {
                    columns: 3
                    spacing: 3

                    Repeater {
                        model: ["↘", "↓", "↙", "→", "•", "←", "↗", "↑", "↖"]

                        delegate: IconButton {
                            required property string modelData
                            required property int index

                            text: modelData
                            on: row.chosen === index
                            available: (row.modelData.allowed & (1 << index)) !== 0
                            onClicked: optionRows.choose(row.modelData, index)
                        }
                    }
                }
            }
        }
    }
}
