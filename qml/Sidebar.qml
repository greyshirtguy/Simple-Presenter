import QtQuick
import QtQuick.Controls.Basic

// The left of the operator window: where things are picked from.
//
// At the top, in one pane that scrolls as a whole, are the workspace's libraries (folders
// of presentations) and its playlists with the folders they are kept in. Below, under a
// header that can be dragged to share out the height, are the presentations of
// whichever library or playlist was picked last: click one to see its slides.
//
// Rows can be dragged: a presentation onto a playlist to add it, a playlist row up and
// down to reorder it, a playlist or folder between others or into a folder. A right
// click opens the row's menu.
Rectangle {
    id: sidebar

    // The operator window: what this shows is its state, and what happens here is done
    // by calling its functions.
    required property var win
    // What rows dragged out of the two lists are carried by (see RowDrag)
    required property Item presentationDrag
    required property Item playlistDrag
    // Whether a playlist is being renamed in place, which has the keyboard
    readonly property bool renaming: playlistList.editingPath !== ""

    // Starts renaming a playlist or folder in place, bringing its row into view first:
    // a new one may be below the bottom of the pane.
    function renamePlaylist(id) {
        playlistList.editingPath = id
        // Once the list has grown to hold the row, if it is new.
        Qt.callLater(() => {
            const index = sidebar.win.catalog.playlists.findIndex(p => p.path === id)
            const bottom = playlistList.y + (index + 1) * 30
            if (index >= 0 && bottom > sourcesView.contentY + sourcesView.height)
                sourcesView.contentY = Math.max(0, Math.min(bottom - sourcesView.height + 8,
                                                            sourcesView.contentHeight - sourcesView.height))
        })
    }

    color: sidebar.win.surfaceColor

    // Libraries and playlists share one pane, a shade lighter than the
    // presentations below it, that scrolls as a whole. Drag the Presentations
    // header to resize it.
    Rectangle {
        id: sources

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.max(90, Math.min(sidebar.win.sourcesHeight, sidebar.height - presentationsHeader.height - 90))
        color: "#383a41"

        Flickable {
            id: sourcesView

            anchors.fill: parent
            contentHeight: sourcesColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            Column {
                id: sourcesColumn

                width: sourcesView.width

                SectionTitle {
                    color: sidebar.win.librariesColor
                    text: "Libraries"
                }

                SidebarList {
                    id: libraryList

                    objectName: "libraryList"
                    width: parent.width
                    height: contentHeight
                    interactive: false
                    model: sidebar.win.catalog.libraries
                    selectedPath: sidebar.win.selectedNode === "" ? sidebar.win.libraryPath : ""
                    livePath: !sidebar.win.cleared && sidebar.win.liveDocument && sidebar.win.livePlaylistId === ""
                              ? sidebar.win.liveDocument.path.substring(0, sidebar.win.liveDocument.path.lastIndexOf("/")) : ""
                    onPicked: (entry) => sidebar.win.openLibrary(entry.path)
                }

                // Playlists and the folders they are kept in, in ProPresenter's
                // own playlists file. Drop a presentation on a playlist to add it.
                Item {
                    width: parent.width
                    height: playlistsTitle.height

                    SectionTitle {
                        id: playlistsTitle

                        color: sidebar.win.playlistsColor
                        text: "Playlists"
                    }

                    // Add a folder or a playlist, or import a playlist
                    Rectangle {
                        id: addButton

                        objectName: "addPlaylistButton"
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 3
                        width: 26
                        height: 22
                        radius: 5
                        color: addMouse.pressed ? "#5c5f67" : addMouse.containsMouse ? "#53565e" : "#474a51"

                        Text {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: -1
                            color: sidebar.win.textColor
                            font.pixelSize: 16
                            text: "+"
                        }

                        MouseArea {
                            id: addMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: sidebar.win.showAddMenu(addButton)
                        }
                    }
                }

                SidebarList {
                    id: playlistList

                    objectName: "playlistList"
                    width: parent.width
                    height: contentHeight
                    interactive: false
                    model: sidebar.win.catalog.playlists
                    selectedPath: sidebar.win.selectedNode
                    livePath: sidebar.win.cleared ? "" : sidebar.win.livePlaylistId
                    dragProxy: sidebar.playlistDrag
                    dropKeys: ["presentation", "playlist"]
                    // A presentation goes onto a playlist. A playlist or folder
                    // goes between the others, or onto a folder to go inside it.
                    dropZone: (node, source) => source === sidebar.presentationDrag ? (node.folder ? "" : "onto")
                                              : node.folder ? "both" : "between"
                    onPicked: (entry) => {
                        if (entry.folder)
                            sidebar.win.selectedNode = entry.path
                        else
                            sidebar.win.openPlaylist(entry.path)
                    }
                    onMenuRequested: (entry, item) => sidebar.win.showPlaylistMenu(entry, item)
                    onRenamed: (entry, name) => sidebar.win.report(sidebar.win.catalog.renamePlaylist(entry.path, name))
                    onEditingEnded: sidebar.win.takeFocus()
                    onDropped: (node, source, where) => {
                        if (source === sidebar.playlistDrag)
                            sidebar.win.report(sidebar.win.catalog.movePlaylistNode(source.entry.path, node.path, where))
                        else if (sidebar.win.openable(source.entry))
                            sidebar.win.addToPlaylist(node.path, source.entry.file)
                    }
                }

                Item {
                    width: 1
                    height: 10
                }
            }
        }
    }

    // The Presentations header: a solid grey-blue bar, so there is no mistaking
    // where the pane above ends, naming the library or playlist whose presentations follow.
    Rectangle {
        id: presentationsHeader

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: sources.bottom
        height: 30
        color: "#4f5665"

        Text {
            id: presentationsTitle

            x: 12
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            font.pixelSize: 12
            font.bold: true
            font.capitalization: Font.AllUppercase
            text: "Presentations"
        }

        Text {
            anchors.left: presentationsTitle.right
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            color: "#c9cdd6"
            font.pixelSize: 12
            text: {
                const source = sidebar.win.playlistId !== ""
                    ? sidebar.win.catalog.playlists.find(p => p.path === sidebar.win.playlistId)
                    : sidebar.win.catalog.libraries.find(l => l.path === sidebar.win.libraryPath)
                return source ? source.name : ""
            }
        }
    }

    // The edge between the two panes, along the top of the header
    Divider {
        vertical: false
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: presentationsHeader.top
        onMoved: (delta) => sidebar.win.sourcesHeight = sources.height + delta
        onReleased: sidebar.win.takeFocus()
    }

    // In a playlist, rows can be dragged up and down to reorder them.
    SidebarList {
        objectName: "presentationList"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: presentationsHeader.bottom
        anchors.bottom: parent.bottom
        model: sidebar.win.documents
        selectedPath: sidebar.win.documentKey
        livePath: !sidebar.win.cleared && sidebar.win.livePlaylistId === sidebar.win.playlistId ? sidebar.win.liveKey : ""
        dragProxy: sidebar.presentationDrag
        draggable: (entry) => entry.playlistItem || sidebar.win.openable(entry)
        dropKeys: sidebar.win.playlistId !== "" ? ["presentation"] : []
        dropZone: (entry, source) => "between"
        onPicked: (entry) => {
            if (entry.missing)
                sidebar.win.report("“" + entry.name + "” is in the playlist but not in the libraries here.")
            else
                sidebar.win.openEntry(entry)
        }
        onMenuRequested: (entry, item) => sidebar.win.showPresentationMenu(entry, item)
        onDropped: (target, source, where) => {
            if (source.entry.playlistItem && source.entry.path !== target.path)
                sidebar.win.report(sidebar.win.catalog.movePlaylistItem(source.entry.path, target.path, where === "after"))
        }
    }
}
