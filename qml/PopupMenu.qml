import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

// The app's one pop-up menu: every right-click menu, and the menus of the + buttons.
//
// There is one instance, in the operator window, and whoever wants a menu calls show()
// on it with the rows and the item to open by. A row is one of
//   { header: "…" }             a caption over the rows that follow
//   { note: "…" }               a line or two of explanation
//   { label: "…", run: () => … } something to do, which may also be marked
//                               `current` (ticked), `danger` (red) or `disabled`
//                               (greyed, and does nothing)
// so a menu is just a list built on the spot, and a row that asks "are you sure?" does
// it by showing another menu from its `run`.
//
// It takes the keyboard while it is open, so that Esc closes it; whoever owns it gives
// the keyboard back to the right place in its `closed` handler.
Popup {
    id: menu

    // The tallest it may be; past that its rows scroll
    property real limit: 10000
    // The tallest it may be this time, which for a menu that opens upwards is no more
    // than the room there is over what it opens from
    property real room: limit
    property var items: []
    // Whether the rows come in sections under captions. If so the rows sit in
    // from the captions, with room at their left for the tick on a current one.
    readonly property bool sectioned: items.some(item => item.header !== undefined)
    // Where in the item it was opened for to open, or null for under it
    property var at: null
    // Whether it opens over the item it was opened for, from that item's left edge
    property bool upward: false

    function show(items, item, x, y) {
        close()
        menu.items = items
        menu.at = x === undefined ? null : Qt.point(x, y)
        upward = false
        room = Qt.binding(() => limit)
        parent = item
        open()
        showCurrent()
    }

    // The same, for an item at the bottom of the window: the menu opens over it, and
    // reaches no higher in the window than `top`.
    function showAbove(items, item, top) {
        close()
        menu.items = items
        menu.at = null
        upward = true
        room = Math.min(limit, item.mapToItem(null, 0, 0).y - top - 2 * padding - 10)
        parent = item
        open()
        showCurrent()
    }

    // A menu too long to show whole opens with its current row in view.
    function showCurrent() {
        const current = rows.itemAt(items.findIndex(row => row.current === true))
        view.contentY = current === null ? 0
            : Math.max(0, Math.min(current.y - (view.height - current.height) / 2, view.contentHeight - view.height))
    }

    // At the point asked for; or under the row it was opened from; or under a
    // small button, ending at the button's right edge; or, opening upwards, over
    // what it was opened from. `margins` then keeps the whole menu inside the
    // window whatever that works out to, and a menu taller than the window
    // scrolls.
    x: upward ? 0 : at ? at.x : parent && parent.width < 60 ? parent.width - width : 24
    y: upward ? -height - 4 : at ? at.y : parent ? parent.height - 2 : 0
    width: 250
    margins: 6
    padding: 6
    // Takes the keyboard while open, so that Esc closes it.
    focus: true

    // A rounded panel a shade lighter than the panes, with a bright edge and a shadow, so
    // that it stands clear of whatever it opens over.
    background: Item {
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
    }

    contentItem: Flickable {
        id: view

        implicitHeight: Math.min(menuRows.height, menu.room)
        contentHeight: menuRows.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        Column {
            id: menuRows

            Repeater {
                id: rows

                model: menu.items

                delegate: Item {
                    id: row

                    required property var modelData
                    required property int index
                    readonly property bool note: modelData.note !== undefined
                    readonly property bool heading: modelData.header !== undefined
                    readonly property bool caption: heading || note
                    readonly property bool unavailable: modelData.disabled === true
                    // A gap above each section after the first sets it apart.
                    readonly property real gap: heading && index > 0 ? 6 : 0
                    readonly property real indent: menu.sectioned && !caption ? 30 : 12

                    width: menu.availableWidth
                    height: gap + (note ? noteText.implicitHeight + 10 : caption ? 26 : 30)

                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: row.gap
                        radius: 4
                        color: row.heading ? "#484b53"
                             : !row.caption && !row.unavailable && rowMouse.containsMouse ? "#565962" : "transparent"
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
                        anchors.rightMargin: 10
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

                    MouseArea {
                        id: rowMouse

                        anchors.fill: parent
                        anchors.topMargin: row.gap
                        hoverEnabled: true
                        enabled: !row.caption && !row.unavailable
                        onClicked: {
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
