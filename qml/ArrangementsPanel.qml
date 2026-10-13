import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The window that lists the arrangements of the presentation being viewed, which
// ProPresenter calls its Arrangement Manager: where one is made, copied, renamed and
// removed. It comes up over the operator window from Edit Arrangements… in the menu of
// the arrangement editor's picker (ArrangementBar.qml), and its own ✕, Esc or a click
// outside puts it away.
//
// Each row is an arrangement: its name, and after it the groups it goes through, each
// a narrow mark in its group's colour with the first letter of the group's name, to
// tell two of a like name apart at a glance (that part is on trial). A click picks
// one; the buttons under the list work on the one picked. A double click on a name
// renames it where it stands, and a right click has the same as the buttons. What an
// arrangement goes through is changed in the editor over the slides, not here.
//
// Master is not in the list: it is the order the presentation's groups are in, and is
// not an arrangement that can be copied, renamed or removed.
//
// Every change is saved in the presentation's file at once (the operator window does
// it: see newArrangement() and the functions after it in Main.qml).
//
// GUESSES: what ProPresenter's window looks like was not looked at; its help says only
// that arrangements are made, removed, renamed and duplicated there. Removing asks
// first, as removing does elsewhere in this app.
Rectangle {
    id: panel

    // The operator window
    required property var win
    readonly property var document: win.document
    readonly property var arrangements: document && document.arrangementList ? document.arrangementList : []
    readonly property var groups: document && document.groups ? document.groups : []
    // The arrangement picked in the list, by id, and the one whose name is being typed
    property string picked: ""
    property string renaming: ""
    readonly property var chosen: arrangements.find(candidate => candidate.id === picked) ?? null
    readonly property color accentColor: "#ff8a1f"

    signal closed

    // The colour a group is shown in: the one its slides' frames have (see colourOf()
    // in ArrangementBar.qml, which this is the same as).
    function colourOf(group) {
        if (group === null)
            return "#5a5d66"
        const known = win.configuredGroup({ group: group.name })
        return known ? known.color : group.color !== "" ? group.color : "#5a5d66"
    }

    // Comes up on the arrangement being viewed, or with Master viewed on the first.
    function open() {
        const viewed = win.viewedArrangement()
        picked = viewed ? viewed.id : arrangements.length > 0 ? arrangements[0].id : ""
        renaming = ""
        forceActiveFocus()
    }

    // Gives the window the keyboard, unless a name is being typed, which has it.
    function takeKeys() {
        if (renaming === "")
            forceActiveFocus()
    }

    function pickNamed(name) {
        const made = arrangements.find(candidate => candidate.name === name)
        picked = made ? made.id : picked
        return made !== undefined
    }

    // A new arrangement, of every group once, to be named at once. It is the one
    // viewed after, as one made from the editor's menu is.
    function add() {
        const name = win.newArrangement()
        if (name !== "" && pickNamed(name))
            renaming = picked
    }

    function duplicate() {
        if (chosen === null)
            return
        const name = win.duplicateArrangement(chosen.id)
        if (name !== "")
            pickNamed(name)
    }

    function rename(id, name) {
        renaming = ""
        win.renameArrangement(id, name)
        forceActiveFocus()
    }

    function remove(id) {
        const place = arrangements.findIndex(candidate => candidate.id === id)
        const wasPicked = picked === id
        win.removeArrangement(id)
        // The one that takes its place in the list is picked, or the one before.
        if (wasPicked && !arrangements.some(candidate => candidate.id === id))
            picked = arrangements.length === 0 ? "" : arrangements[Math.min(place, arrangements.length - 1)].id
    }

    // Removing asks first: a menu, by the thing that was clicked.
    function askToRemove(entry, item, x, y) {
        win.showMenu([{ header: "Delete “" + entry.name + "”?" },
                      { label: "Delete Arrangement", danger: true, run: () => remove(entry.id) }], item, x, y)
    }

    function showRowMenu(entry, item, x, y) {
        win.showMenu([
            { header: entry.name },
            { label: "Rename", run: () => { picked = entry.id; renaming = entry.id } },
            { label: "Duplicate", run: () => { picked = entry.id; duplicate() } },
            { label: "Delete…", items: [{ header: "Delete “" + entry.name + "”?" },
                                        { label: "Delete Arrangement", danger: true, run: () => remove(entry.id) }] }
        ], item, x, y)
    }

    color: "#99000000"

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape)
            closed()
        else if ((event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) && chosen !== null)
            askToRemove(chosen, removeButton)
        // (Nothing typed here is a key of the show's.)
        event.accepted = true
    }

    // Another presentation opened under the window, or the picked arrangement gone
    // from the file: what is picked is one that is there, or none.
    onArrangementsChanged: {
        if (picked !== "" && !arrangements.some(candidate => candidate.id === picked))
            picked = arrangements.length > 0 ? arrangements[0].id : ""
    }

    // A click outside the window puts it away; nothing behind it is clicked or scrolled.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.closed()
    }

    // A button under the list
    component PanelButton: Rectangle {
        id: button

        property string label
        property bool available: true
        signal clicked

        width: words.implicitWidth + 24
        height: 26
        radius: 6
        color: !available ? "#33353a" : buttonMouse.pressed ? "#5c5f67" : buttonMouse.containsMouse ? "#53565e" : "#474a51"

        Text {
            id: words

            anchors.centerIn: parent
            color: button.available ? "#e6e6e6" : "#7d8088"
            font.pixelSize: 12
            text: button.label
        }

        MouseArea {
            id: buttonMouse

            anchors.fill: parent
            hoverEnabled: true
            enabled: button.available
            onClicked: button.clicked()
        }
    }

    Rectangle {
        id: sheet

        objectName: "arrangementsPanel"
        anchors.centerIn: parent
        width: Math.min(panel.width - 40, 560)
        height: Math.min(panel.height - 40, 400)
        radius: 10
        color: "#25272b"
        border.width: 1
        border.color: "#4a4d55"

        // (A click in the window is not a click outside it.)
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: {
                // A click on the window finishes a name that was being typed.
                if (panel.renaming !== "")
                    panel.forceActiveFocus()
            }
        }

        Text {
            id: title

            x: 18
            y: 14
            color: "#f0f0f0"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            text: "Arrangements"
        }

        Text {
            anchors.left: title.right
            anchors.leftMargin: 10
            anchors.right: shut.left
            anchors.rightMargin: 10
            anchors.baseline: title.baseline
            elide: Text.ElideRight
            color: "#9a9da3"
            font.pixelSize: 12
            text: panel.document ? panel.document.name : ""
        }

        Rectangle {
            id: shut

            objectName: "arrangementsClose"
            anchors.right: parent.right
            anchors.rightMargin: 10
            y: 10
            width: 26
            height: 26
            radius: 13
            color: shutMouse.containsMouse ? "#474a51" : "transparent"

            Text {
                anchors.centerIn: parent
                color: "#c8cace"
                font.pixelSize: 13
                text: "✕"
            }

            MouseArea {
                id: shutMouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: panel.closed()
            }
        }

        Rectangle {
            id: well

            x: 12
            y: 46
            width: parent.width - 24
            height: parent.height - y - 52
            radius: 6
            color: "#1b1c1f"

            ListView {
                id: list

                objectName: "arrangementsList"
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                spacing: 2
                model: panel.arrangements

                ScrollBar.vertical: ScrollBar {}

                KineticWheel {}

                delegate: Rectangle {
                    id: row

                    required property var modelData
                    readonly property bool chosen: panel.picked === modelData.id
                    readonly property bool typing: panel.renaming === modelData.id
                    readonly property bool viewed: panel.document !== null && panel.document.arrangement === modelData.name

                    objectName: "arrangementRow"
                    width: list.width
                    height: 32
                    radius: 5
                    color: chosen ? "#3f4249" : rowMouse.containsMouse ? "#2c2e33" : "transparent"

                    MouseArea {
                        id: rowMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: (mouse) => {
                            panel.picked = row.modelData.id
                            if (mouse.button === Qt.RightButton)
                                panel.showRowMenu(row.modelData, row, mouse.x, mouse.y)
                        }
                        onDoubleClicked: (mouse) => {
                            if (mouse.button === Qt.LeftButton)
                                panel.renaming = row.modelData.id
                        }
                    }

                    Text {
                        id: rowName

                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, 170)
                        visible: !row.typing
                        elide: Text.ElideRight
                        color: "#e6e6e6"
                        font.pixelSize: 13
                        text: row.modelData.name
                    }

                    // The one the presentation is being viewed in says so.
                    Text {
                        id: viewedTag

                        anchors.left: rowName.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.viewed && !row.typing
                        color: "#9a9da3"
                        font.pixelSize: 11
                        text: "viewed"
                    }

                    // What it goes through, very small: a narrow mark for each place,
                    // in its group's colour, with the first letter of the group's name.
                    // (ON TRIAL, at the maintainer's asking: labels with the names in
                    // them had no room to show a whole arrangement, and the names
                    // alone would do. To go back to names alone, take this Row out.)
                    Row {
                        objectName: "arrangementMarks"
                        anchors.left: parent.left
                        anchors.leftMargin: 236
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        height: 16
                        spacing: 2
                        clip: true
                        visible: !row.typing

                        Repeater {
                            model: row.modelData.groups

                            delegate: Rectangle {
                                id: mark

                                required property string modelData
                                readonly property var group: panel.groups.find(candidate => candidate.id === modelData) ?? null
                                readonly property color ground: panel.colourOf(group)
                                readonly property bool pale: 0.299 * ground.r + 0.587 * ground.g + 0.114 * ground.b > 0.6
                                // The first letter of its name; nothing for a group with no name
                                readonly property string letter: group === null ? "?" : group.name.trim().charAt(0).toUpperCase()

                                width: 13
                                height: 16
                                radius: 3
                                color: ground

                                Text {
                                    anchors.centerIn: parent
                                    color: mark.pale ? "#1b1c1f" : "white"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    text: mark.letter
                                }
                            }
                        }
                    }

                    // The name, to be typed over
                    Rectangle {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 12
                        height: 24
                        radius: 4
                        visible: row.typing
                        color: "#15161a"
                        border.width: 1
                        border.color: panel.accentColor

                        TextInput {
                            objectName: "arrangementNameBox"
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            color: "#f0f0f0"
                            selectionColor: panel.accentColor
                            selectedTextColor: "black"
                            font.pixelSize: 13
                            onVisibleChanged: {
                                if (visible) {
                                    text = row.modelData.name
                                    forceActiveFocus()
                                    selectAll()
                                }
                            }
                            Component.onCompleted: {
                                if (visible) {
                                    text = row.modelData.name
                                    forceActiveFocus()
                                    selectAll()
                                }
                            }
                            onAccepted: panel.rename(row.modelData.id, text)
                            onActiveFocusChanged: {
                                if (!activeFocus && row.typing)
                                    panel.rename(row.modelData.id, text)
                            }
                            Keys.onEscapePressed: {
                                panel.renaming = ""
                                panel.forceActiveFocus()
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 60
                visible: panel.arrangements.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "This presentation has no arrangements: it is shown as Master, every group once in the order it has them. New makes an arrangement that begins as a copy of that."
            }
        }

        Row {
            x: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 13
            spacing: 8

            PanelButton {
                objectName: "arrangementsNew"
                label: "New"
                onClicked: panel.add()
            }

            PanelButton {
                objectName: "arrangementsDuplicate"
                label: "Duplicate"
                available: panel.chosen !== null
                onClicked: panel.duplicate()
            }

            PanelButton {
                id: removeButton

                objectName: "arrangementsDelete"
                label: "Delete…"
                available: panel.chosen !== null
                onClicked: panel.askToRemove(panel.chosen, removeButton)
            }
        }

        PanelButton {
            objectName: "arrangementsDone"
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 13
            label: "Done"
            onClicked: panel.closed()
        }
    }
}
