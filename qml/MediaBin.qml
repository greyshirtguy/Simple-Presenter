import QtQuick
import QtQuick.Controls.Basic

// The media bin, along the bottom of the operator window: the workspace's media
// playlists and the folders they are kept in on the left, and the files of the playlist
// picked there, as thumbnails, on the right.
//
// Click a file to put it on the output's media layer. Drag one onto a slide to make that
// slide trigger it, or onto another file of the playlist to move it there. The
// thumbnails come from the thumbnail provider (src/thumbnailprovider.h): each is asked
// for by a url and arrives when it has been made, so a playlist full of video opens at
// once and fills in.
Rectangle {
    id: bin

    // The operator window: what this shows is its state, and what happens here is done
    // by calling its functions.
    required property var win
    // How wide the list of media playlists is: the width of the lists above it
    property real listWidth: 260
    // What a row dragged out of the list of playlists, and a file dragged out of the
    // grid, are carried by
    required property Item playlistDrag
    required property Item mediaDrag
    // Whether a media playlist is being renamed in place, which has the keyboard
    readonly property bool renaming: mediaList.editingPath !== ""
    // What the operator window needs of the grid: where it is scrolled to, and its width
    // and columns, for stepping the thumbnail size
    property alias contentY: mediaGrid.contentY
    readonly property int columns: mediaGrid.columns
    readonly property real gridWidth: mediaGrid.width

    // Starts renaming a media playlist or folder in place.
    function renamePlaylist(id) {
        mediaList.editingPath = id
    }

    // Ends a drag out of the grid by dropping what is being dragged. Here, not in the
    // cell the drag began in: a drop that reorders the playlist rebuilds the grid, and
    // that cell is then gone before it could tidy up.
    function dropMedia() {
        mediaDrag.Drag.drop()
        mediaDrag.Drag.active = false
    }

    color: bin.win.surfaceColor

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: "#15161a"
    }

    SectionTitle {
        id: mediaTitle

        anchors.top: parent.top
        color: bin.win.mediaBinColor
        text: "Media bin"
    }

    // Add a media folder or playlist, or media to the playlist being browsed
    Rectangle {
        id: mediaAddButton

        objectName: "addMediaButton"
        x: bin.listWidth - width - 12
        anchors.bottom: mediaTitle.bottom
        anchors.bottomMargin: 3
        width: 26
        height: 22
        radius: 5
        color: mediaAddMouse.pressed ? "#5c5f67" : mediaAddMouse.containsMouse ? "#53565e" : "#474a51"

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            color: bin.win.textColor
            font.pixelSize: 16
            text: "+"
        }

        MouseArea {
            id: mediaAddMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: bin.win.showMediaAddMenu(mediaAddButton)
        }
    }

    // Media playlists and the folders they are kept in, in ProPresenter's own
    // media playlists file. They behave as the playlists of presentations do:
    // picking a playlist shows its media, picking a folder only selects it.
    SidebarList {
        id: mediaList

        objectName: "mediaPlaylistList"
        anchors.left: parent.left
        anchors.top: mediaTitle.bottom
        anchors.bottom: parent.bottom
        width: bin.listWidth
        model: bin.win.catalog.mediaPlaylists
        selectedPath: bin.win.selectedMediaNode
        livePath: bin.win.liveMedia ? bin.win.liveMediaPlaylistId : ""
        dragProxy: bin.playlistDrag
        dropKeys: ["mediaPlaylist"]
        dropZone: (node, source) => node.folder ? "both" : "between"
        onPicked: (entry) => {
            if (entry.folder)
                bin.win.selectedMediaNode = entry.path
            else
                bin.win.openMediaPlaylist(entry.path)
        }
        onMenuRequested: (entry, item) => bin.win.showMediaNodeMenu(entry, item)
        onRenamed: (entry, name) => bin.win.report(bin.win.catalog.renameMediaPlaylist(entry.path, name))
        onEditingEnded: bin.win.takeFocus()
        onDropped: (node, source, where) => bin.win.report(bin.win.catalog.moveMediaPlaylist(source.entry.path, node.path, where))
    }

    GridView {
        id: mediaGrid

        objectName: "mediaGrid"
        readonly property real labelHeight: 24
        readonly property int columns: Math.max(1, Math.floor(width / bin.win.mediaThumbnailWidth))

        anchors.left: mediaList.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 4
        anchors.topMargin: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        cellWidth: Math.floor((width - 12) / columns)
        cellHeight: (cellWidth - 12) * 9 / 16 + labelHeight + 12
        model: bin.win.mediaFiles

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        delegate: Item {
            id: mediaCell

            required property var modelData
            readonly property bool live: bin.win.liveMedia !== null && bin.win.liveMedia.path === modelData.path

            width: mediaGrid.cellWidth
            height: mediaGrid.cellHeight
            // The live file's glow spills a little over its neighbours.
            z: live ? 1 : 0

            // Only the file that is live has a ring, and only it has one made.
            Loader {
                anchors.fill: mediaFrame
                anchors.margins: -5
                active: mediaCell.live

                sourceComponent: LiveRing {
                    background: bin.win.surfaceColor
                }
            }

            Rectangle {
                id: mediaFrame

                anchors.fill: parent
                anchors.margins: 6
                color: mediaMouse.containsMouse ? "#4a4d54" : bin.win.panelColor
                radius: 4

                Rectangle {
                    id: mediaThumbnail

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 3
                    height: width * 9 / 16
                    color: "black"

                    Image {
                        anchors.fill: parent
                        source: mediaCell.modelData.missing ? ""
                              : bin.win.thumbnailUrl(mediaCell.modelData.path)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    // A media playlist can name a file that is not on this machine.
                    Text {
                        anchors.centerIn: parent
                        visible: mediaCell.modelData.missing
                        color: "#d07070"
                        font.pixelSize: 12
                        text: "missing"
                    }

                    // Whether it plays as a background or a foreground
                    MediaBadge {
                        x: 3
                        y: 3
                        foreground: mediaCell.modelData.foreground
                        missing: mediaCell.modelData.missing
                    }
                }

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: mediaThumbnail.bottom
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                    color: mediaCell.modelData.missing ? bin.win.dimTextColor : bin.win.textColor
                    font.pixelSize: 11
                    text: mediaCell.modelData.name
                }
            }

            // Dropping another of the playlist's media here moves it to this
            // place: before this one from the left half, after it from the right.
            DropArea {
                id: reorderDrop

                property bool after: false
                readonly property bool moving: containsDrag && bin.mediaDrag.media !== null
                                               && bin.mediaDrag.media.id !== mediaCell.modelData.id

                anchors.fill: parent
                keys: ["media"]
                onEntered: (drag) => after = drag.x > width / 2
                onPositionChanged: (drag) => after = drag.x > width / 2
                // One call and nothing after it: moving the file rebuilds the grid, and this
                // cell with it.
                onDropped: (drop) => {
                    if (drop.source.media.id !== mediaCell.modelData.id)
                        bin.win.moveMedia(drop.source.media.id, mediaCell.modelData.id, after)
                }
            }

            Rectangle {
                x: reorderDrop.after ? parent.width - 1.5 : -1.5
                y: 6
                width: 3
                height: parent.height - 12
                visible: reorderDrop.moving
                color: "white"
            }

            // Click to put the file on the media layer; drag it onto a slide to
            // make that slide trigger it, or onto another of the playlist's media
            // to move it there.
            MouseArea {
                id: mediaMouse

                property point pressedAt
                property bool dragging: false

                function finish() {
                    dragging = false
                    bin.mediaDrag.Drag.active = false
                }

                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onPressed: (mouse) => {
                    pressedAt = Qt.point(mouse.x, mouse.y)
                    dragging = false
                }
                onPositionChanged: (mouse) => {
                    if (!(pressedButtons & Qt.LeftButton))
                        return
                    const at = mapToItem(bin.mediaDrag.parent, mouse.x, mouse.y)
                    bin.mediaDrag.x = at.x
                    bin.mediaDrag.y = at.y
                    if (!dragging && Math.abs(mouse.x - pressedAt.x) + Math.abs(mouse.y - pressedAt.y) > 10) {
                        dragging = true
                        bin.mediaDrag.media = mediaCell.modelData
                        bin.mediaDrag.Drag.active = true
                    }
                }
                onReleased: (mouse) => {
                    if (dragging) {
                        dragging = false
                        bin.dropMedia()
                    } else if (mouse.button === Qt.RightButton) {
                        bin.win.showMediaItemMenu(mediaCell.modelData, mediaCell)
                    } else if (!mediaCell.modelData.missing) {
                        // The keyboard comes back to the show, as with a click on a slide.
                        bin.win.takeFocus()
                        bin.win.showMedia(mediaCell.modelData, bin.win.mediaPlaylistId)
                    }
                }
                onCanceled: finish()
            }
        }
    }

    EmptyNote {
        anchors.centerIn: mediaGrid
        width: mediaGrid.width - 80
        visible: bin.win.mediaFiles.length === 0
        text: bin.win.mediaPlaylistId !== "" ? "This media playlist is empty. Add media to it from the + above."
            : "No media playlists yet. Add one from the + beside Media bin."
    }

    // Thumbnail size, over the bottom right corner of the media
    ZoomButtons {
        anchors.right: mediaGrid.right
        anchors.bottom: mediaGrid.bottom
        anchors.rightMargin: 22
        anchors.bottomMargin: 10
        visible: bin.win.mediaFiles.length > 0
        canShrink: bin.win.mediaThumbnailWidth > bin.win.smallestMediaThumbnail
        canGrow: bin.win.mediaThumbnailWidth < bin.win.largestMediaThumbnail && mediaGrid.columns > 1
        onShrink: bin.win.zoomMediaThumbnails(-1)
        onGrow: bin.win.zoomMediaThumbnails(1)
    }
}
