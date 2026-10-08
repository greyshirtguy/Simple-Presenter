import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The workspace's macros, collection by collection. A macro is a named list of actions
// (see src/actions.h and src/macros.h), and a click on one runs it: does them all, in
// order.
//
// A macro is a bar of its own colour (royal blue, unless it has been given another),
// with its picture at the left, the M in brackets unless it has a letter or a digit of
// its own, and two lines beside that: its name, and under the name a small picture
// for each of its actions, so that what a macro does can be seen at a glance.
// (ProPresenter draws a macro as a small picture on a block of its colour; colouring
// the whole bar is this app's way.)
//
// Everything else is in the macro's menu, on a right click: adding an action, removing
// one, and for each action that has anything to change a row of its own leading to
// Edit and Remove. A macro can be dragged onto a slide, which gives the slide the
// action that runs it; and one of the clear buttons, or anything else that stands for
// an action, can be dropped on a macro, which gives the macro that action.
//
// The macros themselves are `Macros`; what running one does is the operator window's,
// since it is the operator window that has the timers, the layers and the stage.
Item {
    id: panel

    required property var win
    // The macro or collection being renamed in place, by id; "" for none
    property string renaming: ""
    // The macro that has just been run, by id, for a moment: it lights up
    property string lit: ""
    // The colour of a macro that has not been given one
    readonly property color plain: "#4169e1"
    // The colours there are to give one, by name
    readonly property var colours: [
        { name: "Red", color: "#d32f2f" }, { name: "Orange", color: "#ef6c00" }, { name: "Yellow", color: "#c79a00" },
        { name: "Green", color: "#2e7d32" }, { name: "Teal", color: "#00838f" }, { name: "Blue", color: "#1565c0" },
        { name: "Purple", color: "#6a1b9a" }, { name: "Pink", color: "#c2185b" }, { name: "Grey", color: "#546e7a" }
    ]
    // A macro's colour as its bar is drawn in it. The name on the bar is white whatever
    // the colour, so a colour too pale for white to be read on is drawn darker, by as
    // much as it is pale.
    function shade(colour) {
        const c = Qt.color(colour)
        const light = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
        return light > 0.6 ? Qt.darker(c, 1 + (light - 0.6) * 2.5) : c
    }

    // The collections and their macros as the rows of one list: a heading for each
    // collection, then its macros
    readonly property var rows: {
        const rows = []
        for (const collection of Macros.collections) {
            rows.push({ heading: true, id: collection.id, name: collection.name, count: collection.macros.length })
            for (const macro of collection.macros) {
                rows.push({ heading: false, id: macro.id, name: macro.name, color: macro.color, letter: macro.letter, actions: macro.actions,
                            collection: collection.id, collectionName: collection.id === "" ? "" : collection.name })
            }
        }
        return rows
    }

    // The menu of the + over the list
    function showAddMenu(item) {
        win.showMenu([
            { label: "New Macro", run: () => add("") },
            { label: "New Collection", run: () => addCollection() }
        ], item)
    }

    // Adds a macro with no actions, with its name ready to be typed over.
    function add(collection) {
        const added = Macros.add(collection)
        if (win.report(added.error))
            renaming = added.id
    }

    function addCollection() {
        const added = Macros.addCollection()
        if (win.report(added.error))
            renaming = added.id
    }

    function rename(row, name) {
        renaming = ""
        win.takeFocus()
        if (name !== "" && name !== row.name)
            win.report(row.heading ? Macros.renameCollection(row.id, name) : Macros.rename(row.id, name))
    }

    function run(id) {
        lit = id
        dim.restart()
        win.runMacro(id)
    }

    // Whether an action is one the panel for its kind can show and change
    function changeable(action) {
        return action.kind === "stage" || (action.kind === "timer" && action.action !== Timers.Increment)
    }

    function change(row, action) {
        if (action.kind === "stage")
            win.openStageAction({ macro: row.id }, action)
        else
            win.openTimerAction({ macro: row.id }, action)
    }

    function showMacroMenu(row, item, x, y) {
        const target = { macro: row.id }
        const remove = action => win.removeAction(target, action)
        const items = [
            { label: "Run", run: () => run(row.id) },
            { label: "Add Action", items: () => win.addActionItems(target) },
            { label: "Remove Action", disabled: row.actions.length === 0, items: [{ header: "Remove Action" }].concat(
                row.actions.map(action => ({ label: action.title, glyph: action.kind, run: () => remove(action) }))) }
        ]
        // Each action that has something to change, by itself
        const changeables = row.actions.filter(action => changeable(action))
        if (changeables.length > 0) {
            items.push({ header: "Actions" })
            for (const action of changeables) {
                items.push({ label: action.title, glyph: action.kind, items: [
                    { label: "Edit…", run: () => change(row, action) },
                    { label: "Remove", danger: true, run: () => remove(action) }
                ] })
            }
        }
        items.push({ header: "Macro" })
        items.push({ label: "Rename", run: () => renaming = row.id })
        items.push({ label: "Colour", items: [{ header: "Colour" },
            { label: "Royal Blue", current: row.color === "", run: () => win.report(Macros.setColor(row.id, "")) }].concat(colours.map(colour => ({
                label: colour.name, current: row.color === colour.color, run: () => win.report(Macros.setColor(row.id, colour.color)) }))) })
        items.push({ label: "Duplicate", run: () => win.report(Macros.duplicate(row.id).error) })
        const others = Macros.collections.filter(collection => collection.id !== row.collection && collection.id !== "")
        if (others.length > 0) {
            items.push({ label: "Move to", items: [{ header: "Move to" }].concat(others.map(collection => ({
                label: collection.name, run: () => win.report(Macros.move(row.id, collection.id)) }))) })
        }
        items.push({ label: "Remove…", danger: true, run: () => win.showMenu([
            { note: "“" + row.name + "” will be removed. That cannot be undone. Slides that run it will have nothing to run." },
            { label: "Remove", danger: true, run: () => win.report(Macros.remove(row.id)) }
        ], item, x, y) })
        win.showMenu(items, item, x, y)
    }

    function showCollectionMenu(row, item, x, y) {
        // Macros a file has outside any collection are shown as one, which is not there
        // to be changed until the file has been written with them in one.
        if (row.id === "") {
            win.showMenu([{ label: "New Macro", run: () => add("") }], item, x, y)
            return
        }
        win.showMenu([
            { label: "New Macro", run: () => add(row.id) },
            { label: "Rename", run: () => renaming = row.id },
            { label: row.count > 0 ? "Remove with its Macros…" : "Remove", danger: true,
              run: () => row.count === 0 ? win.report(Macros.removeCollection(row.id)) : win.showMenu([
                  { note: "“" + row.name + "” and the " + (row.count === 1 ? "macro" : row.count + " macros") + " in it will be removed." },
                  { label: "Remove", danger: true, run: () => win.report(Macros.removeCollection(row.id)) }
              ], item, x, y) }
        ], item, x, y)
    }

    Timer {
        id: dim

        interval: 350
        onTriggered: panel.lit = ""
    }

    EmptyNote {
        anchors.centerIn: parent
        width: parent.width - 24
        visible: list.count === 0
        font.pixelSize: 13
        text: "No macros. Add one with the + above."
    }

    ListView {
        id: list

        objectName: "macroList"
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 3
        model: panel.rows

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        delegate: Rectangle {
            id: row

            required property var modelData
            readonly property bool heading: modelData.heading === true
            readonly property bool naming: panel.renaming === modelData.id && modelData.id !== ""
            // A macro's own colour, a little brighter under the pointer and brighter
            // again for a moment when it is run
            readonly property color tint: heading ? "transparent" : panel.shade(modelData.color !== "" ? modelData.color : panel.plain)

            objectName: heading ? "" : "macroRow"
            width: list.width
            height: heading ? 24 : 46
            radius: 6
            color: heading ? "transparent" : panel.lit === modelData.id ? Qt.lighter(tint, 1.45)
                 : rowMouse.containsMouse || dropArea.containsDrag ? Qt.lighter(tint, 1.15) : tint
            border.width: dropArea.containsDrag ? 2 : 0
            border.color: panel.win.accentColor

            Behavior on color {
                enabled: !row.heading
                ColorAnimation { duration: 120 }
            }

            // A click on a macro runs it, and it can be dragged onto a slide; a right
            // click on any row offers what can be done with it.
            DragSource {
                id: rowMouse

                anchors.fill: parent
                win: panel.win
                payload: row.heading ? null : { kind: "macro", id: row.modelData.id, name: row.modelData.name,
                                                collectionId: row.modelData.collection, collectionName: row.modelData.collectionName }
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (dragged)
                        return
                    panel.win.takeFocus()
                    if (mouse.button === Qt.RightButton) {
                        if (row.heading)
                            panel.showCollectionMenu(row.modelData, row, mouse.x, mouse.y)
                        else
                            panel.showMacroMenu(row.modelData, row, mouse.x, mouse.y)
                    } else if (!row.heading) {
                        panel.run(row.modelData.id)
                    }
                }
            }

            // Something that stands for an action, dropped on a macro, gives the macro
            // that action: one of the clear buttons, say. (Not the macro itself.)
            DropArea {
                id: dropArea

                anchors.fill: parent
                keys: ["action"]
                enabled: !row.heading
                onEntered: (drag) => drag.accepted = !(drag.source && drag.source.payload && drag.source.payload.id === row.modelData.id)
                onDropped: panel.win.dropAction({ macro: row.modelData.id }, row)
            }

            // The macro's picture
            MacroGlyph {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                visible: !row.heading
                size: 1.6
                ink: "white"
                letter: row.heading ? "" : row.modelData.letter
            }

            // Its name, or a collection's
            Text {
                id: label

                x: row.heading ? 4 : 46
                y: row.heading ? (parent.height - height) / 2 : 6
                width: parent.width - x - 8
                visible: !row.naming
                elide: Text.ElideRight
                color: row.heading ? "#9a9da3" : "white"
                font.pixelSize: row.heading ? 11 : 13
                font.bold: true
                font.capitalization: row.heading ? Font.AllUppercase : Font.MixedCase
                text: row.modelData.name
            }

            // What it does: a small picture for each of its actions, in their order
            Row {
                objectName: "macroActions"
                x: 46
                y: 27
                width: parent.width - x - 8
                spacing: 6
                clip: true
                visible: !row.heading

                Repeater {
                    model: row.heading ? [] : row.modelData.actions

                    delegate: ActionGlyph {
                        required property var modelData

                        kind: modelData.kind
                        // Fainter for a kind that is kept and not done here
                        ink: modelData.done ? "#f2f4f7" : "#a0ffffff"
                    }
                }

                Text {
                    visible: !row.heading && row.modelData.actions.length === 0
                    color: "#c0ffffff"
                    font.pixelSize: 11
                    text: "No actions yet"
                }
            }

            // Renaming in place
            AppTextField {
                x: row.heading ? 0 : 42
                y: row.heading ? 0 : 3
                width: parent.width - x - 4
                height: 24
                leftPadding: 6
                rightPadding: 6
                font.pixelSize: 13
                visible: row.naming
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
                // One call and nothing after it: the change rebuilds the list, and
                // this row with it.
                onEditingFinished: {
                    if (row.naming)
                        panel.rename(row.modelData, text.trim())
                }
                Keys.onEscapePressed: panel.rename(row.modelData, "")
            }
        }
    }
}
