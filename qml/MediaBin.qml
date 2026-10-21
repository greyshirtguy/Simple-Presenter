import QtQuick
import QtQuick.Controls.Basic

// The media bin, along the bottom of the operator window: the workspace's media
// playlists and the folders they are kept in on the left, and the files of the playlist
// picked there, as thumbnails, on the right.
//
// At the top of the list is Video Inputs, ProPresenter's playlist of the workspace's
// video inputs (see VideoInputs): picked, the grid has the inputs, by their thumbnails
// and names, the one on the video input layer ringed. A click on one puts it on that
// layer; dragged onto a slide or a macro it gives it a Video Input Action. The inputs
// are set up in Settings → Inputs, which + and a right click lead to.
//
// Click a file to put it on the output's media layer. Drag one onto a slide to make that
// slide trigger it, between two slides to give it a slide of its own, or onto another
// file of the playlist to move it there. Files dragged in from another application,
// such as the file manager, are added: to a playlist they are dropped on in the list,
// or to the one being browsed, at the place among its thumbnails where they are
// dropped. They are referred to where they are on disk, not copied. The thumbnails come
// from the thumbnail provider (src/thumbnailprovider.h): each is asked for by a url
// and arrives when it has been made, so a playlist full of video opens at once and
// fills in.
Rectangle {
    id: bin

    // The operator window: what this shows is its state, and what happens here is done
    // by calling its functions.
    required property var win
    // How wide the list of media playlists is, the thumbnails having the rest. The
    // operator window keeps it, and has the line between the two that is dragged to
    // change it.
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
        // (Video Inputs first: it is the inputs themselves, and no playlist of the file's)
        model: [{ name: "Video Inputs", path: bin.win.videoInputsNode, parent: "", depth: 0, folder: false, icon: "videoInputs" }]
               .concat(bin.win.catalog.mediaPlaylists)
        selectedPath: bin.win.selectedMediaNode
        livePath: bin.win.liveMedia ? bin.win.liveMediaPlaylistId : Show.liveVideoInput >= 0 ? bin.win.videoInputsNode : ""
        dragProxy: bin.playlistDrag
        draggable: (entry) => entry.path !== bin.win.videoInputsNode
        // A playlist or folder of the list can be moved by dropping it on another; files
        // from another application go onto a playlist, if there is media among them.
        dropKeys: bin.win.takesDrops ? ["mediaPlaylist", "text/uri-list"] : ["mediaPlaylist"]
        dropZone: (node, source, urls) => node.path === bin.win.videoInputsNode ? ""
                                         : source !== null ? (node.folder ? "both" : "between")
                                         : !node.folder && bin.win.catalog.mediaAmong(urls).length > 0 ? "onto" : ""
        onPicked: (entry) => {
            if (entry.path === bin.win.videoInputsNode)
                bin.win.openVideoInputs()
            else if (entry.folder)
                bin.win.selectedMediaNode = entry.path
            else
                bin.win.openMediaPlaylist(entry.path)
        }
        onMenuRequested: (entry, item) => bin.win.showMediaNodeMenu(entry, item)
        onRenamed: (entry, name) => bin.win.report(bin.win.catalog.renameMediaPlaylist(entry.path, name))
        onEditingEnded: bin.win.takeFocus()
        onDropped: (node, source, where, drop) => {
            if (source !== null)
                bin.win.report(bin.win.catalog.moveMediaPlaylist(source.entry.path, node.path, where))
            else
                bin.win.dropOnMediaPlaylist(node.path, drop)
        }
    }

    // Files dropped among the thumbnails but on none of them go at the end of the
    // playlist being browsed. (This is under the grid, so a thumbnail that is there has
    // the drag first.)
    DropArea {
        id: endDrop

        anchors.fill: mediaGrid
        keys: ["text/uri-list"]
        enabled: bin.win.takesDrops && bin.win.mediaPlaylistId !== "" && bin.win.mediaPlaylistId !== bin.win.videoInputsNode
        onEntered: (drag) => bin.win.acceptMediaDrag(drag)
        onPositionChanged: (drag) => bin.win.acceptMediaDrag(drag)
        onDropped: (drop) => bin.win.dropOnMediaPlaylist(bin.win.mediaPlaylistId, drop)
    }

    // An outline round the thumbnails says that is where they would go.
    Rectangle {
        anchors.fill: mediaGrid
        anchors.bottomMargin: 4
        visible: endDrop.containsDrag
        radius: 4
        color: "transparent"
        border.width: 2
        border.color: "#c0ffffff"
    }

    // ---- Video Inputs: the workspace's inputs, in their order, by thumbnail and name.
    // (Made only while they are picked: most workspaces are never shown them.)
    Loader {
        anchors.fill: mediaGrid
        active: bin.win.mediaPlaylistId === bin.win.videoInputsNode

        sourceComponent: Item {
            GridView {
                id: inputGrid

                objectName: "videoInputGrid"
                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cellWidth: mediaGrid.cellWidth
                cellHeight: mediaGrid.cellHeight
                model: VideoInputs.inputs

                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: inputCell

                    required property var modelData
                    readonly property bool live: Show.liveVideoInput === modelData.index

                    objectName: "videoInputCell:" + modelData.name
                    width: inputGrid.cellWidth
                    height: inputGrid.cellHeight
                    z: live ? 1 : 0

                    Loader {
                        anchors.fill: inputFrame
                        anchors.margins: -5
                        active: inputCell.live

                        sourceComponent: LiveRing {
                            background: bin.win.surfaceColor
                        }
                    }

                    Rectangle {
                        id: inputFrame

                        anchors.fill: parent
                        anchors.margins: 6
                        color: inputMouse.containsMouse ? "#4a4d54" : bin.win.panelColor
                        radius: 4

                        // Its thumbnail, or its colour with ProPresenter's camera on it: the
                        // inputs themselves are not captured for this (see VideoInputs)
                        Rectangle {
                            id: inputThumbnail

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 3
                            height: width * 9 / 16
                            color: inputCell.modelData.color !== "" ? inputCell.modelData.color : "black"
                            clip: true

                            Image {
                                anchors.fill: parent
                                visible: inputCell.modelData.thumbnail !== ""
                                source: inputCell.modelData.thumbnail
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                            }

                            ProIcon {
                                anchors.centerIn: parent
                                visible: inputCell.modelData.thumbnail === ""
                                name: "VideoInput"
                                ink: "#c0ffffff"
                                size: Math.min(parent.height * 0.9, 64)
                            }

                            Text {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottomMargin: 4
                                visible: inputCell.modelData.device === "" || inputCell.modelData.missing
                                color: "#ffb4a9"
                                font.pixelSize: 11
                                text: inputCell.modelData.device === "" ? "no device" : "not here"
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: inputThumbnail.bottom
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                            color: bin.win.textColor
                            font.pixelSize: 11
                            text: inputCell.modelData.name
                        }
                    }

                    // A click puts it on the video input layer; dragged, it is a Video Input
                    // Action for a slide or a macro
                    DragSource {
                        id: inputMouse

                        anchors.fill: parent
                        win: bin.win
                        payload: ({ kind: "videoInput", id: inputCell.modelData.id, inputIndex: inputCell.modelData.index,
                                    name: inputCell.modelData.name, picture: "VideoInput" })
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton)
                                bin.win.showVideoInputMenu(inputCell.modelData, inputCell, mouse.x, mouse.y)
                            else if (!dragged)
                                bin.win.showVideoInput(inputCell.modelData.index)
                        }
                    }
                }
            }

            EmptyNote {
                anchors.centerIn: inputGrid
                width: inputGrid.width - 40
                visible: VideoInputs.inputs.length === 0
                text: "No video inputs. They are set up in Settings → Inputs."
            }
        }
    }

    GridView {
        id: mediaGrid

        objectName: "mediaGrid"
        readonly property real labelHeight: 24

        visible: bin.win.mediaPlaylistId !== bin.win.videoInputsNode
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
                        visible: !lookedMedia.active
                        source: mediaCell.modelData.missing || lookedMedia.active ? "" : bin.win.thumbnailUrl(mediaCell.modelData.path)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    // Media set to anything in its Media Inspector, drawn as it is set:
                    // cropped, turned and flipped in its thumbnail, and fitted, blurred and
                    // given its effects over it. Made only for such media.
                    Loader {
                        id: lookedMedia

                        anchors.fill: parent
                        active: !mediaCell.modelData.missing && mediaCell.modelData.settings !== undefined

                        sourceComponent: MediaPicture {
                            source: bin.win.thumbnailUrl(mediaCell.modelData.path, mediaCell.modelData.settings)
                            settings: mediaCell.modelData.settings
                            shaped: true
                            animated: false
                        }
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
                    ActionIcon {
                        x: 3
                        y: 3
                        width: 21
                        strength: bin.win.actionIconOpacity

                        MediaBadge {
                            anchors.centerIn: parent
                            size: 0.8
                            color: "transparent"
                            foreground: mediaCell.modelData.foreground
                            missing: mediaCell.modelData.missing
                        }
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
            // Files dragged in from another application are added at the same place.
            DropArea {
                id: reorderDrop

                property bool after: false
                // Whether what is over the cell is files from another application
                property bool files: false
                readonly property bool moving: containsDrag && (files || (bin.mediaDrag.media !== null
                                                                          && bin.mediaDrag.media.id !== mediaCell.modelData.id))

                function follow(drag) {
                    after = drag.x > width / 2
                    files = drag.source === null
                    bin.win.acceptMediaDrag(drag)
                }

                anchors.fill: parent
                keys: ["media", "text/uri-list"]
                enabled: bin.win.takesDrops
                onEntered: (drag) => follow(drag)
                onPositionChanged: (drag) => follow(drag)
                // One call and nothing after it: either way the grid is rebuilt, and this
                // cell with it.
                onDropped: (drop) => {
                    if (drop.source === null)
                        bin.win.dropOnMediaPlaylist(bin.win.mediaPlaylistId, drop, mediaCell.modelData.id, after)
                    else if (drop.source.media.id !== mediaCell.modelData.id)
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
        visible: bin.win.mediaFiles.length === 0 && mediaGrid.visible
        text: bin.win.mediaPlaylistId !== "" ? "This media playlist is empty. Add media to it from the + above, or drag files in."
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
