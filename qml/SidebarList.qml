import QtQuick
import QtQuick.Controls.Basic

// A list of { name, path } entries, one selected and one optionally marked live. Entries
// with a `depth` are indented by it, which is how trees are drawn, and a `detail` is
// shown dimmed after the name. Some entries only label what follows them and are not
// picked by a click: a `folder`, and a `kind` of "header", which is drawn in its
// `color`. An entry marked `missing` is drawn as unavailable.
//
// Optionally, entries can be renamed in place, dragged out, and dropped onto.
ListView {
    id: list

    property string selectedPath
    property string livePath
    // The entry being renamed in place, by path; "" for none. Set it to start a rename.
    property string editingPath: ""
    // Set to an item with an `entry` property and Drag.keys to let entries be dragged
    // out of the list: the item is moved with the pointer, in its parent's coordinates,
    // and given the dragged entry. Entries for which `draggable(entry)` is false stay put.
    property Item dragProxy: null
    property var draggable: (entry) => true
    // Drags with any of these keys can be dropped on an entry for which
    // `acceptsDrop(entry)` is true.
    property var dropKeys: []
    property var acceptsDrop: (entry) => true

    signal picked(var entry)
    // Right-click; `item` is the entry's row, for placing a menu by it.
    signal menuRequested(var entry, Item item)
    // A rename in place was confirmed with a new, non-empty name.
    signal renamed(var entry, string name)
    // A rename in place ended, whether or not anything changed.
    signal editingEnded
    // Something was dropped on an entry; `source` is the dragged item.
    signal dropped(var entry, var source)

    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;")
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
        readonly property bool label: header || modelData.folder === true
        readonly property bool editing: list.editingPath !== "" && modelData.path === list.editingPath
        readonly property real indent: 12 + (modelData.depth ?? 0) * 14

        width: ListView.view.width
        height: 30
        color: header ? "#232427" : selected ? "#3d4046" : mouse.containsMouse && !label ? "#33353a" : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            visible: list.livePath !== "" && entry.modelData.path === list.livePath
            color: "#ff8a1f"
        }

        Text {
            anchors.fill: parent
            anchors.leftMargin: entry.indent
            anchors.rightMargin: 12
            visible: !entry.editing
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            textFormat: Text.StyledText
            color: entry.header ? (entry.modelData.color || "#9a9da3")
                 : entry.modelData.folder ? "#9a9da3"
                 : entry.modelData.missing ? "#d07070" : "#e6e6e6"
            font.pixelSize: entry.header ? 12 : 14
            font.bold: entry.label
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
            readonly property bool canDrag: list.dragProxy !== null && !entry.label && list.draggable(entry.modelData)

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

            anchors.fill: parent
            keys: list.dropKeys
            enabled: list.dropKeys.length > 0 && list.acceptsDrop(entry.modelData)
            onDropped: (drop) => list.dropped(entry.modelData, drop.source)
        }

        Rectangle {
            anchors.fill: parent
            visible: dropArea.containsDrag
            color: "transparent"
            border.width: 2
            border.color: "white"
        }

        // Rename in place: Enter or clicking away confirms, Esc cancels.
        Loader {
            anchors.fill: parent
            anchors.leftMargin: entry.indent - 8
            anchors.rightMargin: 6
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            active: entry.editing

            sourceComponent: AppTextField {
                property bool cancelled: false

                text: entry.modelData.name
                onEditingFinished: {
                    const name = text.trim()
                    const changed = !cancelled && name !== "" && name !== entry.modelData.name
                    const edited = entry.modelData
                    list.editingPath = ""
                    if (changed)
                        list.renamed(edited, name)
                    list.editingEnded()
                }
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
