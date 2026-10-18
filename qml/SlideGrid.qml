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

    // A thin header over the slides. In its middle is the name of the presentation
    // being viewed. At its left is the key a song's chords are shown in, for a song
    // that has chords, and after that whatever there is to tell the user. At its right
    // is the button that opens the arrangement editor under this line
    // (ArrangementBar.qml), as ProPresenter has its Arrangements button in the
    // presentation's header, with the arrangement being viewed named beside it if that
    // is not Master. The line is as high as it was without the editor, and the
    // editor's two rows are added to it while it is open.
    Item {
        id: gridHeader

        // Whether the arrangement editor is open under the line. Not in Simple View,
        // which is for running a show and has nothing to set things up with: the
        // button is not there either (it would be under that view's own button, where
        // the pane at the right is kept).
        // (Not for Bible mode's slides, which have no arrangements, destination or Slide
        // Show of their own to set: the maintainer, 2026-10-15)
        readonly property bool arranging: slides.win.arrangementsShown && slides.win.document !== null && !slides.win.simple && !slides.win.bibleMode

        onArrangingChanged: {
            if (arranging)
                arrangementBar.wanted = true
        }

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        visible: slides.win.document !== null || slides.win.notice !== ""
        height: visible ? 30 + (arranging ? arrangementBar.height : 0) : 6

        // The key a song's chords are shown in, where a stage screen shows them: at the
        // left of the line. It is there only for a presentation that has chords.
        // Picking a key changes what is shown and nothing in the file: the chords stay
        // written in the song's own key, and the pick lasts while the app is open.
        Text {
            id: keyLabel

            anchors.left: parent.left
            anchors.verticalCenter: keyBox.verticalCenter
            visible: keyBox.visible
            color: slides.win.dimTextColor
            font.pixelSize: 12
            text: "Key"
        }

        AppComboBox {
            id: keyBox

            objectName: "chordKeyPicker"
            anchors.left: keyLabel.right
            anchors.leftMargin: 8
            y: 3
            width: 70
            height: 24
            font.pixelSize: 12
            visible: slides.win.document !== null && slides.win.document.hasChords === true
            // The major keys, or for a song in a minor key the minor ones; and the key
            // it is in first if that is not one of the usual names (C sharp major, which
            // ProPresenter's files can say and a musician would call D flat).
            readonly property string shownKey: slides.win.document ? slides.win.keyOf(slides.win.document) : ""
            readonly property var usual: shownKey.length > 1 && shownKey.endsWith("m") ? Chords.minorKeys : Chords.majorKeys
            model: shownKey === "" || usual.includes(shownKey) ? usual : [shownKey].concat(usual)
            currentIndex: model.indexOf(shownKey)
            onActivated: (index) => slides.win.setChordKey(slides.win.document, model[index])
        }

        // The presentation's name, in the middle of the line: as wide as its words, up
        // to what keeps it in the middle between what is at the line's two ends; and
        // while there is something to tell the user, up to a third of the line, the
        // rest being for that.
        //
        // In Simple View it is at the left, as it always was there: the middle of the
        // top of the slides is where that view's own button is, which is wide, and
        // over this line, for the first seconds of the view (SimpleViewToggle.qml).
        Text {
            id: gridTitle

            // Where what is at the left ends, and where what is at the right begins
            readonly property real clearFrom: keyBox.visible ? keyBox.x + keyBox.width + 16 : 0
            readonly property real clearTo: (headerMarks.visible ? headerMarks.x : arrangementName.visible ? arrangementName.x : arrangementToggle.x) - 16
            readonly property bool aside: slides.win.simple

            objectName: "gridTitle"
            x: aside ? clearFrom : Math.round((parent.width - width) / 2)
            y: (30 - height) / 2
            width: aside ? Math.min(implicitWidth, Math.max(40, (clearTo - clearFrom) * (slides.win.notice !== "" ? 0.5 : 1)))
                         : Math.min(implicitWidth, Math.max(40, 2 * Math.min(parent.width / 2 - clearFrom, clearTo - parent.width / 2)),
                                    slides.win.notice !== "" ? Math.max(40, parent.width * 0.34) : implicitWidth)
            elide: Text.ElideRight
            color: slides.win.textColor
            font.pixelSize: 13
            font.weight: Font.DemiBold
            text: slides.win.document ? slides.win.document.name : ""
        }

        // Whatever there is to tell the user: at the left, as far as the name; or,
        // where the name is at the left (Simple View), after it
        Text {
            objectName: "gridNotice"
            anchors.left: gridTitle.aside ? gridTitle.right : keyBox.visible ? keyBox.right : parent.left
            anchors.leftMargin: gridTitle.aside ? (gridTitle.text !== "" ? 14 : 0) : keyBox.visible ? 14 : 0
            anchors.right: !gridTitle.aside ? gridTitle.left : headerMarks.visible ? headerMarks.left
                                                           : arrangementName.visible ? arrangementName.left : arrangementToggle.left
            anchors.rightMargin: 16
            anchors.verticalCenter: gridTitle.verticalCenter
            elide: Text.ElideRight
            color: slides.win.noticeIsError ? "#ff6b6b" : slides.win.dimTextColor
            font.pixelSize: 13
            text: slides.win.notice
        }

        // The small buttons ProPresenter has over a presentation, which say a thing
        // about the whole of it and open the menu that changes it. Here they are at
        // the left of the arrangement's name.
        Row {
            id: headerMarks

            anchors.right: arrangementName.visible ? arrangementName.left : arrangementToggle.left
            anchors.rightMargin: 10
            y: 4
            spacing: 4
            visible: slides.win.document !== null && !slides.win.simple && !slides.win.bibleMode

            // The destination: where the presentation's slides go when they are
            // triggered, the slide and media layers ("Presentation") or the
            // announcements layer. ProPresenter's picture of the announcements, which
            // is green for a presentation that is sent to them, as it is there.
            Rectangle {
                id: destinationButton

                readonly property bool announces: slides.win.viewedGoesToAnnouncements

                objectName: "destinationButton"
                width: 26
                height: 22
                radius: 6
                color: destinationMouse.pressed ? "#50535a" : destinationMouse.containsMouse ? "#45484e" : "transparent"

                ProIcon {
                    anchors.centerIn: parent
                    name: "Announcements"
                    size: 24
                    ink: destinationButton.announces ? "#3ddc68" : "#8d9096"
                }

                MouseArea {
                    id: destinationMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: slides.win.showDestinationMenu(destinationButton)
                }
            }

            // The Slide Show: the seconds each slide of the presentation stays before
            // the next is triggered by itself, round and round, once one of them has
            // been triggered. The button opens its menu, and for a presentation that
            // has one is lit and says the seconds.
            //
            // (ProPresenter's pack has no picture of it: the arrow going round is the
            // one this app has for a video that goes round.)
            Rectangle {
                id: slideShowButton

                readonly property real seconds: slides.win.document ? (slides.win.document.slideShow ?? 0) : 0
                readonly property bool on: seconds > 0

                objectName: "slideShowButton"
                width: on ? 24 + slideShowSeconds.implicitWidth : 22
                height: 22
                radius: 6
                color: slideShowMouse.pressed ? "#50535a" : slideShowMouse.containsMouse ? "#45484e" : "transparent"

                ActionGlyph {
                    x: 5
                    anchors.verticalCenter: parent.verticalCenter
                    kind: "loop"
                    ink: slideShowButton.on ? "#4da3ff" : "#8d9096"
                }

                Text {
                    id: slideShowSeconds

                    x: 20
                    anchors.verticalCenter: parent.verticalCenter
                    visible: slideShowButton.on
                    color: "#4da3ff"
                    font.pixelSize: 12
                    text: slideShowButton.on ? Number(slideShowButton.seconds.toFixed(2)) + "s" : ""
                }

                MouseArea {
                    id: slideShowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: slides.win.showSlideShowMenu(slideShowButton)
                }
            }
        }

        // The arrangement it is viewed in, where that is not Master: beside the
        // Arrangements button, on its left and set against it, so that the button is
        // always in the one place. (ProPresenter says it to the right of the title.)
        Text {
            id: arrangementName

            objectName: "gridArrangement"
            anchors.right: arrangementToggle.left
            anchors.rightMargin: 8
            anchors.verticalCenter: gridTitle.verticalCenter
            width: Math.min(implicitWidth, 200, parent.width * 0.25)
            visible: slides.win.document !== null && slides.win.document.arrangement !== ""
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            color: slides.win.dimTextColor
            font.pixelSize: 12
            text: slides.win.document ? slides.win.document.arrangement : ""
        }

        // The Arrangements button: opens the arrangement editor under this line, and
        // shuts it. ProPresenter's own picture for it; lit while the editor is open.
        // It is there for every presentation, with arrangements or without: it is
        // where one is made. (Before the editor there was a drop-down here, for a
        // presentation that had arrangements, to pick one of them from: the picker is
        // in the editor now, and a row's own menu in the lists picks one as before.)
        Rectangle {
            id: arrangementToggle

            objectName: "arrangementToggle"
            readonly property bool on: gridHeader.arranging

            anchors.right: parent.right
            y: 4
            width: 30
            height: 22
            radius: 6
            visible: slides.win.document !== null && !slides.win.simple && !slides.win.bibleMode
            color: on ? "#4da3ff" : toggleMouse.pressed ? "#50535a" : toggleMouse.containsMouse ? "#45484e" : "#3a3c42"

            ProIcon {
                anchors.centerIn: parent
                name: "Arrangements"
                size: 24
                ink: arrangementToggle.on ? "black" : "#e6e6e6"
            }

            MouseArea {
                id: toggleMouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    slides.win.takeFocus()
                    slides.win.arrangementsShown = !slides.win.arrangementsShown
                }
            }
        }

        // The arrangement editor, under the line. It is made when it is first opened,
        // and kept: nothing of it is made as the app starts unless it was left open.
        Loader {
            id: arrangementBar

            property bool wanted: false

            objectName: "arrangementBarLoader"
            anchors.left: parent.left
            anchors.right: parent.right
            y: 30
            height: item ? item.height : 0
            active: wanted
            visible: gridHeader.arranging
            sourceComponent: ArrangementBar {
                objectName: "arrangementBar"
                win: slides.win
            }
            Component.onCompleted: wanted = gridHeader.arranging
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
        anchors.bottom: gridFooter.top
        anchors.leftMargin: 10
        anchors.rightMargin: 4
        clip: true
        boundsBehavior: Flickable.StopAtBounds
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
            // The slide the show is at; or, in a presentation that is sent to the
            // announcements, the slide the announcements are at
            readonly property bool live: (slides.win.viewingLive && slides.win.cueLive && slides.win.liveIndex === index)
                                         || (slides.win.viewingAnnouncements && Show.announcementLive && Show.announcementIndex === index)
            // The frame is the slide's group colour, and the label is drawn on it.
            readonly property color frame: slides.win.groupColor(modelData)
            readonly property bool lightFrame: 0.299 * frame.r + 0.587 * frame.g + 0.114 * frame.b > 0.6
            // The hotkey that goes to this slide, or ""
            readonly property string hotkey: slides.win.groupKeyAt[index] ?? ""
            // Whether its label has a colour of its own, and is drawn as a tag of it
            readonly property bool tagged: (modelData.labelColor ?? "") !== ""
            readonly property bool lightTag: {
                const c = modelData.labelColor ?? ""
                const part = i => parseInt(c.substring(1 + 2 * i, 3 + 2 * i), 16) / 255
                return c !== "" && 0.299 * part(0) + 0.587 * part(1) + 0.114 * part(2) > 0.6
            }

            width: grid.cellWidth
            height: grid.cellHeight
            // The live slide's glow spills a little over its neighbours.
            z: live ? 1 : 0

            // Only the live slide has a ring, and only it has one made. It is green for a
            // slide that is live on the announcements' layer, which is the green of the
            // button that sends a presentation there, and orange for one that is live on
            // the slides' own (which wins, should one slide be both). Which it is, is asked
            // in the ring, which only the live slide has, and not by every slide.
            //
            // (The user's choice of colour, asked for 2026-10-16. Whether ProPresenter marks
            // the two apart, and how, has not been looked at.)
            Loader {
                anchors.fill: frameRect
                anchors.margins: -5
                active: cell.live
                sourceComponent: LiveRing {
                    objectName: "liveRing"
                    color: slides.win.viewingLive && slides.win.cueLive && slides.win.liveIndex === cell.index ? "#ff8a1f" : "#3ddc68"
                }
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
                    // (Not while Alt is held, when a click would show the slide
                    // without it.)
                    Image {
                        anchors.fill: parent
                        visible: cell.modelData.media !== undefined && !Cursors.altHeld && !lookedMedia.active
                        source: visible ? slides.win.thumbnailUrl(cell.modelData.media.path) : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    // Media set to anything in its Media Inspector, drawn as it is set:
                    // cropped, turned and flipped in its thumbnail, and fitted, blurred and
                    // given its effects over it. Made only for such media.
                    Loader {
                        id: lookedMedia

                        anchors.fill: parent
                        active: cell.modelData.media !== undefined && cell.modelData.media.settings !== undefined && !Cursors.altHeld

                        sourceComponent: MediaPicture {
                            source: slides.win.thumbnailUrl(cell.modelData.media.path, cell.modelData.media.settings)
                            settings: cell.modelData.media.settings
                            shaped: true
                            animated: false
                        }
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
                    //
                    // The top left corner is where a slide says what comes with it: first
                    // the key that goes to it, if its group has a hotkey and this is the
                    // slide the key goes to, then its media and how that plays, then
                    // what else its cue does. Each of these but the key has a menu of
                    // its own, on a right click (see cellMouse).
                    Flow {
                        id: icons

                        x: 4
                        y: 4
                        width: parent.width - 8
                        spacing: 3

                        Loader {
                            active: cell.hotkey !== ""
                            visible: active

                            sourceComponent: ActionIcon {
                                objectName: "hotkeyBadge"
                                color: "#ff8a1f"
                                strength: slides.win.actionIconOpacity

                                Text {
                                    anchors.centerIn: parent
                                    color: "#15161a"
                                    font.pixelSize: 11
                                    font.bold: true
                                    text: cell.hotkey
                                }
                            }
                        }

                        // Its media, as two icons, one over the other: whether it is a
                        // background or a foreground, and under that, for a video, how
                        // it plays on from its end.
                        Column {
                            spacing: 3
                            visible: cell.modelData.mediaName !== ""

                            // Its media: whether it is a background or a foreground,
                            Loader {
                                active: cell.modelData.mediaName !== ""
                                visible: active

                                sourceComponent: ActionIcon {
                                    id: mediaIcon

                                    objectName: "mediaBadge"
                                    width: 21
                                    strength: slides.win.actionIconOpacity

                                    function open() {
                                        slides.win.showMediaBehaviourMenu(cell.index, mediaIcon)
                                    }

                                    MediaBadge {
                                        anchors.centerIn: parent
                                        size: 0.8
                                        color: "transparent"
                                        foreground: cell.modelData.mediaForeground
                                        missing: cell.modelData.media === undefined
                                    }
                                }
                            }

                            // and, for a video, how it plays on from its end: it stops, or
                            // goes round, for good or a number of times or for a while.
                            Loader {
                                active: cell.modelData.mediaName !== "" && cell.modelData.mediaVideo
                                visible: active

                                sourceComponent: ActionIcon {
                                    id: playbackIcon

                                    readonly property string words: slides.win.playbackWords(cell.modelData.mediaPlayback,
                                                                                             cell.modelData.mediaLoopCount,
                                                                                             cell.modelData.mediaLoopSeconds)

                                    objectName: "playbackBadge"
                                    width: words === "" ? 18 : 21 + playbackLabel.implicitWidth
                                    strength: slides.win.actionIconOpacity

                                    function open() {
                                        slides.win.showMediaPlaybackMenu(cell.index, playbackIcon)
                                    }

                                    ActionGlyph {
                                        x: 3
                                        anchors.verticalCenter: parent.verticalCenter
                                        kind: cell.modelData.mediaPlayback === 0 ? "stop" : "loop"
                                    }

                                    Text {
                                        id: playbackLabel

                                        x: 17
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#e3e5e9"
                                        font.pixelSize: 10
                                        text: playbackIcon.words
                                    }
                                }
                            }
                        }

                        // Its Go To Next Timer, for a slide that has one: an arrowhead
                        // against a bar for the next slide, or an arrow going round
                        // for the first ("Loop to Beginning"), and the seconds. A
                        // right click on it has the timer's panel.
                        Loader {
                            active: cell.modelData.goToNext !== undefined
                            visible: active

                            sourceComponent: ActionIcon {
                                id: nextIcon

                                objectName: "goToNextBadge"
                                width: 21 + nextLabel.implicitWidth
                                strength: slides.win.actionIconOpacity

                                function open() {
                                    slides.win.editGoToNext(cell.index)
                                }

                                ActionGlyph {
                                    x: 3
                                    anchors.verticalCenter: parent.verticalCenter
                                    kind: cell.modelData.goToNext.loop ? "loop" : "next"
                                }

                                Text {
                                    id: nextLabel

                                    x: 17
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: "#e3e5e9"
                                    font.pixelSize: 10
                                    text: Number(cell.modelData.goToNext.seconds.toFixed(2)) + "s"
                                }
                            }
                        }

                        // What else its cue does: an icon for each action
                        Repeater {
                            model: cell.modelData.actions

                            delegate: ActionIcon {
                                id: actionIcon

                                required property var modelData

                                objectName: "actionBadge"
                                // (A little wider for a picture that is.)
                                width: actionGlyph.wide ? 22 : 18
                                strength: slides.win.actionIconOpacity

                                function open() {
                                    slides.win.showActionMenu({ slide: cell.index }, modelData, actionIcon, 0, height + 2)
                                }

                                ActionGlyph {
                                    id: actionGlyph

                                    anchors.centerIn: parent
                                    kind: actionIcon.modelData.kind
                                    picture: actionIcon.modelData.picture ?? ""
                                    // Fainter for a kind that is kept and not done here
                                    ink: actionIcon.modelData.done ? "#e3e5e9" : "#8a8d93"
                                }
                            }
                        }
                    }
                }

                // The group's name appears on the first slide of each run of it. The slide's
                // label is after it, as plain words if it has no colour of its own.
                Text {
                    anchors.left: parent.left
                    anchors.right: labelTag.visible ? labelTag.left : parent.right
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
                        + (!cell.tagged && cell.modelData.label !== "" && cell.modelData.label !== cell.modelData.group
                           ? "  " + cell.modelData.label : "")
                }

                // A label that has a colour is a tag of that colour, at the end of the
                // caption. Only a slide that has such a label has one made, and few do: a
                // tag for every slide would be a thing more to make for each of them.
                // (A GUESS at how ProPresenter shows a label's colour: it has not been
                // looked at there.)
                Loader {
                    id: labelTag

                    objectName: "slideLabelTag"
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: thumbnail.bottom
                    anchors.verticalCenterOffset: grid.labelHeight / 2
                    active: cell.tagged
                    visible: active
                    width: active && item ? Math.min(parent.width * 0.6, Math.max(18, item.wordsWidth + 12)) : 0
                    height: 18

                    sourceComponent: Rectangle {
                        readonly property real wordsWidth: tagWords.implicitWidth

                        radius: 3
                        color: cell.modelData.labelColor
                        border.width: 1
                        border.color: "#40000000"

                        Text {
                            id: tagWords

                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            color: cell.lightTag ? "black" : "white"
                            font.pixelSize: 11
                            text: cell.modelData.label
                        }
                    }
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
                        // With Alt held, the slide without the media it brings
                        slides.win.goLive(cell.index, (mouse.modifiers & Qt.AltModifier) !== 0)
                    } else {
                        // A right click on one of the icons in the corner is for that
                        // icon, which has a menu of its own; anywhere else it is for
                        // the slide. (A click with the other button is always the
                        // slide's: the icons are not in the way of showing it.)
                        // (An icon may be inside something that holds two of them.)
                        let hit = icons
                        let icon = null
                        while (hit && !icon) {
                            const at = mapToItem(hit, mouse.x, mouse.y)
                            hit = hit.childAt(at.x, at.y)
                            const inner = hit ? (hit.item ?? hit) : null
                            if (inner && inner.open)
                                icon = inner
                        }
                        if (icon)
                            icon.open()
                        else
                            slides.win.showSlideMenu(cell.index, cell, mouse.x, mouse.y)
                    }
                }
            }

            // Media dropped on the middle of the slide becomes the media the slide
            // triggers. Dropped on its left or right edge, which is to say in the gap
            // beside it, it becomes a slide of its own there.
            //
            // And a timer, a prop, a macro or a stage screen dragged out of the show
            // controls and dropped anywhere on the slide gives the slide the action
            // that goes with it. (One drop area for both: two, one over the other,
            // would each have a say in what a drag of files is taken as, and a drag
            // from the file manager must only ever be taken as a copy.)
            DropArea {
                id: mediaDrop

                // Which of those the drag is over: "onto", "before" or "after"
                property string zone: "onto"
                readonly property bool onto: containsDrag && zone === "onto"
                // How far in from each side the edges reach
                readonly property real edge: Math.max(16, Math.min(30, width * 0.14))

                function ofAction(drag) {
                    return drag.keys.indexOf("action") >= 0
                }

                function follow(drag) {
                    if (ofAction(drag)) {
                        zone = "onto"
                        drag.accepted = true
                        return
                    }
                    zone = drag.x < edge ? "before" : drag.x > width - edge ? "after" : "onto"
                    slides.win.acceptMediaDrag(drag)
                }

                anchors.fill: parent
                keys: ["media", "text/uri-list", "action"]
                enabled: slides.win.takesDrops
                onEntered: (drag) => follow(drag)
                onPositionChanged: (drag) => follow(drag)
                onDropped: (drop) => {
                    if (ofAction(drop))
                        slides.win.dropActionOnSlide(cell.index, cell)
                    else
                        slides.win.dropOnSlides(cell.index, zone, drop)
                }
            }
        }
    }

    // A thin footer under the slides, with a line over it: the transition at its left
    // and the size of the thumbnails at its right
    Item {
        id: gridFooter

        objectName: "gridFooter"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 34

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: "#3a3c42"
        }

        // Bible mode's Save (BiblePanel's keepMenu): the slides it made saved as a
        // presentation, to a playlist, or put into the presentation that was open. At the
        // left, the transition moved over for it, where the maintainer asked for it
        // (2026-10-15). Made only once Bible mode is come to.
        Loader {
            id: bibleSave

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            active: slides.win.bibleMode
            visible: active

            sourceComponent: AppButton {
                id: saveButton

                objectName: "bibleSave"
                height: 24
                font.pixelSize: 12
                text: "Save ▾"
                enabled: slides.win.document !== null && slides.win.documentKey === slides.win.bibleKey
                onClicked: slides.win.bibleKeepMenu(saveButton)
            }
        }

        TransitionControls {
            id: transitionControls

            anchors.left: bibleSave.active ? bibleSave.right : parent.left
            anchors.leftMargin: bibleSave.active ? 12 : 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            win: slides.win
        }

        // Bible mode's ← Verse and Verse →: the passage grown by the verse before it or
        // after it (BiblePanel's step). Midway between the transition and the size of the
        // thumbnails, where the maintainer asked for them (2026-10-15).
        Loader {
            x: transitionControls.x + transitionControls.width + (zoomButtons.x - transitionControls.x - transitionControls.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            active: slides.win.bibleMode
            visible: active

            sourceComponent: Row {
                spacing: 8

                AppButton {
                    objectName: "biblePrevious"
                    height: 24
                    font.pixelSize: 12
                    text: "← Verse"
                    enabled: slides.win.bibleCanStep
                    onClicked: slides.win.bibleStep(-1)
                }

                AppButton {
                    objectName: "bibleNext"
                    height: 24
                    font.pixelSize: 12
                    text: "Verse →"
                    enabled: slides.win.bibleCanStep
                    onClicked: slides.win.bibleStep(1)
                }
            }
        }

        ZoomButtons {
            id: zoomButtons

            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            solid: true
            size: 22
            visible: slides.win.document !== null && slides.win.document.slides.length > 0
            canShrink: slides.win.thumbnailWidth > slides.win.smallestThumbnail
            canGrow: slides.win.thumbnailWidth < slides.win.largestThumbnail && grid.columns > 1
            onShrink: slides.win.zoomThumbnails(-1)
            onGrow: slides.win.zoomThumbnails(1)
        }
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
