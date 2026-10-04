import QtQuick
import QtQuick.Controls.Basic

// A list of { name, path } entries, one selected and one optionally marked live. Entries
// with a `depth` are indented by it, which is how the media folder tree is drawn, and a
// `detail` is shown dimmed after the name.
ListView {
    id: list

    property string selectedPath
    property string livePath

    signal picked(var entry)
    // Right-click; `item` is the entry's row, for placing a menu by it.
    signal menuRequested(var entry, Item item)

    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;")
    }

    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar {}

    KineticWheel {}

    delegate: Rectangle {
        id: entry

        required property var modelData
        readonly property bool selected: modelData.path === list.selectedPath

        width: ListView.view.width
        height: 30
        color: selected ? "#3d4046" : mouse.containsMouse ? "#33353a" : "transparent"

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
            anchors.leftMargin: 12 + (entry.modelData.depth ?? 0) * 14
            anchors.rightMargin: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            textFormat: Text.StyledText
            color: "#e6e6e6"
            font.pixelSize: 14
            text: list.escaped(entry.modelData.name)
                + (entry.modelData.detail
                   ? " <font color=\"#9a9da3\">" + list.escaped(entry.modelData.detail) + "</font>" : "")
        }

        MouseArea {
            id: mouse

            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton)
                    list.menuRequested(entry.modelData, entry)
                else
                    list.picked(entry.modelData)
            }
        }
    }
}
