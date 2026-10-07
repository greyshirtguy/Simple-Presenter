import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The middle of the operator window: the slides of the presentation being viewed, as a
// grid of thumbnails. Click one to put it on the output; right-click for its menu.
//
// Media can be dropped here, dragged out of the media bin or, as files, out of another
// application such as the file manager. Dropped on a slide, it becomes the media that
// slide triggers; dropped between two slides, or before the first or after the last,
// it becomes a slide of its own there. Where it lands also settles how it plays (see
// assignMedia() and insertMediaSlides() in Main.qml).
//
// Each thumbnail is the slide itself, drawn small by the same Slide that draws the
// output, not a picture of it, so what is here is always what would be shown. Its frame
// is the colour of the slide's group, with the group's name on the first slide of each
// run; the slide that is live has an orange ring.
Item {
    id: slides

    // The operator window: what this shows is its state, and what happens here is done
    // by calling its functions.
    required property var win
    // What the operator window needs of the grid itself: where it is scrolled to, how
    // wide it is and how many columns it has (for stepping the thumbnail size), and
    // bringing a slide into view.
    property alias contentY: grid.contentY
    readonly property int columns: grid.columns
    readonly property real gridWidth: grid.width

    function positionViewAtBeginning() {
        grid.positionViewAtBeginning()
    }

    function positionViewAtIndex(index, mode) {
        grid.positionViewAtIndex(index, mode)
    }

    // Whatever there is to tell the user, and the arrangement of the presentation being viewed
    Item {
        id: gridHeader

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        visible: (slides.win.document !== null && slides.win.document.arrangements.length > 0) || slides.win.notice !== ""
        height: visible ? 34 : 6

        Text {
            anchors.left: parent.left
            anchors.right: arrangementLabel.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: slides.win.noticeIsError ? "#ff6b6b" : slides.win.dimTextColor
            font.pixelSize: 13
            text: slides.win.notice
        }

        Text {
            id: arrangementLabel

            anchors.right: arrangementBox.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: arrangementBox.visible
            color: slides.win.dimTextColor
            font.pixelSize: 12
            text: "Arrangement"
        }

        // "Master" is every group once, in the order the presentation stores them.
        // The choice is saved in the presentation file, or for a playlist row in the
        // playlist.
        AppComboBox {
            id: arrangementBox

            objectName: "arrangementBox"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 160
            height: 24
            font.pixelSize: 12
            visible: slides.win.document !== null && slides.win.document.arrangements.length > 0
            model: slides.win.document ? ["Master"].concat(slides.win.document.arrangements) : []
            currentIndex: slides.win.document && slides.win.document.arrangement !== ""
                          ? slides.win.document.arrangements.indexOf(slides.win.document.arrangement) + 1 : 0
            onActivated: (index) => slides.win.setArrangement(slides.win.currentEntry(), index === 0 ? "" : model[index])
        }
    }

    // Media dropped on the grid but on none of its slides goes after the last of them.
    // (This is under the grid, so a slide that is there has the drag first.)
    DropArea {
        id: endDrop

        anchors.fill: grid
        keys: ["media", "text/uri-list"]
        enabled: slides.win.takesDrops && slides.win.document !== null
        onEntered: (drag) => slides.win.acceptMediaDrag(drag)
        onPositionChanged: (drag) => slides.win.acceptMediaDrag(drag)
        onDropped: (drop) => slides.win.dropOnSlides(-1, "after", drop)
    }

    // Slides
    GridView {
        id: grid

        objectName: "slideGrid"

        readonly property int columns: Math.max(1, Math.floor(width / slides.win.thumbnailWidth))
        readonly property real labelHeight: 26
        readonly property int frameWidth: 4

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: gridHeader.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 4
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Room under the last row for what sits over the bottom corners, so that the
        // last slides can be scrolled clear of it
        footer: Item {
            height: 40
        }
        cellWidth: Math.floor((width - 12) / columns)
        cellHeight: (cellWidth - 12 - 2 * frameWidth) * 9 / 16 + frameWidth + labelHeight + 12
        // One row beyond what is in view is kept ready, so that scrolling does not wait for
        // thumbnails to be built, and no more: each is a slide drawn afresh.
        cacheBuffer: Math.max(0, cellHeight)
        model: slides.win.document ? slides.win.document.slides : []

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        delegate: Item {
            id: cell

            required property var modelData
            required property int index
            readonly property bool live: slides.win.viewingLive && !slides.win.cleared && slides.win.liveIndex === index
            // The frame is the slide's group colour, and the label is drawn on it.
            readonly property color frame: slides.win.groupColor(modelData)
            readonly property bool lightFrame: 0.299 * frame.r + 0.587 * frame.g + 0.114 * frame.b > 0.6

            width: grid.cellWidth
            height: grid.cellHeight
            // The live slide's glow spills a little over its neighbours.
            z: live ? 1 : 0

            // Only the live slide has a ring, and only it has one made.
            Loader {
                anchors.fill: frameRect
                anchors.margins: -5
                active: cell.live
                sourceComponent: LiveRing {}
            }

            Rectangle {
                id: frameRect

                anchors.fill: parent
                anchors.margins: 6
                color: cell.frame
                radius: 4

                Rectangle {
                    id: thumbnail

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: grid.frameWidth
                    height: width * 9 / 16
                    color: "black"

                    // The media the cue triggers alongside the slide
                    Image {
                        anchors.fill: parent
                        visible: cell.modelData.media !== undefined
                        source: visible ? slides.win.thumbnailUrl(cell.modelData.media.path) : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    Slide {
                        anchors.fill: parent
                        slide: cell.modelData
                        effects: false
                    }

                    // Marks a slide that triggers media, and says whether as a background
                    // or a foreground; amber when the media file cannot be found, in which
                    // case there is no thumbnail behind the slide either. Only made for the
                    // slides that have media.
                    Loader {
                        x: 4
                        y: 4
                        active: cell.modelData.mediaName !== ""

                        sourceComponent: MediaBadge {
                            foreground: cell.modelData.mediaForeground
                            missing: cell.modelData.media === undefined
                        }
                    }
                }

                // The group's name appears on the first slide of each run of it.
                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: thumbnail.bottom
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: cell.lightFrame ? "black" : "white"
                    font.pixelSize: 12
                    text: (cell.index + 1)
                        + (cell.modelData.groupStart && cell.modelData.group !== "" ? "  " + cell.modelData.group : "")
                        + (cell.modelData.label !== "" && cell.modelData.label !== cell.modelData.group
                           ? "  " + cell.modelData.label : "")
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    visible: (cellMouse.containsMouse && !cell.live) || mediaDrop.onto
                    color: "transparent"
                    border.width: mediaDrop.onto ? 3 : 1
                    border.color: mediaDrop.onto ? "white" : "#c0ffffff"
                }
            }

            // Where media being dragged would become a slide of its own: a line in the
            // gap before this slide or after it. The last slide also shows it for a
            // drag that is over the grid but past every slide.
            Rectangle {
                readonly property bool atEnd: endDrop.containsDrag && cell.index === grid.count - 1

                x: mediaDrop.zone === "after" || atEnd ? parent.width - 1.5 : -1.5
                y: 6
                width: 3
                height: parent.height - 12
                visible: (mediaDrop.containsDrag && !mediaDrop.onto) || atEnd
                color: "white"
            }

            MouseArea {
                id: cellMouse

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton) {
                        // The keyboard comes back to the show, from whatever box of the
                        // window was being typed in.
                        slides.win.takeFocus()
                        slides.win.goLive(cell.index)
                    } else {
                        slides.win.showSlideMenu(cell.index, cell, mouse.x, mouse.y)
                    }
                }
            }

            // Media dropped on the middle of the slide becomes the media the slide
            // triggers. Dropped on its left or right edge, which is to say in the gap
            // beside it, it becomes a slide of its own there.
            DropArea {
                id: mediaDrop

                // Which of those the drag is over: "onto", "before" or "after"
                property string zone: "onto"
                readonly property bool onto: containsDrag && zone === "onto"
                // How far in from each side the edges reach
                readonly property real edge: Math.max(16, Math.min(30, width * 0.14))

                function follow(drag) {
                    zone = drag.x < edge ? "before" : drag.x > width - edge ? "after" : "onto"
                    slides.win.acceptMediaDrag(drag)
                }

                anchors.fill: parent
                keys: ["media", "text/uri-list"]
                enabled: slides.win.takesDrops
                onEntered: (drag) => follow(drag)
                onPositionChanged: (drag) => follow(drag)
                onDropped: (drop) => slides.win.dropOnSlides(cell.index, zone, drop)
            }
        }
    }

    // The transition, over the bottom left corner of the slides
    TransitionControls {
        anchors.left: grid.left
        anchors.bottom: grid.bottom
        anchors.leftMargin: 8
        anchors.bottomMargin: 10
        win: slides.win
    }

    // Thumbnail size, over the bottom right corner of the slides
    ZoomButtons {
        anchors.right: grid.right
        anchors.bottom: grid.bottom
        anchors.rightMargin: 22
        anchors.bottomMargin: 12
        visible: slides.win.document !== null && slides.win.document.slides.length > 0
        canShrink: slides.win.thumbnailWidth > slides.win.smallestThumbnail
        canGrow: slides.win.thumbnailWidth < slides.win.largestThumbnail && grid.columns > 1
        onShrink: slides.win.zoomThumbnails(-1)
        onGrow: slides.win.zoomThumbnails(1)
    }

    EmptyNote {
        anchors.centerIn: grid
        width: grid.width - 80
        visible: text !== ""
        text: slides.win.document ? slides.win.document.error
            : slides.win.catalog.libraries.length === 0 ? "No libraries yet. Add a folder of .pro files to\n" + slides.win.catalog.librariesDirectory
            : "This library has no presentations."
    }
}
