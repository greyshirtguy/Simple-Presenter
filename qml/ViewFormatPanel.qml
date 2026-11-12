import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The panel of the … at the right of the bar under the slides, in Easy View and in Table
// View (see Main's slideView): how the slides' words are shown there, as ProPresenter's
// options for those views have it, by its support page on them (2026-11-12): the font,
// its size, colour and style (bold, italic), the alignment, and the background; for
// Easy View whether the slide's media is shown behind the words ("Media Actions can be
// toggled to show in Easy View"), and for Table View where words that do not fit are cut
// (Truncate: at the beginning, in the middle or at the end). Its Slides by Group, and
// the Grid's own options, are left out, as the maintainer asked (Slides by Group), or
// for now (the colour behind a grid thumbnail).
//
// Each change shows at once and is kept (Main's setViewFormat), in the app's settings:
// it is how this computer shows the slides to whoever runs the show, and nothing of it
// goes in a presentation or to the screens. Reset puts the view back as it starts.
//
// GUESSES, ProPresenter's own panel not having been seen: the names, the order and the
// look of the controls, and the size as pixels (on a thumbnail 250 wide, in Easy View).
Rectangle {
    id: panel

    // The operator window, and the view whose format this is: "easy" or "table"
    required property var win
    required property string view
    readonly property bool easy: view === "easy"
    readonly property var format: easy ? win.easyFormat : win.tableFormat
    readonly property real gap: 8

    function put(changes) {
        win.setViewFormat(view, changes)
    }

    // A colour as it is kept: "#rrggbb"
    function written(color) {
        return String(color).substring(0, 7).toLowerCase()
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

    component Caption: Text {
        anchors.verticalCenter: parent.verticalCenter
        color: "#9a9da3"
        font.pixelSize: 12
    }

    Column {
        id: column

        x: 12
        y: 12
        width: parent.width - 24
        spacing: panel.gap

        Text {
            color: "#e6e6e6"
            font.pixelSize: 13
            font.bold: true
            text: panel.easy ? "Easy View" : "Table View"
        }

        FontPicker {
            objectName: "viewFormat:family"
            width: column.width
            family: panel.format.family
            families: bridge.fontFamilies()
            onPicked: (family) => panel.put({ family: family })
        }

        // The size, the colour, bold and italic
        Row {
            spacing: panel.gap

            NumberField {
                objectName: "viewFormat:size"
                width: 52
                from: 6
                to: 96
                value: panel.format.size
                onEdited: (value) => panel.put({ size: Math.round(value) })
                onStepped: (by) => panel.put({ size: Math.max(6, Math.min(96, panel.format.size + by)) })
                onFinished: panel.win.takeFocus()
            }

            ColorButton {
                objectName: "viewFormat:color"
                hasAlpha: false
                value: panel.format.color
                onChanging: (value) => panel.put({ color: panel.written(value) })
                onPicked: (value) => panel.put({ color: panel.written(value) })
            }

            IconButton {
                objectName: "viewFormat:bold"
                kind: "bold"
                on: panel.format.bold
                onClicked: panel.put({ bold: !panel.format.bold })
            }

            IconButton {
                objectName: "viewFormat:italic"
                kind: "italic"
                on: panel.format.italic
                onClicked: panel.put({ italic: !panel.format.italic })
            }
        }

        // The alignment in the line
        Row {
            spacing: 4

            Repeater {
                model: [{ kind: "alignLeft", align: "left" }, { kind: "alignCenter", align: "center" }, { kind: "alignRight", align: "right" }]

                delegate: IconButton {
                    required property var modelData

                    objectName: "viewFormat:" + modelData.kind
                    width: (column.width - 8) / 3
                    kind: modelData.kind
                    on: panel.format.align === modelData.align
                    onClicked: panel.put({ align: modelData.align })
                }
            }
        }

        Row {
            spacing: panel.gap

            Caption {
                text: panel.easy ? "Background" : "Table Background"
            }

            ColorButton {
                objectName: "viewFormat:background"
                hasAlpha: false
                value: panel.format.background
                onChanging: (value) => panel.put({ background: panel.written(value) })
                onPicked: (value) => panel.put({ background: panel.written(value) })
            }
        }

        // Easy View: the slide's media behind its words
        AppCheck {
            objectName: "viewFormat:showMedia"
            visible: panel.easy
            text: "Show Media"
            checked: panel.format.showMedia
            onToggled: (checked) => panel.put({ showMedia: checked })
        }

        // Table View: where words that do not fit are cut
        Row {
            visible: !panel.easy
            spacing: 4

            Caption {
                width: 64
                text: "Truncate"
            }

            Repeater {
                model: [{ label: "Beginning", truncate: "start" }, { label: "Middle", truncate: "middle" }, { label: "End", truncate: "end" }]

                delegate: IconButton {
                    required property var modelData

                    objectName: "viewFormat:truncate:" + modelData.truncate
                    width: (column.width - 64 - 12) / 3
                    text: modelData.label
                    on: panel.format.truncate === modelData.truncate
                    onClicked: panel.put({ truncate: modelData.truncate })
                }
            }
        }

        AppButton {
            objectName: "viewFormat:reset"
            height: 26
            font.pixelSize: 12
            text: "Reset"
            onClicked: panel.win.resetViewFormat(panel.view)
        }
    }
}
