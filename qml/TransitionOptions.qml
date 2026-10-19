import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

// What can be adjusted about the chosen transition, as a panel that opens from the
// button for it (see TransitionControls): a slider for each number, a swatch for a
// colour, and a pad of nine for a direction (TransitionOptionRows; see
// TransitionCatalogue for what an option is).
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

        TransitionOptionRows {
            width: parent.width
            options: panel.transition.options
            valueOf: (option) => panel.win.transitionOption(option)
            choose: (option, value) => panel.win.setTransitionOption(option, value)
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
