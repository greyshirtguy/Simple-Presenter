import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The popover of ProPresenter's Text button, in the toolbar after Search: formats for the
// words of the slides being shown, put on the selected slides (see SlideSelection), or on
// every slide of the presentation when none is selected. Laid out as ProPresenter's is,
// from a picture of it (2026-10-27): Font (the family; its style and colour;
// Capitalization, as Aa and AA, and Size), the alignment in the line and in the box,
// Stroke (On or Off, its width and colour) and Shadow (On or Off, its blur, opacity and
// colour, its angle, as a dial and a number, and its offset), with arrows beside every
// number.
//
// Each control shows what the text of those slides has in common, and is left empty
// where it differs (Themes.textFormatOf, textformat::Common), as ProPresenter's are: a
// colour that differs is shown as its two halves of white and black. Whatever is set
// goes to every text box with words on those slides at once, and only what is set
// changes (textformat.h says what each does; ProDocument::formatCuesText writes it). The
// arrows step what each box or run has: the size's each run's own size a point at a
// time, so that a slide's large words and small ones keep apart, what a typed size
// cannot do; Up, Down and the wheel in an empty box do the same. The slide that is live,
// if it is one of them, is drawn again on the screens (Main's redrawLive).
//
// Each change is for good, as Paste Text Style's is while showing: the first a session
// of the popover makes keeps a copy of the file with the editor's backups, and Revert
// puts the file back as it was before it, exactly, until the popover closes.
//
// GUESSES, ProPresenter's own having been seen in a picture and not used: that Aa is
// Title Case and AA All Capitals (a second click on either back to as typed); that a step
// of the size's arrows is a point; that the other numbers' arrows step each box's own
// where they differ, as the size's do; the angle dial's way round; that a colour given to
// the outline or the shadow while it is off turns it on (as the editor's colours do); and
// Revert, which ProPresenter has as Undo.
//
// The presentation's file is read for what the controls show when the popover opens and
// after each change: once for each thing the operator does, as the slides are read again.
Rectangle {
    id: panel

    // The operator window
    required property var win
    // The slides the formats go to (their ids), and what their text has in common
    readonly property var targets: win.textFormatTargets()
    readonly property bool someSelected: win.selectedPlaces().length > 0
    readonly property var common: win.document ? Themes.textFormatOf(win.document.path, targets) : ({})
    // Whether this session has changed the presentation, which Revert can take back
    property bool changedHere: false
    readonly property real gap: 8

    function has(key) {
        return common[key] !== undefined
    }

    // A format put on the slides' text; the slides read again only if a box changed,
    // and the live slide drawn again on the screens if it is one of them
    function put(format) {
        if (!win.document)
            return
        const done = Themes.formatText(win.document.path, targets, format)
        if (!win.report(done.error) || done.boxes === 0)
            return
        changedHere = true
        win.reloadDocument()
        win.redrawLive(targets)
    }

    function revert() {
        if (win.document && win.report(Themes.revertTextFormat(win.document.path))) {
            changedHere = false
            win.reloadDocument()
            win.redrawLive(targets)
        }
    }

    width: 300
    height: column.implicitHeight + 24
    color: "#25272b"
    radius: 8
    border.width: 1
    border.color: "#4a4d55"

    // (So that clicks on it do not reach what is under it, and put it away)
    MouseArea {
        anchors.fill: parent
    }

    RichTextBridge {
        id: bridge
    }

    // A heading, as ProPresenter's are: Font, Stroke, Shadow
    component Heading: Text {
        color: "#e6e6e6"
        font.pixelSize: 13
        font.bold: true
    }

    // The word under a control
    component Caption: Text {
        color: "#9a9da3"
        font.pixelSize: 10
        horizontalAlignment: Text.AlignHCenter
    }

    component Divider: Rectangle {
        width: column.width
        height: 1
        color: "#3a3c42"
    }

    // ProPresenter's On and Off, side by side: lit by what the text has, neither when it
    // differs
    component OnOff: Row {
        id: onOff

        property var value: undefined
        signal chosen(bool on)

        spacing: 0

        Repeater {
            model: [true, false]

            delegate: Rectangle {
                required property bool modelData

                width: 36
                height: 22
                radius: 4
                color: onOff.value === modelData ? "#ff8a1f" : choiceMouse.containsMouse ? "#45484e" : "#3a3c42"

                Text {
                    anchors.centerIn: parent
                    color: onOff.value === parent.modelData ? "black" : "#e6e6e6"
                    font.pixelSize: 12
                    text: parent.modelData ? "On" : "Off"
                }

                MouseArea {
                    id: choiceMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: onOff.chosen(parent.modelData)
                }
            }
        }
    }

    // ProPresenter's arrows beside a number, which step it: the arrows of a number that
    // is "textFormat:<name>" are "textFormat:<name>Up" and "...Down"
    component Stepper: Column {
        id: stepper

        property string name
        signal stepped(int by)

        Repeater {
            model: [1, -1]

            delegate: Rectangle {
                required property int modelData

                objectName: "textFormat:" + stepper.name + (modelData > 0 ? "Up" : "Down")
                width: 18
                height: 13
                radius: 3
                color: arrowMouse.pressed ? "#50535a" : arrowMouse.containsMouse ? "#45484e" : "#3a3c42"

                Text {
                    anchors.centerIn: parent
                    color: "#e6e6e6"
                    font.pixelSize: 8
                    text: parent.modelData > 0 ? "▲" : "▼"
                }

                MouseArea {
                    id: arrowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: stepper.stepped(parent.modelData)
                }
            }
        }
    }

    // A number with its arrows and the word for it under it, empty while the text
    // differs. A number typed is put as `key`'s format key; a step of the arrows (or of
    // Up, Down and the wheel, while it is empty) as `by`, each one's own stepped.
    component Number: Column {
        id: number

        // What it shows, of panel.common
        property string key
        property string by
        property alias caption: caption.text
        property real from: 0
        property real to: 1000
        property int decimals: 0
        // What is shown for a value of 1 (100 for an opacity shown in hundredths)
        property real factor: 1
        signal edited(real value)

        function stepBy(shown) {
            const format = {}
            format[by] = shown / factor
            panel.put(format)
        }

        spacing: 2

        Row {
            spacing: 2

            NumberField {
                objectName: "textFormat:" + number.key
                width: 52
                from: number.from
                to: number.to
                decimals: number.decimals
                blank: !panel.has(number.key)
                value: panel.has(number.key) ? panel.common[number.key] * number.factor : 0
                onEdited: (value) => number.edited(value)
                onStepped: (by) => number.stepBy(by)
                onFinished: panel.win.takeFocus()
            }

            Stepper {
                name: number.key
                onStepped: (by) => number.stepBy(by)
            }
        }

        Caption {
            id: caption

            width: 72
        }
    }

    Column {
        id: column

        x: 12
        y: 12
        width: parent.width - 24
        spacing: panel.gap

        Heading {
            text: "Font"
        }

        FontPicker {
            objectName: "textFormat:family"
            width: column.width
            family: panel.has("family") ? panel.common.family : ""
            families: bridge.fontFamilies()
            onPicked: (family) => panel.put({ family: family })
        }

        Row {
            spacing: panel.gap

            AppComboBox {
                id: style

                objectName: "textFormat:style"
                width: column.width - colour.width - panel.gap
                height: 26
                font.pixelSize: 12
                model: Themes.textStyles()
                // (Empty while the styles differ)
                currentIndex: panel.has("style") ? model.indexOf(panel.common.style) : -1
                displayText: currentIndex >= 0 ? currentText : ""
                onActivated: (index) => panel.put({ style: model[index] })
            }

            ColorButton {
                id: colour

                objectName: "textFormat:color"
                hasAlpha: false
                value: panel.has("color") ? panel.common.color : "transparent"
                onPicked: (value) => panel.put({ color: value })
            }
        }

        Row {
            spacing: panel.gap

            Column {
                spacing: 2

                Row {
                    spacing: 4

                    // Title Case and All Capitals; the one lit clicked again, as typed
                    Repeater {
                        model: [{ text: "Aa", value: 3 }, { text: "AA", value: 1 }]

                        delegate: IconButton {
                            required property var modelData

                            objectName: "textFormat:capitals" + modelData.value
                            width: 50
                            text: modelData.text
                            on: panel.has("capitalization") && panel.common.capitalization === modelData.value
                            onClicked: panel.put({ capitalization: on ? 0 : modelData.value })
                        }
                    }
                }

                Caption {
                    width: 104
                    text: "Capitalization"
                }
            }

            Item {
                width: column.width - 104 - sizeColumn.width - 2 * panel.gap
                height: 1
            }

            Column {
                id: sizeColumn

                spacing: 2

                // (The arrows: every run's own size a point larger, or smaller)
                Row {
                    spacing: 2

                    NumberField {
                        objectName: "textFormat:size"
                        width: 52
                        from: 1
                        to: 2000
                        blank: !panel.has("size")
                        value: panel.has("size") ? panel.common.size : 0
                        onEdited: (value) => panel.put({ size: value })
                        onStepped: (by) => panel.put({ sizeBy: by })
                        onFinished: panel.win.takeFocus()
                    }

                    Stepper {
                        name: "size"
                        onStepped: (by) => panel.put({ sizeBy: by })
                    }
                }

                Caption {
                    width: 72
                    text: "Size"
                }
            }
        }

        Divider {}

        Row {
            spacing: 4

            Repeater {
                model: [{ kind: "alignLeft", flag: Qt.AlignLeft }, { kind: "alignCenter", flag: Qt.AlignHCenter },
                        { kind: "alignRight", flag: Qt.AlignRight }, { kind: "alignJustify", flag: Qt.AlignJustify }]

                delegate: IconButton {
                    required property var modelData

                    objectName: "textFormat:" + modelData.kind
                    width: (column.width - 12) / 4
                    kind: modelData.kind
                    on: panel.has("alignment") && panel.common.alignment === modelData.flag
                    onClicked: panel.put({ alignment: modelData.flag })
                }
            }
        }

        Row {
            spacing: 4

            Repeater {
                model: [{ kind: "alignTop", flag: Qt.AlignTop }, { kind: "alignMiddle", flag: Qt.AlignVCenter },
                        { kind: "alignBottom", flag: Qt.AlignBottom }]

                delegate: IconButton {
                    required property var modelData

                    objectName: "textFormat:" + modelData.kind
                    width: (column.width - 8) / 3
                    kind: modelData.kind
                    on: panel.has("verticalAlignment") && panel.common.verticalAlignment === modelData.flag
                    onClicked: panel.put({ verticalAlignment: modelData.flag })
                }
            }
        }

        Divider {}

        Item {
            width: column.width
            height: 22

            Heading {
                anchors.verticalCenter: parent.verticalCenter
                text: "Stroke"
            }

            OnOff {
                objectName: "textFormat:stroke"
                anchors.right: parent.right
                value: panel.has("strokeOn") ? panel.common.strokeOn : undefined
                onChosen: (on) => panel.put({ strokeOn: on })
            }
        }

        Row {
            spacing: panel.gap
            anchors.right: parent.right

            Number {
                key: "strokeWidth"
                by: "strokeWidthBy"
                caption: "Width"
                to: 200
                decimals: 1
                onEdited: (value) => panel.put({ strokeWidth: value })
            }

            ColorButton {
                objectName: "textFormat:strokeColor"
                hasAlpha: false
                value: panel.has("strokeColor") ? panel.common.strokeColor : "transparent"
                onPicked: (value) => panel.put({ strokeColor: value })
            }
        }

        Divider {}

        Item {
            width: column.width
            height: 22

            Heading {
                anchors.verticalCenter: parent.verticalCenter
                text: "Shadow"
            }

            OnOff {
                objectName: "textFormat:shadow"
                anchors.right: parent.right
                value: panel.has("shadowOn") ? panel.common.shadowOn : undefined
                onChosen: (on) => panel.put({ textShadowEnabled: on })
            }
        }

        Row {
            spacing: panel.gap

            Number {
                key: "shadowBlur"
                by: "textShadowRadiusBy"
                caption: "Blur"
                to: 500
                onEdited: (value) => panel.put({ textShadowRadius: value })
            }

            Number {
                key: "shadowOpacity"
                by: "textShadowOpacityBy"
                caption: "Opacity"
                to: 100
                factor: 100
                onEdited: (value) => panel.put({ textShadowOpacity: value / 100 })
            }

            ColorButton {
                objectName: "textFormat:shadowColor"
                hasAlpha: false
                value: panel.has("shadowColor") ? panel.common.shadowColor : "transparent"
                onPicked: (value) => panel.put({ textShadowSolidColor: value })
            }
        }

        Row {
            spacing: panel.gap

            // The angle as ProPresenter shows it, a dial, beside the number; put when the
            // dial is let go of (each change being written)
            Dial {
                id: angleDial

                objectName: "textFormat:angleDial"
                width: 34
                height: 34
                from: 0
                to: 360
                startAngle: 0
                endAngle: 360
                wrap: true
                inputMode: Dial.Circular
                value: panel.has("shadowAngle") ? panel.common.shadowAngle : 0
                onPressedChanged: {
                    if (!pressed)
                        panel.put({ textShadowAngle: Math.round(value) % 360 })
                }
            }

            Number {
                key: "shadowAngle"
                by: "textShadowAngleBy"
                caption: "Angle"
                to: 360
                onEdited: (value) => panel.put({ textShadowAngle: value % 360 })
            }

            Number {
                key: "shadowOffset"
                by: "textShadowOffsetBy"
                caption: "Offset"
                to: 500
                onEdited: (value) => panel.put({ textShadowOffset: value })
            }
        }

        Divider {}

        // What the changes go to, and taking them back
        Text {
            objectName: "textFormat:goesTo"
            width: column.width
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 11
            text: (panel.common.boxes ?? 0) === 0
                  ? (panel.someSelected ? "The selected slides have" : "This presentation has") + " no words for a format to go on."
                  : "Changes go to " + (panel.someSelected ? (panel.targets.length === 1 ? "the selected slide" : "the " + panel.targets.length + " selected slides")
                                                           : panel.targets.length === 1 ? "the slide" : "all " + panel.targets.length + " slides")
                    + (panel.someSelected ? "" : " (Ctrl or Shift and a click on a slide selects it)") + ", for good."
        }

        AppButton {
            objectName: "textFormat:revert"
            height: 26
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 12
            text: "Revert"
            enabled: panel.changedHere
            onClicked: panel.revert()
        }
    }
}
