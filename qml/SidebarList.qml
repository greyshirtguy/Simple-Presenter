import QtQuick
import QtQuick.Controls.Basic

// A list of { name, path } entries, one selected and one optionally marked live. Entries
// with a `depth` are indented by it, which is how trees are drawn, and a `detail` is
// shown dimmed after the name. An entry with a `kind` of "header" only labels what
// follows it: it is drawn in its `color` and is not picked by a click. An entry marked
// `missing` is drawn as unavailable. An entry with an `icon` (see RowIcon) has it drawn
// before its name.
//
// Entries sit a little in from the left edge, so a title above the list reads as their
// heading. Optionally, entries can be renamed in place, dragged out, and dropped onto.
ListView {
    id: list

    property string selectedPath
    property string livePath
    // How far in from the left edge entries start.
    property real baseIndent: 22
    // The entry being renamed in place, by path; "" for none. Set it to start a rename.
    property string editingPath: ""
    // Set to an item with an `entry` property and Drag.keys to let entries be dragged
    // out of the list: the item is moved with the pointer, in its parent's coordinates,
    // and given the dragged entry. Entries for which `draggable(entry)` is false stay put.
    property Item dragProxy: null
    property var draggable: (entry) => true
    // Drags with any of these keys can be dropped on entries. `dropZone(entry, source)`
    // says how, for the item being dragged: "onto" the entry, shown by outlining it;
    // "between", into the gap above or below it, shown by a line there; "both", where
    // the middle of the entry is onto and its top and bottom edges are between; or ""
    // for not at all.
    property var dropKeys: []
    property var dropZone: (entry, source) => "onto"

    signal picked(var entry)
    // Right-click; `item` is the entry's row, for placing a menu by it.
    signal menuRequested(var entry, Item item)
    // A rename in place was confirmed with a new, non-empty name.
    signal renamed(var entry, string name)
    // A rename in place ended, whether or not anything changed.
    signal editingEnded
    // Something was dropped; `source` is the dragged item, and `where` is "onto",
    // "before" or "after" the entry.
    signal dropped(var entry, var source, string where)

    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;")
    }

    // Ends a rename in place. `name` is what was typed, or "" if it was abandoned. Done
    // here, not in the row: reporting the new name has the list rebuilt, and the row
    // that asked is then gone before it could say that it has finished.
    function finishRename(entry, name) {
        editingPath = ""
        editingEnded()
        if (name !== "" && name !== entry.name)
            renamed(entry, name)
    }

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    // A new model starts scrolled to the top; bring the selected entry back into view.
    onModelChanged: Qt.callLater(() => {
        const index = Array.isArray(model) ? model.findIndex(entry => entry.path === selectedPath) : -1
        if (index >= 0)
            positionViewAtIndex(index, ListView.Contain)
    })

    ScrollBar.vertical: ScrollBar {}

    KineticWheel {}

    delegate: Rectangle {
        id: entry

        required property var modelData
        readonly property bool selected: modelData.path === list.selectedPath
        readonly property bool header: modelData.kind === "header"
        readonly property bool label: header
        readonly property bool editing: list.editingPath !== "" && modelData.path === list.editingPath
        readonly property real indent: list.baseIndent + (modelData.depth ?? 0) * 14
        readonly property bool hasIcon: (modelData.icon ?? "") !== ""
        readonly property real textIndent: indent + (hasIcon ? rowIcon.width + 7 : 0)

        width: ListView.view.width
        height: 30
        // Selection and hover both lighten whatever is behind, so they read on any
        // pane's background without bringing in a colour of their own; selection is
        // the stronger, and is edged above and below.
        color: header ? "#232427" : selected ? "#22ffffff" : mouse.containsMouse && !label ? "#10ffffff" : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            visible: entry.selected
            color: "#55ffffff"
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            visible: entry.selected
            color: "#55ffffff"
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
            visible: list.livePath !== "" && entry.modelData.path === list.livePath
            color: "#ff8a1f"
        }

        RowIcon {
            id: rowIcon

            x: entry.indent
            anchors.verticalCenter: parent.verticalCenter
            visible: entry.hasIcon
            kind: entry.modelData.icon ?? ""
            opacity: entry.modelData.missing ? 0.4 : 1
        }

        Text {
            anchors.fill: parent
            anchors.leftMargin: entry.textIndent
            anchors.rightMargin: 12
            visible: !entry.editing
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            textFormat: Text.StyledText
            color: entry.header ? (entry.modelData.color || "#9a9da3")
                 : entry.modelData.folder ? "#c9cbd0"
                 : entry.modelData.missing ? "#d07070" : "#e6e6e6"
            font.pixelSize: entry.header ? 12 : 14
            font.bold: entry.header
            text: list.escaped(entry.modelData.name)
                + (entry.modelData.missing ? " <font color=\"#9a9da3\">(missing)</font>"
                   : entry.modelData.detail
                     ? " <font color=\"#9a9da3\">" + list.escaped(entry.modelData.detail) + "</font>" : "")
        }

        // Click to pick, right-click for a menu, or press and move to drag the entry out.
        MouseArea {
            id: mouse

            property point pressedAt
            property bool dragging: false
            readonly property bool canDrag: list.dragProxy !== null && list.draggable(entry.modelData)

            function follow(mouse) {
                const at = mapToItem(list.dragProxy.parent, mouse.x, mouse.y)
                list.dragProxy.x = at.x
                list.dragProxy.y = at.y
            }

            function finish() {
                dragging = false
                if (list.dragProxy)
                    list.dragProxy.Drag.active = false
            }

            anchors.fill: parent
            enabled: !entry.editing
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            preventStealing: canDrag
            onPressed: (mouse) => {
                pressedAt = Qt.point(mouse.x, mouse.y)
                dragging = false
            }
            onPositionChanged: (mouse) => {
                if (!canDrag || !(pressedButtons & Qt.LeftButton))
                    return
                if (!dragging && Math.abs(mouse.x - pressedAt.x) + Math.abs(mouse.y - pressedAt.y) > 10) {
                    dragging = true
                    follow(mouse)
                    list.dragProxy.entry = entry.modelData
                    list.dragProxy.Drag.active = true
                }
                if (dragging)
                    follow(mouse)
            }
            onReleased: (mouse) => {
                if (dragging) {
                    list.dragProxy.Drag.drop()
                    finish()
                } else if (entry.header) {
                    return
                } else if (mouse.button === Qt.RightButton) {
                    list.menuRequested(entry.modelData, entry)
                } else if (!entry.label) {
                    list.picked(entry.modelData)
                }
            }
            onCanceled: finish()
        }

        DropArea {
            id: dropArea

            // Where a drop would land for the drag now over the entry: "onto",
            // "before", "after", or "" if it cannot be dropped here.
            property string where: ""

            function update(drag) {
                const zone = list.dropZone(entry.modelData, drag.source)
                const edge = zone === "both" ? 0.28 : 0.5
                where = zone === "" ? ""
                      : zone === "onto" ? "onto"
                      : drag.y < height * edge ? "before"
                      : drag.y > height * (1 - edge) ? "after"
                      : "onto"
            }

            anchors.fill: parent
            keys: list.dropKeys
            enabled: list.dropKeys.length > 0
            onEntered: (drag) => update(drag)
            onPositionChanged: (drag) => update(drag)
            onDropped: (drop) => {
                if (where !== "")
                    list.dropped(entry.modelData, drop.source, where)
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: dropArea.containsDrag && dropArea.where === "onto"
            color: "transparent"
            border.width: 2
            border.color: "white"
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            y: dropArea.where === "after" ? parent.height - 1.5 : -1.5
            height: 3
            visible: dropArea.containsDrag && (dropArea.where === "before" || dropArea.where === "after")
            color: "white"
        }

        // Rename in place: Enter or clicking away confirms, Esc cancels.
        Loader {
            anchors.fill: parent
            anchors.leftMargin: entry.textIndent - 8
            anchors.rightMargin: 6
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            active: entry.editing

            sourceComponent: AppTextField {
                property bool cancelled: false

                text: entry.modelData.name
                onEditingFinished: list.finishRename(entry.modelData, cancelled ? "" : text.trim())
                Keys.onEscapePressed: {
                    cancelled = true
                    editingFinished()
                }
                Component.onCompleted: {
                    forceActiveFocus()
                    selectAll()
                }
            }
        }
    }
}
