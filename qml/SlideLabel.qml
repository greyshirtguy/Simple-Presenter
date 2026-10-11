import QtQuick
import QtQuick.Dialogs
import SimplePresenterApp

// The panel a label is made in: its words, and its colour. It is put up over the operator
// window for a slide's label (Edit… in the Label menu of a slide), and for one of the
// labels the workspace keeps ready (the Labels part of the settings), which are the same
// thing: a few words and a colour. Save, or Enter, hands both to whoever asked; Cancel,
// Esc or a click outside changes nothing.
//
// The colour is picked from a row of swatches, the first of which is no colour at all,
// or with the system's own colour picker, behind Other…. A label of no colour is drawn
// as plain words; one with a colour as a tag of that colour (see SlideGrid.qml), and a
// tag is what the panel shows as it is made.
//
// What ProPresenter's pop-up has, as the maintainer describes it: a box for the words, a
// swatch of ready colours, one for no colour, and a button for the system's picker. A
// GUESS: which colours its swatch has. These are the ones the settings here offer for a
// group's colour.
Rectangle {
    id: panel

    property bool open: false
    property string heading: "Slide Label"
    property string about: ""
    // The colour being made: "#rrggbb", or "" for no colour. (Not `color`, which is the
    // colour of the panel's own shade over the window.)
    property string tint: ""
    // Called with the words and the colour when it is saved
    property var done: null
    readonly property var swatches: [
        "#e53935", "#d81b60", "#8e24aa", "#5e35b1", "#3949ab", "#1e88e5",
        "#039be5", "#00acc1", "#00897b", "#43a047", "#7cb342", "#c0ca33",
        "#fdd835", "#ffb300", "#fb8c00", "#f4511e", "#6d4c41", "#757575"
    ]
    readonly property string words: field.text.trim()

    signal closed

    function show(heading, about, text, color, done) {
        panel.heading = heading
        panel.about = about
        panel.tint = color
        panel.done = done
        open = true
        field.text = text
        field.forceActiveFocus()
        field.selectAll()
    }

    function close() {
        open = false
        done = null
        closed()
    }

    function save() {
        const finished = done
        const text = words
        const picked = tint
        open = false
        done = null
        if (finished)
            finished(text, picked)
        closed()
    }

    // Whether words on a colour are easier read in black
    function light(color) {
        const part = i => parseInt(color.substring(1 + 2 * i, 3 + 2 * i), 16) / 255
        return color !== "" && 0.299 * part(0) + 0.587 * part(1) + 0.114 * part(2) > 0.6
    }

    visible: open
    color: "#99000000"

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    ColorDialog {
        id: systemPicker

        title: "Label Colour"
        onAccepted: panel.tint = String(selectedColor).substring(0, 7).toLowerCase()
    }

    Rectangle {
        objectName: "slideLabelPanel"
        anchors.centerIn: parent
        width: 440
        height: content.height + 36
        radius: 10
        color: "#2b2d31"
        border.width: 1
        border.color: "#45484e"

        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: content

            x: 20
            y: 18
            width: parent.width - 40
            spacing: 12

            Text {
                width: parent.width
                color: "#f0f0f0"
                font.pixelSize: 17
                font.bold: true
                text: panel.heading
            }

            Text {
                width: parent.width
                visible: panel.about !== ""
                elide: Text.ElideRight
                color: "#b4b7bd"
                font.pixelSize: 13
                text: panel.about
            }

            AppTextField {
                id: field

                objectName: "slideLabelText"
                width: parent.width
                maximumLength: 80
                placeholderText: "No words"
                placeholderTextColor: "#7d8088"
                onAccepted: panel.save()
                Keys.onEscapePressed: panel.close()
            }

            // The colours: none first, then the ready ones, then whatever the system's picker gives
            Flow {
                width: parent.width
                spacing: 6

                Rectangle {
                    objectName: "slideLabelNoColor"
                    width: 30
                    height: 24
                    radius: 4
                    color: "#15161a"
                    border.width: panel.tint === "" ? 2 : 1
                    border.color: panel.tint === "" ? "#ff8a1f" : "#5c5f66"

                    // No colour: an empty swatch with a line through it
                    Rectangle {
                        anchors.centerIn: parent
                        width: 30
                        height: 1.5
                        rotation: -38
                        color: "#e25555"
                        antialiasing: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: panel.tint = ""
                    }
                }

                Repeater {
                    model: panel.swatches

                    delegate: Rectangle {
                        required property string modelData

                        objectName: "slideLabelSwatch"
                        width: 30
                        height: 24
                        radius: 4
                        color: modelData
                        border.width: panel.tint === modelData ? 2 : 0
                        border.color: "white"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: panel.tint = parent.modelData
                        }
                    }
                }
            }

            Row {
                spacing: 10

                AppButton {
                    objectName: "slideLabelOther"
                    height: 28
                    font.pixelSize: 13
                    text: "Other…"
                    onClicked: {
                        if (panel.tint !== "")
                            systemPicker.selectedColor = panel.tint
                        systemPicker.open()
                    }
                }

                // The label as it will be drawn on a slide
                Rectangle {
                    objectName: "slideLabelPreview"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: panel.words !== "" || panel.tint !== ""
                    width: Math.min(260, preview.implicitWidth + 14)
                    height: 20
                    radius: 3
                    color: panel.tint !== "" ? panel.tint : "transparent"
                    border.width: panel.tint !== "" ? 0 : 1
                    border.color: "#5c5f66"

                    Text {
                        id: preview

                        anchors.fill: parent
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        color: panel.light(panel.tint) ? "black" : "white"
                        font.pixelSize: 12
                        text: panel.words
                    }
                }
            }

            Row {
                anchors.right: parent.right
                spacing: 10

                AppButton {
                    text: "Cancel"
                    onClicked: panel.close()
                }

                AppButton {
                    objectName: "slideLabelSave"
                    text: "Save"
                    onClicked: panel.save()
                }
            }
        }
    }
}
