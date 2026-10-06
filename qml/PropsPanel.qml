import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The workspace's props, collection by collection. A click on a prop turns it on, over
// whatever else is on the output and in front of the props already on, and a click on
// one that is on turns it off; a right click offers the rest (edit, rename, duplicate,
// move to another collection, remove). A collection's name has a menu too, where it can
// be set to show one prop at a time.
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
    // The collections and their props as the rows of one list: a heading for each
    // collection, then its props
    readonly property var rows: {
        const rows = []
        for (const collection of Props.collections) {
            rows.push({ heading: true, id: collection.id, name: collection.name, single: collection.single,
                        count: collection.props.length })
            for (const prop of collection.props)
                rows.push({ heading: false, id: prop.id, name: prop.name, slide: prop.slide, collection: collection.id })
        }
        return rows
    }

    // The menu of the + over the list: a prop goes into the collection of the prop last
    // clicked, or the first.
    function showAddMenu(item) {
        win.showMenu([
            { label: "New Prop", run: () => add("") },
            { label: "New Collection", run: () => addCollection() }
        ], item)
    }

    // Adds a prop with nothing on it, and opens the editor on it.
    function add(collection) {
        const added = Props.add(collection)
        if (win.report(added.error))
            win.startEditingProps(added.id)
    }

    function addCollection() {
        const added = Props.addCollection()
        if (win.report(added.error))
            renaming = added.id
    }

    // Ends a rename in place, with the name typed; "" leaves the name as it was.
    function rename(row, name) {
        renaming = ""
        win.takeFocus()
        if (name !== "" && name !== row.name)
            win.report(row.heading ? Props.renameCollection(row.id, name) : Props.rename(row.id, name))
    }

    function showPropMenu(row, item, x, y) {
        const items = [
            { label: "Edit", run: () => win.startEditingProps(row.id) },
            { label: "Rename", run: () => renaming = row.id },
            { label: "Duplicate", run: () => win.report(Props.duplicate(row.id).error) }
        ]
        const others = Props.collections.filter(collection => collection.id !== row.collection && collection.id !== "")
        if (others.length > 0) {
            items.push({ header: "Move to" })
            for (const collection of others)
                items.push({ label: collection.name, run: () => win.report(Props.move(row.id, collection.id)) })
            items.push({ header: "Prop" })
        }
        items.push({ label: "Remove…", danger: true, run: () => win.showMenu([
            { note: "“" + row.name + "” will be removed. That cannot be undone." },
            { label: "Remove", danger: true, run: () => win.report(Props.remove(row.id)) }
        ], item, x, y) })
        win.showMenu(items, item, x, y)
    }

    function showCollectionMenu(row, item, x, y) {
        // The props a file has outside any collection are shown as one, which is not
        // there to be changed until the file has been written with them in one.
        if (row.id === "") {
            win.showMenu([{ label: "New Prop", run: () => add("") }], item, x, y)
            return
        }
        win.showMenu([
            { label: "New Prop", run: () => add(row.id) },
            { label: "Rename", run: () => renaming = row.id },
            { label: "One at a Time", current: row.single, run: () => win.report(Props.setSingle(row.id, !row.single)) },
            { label: row.count > 0 ? "Remove with its Props…" : "Remove", danger: true,
              run: () => row.count === 0 ? win.report(Props.removeCollection(row.id)) : win.showMenu([
                  { note: "“" + row.name + "” and the " + (row.count === 1 ? "prop" : row.count + " props") + " in it will be removed." },
                  { label: "Remove", danger: true, run: () => win.report(Props.removeCollection(row.id)) }
              ], item, x, y) }
        ], item, x, y)
    }

    EmptyNote {
        anchors.centerIn: parent
        width: parent.width - 24
        visible: list.count === 0
        font.pixelSize: 13
        text: "No props. Add one with the + above."
    }

    ListView {
        id: list

        objectName: "propList"
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
            readonly property bool on: !modelData.heading && panel.win.liveProps.includes(modelData.id)
            readonly property bool naming: panel.renaming === modelData.id && modelData.id !== ""

            width: list.width
            height: modelData.heading ? 24 : 40
            radius: 6
            color: modelData.heading ? "transparent" : on ? "#4a3a22" : rowMouse.containsMouse ? "#33353a" : "#2b2d31"

            MouseArea {
                id: rowMouse

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    panel.win.takeFocus()
                    if (mouse.button === Qt.RightButton) {
                        if (row.modelData.heading)
                            panel.showCollectionMenu(row.modelData, row, mouse.x, mouse.y)
                        else
                            panel.showPropMenu(row.modelData, row, mouse.x, mouse.y)
                    } else if (!row.modelData.heading) {
                        panel.win.toggleProp(row.modelData.id)
                    }
                }
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
                visible: !row.modelData.heading
                color: "#101114"
                border.width: 1
                border.color: row.on ? panel.win.accentColor : "#45484e"

                Loader {
                    anchors.fill: parent
                    anchors.margins: 1
                    active: !row.modelData.heading

                    sourceComponent: Slide {
                        slide: row.modelData.slide
                        effects: false
                    }
                }
            }

            Text {
                x: row.modelData.heading ? 4 : thumbnail.x + thumbnail.width + 8
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x - (single.visible ? single.width + 12 : 8)
                visible: !row.naming
                elide: Text.ElideRight
                color: row.modelData.heading ? "#9a9da3" : row.on ? "white" : "#e6e6e6"
                font.pixelSize: row.modelData.heading ? 11 : 13
                font.bold: row.modelData.heading
                font.capitalization: row.modelData.heading ? Font.AllUppercase : Font.MixedCase
                text: row.modelData.name
            }

            // A collection that shows one prop at a time says so.
            Text {
                id: single

                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                visible: row.modelData.heading && row.modelData.single === true
                color: "#9a9da3"
                font.pixelSize: 10
                text: "one at a time"
            }

            // Renaming in place
            AppTextField {
                x: row.modelData.heading ? 0 : thumbnail.x + thumbnail.width + 6
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
