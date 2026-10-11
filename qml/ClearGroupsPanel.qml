import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The window the workspace's clear groups are set up in: the groups down the left, and
// on the right what the one picked clears, the picture on its button and that picture's
// tint. It comes up over the operator window from a right click on any of the clear
// buttons (Configure Clear Groups…), and its own ✕, Esc or a click outside puts it away.
//
// The groups are ProPresenter's (see src/clearfile.h, which says how the file keeps
// them), and what is here is what a group has there:
//
//   - The layers are ProPresenter's eight, in its order. This app has three of them
//     (the props, the slide and the media); the others are shown greyed, as the group
//     has them in the file, and cannot be changed here. Nothing a group says of them is
//     lost.
//
//     TO COME: as this app gains a layer (audio, messages, announcements, video
//     inputs), it is switched on here by being marked as one of ours in
//     src/clearfile.cpp, which is all this window goes by; the show is then to clear
//     it (State::clearGroup in src/showstate.cpp), and it wants a clear button of its
//     own in the row of the layers (PreviewPanel.qml).
//   - The picture is one of ProPresenter's thirty-two (ClearGroupGlyph says how each is
//     drawn here). A group that has a picture of the user's own, given it in
//     ProPresenter, shows that; one cannot be given here.
//   - The tint is the colour the picture is drawn in, when it is switched on.
//   - A group can be kept out of sight, which takes its button away and nothing else:
//     an action that clears it still does, and so does F1 if it is the first.
//
// The order of the list is the order of the buttons, and the first group is the one F1
// clears; a group's menu moves it up and down. A new group (the +) clears nothing until
// it is given layers.
//
// GUESSES, ProPresenter's own window for this not having been looked at: how it is laid
// out, the order of the layers (taken from the order its own group has them in), and
// that a group can be renamed, moved and removed freely there, the one a workspace
// starts with among them.
Rectangle {
    id: panel

    // The operator window
    required property var win
    // The group picked in the list, by id
    property string picked: ""
    readonly property var group: ClearGroups.groups.find(candidate => candidate.id === picked) ?? null
    // The group whose name is being typed in place, by id
    property string renaming: ""
    readonly property color accentColor: "#ff8a1f"

    signal closed

    function open(groupId) {
        picked = ClearGroups.groups.some(candidate => candidate.id === groupId) ? groupId
               : ClearGroups.groups.length > 0 ? ClearGroups.groups[0].id : ""
        renaming = ""
        forceActiveFocus()
    }

    // Gives the window the keyboard, unless a group's name is being typed, which has it.
    function takeKeys() {
        if (renaming === "")
            forceActiveFocus()
    }

    function say(error) {
        if (error !== "")
            win.report(error)
        return error === ""
    }

    function described(entry) {
        return "the clear group \"" + (entry ? entry.name : "") + "\""
    }

    // A new group, last in the list, to be named at once.
    function add() {
        let name = "Clear Group"
        for (let number = 2; ClearGroups.groups.some(candidate => candidate.name === name); ++number)
            name = "Clear Group " + number
        const made = ClearGroups.add(name)
        if (!say(made.error))
            return
        Log.note("edit", "a clear group made, \"" + name + "\"")
        picked = made.id
        renaming = made.id
    }

    function rename(id, name) {
        renaming = ""
        const from = ClearGroups.groups.find(candidate => candidate.id === id)
        if (from && name.trim() !== "" && name.trim() !== from.name && say(ClearGroups.rename(id, name)))
            Log.note("edit", described(from) + " renamed \"" + name.trim() + "\"")
        forceActiveFocus()
    }

    function remove(id) {
        const from = ClearGroups.groups.find(candidate => candidate.id === id)
        const place = ClearGroups.groups.findIndex(candidate => candidate.id === id)
        const wasPicked = picked === id
        if (!say(ClearGroups.remove(id)))
            return
        Log.note("edit", described(from) + " removed")
        // The one that takes its place in the list is picked, or the one before. (Asked
        // beforehand whether it was the one picked: by now the list has changed, and
        // what is picked has been moved off a group that is no longer there.)
        if (wasPicked) {
            const left = ClearGroups.groups
            picked = left.length === 0 ? "" : left[Math.min(place, left.length - 1)].id
        }
    }

    function move(id, by) {
        const from = ClearGroups.groups.find(candidate => candidate.id === id)
        if (say(ClearGroups.move(id, by)))
            Log.note("edit", described(from) + " moved " + (by < 0 ? "up" : "down") + " the list")
    }

    // What the group that is picked clears. The log is told, with the names: which
    // group cleared what, and since when, is the first thing wanted when a button did
    // not clear what was expected.
    function setLayer(layer, on) {
        if (group && say(ClearGroups.setLayer(picked, layer.key, on)))
            Log.note("edit", described(group) + (on ? " clears " : " no longer clears ") + layer.name)
    }

    function setIcon(picture) {
        if (group && group.icon !== picture.icon && say(ClearGroups.setIcon(picked, picture.icon)))
            Log.note("edit", described(group) + " given the picture " + picture.name)
    }

    // Whether the picture is tinted, and with what ("" to leave the colour as it is)
    function setTint(tinted, color) {
        if (group && say(ClearGroups.setTint(picked, tinted, color)))
            Log.note("edit", described(group) + (tinted ? " tinted " + (color !== "" ? color : group.tint) : " no longer tinted"))
    }

    function setHidden(hidden) {
        if (group && say(ClearGroups.setHidden(picked, hidden)))
            Log.note("edit", described(group) + (hidden ? " kept out of sight" : " given its button back"))
    }

    function showGroupMenu(entry, item, x, y) {
        const place = ClearGroups.groups.findIndex(candidate => candidate.id === entry.id)
        win.showMenu([
            { header: entry.name },
            { label: "Rename", run: () => { picked = entry.id; renaming = entry.id } },
            { label: "Move Up", disabled: place <= 0, run: () => move(entry.id, -1) },
            { label: "Move Down", disabled: place < 0 || place >= ClearGroups.groups.length - 1, run: () => move(entry.id, 1) },
            { label: "Delete…", items: [{ header: "Delete “" + entry.name + "”?" },
                                        { label: "Delete Clear Group", danger: true, run: () => remove(entry.id) }] }
        ], item, x, y)
    }

    color: "#99000000"

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape)
            closed()
        // (Nothing typed here is a key of the show's.)
        event.accepted = true
    }

    Connections {
        target: ClearGroups

        // A group that has gone is not picked any more.
        function onChanged() {
            if (panel.picked !== "" && !ClearGroups.groups.some(candidate => candidate.id === panel.picked))
                panel.picked = ClearGroups.groups.length > 0 ? ClearGroups.groups[0].id : ""
        }
    }

    // A click outside the window puts it away; nothing behind it is clicked or scrolled.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.closed()
    }

    // A small heading over a part of the window
    component Caption: Text {
        color: "#8d9097"
        font.pixelSize: 11
        font.bold: true
    }

    // A row of the list: a group, with the picture its button has
    component GroupRow: Rectangle {
        id: row

        required property var modelData
        required property int index
        readonly property bool chosen: panel.picked === modelData.id
        readonly property bool typing: panel.renaming === modelData.id

        objectName: "clearGroupRow"
        width: parent ? parent.width : 0
        height: 30
        radius: 5
        color: chosen ? "#3f4249" : rowMouse.containsMouse ? "#33353a" : "transparent"

        ClearGroupGlyph {
            x: 6
            anchors.verticalCenter: parent.verticalCenter
            icon: row.modelData.icon
            image: row.modelData.image
            ink: row.modelData.tinted && row.modelData.tint !== "" ? row.modelData.tint : "#d6d8dc"
            opacity: row.modelData.hidden ? 0.45 : 1
            size: 22
        }

        Text {
            x: 34
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 42 - (tag.visible ? tag.implicitWidth + 8 : 0)
            visible: !row.typing
            elide: Text.ElideRight
            color: row.modelData.hidden ? "#8d9097" : "#e6e6e6"
            font.pixelSize: 13
            text: row.modelData.name
        }

        // What marks a row out: the first is the one F1 clears; one kept out of sight
        // says so.
        Text {
            id: tag

            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            visible: !row.typing && (row.index === 0 || row.modelData.hidden)
            color: "#9a9da3"
            font.pixelSize: 11
            text: row.modelData.hidden ? "hidden" : "F1"
        }

        MouseArea {
            id: rowMouse

            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: (mouse) => {
                panel.picked = row.modelData.id
                if (mouse.button === Qt.RightButton)
                    panel.showGroupMenu(row.modelData, row, mouse.x, mouse.y)
            }
            onDoubleClicked: (mouse) => {
                if (mouse.button === Qt.LeftButton)
                    panel.renaming = row.modelData.id
            }
        }

        // The name, to be typed over
        Rectangle {
            x: 30
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 36
            height: 24
            radius: 4
            visible: row.typing
            color: "#15161a"
            border.width: 1
            border.color: panel.accentColor

            TextInput {
                objectName: "clearGroupNameBox"
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

    Rectangle {
        id: sheet

        objectName: "clearGroupsPanel"
        anchors.centerIn: parent
        width: Math.min(panel.width - 40, 660)
        height: Math.min(panel.height - 40, 552)
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

        // ---- The groups
        Item {
            id: list

            objectName: "clearGroupsList"
            x: 10
            y: 10
            width: 214
            height: parent.height - 20

            Caption {
                id: listCaption

                x: 10
                y: 8
                text: "CLEAR GROUPS"
            }

            // A new group
            Rectangle {
                objectName: "clearGroupsAdd"
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: listCaption.verticalCenter
                width: 26
                height: 22
                radius: 5
                color: addMouse.pressed ? "#5c5f67" : addMouse.containsMouse ? "#53565e" : "#474a51"

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -1
                    color: "#e6e6e6"
                    font.pixelSize: 16
                    text: "+"
                }

                MouseArea {
                    id: addMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: panel.add()
                }
            }

            Flickable {
                id: groupsView

                y: listCaption.y + listCaption.height + 10
                width: parent.width
                height: parent.height - y
                clip: true
                contentHeight: groups.height
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                KineticWheel {}

                Column {
                    id: groups

                    width: groupsView.width
                    spacing: 2

                    Repeater {
                        model: ClearGroups.groups

                        GroupRow {}
                    }

                    EmptyNote {
                        width: parent.width
                        visible: ClearGroups.groups.length === 0
                        text: "No clear groups. The + makes one."
                    }
                }
            }
        }

        Rectangle {
            id: listEdge

            x: list.x + list.width + 10
            y: 0
            width: 1
            height: parent.height
            color: "#3a3c42"
        }

        // ---- The group that is picked
        Item {
            id: detail

            anchors.left: listEdge.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            Text {
                objectName: "clearGroupsTitle"
                x: 18
                y: 16
                width: close.x - 30
                elide: Text.ElideRight
                color: "#f0f0f0"
                font.pixelSize: 16
                font.bold: true
                text: panel.group ? panel.group.name : "Clear Groups"
            }

            // Puts the window away, as Esc and a click outside it do. Drawn as the
            // operator window's own close button is.
            AppButton {
                id: close

                objectName: "clearGroupsClose"
                anchors.right: parent.right
                anchors.rightMargin: 10
                y: 10
                width: 34
                leftPadding: 0
                rightPadding: 0
                font.pixelSize: 15
                text: "✕"
                onClicked: panel.closed()
            }

            Text {
                id: lead

                x: 18
                y: 46
                width: parent.width - 36
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 12
                text: panel.group
                      ? "What its button clears, all at one click. The buttons of the groups are under the clears of the layers, in the order "
                        + "of the list; the first group is the one F1 clears."
                      : "A clear group is a set of layers that are cleared together, at one click. The + makes one."
            }

            Flickable {
                id: settings

                x: 18
                y: lead.y + lead.height + 14
                width: parent.width - 36
                height: parent.height - y - 12
                clip: true
                visible: panel.group !== null
                contentHeight: parts.height
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                KineticWheel {}

                Column {
                    id: parts

                    width: settings.width
                    spacing: 8

                    Caption {
                        text: "LAYERS IT CLEARS"
                    }

                    // The layers, in ProPresenter's order, down two columns
                    Grid {
                        objectName: "clearLayers"
                        width: parent.width
                        columns: 2
                        flow: Grid.TopToBottom
                        rows: Math.ceil(ClearGroups.layers.length / 2)
                        columnSpacing: 12
                        rowSpacing: 2

                        Repeater {
                            model: ClearGroups.layers

                            AppCheck {
                                required property var modelData

                                objectName: "clearLayer:" + modelData.key
                                width: (parts.width - 12) / 2
                                text: modelData.name
                                available: modelData.ours
                                checked: panel.group !== null && panel.group.layers.indexOf(modelData.key) >= 0
                                onToggled: (checked) => panel.setLayer(modelData, checked)
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: "#6c6f75"
                        font.pixelSize: 11
                        text: "The greyed layers are ProPresenter's, which this app has not got yet: what the group says of them is shown, "
                              + "and kept as it is."
                    }

                    Item {
                        width: 1
                        height: 4
                    }

                    Caption {
                        text: "PICTURE"
                    }

                    // ProPresenter's pictures, in the order the file numbers them
                    Grid {
                        objectName: "clearPictures"
                        // (As many to a row as there is room for: eleven, in a window
                        // of the usual size.)
                        columns: Math.max(4, Math.floor((parts.width + 3) / 35))
                        spacing: 3

                        Repeater {
                            model: ClearGroups.pictures

                            Rectangle {
                                id: cell

                                required property var modelData
                                readonly property bool chosen: panel.group !== null && panel.group.icon === modelData.icon

                                objectName: "clearPicture:" + modelData.icon
                                width: 32
                                height: 32
                                radius: 5
                                color: chosen ? "#3f4249" : cellMouse.containsMouse ? "#33353a" : "#1c1d21"
                                border.width: chosen ? 1.5 : 0
                                border.color: panel.accentColor

                                ClearGroupGlyph {
                                    anchors.centerIn: parent
                                    icon: cell.modelData.icon
                                    ink: cell.chosen && panel.group.tinted && panel.group.tint !== "" ? panel.group.tint : "#d6d8dc"
                                    size: 24
                                }

                                MouseArea {
                                    id: cellMouse

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: panel.setIcon(cell.modelData)
                                }
                            }
                        }
                    }

                    Text {
                        objectName: "clearPictureOwn"
                        width: parent.width
                        wrapMode: Text.Wrap
                        visible: panel.group !== null && panel.group.icon === 0
                        color: "#9a9da3"
                        font.pixelSize: 11
                        text: "This group has a picture of its own, given it in ProPresenter. Choosing one of these puts it aside; it stays in the file."
                    }

                    // The tint: whether, and a swatch that leads to the colours
                    Row {
                        spacing: 10

                        AppCheck {
                            objectName: "clearTint"
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Tint the picture"
                            checked: panel.group !== null && panel.group.tinted
                            onToggled: (checked) => panel.setTint(checked, "")
                        }

                        ColorButton {
                            objectName: "clearTintColor"
                            anchors.verticalCenter: parent.verticalCenter
                            hasAlpha: false
                            available: panel.group !== null && panel.group.tinted
                            value: panel.group !== null && panel.group.tint !== "" ? panel.group.tint : "#ffffff"
                            onPicked: (value) => panel.setTint(true, String(value).substring(0, 7))
                            onClosed: panel.takeKeys()
                        }
                    }

                    Item {
                        width: 1
                        height: 4
                    }

                    Caption {
                        text: "BUTTON"
                    }

                    AppCheck {
                        objectName: "clearShown"
                        text: "Show its button under the clears"
                        checked: panel.group !== null && !panel.group.hidden
                        onToggled: (checked) => panel.setHidden(!checked)
                    }
                }
            }
        }
    }
}
