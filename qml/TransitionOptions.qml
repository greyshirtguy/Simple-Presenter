import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

// What can be adjusted about the chosen transition, as a panel that opens under the
// button for it in the toolbar: a slider for each number, a swatch for a colour, and a
// pad of nine for a direction (see TransitionCatalogue for what an option is).
//
// A change takes effect with the next transition and is remembered for that transition;
// nothing here stores anything itself. Reset puts the transition back as it comes.
Popup {
    id: panel

    // The operator window, which has the chosen transition and what is chosen for it
    required property var win
    readonly property var transition: win.transition

    width: 292
    margins: 6
    padding: 14
    // Takes the keyboard while open, so that Esc closes it.
    focus: true
    // Another transition may be chosen while it is open; one with nothing to adjust
    // leaves it nothing to show.
    onTransitionChanged: {
        if (transition.options.length === 0)
            close()
    }

    // As the pop-up menu is: a rounded panel with a bright edge and a shadow.
    background: Item {
        RectangularShadow {
            anchors.fill: backdrop
            radius: backdrop.radius
            blur: 24
            spread: 2
            offset: Qt.vector2d(0, 6)
            color: "#b0000000"
        }

        Rectangle {
            id: backdrop

            anchors.fill: parent
            radius: 9
            color: "#33363d"
            border.width: 1.5
            border.color: "#8b8f98"
        }
    }

    contentItem: Column {
        spacing: 12

        Text {
            width: parent.width
            elide: Text.ElideRight
            color: "#e6e6e6"
            font.pixelSize: 14
            font.bold: true
            text: panel.transition.name
        }

        Repeater {
            model: panel.transition.options

            delegate: Column {
                id: row

                required property var modelData
                // What it is set to, which is the catalogue's value until it is changed
                readonly property var chosen: panel.win.transitionOption(modelData)

                width: parent.width
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
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - field.width - parent.spacing
                            from: row.modelData.from
                            to: row.modelData.to
                            value: row.chosen
                            onMoved: panel.win.setTransitionOption(row.modelData, Math.round(value * 100) / 100)
                        }

                        NumberField {
                            id: field

                            anchors.verticalCenter: parent.verticalCenter
                            width: 62
                            decimals: 2
                            from: row.modelData.from
                            to: row.modelData.to
                            step: (row.modelData.to - row.modelData.from) / 100
                            value: row.chosen
                            onEdited: (value) => panel.win.setTransitionOption(row.modelData, value)
                        }
                    }
                }

                // In a row of its own, or the swatch would be stretched to the panel's width.
                Component {
                    id: colour

                    Row {
                        ColorButton {
                            value: row.chosen
                            onChanging: (value) => panel.win.setTransitionOption(row.modelData, value.toString())
                            onPicked: (value) => panel.win.setTransitionOption(row.modelData, value.toString())
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
                                onClicked: panel.win.setTransitionOption(row.modelData, index)
                            }
                        }
                    }
                }
            }
        }

        AppButton {
            height: 28
            font.pixelSize: 13
            text: "Reset"
            enabled: panel.win.transitionChoices[panel.transition.name] !== undefined
            onClicked: panel.win.resetTransitionOptions()
        }
    }
}
