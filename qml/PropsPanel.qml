import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The workspace's props, a collection at a time: the collection is chosen from the
// drop-down over the list, and the button beside that has what can be done with the
// collection itself (rename it, set it to show one prop at a time, remove it). A click
// on a prop turns it on, over whatever else is on the output and in front of the props
// already on, and a click on one that is on turns it off; a right click offers the rest
// (edit, rename, duplicate, move to another collection, remove).
//
// A prop can be dragged: up or down the list, to put it somewhere else in its
// collection, or onto a slide, which gives the slide an action that puts the prop on or
// takes it off.
//
// The props themselves are `Props` (src/props.h), and which are on is the operator
// window's; this only shows them and passes on what is asked. What a prop looks like is
// changed in the editor, as a slide's is.
Item {
    id: panel

    // The operator window: which props are on is its state, and turning one on or off
    // is done by calling its functions.
    required property var win
    // The prop or collection being renamed in place, by id; "" for none
    property string renaming: ""
    // The collection being shown, by id: the one chosen, while there is such a one,
    // and otherwise the first
    property string chosen: ""
    readonly property var collection: Props.collections.find(candidate => candidate.id === chosen) ?? Props.collections[0] ?? null
    readonly property string collectionId: collection ? collection.id : ""
    // Its props, as the rows of the list
    readonly property var rows: collection ? collection.props.map(prop => ({
        id: prop.id, name: prop.name, slide: prop.slide, collection: collection.id, collectionName: collection.id === "" ? "" : collection.name })) : []
    // Where a prop being dragged up or down the list would land: just before the prop
    // with this id, or with "end" after the last; "" while none is
    property string landing: ""

    // The menu of the + over the list: a prop goes into the collection being shown.
    function showAddMenu(item) {
        win.showMenu([
            { label: "New Prop", run: () => add(collectionId) },
            { label: "New Collection", run: () => addCollection() }
        ], item)
    }

    // Adds a prop with nothing on it, and opens the editor on it.
    function add(collection) {
        const added = Props.add(collection)
        if (win.report(added.error))
            win.startEditingProps(added.id)
    }

    // A new collection is the one shown, with its name ready to be typed over.
    function addCollection() {
        const added = Props.addCollection()
        if (!win.report(added.error))
            return
        chosen = added.id
        renaming = added.id
    }

    // Ends a rename in place, with the name typed; "" leaves the name as it was.
    function rename(row, name) {
        renaming = ""
        win.takeFocus()
        if (name !== "" && name !== row.name)
            win.report(row.heading ? Props.renameCollection(row.id, name) : Props.rename(row.id, name))
    }

    // A prop dragged up or down the list has been dropped: it goes where it would land.
    function place(id) {
        const before = landing === "end" ? "" : landing
        landing = ""
        if (id !== before)
            win.report(Props.place(id, before))
    }

    function showPropMenu(row, item, x, y) {
        const items = [
            { label: "Edit", run: () => win.startEditingProps(row.id) },
            { label: "Rename", run: () => renaming = row.id },
            { label: "Duplicate", run: () => win.report(Props.duplicate(row.id).error) }
        ]
        // To another collection, where it goes at the end, and which is then shown
        const others = Props.collections.filter(candidate => candidate.id !== row.collection && candidate.id !== "")
        if (others.length > 0) {
            items.push({ label: "Move to", items: [{ header: "Move to" }].concat(others.map(other => ({
                label: other.name, run: () => {
                    if (win.report(Props.move(row.id, other.id)))
                        chosen = other.id
                } }))) })
        }
        items.push({ label: "Remove…", danger: true, run: () => win.showMenu([
            { note: "“" + row.name + "” will be removed. That cannot be undone." },
            { label: "Remove", danger: true, run: () => win.report(Props.remove(row.id)) }
        ], item, x, y) })
        win.showMenu(items, item, x, y)
    }

    // What can be done with the collection being shown, from the button beside its name
    function showCollectionMenu(item) {
        const shown = collection
        // The props a file has outside any collection are shown as one, which is not
        // there to be changed until the file has been written with them in one.
        if (!shown || shown.id === "") {
            win.showMenu([{ label: "New Prop", run: () => add("") }, { label: "New Collection", run: () => addCollection() }], item)
            return
        }
        const count = shown.props.length
        win.showMenu([
            { header: shown.name },
            { label: "New Prop", run: () => add(shown.id) },
            { label: "Rename", run: () => renaming = shown.id },
            { label: "One at a Time", current: shown.single, run: () => win.report(Props.setSingle(shown.id, !shown.single)) },
            { label: count > 0 ? "Remove with its Props…" : "Remove", danger: true,
              run: () => count === 0 ? win.report(Props.removeCollection(shown.id)) : win.showMenu([
                  { note: "“" + shown.name + "” and the " + (count === 1 ? "prop" : count + " props") + " in it will be removed." },
                  { label: "Remove", danger: true, run: () => win.report(Props.removeCollection(shown.id)) }
              ], item) },
            { header: "Collections" },
            { label: "New Collection", run: () => addCollection() }
        ], item)
    }

    // The collection being shown, and what can be done with it
    Item {
        id: header

        width: parent.width
        height: Props.collections.length > 0 ? 28 : 0
        visible: height > 0

        AppComboBox {
            id: picker

            objectName: "propCollection"
            width: parent.width - more.width - 6
            height: 24
            font.pixelSize: 12
            visible: panel.renaming !== panel.collectionId || panel.collectionId === ""
            model: Props.collections.map(candidate => candidate.name + (candidate.single ? "  (one at a time)" : ""))
            currentIndex: Math.max(0, Props.collections.findIndex(candidate => candidate.id === panel.collectionId))
            onActivated: (index) => {
                panel.chosen = Props.collections[index].id
                panel.win.takeFocus()
            }
        }

        // Renaming the collection, in place of its name
        AppTextField {
            width: picker.width
            height: 24
            leftPadding: 6
            rightPadding: 6
            font.pixelSize: 13
            visible: !picker.visible
            onVisibleChanged: {
                if (visible) {
                    text = panel.collection.name
                    forceActiveFocus()
                    selectAll()
                }
            }
            onEditingFinished: {
                if (visible)
                    panel.rename({ heading: true, id: panel.collectionId, name: panel.collection.name }, text.trim())
            }
            Keys.onEscapePressed: panel.rename({ heading: true, id: panel.collectionId, name: panel.collection.name }, "")
        }

        Rectangle {
            id: more

            objectName: "propCollectionMenu"
            anchors.right: parent.right
            width: 24
            height: 24
            radius: 5
            color: moreMouse.pressed ? "#50535a" : moreMouse.containsMouse ? "#45484e" : "#3a3c42"

            Text {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -3
                color: "#e6e6e6"
                font.pixelSize: 15
                font.bold: true
                text: "…"
            }

            MouseArea {
                id: moreMouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    panel.win.takeFocus()
                    panel.showCollectionMenu(more)
                }
            }
        }
    }

    EmptyNote {
        anchors.centerIn: list
        width: parent.width - 24
        visible: list.count === 0
        font.pixelSize: 13
        text: Props.collections.length === 0 ? "No props. Add one with the + above." : "No props in this collection. Add one with the + above."
    }

    ListView {
        id: list

        objectName: "propList"
        anchors.fill: parent
        anchors.topMargin: header.height + (header.visible ? 4 : 0)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 3
        model: panel.rows

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        delegate: Rectangle {
            id: row

            required property var modelData
            required property int index
            readonly property bool on: panel.win.liveProps.includes(modelData.id)
            readonly property bool naming: panel.renaming === modelData.id && modelData.id !== ""

            width: list.width
            height: 40
            radius: 6
            color: on ? "#4a3a22" : rowMouse.containsMouse ? "#33353a" : "#2b2d31"

            // A click turns the prop on or off; a right click offers the rest. And it
            // can be dragged: up or down the list, or onto a slide.
            DragSource {
                id: rowMouse

                anchors.fill: parent
                win: panel.win
                payload: ({ kind: "prop", id: row.modelData.id, name: row.modelData.name, collectionId: row.modelData.collection,
                            collectionName: row.modelData.collectionName })
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (dragged)
                        return
                    panel.win.takeFocus()
                    if (mouse.button === Qt.RightButton)
                        panel.showPropMenu(row.modelData, row, mouse.x, mouse.y)
                    else
                        panel.win.toggleProp(row.modelData.id)
                }
            }

            // Another prop of this collection dragged over this one would land before
            // it, or after it, by which half of it the pointer is over.
            DropArea {
                id: landingArea

                function over(drag) {
                    const dragged = drag.source ? drag.source.payload : null
                    if (!dragged || dragged.kind !== "prop" || dragged.collectionId !== row.modelData.collection) {
                        drag.accepted = false
                        return
                    }
                    drag.accepted = true
                    const after = drag.y >= height / 2
                    const next = panel.rows[row.index + 1]
                    panel.landing = !after ? row.modelData.id : next ? next.id : "end"
                }

                anchors.fill: parent
                keys: ["action"]
                onEntered: (drag) => over(drag)
                onPositionChanged: (drag) => over(drag)
                onExited: panel.landing = ""
                onDropped: (drop) => panel.place(drop.source.payload.id)
            }

            // Where a prop being dragged would land: a line over this row, or under
            // the last
            Rectangle {
                y: panel.landing === row.modelData.id ? -3 : parent.height + 1
                width: parent.width
                height: 2
                radius: 1
                visible: panel.landing === row.modelData.id || (panel.landing === "end" && row.index === list.count - 1)
                color: panel.win.accentColor
            }

            // On: a bar down its left edge, as the live slide has a ring
            Rectangle {
                width: 3
                height: parent.height - 8
                y: 4
                radius: 1.5
                visible: row.on
                color: panel.win.accentColor
            }

            // The prop, small, on the dark it will mostly be seen over
            Rectangle {
                id: thumbnail

                x: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 57
                height: 32
                color: "#101114"
                border.width: 1
                border.color: row.on ? panel.win.accentColor : "#45484e"

                Slide {
                    anchors.fill: parent
                    anchors.margins: 1
                    slide: row.modelData.slide
                    effects: false
                }
            }

            Text {
                x: thumbnail.x + thumbnail.width + 8
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x - 8
                visible: !row.naming
                elide: Text.ElideRight
                color: row.on ? "white" : "#e6e6e6"
                font.pixelSize: 13
                text: row.modelData.name
            }

            // Renaming in place
            AppTextField {
                x: thumbnail.x + thumbnail.width + 6
                anchors.verticalCenter: parent.verticalCenter
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
