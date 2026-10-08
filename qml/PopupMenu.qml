import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

// The app's one pop-up menu: every right-click menu, and the menus of the + buttons.
//
// There is one instance, in the operator window, and whoever wants a menu calls show()
// on it with the rows and the item to open by. A row is one of
//   { header: "…" }             a caption over the rows that follow; with a `run`
//                               it can be clicked, and has a mark to say so
//   { note: "…" }               a line or two of explanation
//   { label: "…", run: () => … } something to do, which may also be marked
//                               `current` (ticked), `danger` (red) or `disabled`
//                               (greyed, and does nothing), and may have a `glyph`,
//                               the kind of a small picture drawn before its label
//                               (see ActionGlyph)
//   { label: "…", items: […] }  a row that leads to a menu of its own, which opens
//                               beside it when the pointer rests on the row, or on a
//                               click. `items` is the rows of that menu, or a function
//                               that gives them, called when the menu is wanted
// so a menu is just a list built on the spot. The menu a row leads to opens beside its
// row and the one it came from stays where it is, so that the way to a choice can be
// seen and gone back along; such menus can lead to more, as deep as is wanted. A row
// that asks "are you sure?" does it more simply, by showing another menu from its
// `run`, which takes the place of this one.
//
// It is one sheet over the whole window, with nothing drawn on it but the menus, each
// a pane placed where it belongs and kept inside the window. A click anywhere else
// on the sheet shuts them all and goes no further.
//
// It takes the keyboard while it is open, so that Esc closes it; whoever owns it gives
// the keyboard back to the right place in its `closed` handler.
Popup {
    id: menu

    // The tallest the first menu may be; past that its rows scroll
    property real limit: 10000
    // The rows of the first menu
    property var items: []
    // The item it was opened for
    property var opener: null
    // The rows of the menu furthest along: the first, or the last one opened from it
    property var shown: []
    // Which row of each open menu has a menu open from it: for the first menu, then
    // for the one opened from that, and so on
    property var trail: []
    readonly property real paneWidth: 250
    // The rows of each open menu, in the order they were opened. (A plain store, read
    // by each pane as it is made: a pane's rows do not change while it is there.)
    readonly property var store: ({ levels: [] })
    // The row the pointer is resting on, as { level, index }, until it has rested there
    // long enough for its menu to be opened, or the others to be shut
    property var resting: null

    function show(items, item, x, y) {
        const at = x === undefined ? Qt.point(item.width < 60 ? item.width - paneWidth : 24, item.height - 2) : Qt.point(x, y)
        start(items, item, item.mapToItem(null, at.x, at.y), -1, limit)
    }

    // The same, for an item at the bottom of the window: the menu opens over it, and
    // reaches no higher in the window than `top`.
    function showAbove(items, item, top) {
        const corner = item.mapToItem(null, 0, 0)
        start(items, item, Qt.point(corner.x, 0), corner.y - 4, Math.min(limit, corner.y - top - 22))
    }

    // Opens the first menu with its top left corner at `at`, a point of the window,
    // or with `bottom` not negative, its bottom edge there.
    function start(items, item, at, bottom, room) {
        close()
        menu.items = items
        opener = item
        store.levels = [items]
        panes.clear()
        panes.append({ px: at.x, py: at.y, foot: bottom, room: room })
        trail = []
        shown = items
        open()
    }

    // The rows of the menu a row leads to.
    function childrenOf(row) {
        return typeof row.items === "function" ? row.items() : row.items
    }

    // Shuts every menu opened from the one at `level`, and those opened from them.
    function trim(level) {
        while (panes.count > level + 1)
            panes.remove(panes.count - 1)
        store.levels.length = level + 1
        trail = trail.slice(0, level)
        shown = store.levels[level]
    }

    // Opens the menu a row leads to, beside the row: to its right, or to its left if
    // there is no room there. `rowItem` is the row as it is drawn.
    function openFrom(level, index, rowItem) {
        if (trail[level] === index)
            return
        trim(level)
        const rows = childrenOf(store.levels[level][index])
        const pane = paneItems.itemAt(level)
        const top = rowItem.mapToItem(null, 0, 0).y - 6
        const right = pane.x + pane.width - 4
        const left = pane.x - paneWidth + 4
        store.levels.push(rows)
        panes.append({ px: right + paneWidth <= menu.width - 6 || left < 6 ? right : left, py: top, foot: -1, room: limit })
        trail = trail.concat([index])
        shown = rows
    }

    // The pointer has come to rest on a row.
    function settle() {
        const at = resting
        resting = null
        if (!at || at.level >= panes.count)
            return
        const row = store.levels[at.level][at.index]
        if (row && row.items !== undefined && row.disabled !== true)
            openFrom(at.level, at.index, at.item)
        else if (trail.length > at.level)
            trim(at.level)
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    margins: -1
    padding: 0
    modal: false
    closePolicy: Popup.CloseOnEscape
    // Takes the keyboard while open, so that Esc closes it.
    focus: true
    background: null
    enter: null
    exit: null
    // (A moment later, and not at once: it is shut from a click on one of its own
    // rows, which is not the time to take the rows away.)
    onClosed: Qt.callLater(forget)

    function forget() {
        if (opened)
            return
        resting = null
        panes.clear()
        store.levels = []
        trail = []
        shown = []
    }

    ListModel {
        id: panes
    }

    // A row's own menu opens once the pointer has rested on the row a moment, so that
    // a pointer on its way across the rows to a menu that is open does not shut it.
    Timer {
        id: rest

        interval: 180
        onTriggered: menu.settle()
    }

    contentItem: Item {
        // Anywhere but on a menu: shuts them
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: menu.close()
            onWheel: (wheel) => wheel.accepted = true
        }

        Repeater {
            id: paneItems

            model: panes

            // One menu: a rounded panel a shade lighter than the panes of the window,
            // with a bright edge and a shadow, so that it stands clear of whatever it
            // opens over.
            delegate: Item {
                id: pane

                required property int index
                required property real px
                required property real py
                // Where its bottom edge is to be, for one that opens upwards; else -1
                required property real foot
                required property real room
                property var rows: []
                // Whether the rows come in sections under captions. If so the rows
                // sit in from the captions, with room at their left for the tick on a
                // current one.
                readonly property bool sectioned: rows.some(row => row.header !== undefined)

                // Where it was asked for, moved as far as it takes to be all inside
                // the window
                x: Math.max(6, Math.min(px, menu.width - width - 6))
                y: Math.max(6, Math.min(foot >= 0 ? foot - height : py, menu.height - height - 6))
                width: menu.paneWidth
                height: view.height + 12
                Component.onCompleted: {
                    rows = menu.store.levels[index] ?? []
                    Qt.callLater(showCurrent)
                }

                // A menu too long to show whole opens with its current row in view.
                function showCurrent() {
                    const current = rowItems.itemAt(rows.findIndex(row => row.current === true))
                    view.contentY = current === null ? 0
                        : Math.max(0, Math.min(current.y - (view.height - current.height) / 2, view.contentHeight - view.height))
                }

                RectangularShadow {
                    anchors.fill: panel
                    radius: panel.radius
                    blur: 24
                    spread: 2
                    offset: Qt.vector2d(0, 6)
                    color: "#b0000000"
                }

                Rectangle {
                    id: panel

                    anchors.fill: parent
                    radius: 9
                    color: "#33363d"
                    border.width: 1.5
                    border.color: "#8b8f98"
                }

                // Not through to the sheet, which would shut the menus
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.AllButtons
                }

                Flickable {
                    id: view

                    x: 6
                    y: 6
                    width: parent.width - 12
                    height: Math.min(menuRows.height, pane.room, menu.height - 24)
                    contentHeight: menuRows.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {}

                    KineticWheel {}

                    Column {
                        id: menuRows

                        Repeater {
                            id: rowItems

                            model: pane.rows

                            delegate: Item {
                                id: row

                                required property var modelData
                                required property int index
                                readonly property bool note: modelData.note !== undefined
                                readonly property bool heading: modelData.header !== undefined
                                readonly property bool caption: heading || note
                                readonly property bool unavailable: modelData.disabled === true
                                // Whether it leads to a menu of its own, and whether that is open
                                readonly property bool leads: modelData.items !== undefined
                                readonly property bool led: menu.trail[pane.index] === index
                                // A gap above each section after the first sets it apart.
                                readonly property real gap: heading && index > 0 ? 6 : 0
                                readonly property bool pictured: !caption && modelData.glyph !== undefined
                                readonly property real indent: pictured ? 34 : pane.sectioned && !caption ? 30 : 12
                                // A caption that can be clicked
                                readonly property bool pressable: heading && modelData.run !== undefined

                                width: view.width
                                height: gap + (note ? noteText.implicitHeight + 10 : caption ? 26 : 30)

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.topMargin: row.gap
                                    radius: 4
                                    color: row.pressable && rowMouse.containsMouse ? "#5d616b"
                                         : row.heading ? "#484b53"
                                         : !row.caption && !row.unavailable && (rowMouse.containsMouse || row.led) ? "#565962" : "transparent"
                                }

                                // A row's small picture, before its label
                                Loader {
                                    x: 10
                                    anchors.verticalCenter: label.verticalCenter
                                    active: row.pictured

                                    sourceComponent: ActionGlyph {
                                        kind: row.modelData.glyph
                                        ink: row.unavailable ? "#6c6f75" : "#c9cdd6"
                                    }
                                }

                                // The mark of a caption that can be clicked: the
                                // triangle that means play
                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: label.verticalCenter
                                    visible: row.pressable
                                    color: rowMouse.containsMouse ? "#ff8a1f" : "#d5d7dc"
                                    font.pixelSize: 10
                                    text: "▶"
                                }

                                Text {
                                    id: noteText

                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 10
                                    visible: row.note
                                    verticalAlignment: Text.AlignVCenter
                                    wrapMode: Text.Wrap
                                    color: "#9a9da3"
                                    font.pixelSize: 12
                                    text: row.note ? row.modelData.note : ""
                                }

                                // The tick of the current row, in the space the indent leaves
                                Text {
                                    x: 12
                                    anchors.verticalCenter: label.verticalCenter
                                    visible: !row.caption && row.modelData.current === true
                                    color: "#ff8a1f"
                                    font.pixelSize: 13
                                    text: "✓"
                                }

                                Text {
                                    id: label

                                    anchors.fill: parent
                                    anchors.topMargin: row.gap
                                    anchors.leftMargin: row.indent
                                    anchors.rightMargin: row.leads ? 24 : 10
                                    visible: !row.note
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    color: row.heading ? "#d5d7dc"
                                         : row.unavailable ? "#6c6f75"
                                         : row.modelData.danger ? "#ff6b6b"
                                         : row.modelData.current ? "#ff8a1f" : "#e6e6e6"
                                    font.pixelSize: row.heading ? 11 : 14
                                    font.bold: row.heading
                                    font.capitalization: row.heading ? Font.AllUppercase : Font.MixedCase
                                    text: row.note ? "" : row.heading ? row.modelData.header : row.modelData.label
                                }

                                // The mark of a row that leads to a menu of its own
                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: label.verticalCenter
                                    visible: !row.caption && row.leads
                                    color: row.unavailable ? "#6c6f75" : "#9a9da3"
                                    font.pixelSize: 15
                                    text: "›"
                                }

                                MouseArea {
                                    id: rowMouse

                                    anchors.fill: parent
                                    anchors.topMargin: row.gap
                                    hoverEnabled: true
                                    // (A caption is still somewhere for the pointer to
                                    // rest, which shuts what was open from another row.)
                                    onContainsMouseChanged: {
                                        if (!containsMouse)
                                            return
                                        menu.resting = { level: pane.index, index: row.index, item: row }
                                        rest.restart()
                                    }
                                    onClicked: {
                                        if ((row.caption && !row.pressable) || row.unavailable)
                                            return
                                        if (row.leads) {
                                            rest.stop()
                                            menu.openFrom(pane.index, row.index, row)
                                            return
                                        }
                                        const run = row.modelData.run
                                        menu.close()
                                        run()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
