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
//
// A click with Ctrl held selects a slide, or lets it go again, and one with Shift held
// selects every slide from the last one selected, and neither takes anything live, as
// the maintainer asked (Main's selectSlide, SlideSelection): a selected slide has a blue
// ring. What is selected is what the Text button's formats go to.
//
// While the timeline is open over the slides, a slide can be carried up onto it, as in
// ProPresenter (Main's beginSlideDrag), once the press has become a drag by the rule of
// every drag here (DragStart, src/dragstart.h): some way moved, the button held a
// moment. Not otherwise: with the timeline shut a slide is never carried. (While it is
// open, a drag that starts on a slide carries the slide and does not scroll the grid;
// the wheel and the scroll bar still do.)
//
// Three ways of showing them, ProPresenter's, picked by the buttons at the right of the
// bar under them, before the size buttons (Main's slideView; the maintainer's ask of
// 2026-11-12): the grid; Easy View, the same grid with each thumbnail showing the slide's
// words alone, in the view's own format, over a plain colour (or over the slide's media,
// where its format says to show that); and Table View, a row for each slide, with the
// grid's cell for it at the left (its thumbnail, its number, group and label) and its
// words and its notes beside that. One GridView does all three, a table being a grid of
// one column, so that everything a slide does here (a click, the rings, its menu, a drop,
// the scroll that follows the live slide) is the same in each. Each view's format is set
// in the panel of the … beside the buttons (ViewFormatPanel; dimmed in the grid, which
// has no format), and the size buttons make
// Table View's thumbnails, and so its rows, larger or smaller. In Easy View the slides
// are not drawn at all, which makes it the quicker to open.
//
// GUESSES at ProPresenter, whose views have been read of and not seen: Table View's group
// shown as the grid has it, on the first slide of each run of a group, and not as a line
// over the run; the notes at the right of the words, in grey; Easy View's words made
// smaller to fit their thumbnail, as large as the format's size on one 250 wide.
Item {
    id: slides

    // The operator window: what this shows is its state, and what happens here is done
    // by calling its functions.
    required property var win
    // What the operator window needs of the grid itself: where it is scrolled to, how
    // wide it is and how many columns it has (for stepping the thumbnail size), and
    // bringing a slide into view (and a live one with the row under it, revealLive).
    property alias contentY: grid.contentY
    readonly property int columns: grid.columns
    readonly property real gridWidth: grid.width
    // Which view the slides are shown in (see the top), and its format
    readonly property bool easy: win.slideView === "easy"
    readonly property bool table: win.slideView === "table"
    readonly property var viewFormat: table ? win.tableFormat : win.easyFormat

    // A press on a slide, which may become its being carried onto the timeline: where it
    // was in the window, when (DragStart.now()), and whether it has become that. Kept here, once,
    // and not in every slide's cell: there is one press at a time. `carrying` stays set
    // after the release, for the click that a MouseArea reports after it, until the next
    // press.
    property point pressedAt
    property real pressedTime: 0
    property bool carrying: false

    // Words cut to `fits` lines, as Table View's Truncate says: at their beginning or in
    // their middle, a line of "…" taking the place of what is left out. (At their end, Qt's
    // own eliding does it, a word at a time.) By the slide's lines: a line long enough to
    // wrap is more than one, and what still does not fit is cut at the end.
    function truncated(words, how, fits) {
        const lines = words.split("\n")
        if (how === "end" || fits < 2 || lines.length <= fits)
            return words
        if (how === "start")
            return ["…"].concat(lines.slice(lines.length - (fits - 1))).join("\n")
        const head = Math.ceil((fits - 1) / 2)
        return lines.slice(0, head).concat(["…"], lines.slice(lines.length - (fits - 1 - head))).join("\n")
    }

    function alignment(align) {
        return align === "center" ? Text.AlignHCenter : align === "right" ? Text.AlignRight : Text.AlignLeft
    }

    // Easy View's thumbnail: the slide's words alone, as the view's format says, over its
    // background, or over the slide's media where that is shown (with an outline then, to
    // be read over it)
    component EasyWords: Rectangle {
        id: easyWords

        // The cell, and its thumbnail, which this is laid over (handed in as it is made)
        property Item cell: null
        property Item box: null
        readonly property string words: cell ? (cell.modelData.plainText ?? "") : ""
        readonly property bool overMedia: cell !== null && format.showMedia && cell.modelData.media !== undefined
        readonly property var format: slides.viewFormat

        objectName: "easyWords"
        width: box ? box.width : 0
        height: box ? box.height : 0
        color: overMedia ? "transparent" : format.background

        Text {
            anchors.fill: parent
            anchors.margins: Math.max(3, parent.width * 0.04)
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: slides.alignment(easyWords.format.align)
            wrapMode: Text.Wrap
            fontSizeMode: Text.Fit
            minimumPixelSize: 6
            font.pixelSize: Math.max(6, easyWords.format.size * easyWords.width / 250)
            font.family: easyWords.format.family !== "" ? easyWords.format.family : Qt.application.font.family
            font.bold: easyWords.format.bold
            font.italic: easyWords.format.italic
            color: easyWords.format.color
            style: easyWords.overMedia ? Text.Outline : Text.Normal
            styleColor: "black"
            elide: Text.ElideRight
            text: easyWords.words
        }
    }

    // Table View's words and notes, beside a slide's thumbnail: the words as the view's
    // format says, cut as its Truncate says; the notes, where the slide has any, at the
    // right, smaller and grey
    component TableWords: Rectangle {
        id: tableWords

        // The cell, and its thumbnail, past which this is laid, from the top of the frame to
        // its foot and on to the row's end (handed in as it is made)
        property Item cell: null
        property Item box: null
        readonly property string words: cell ? (cell.modelData.plainText ?? "") : ""
        readonly property string notes: cell ? (cell.modelData.notesText ?? "") : ""
        readonly property var format: slides.viewFormat
        readonly property real inset: 8

        x: box ? box.width + grid.frameWidth + 8 : 0
        y: -grid.frameWidth
        width: box && cell ? cell.width - box.parent.x - box.parent.width - 14 : 0
        height: box ? box.parent.height : 0
        radius: 4
        color: format.background

        Text {
            id: wordsText

            objectName: "tableWords"
            x: tableWords.inset
            y: 6
            width: (tableWords.notes !== "" ? 0.62 : 1) * (tableWords.width - 2 * tableWords.inset)
            height: tableWords.height - 12
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: slides.alignment(tableWords.format.align)
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            font.pixelSize: tableWords.format.size
            font.family: tableWords.format.family !== "" ? tableWords.format.family : Qt.application.font.family
            font.bold: tableWords.format.bold
            font.italic: tableWords.format.italic
            color: tableWords.format.color
            text: slides.truncated(tableWords.words, tableWords.format.truncate, Math.floor(height / Math.max(1, metrics.lineSpacing)))
        }

        FontMetrics {
            id: metrics

            font: wordsText.font
        }

        Rectangle {
            x: wordsText.x + wordsText.width + tableWords.inset
            y: 8
            width: 1
            height: tableWords.height - 16
            visible: tableWords.notes !== ""
            color: "#3a3c42"
        }

        Text {
            objectName: "tableNotes"
            x: wordsText.x + wordsText.width + 2 * tableWords.inset + 1
            y: 6
            width: tableWords.width - x - tableWords.inset
            height: tableWords.height - 12
            visible: tableWords.notes !== ""
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            color: "#9a9da3"
            font.pixelSize: Math.max(9, tableWords.format.size - 2)
            text: tableWords.notes
        }
    }

    // Easy View's and Table View's words, as a cell's Loader makes them (one Component of
    // each for the grid, rather than a pair in every cell: what each cell holds, in the
    // grid too, is memory for Qt's collector to go through)
    Component {
        id: easyWordsMade

        EasyWords {}
    }

    Component {
        id: tableWordsMade

        TableWords {}
    }

    // A button of the three at the right of the bar, which shows the slides in its view:
    // drawn as ProPresenter's are described (its pack of pictures has none of them), the
    // grid as four squares, Easy View as a large T, Table View as four lines; lit while it
    // is the view
    component ViewButton: Rectangle {
        id: viewButton

        property string view
        readonly property bool on: slides.win.slideView === view
        readonly property color ink: on ? "black" : "#c9ccd1"

        objectName: "slideView:" + view
        width: 26
        height: 22
        radius: 5
        color: on ? "#4da3ff" : viewMouse.pressed ? "#50535a" : viewMouse.containsMouse ? "#45484e" : "transparent"

        Grid {
            anchors.centerIn: parent
            visible: viewButton.view === "grid"
            columns: 2
            spacing: 2

            Repeater {
                model: 4

                Rectangle {
                    width: 5
                    height: 5
                    radius: 1
                    color: viewButton.ink
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: viewButton.view === "easy"
            color: viewButton.ink
            font.pixelSize: 16
            font.bold: true
            text: "T"
        }

        Column {
            anchors.centerIn: parent
            visible: viewButton.view === "table"
            spacing: 2

            Repeater {
                model: 4

                Rectangle {
                    width: 13
                    height: 1.5
                    color: viewButton.ink
                }
            }
        }

        MouseArea {
            id: viewMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: slides.win.setSlideView(viewButton.view)
        }
    }

    function positionViewAtBeginning() {
        grid.positionViewAtBeginning()
    }

    function positionViewAtIndex(index, mode) {
        grid.positionViewAtIndex(index, mode)
    }

    // Brings the slide at `index`, which has just gone live, into view, as the settings
    // say (Settings → Slides, Main's slideScroll): "early", the row under it as well, so
    // that a slide that goes live in the bottom row in view has the grid moved up a row
    // and the slides to come are seen before they are reached (the maintainer's ask of
    // 2026-11-08, the default); or "justInTime", the live slide only, once it is out of
    // view, as the grid always had it before. The row under first and the slide after it,
    // so that in a grid too short for two rows it is the live slide that is seen.
    //
    // Smoothly (`scroller`), and at once: the move starts in the next frame, as the
    // change's own does, and not after the change's transition, as in 0.129 (the
    // maintainer, 2026-11-09: "it should start instantly upon triggering a slide"). In the
    // click itself nothing is laid out: where the grid is to go is worked out from its rows
    // (containing), so the click's work is what it was (the benchmark, 2026-11-08: bringing
    // the slide into view by positionViewAtIndex in the click was 1.3 ms more work in a
    // click on the Shapes). The rows the move brings are made in the frames after, as it
    // goes, while the change's transition runs: what that costs is measured by the
    // benchmark's lines for slides shown one after another as fast as the output shows them.
    //
    // A jump, to a slide out of view that is not the one beside the slide live before (a
    // group's hotkey, a typed number, the API), is brought in a tenth of a second, whatever
    // the setting ("almost instant", the maintainer, 2026-11-09). A step, by Next or
    // Previous, takes the time set.
    function revealLive(index) {
        if (index < 0 || index >= grid.count)
            return
        // (From where the grid is going, if it is on its way: where it is now is behind that)
        const at = scroller.moving() ? scroller.to : grid.contentY
        const stepped = Math.abs(index - lastLive) === 1
        const jump = Math.abs(index - lastLive) > 1 && Math.abs(slides.containing(index, at) - at) > 0.5
        // How long a row of slides takes to come, at the pace of this step and the last
        const now = clock.now()
        const rowEvery = stepped ? (now - lastLiveAt) * grid.columns : 0
        lastLive = index
        lastLiveAt = now
        let y = at
        if (slides.win.slideScroll !== "justInTime")
            y = slides.containing(Math.min(index + grid.columns, grid.count - 1), y)
        scroller.go(slides.containing(index, y), jump, rowEvery)
    }

    // The slide revealLive last brought into view, for telling a step from a jump (none in
    // a presentation just opened or read again, but the one showOpened put in view), and
    // when
    property int lastLive: -1
    property double lastLiveAt: 0

    // Where the grid is put for a presentation just opened: at its beginning; or, where the
    // show is at a slide of it (`live`, on the screens or on the announcements, cleared or
    // not: see Main.showAtViewed; -1 for none), where revealLive would have brought that
    // slide from there, so that coming back to the presentation that is live shows its live
    // slide (the maintainer's ask of 2026-11-09: "coming back to an ACTIVE presentation -
    // should scroll to show the active slide").
    //
    // Put there at once, and not scrolled: the grid has only just been filled, and a move
    // down from its top would be through rows nobody had seen, each of them to be made on
    // the way. The rows at its top have been made by then all the same: the GridView, given
    // its model, goes to its beginning and makes the rows in view there at once. With the
    // live slide at the end of a presentation of the Demo's, in the scripted tests, that
    // made opening it 73 ms to the frame, against 45 with the live slide at its top. (A
    // margin of less than nothing at the GridView's end while the model was set, for it to
    // make none of those, was tried, and was slower still, 101 ms: it seems to make every
    // row from the top down to where it is put, when it has none in view to go on from.)
    // A GUESS that ProPresenter shows the live slide; not looked at there.
    function showOpened(live) {
        scroller.stop()
        grid.positionViewAtBeginning()
        if (live < 0 || live >= grid.count)
            return
        let y = grid.contentY
        if (slides.win.slideScroll !== "justInTime")
            y = slides.containing(Math.min(live + grid.columns, grid.count - 1), y)
        grid.contentY = slides.containing(live, y)
        // (So that Next from it is a step, which takes the time set, and not a jump)
        lastLive = live
    }

    // Where the grid is to be scrolled to, from `y`, for the cell at `index` to be wholly
    // in view: as far as it must go and no further, as positionViewAtIndex's Contain has
    // it, but worked out from the grid's rows, so that nothing is laid out to know it.
    function containing(index, y) {
        const top = grid.originY + Math.floor(index / grid.columns) * grid.cellHeight
        if (top + grid.cellHeight > y + grid.height)
            y = top + grid.cellHeight - grid.height
        if (top < y)
            y = top
        return Math.max(grid.originY, Math.min(y, grid.originY + Math.max(0, grid.contentHeight - grid.height)))
    }

    // A scroll of the grid to where it is to be, smoothly: over the time the settings give
    // it (Settings → Slides, Main's slideScrollDuration), easing in and out of the move or
    // at one speed throughout as they say (slideScrollEased), moved on at each frame of the
    // operator window by a FrameClock, as anything that moves is here, and not by an
    // animation of Qt's (see src/frameclock.h). The
    // rows that come into view have their thumbnails made as they come, a row at a time,
    // rather than all in one frame. A scroll by hand (a drag, a flick, the wheel) stops
    // it where it is. One of more than three screens is made at once, there being
    // nothing on the way worth passing through, and every row of it to be made.
    //
    // A new place given while it is still moving to the last (Next held down and its key
    // repeating, a slide live every 30 ms or so and a row every few of them) is gone on to
    // from where the grid is and at the speed it is going, so that the grid keeps moving
    // and does not stop and start again at each row. Eased, that is a cubic from that speed
    // to rest at the new place. And in no longer than half as much again as the time since
    // the last place was given (but three frames at least), the time set or not, so that it
    // keeps up with them: a simulation of a key held down (four or six slides a row, at 30
    // and 40 ms a slide, 0.4 and 0.8 s set) had the grid go at the rows' own speed, its
    // speed from frame to frame changing by a pixel or so, and the live slide, once the
    // grid was under way, never more than about a quarter of it below the view. Each move
    // taking the time set, the grid fell further behind at every row; and gone on to from
    // where it had last been put, and not from where the move had it at the moment the
    // place was given, it slowed for a frame at every row. A move from standing still, too,
    // takes no longer than half as much again as a row of the slides takes to come, at the
    // pace of the step that brought it and the one before: at the 0.4 s set, a key held
    // down had the live slide all but out of view for three frames at the start, while the
    // grid was easing into its first move and the slides were already a row on.
    Item {
        id: scroller

        property real from: 0
        property real to: 0
        property double startedAt: 0
        // How long this move takes, in milliseconds, and the speed it set off at, in pixels
        // a millisecond: 0 for one from standing still, which is eased in as well as out
        property double span: 0
        property double speed: 0
        // When the last place to go to was given
        property double aimedAt: 0
        // Where it last put the grid: the grid somewhere else has been scrolled by hand
        // (the wheel moves it without dragging or flicking it)
        property real put: 0
        readonly property int duration: Math.round(Math.max(0.1, slides.win.slideScrollDuration) * 1000)
        // How long a jump takes (see revealLive)
        readonly property int jumpDuration: 100

        // Whether it is moving the grid, and the grid has not been scrolled by hand since
        function moving() {
            return clock.running && !grid.dragging && !grid.flicking && Math.abs(grid.contentY - put) <= 1
        }

        // Where the move has the grid at `now`, and how fast it is going there then
        function placeAt(now) {
            const t = Math.min(1, Math.max(0, (now - startedAt) / span))
            const d = to - from
            if (!slides.win.slideScrollEased)
                return from + d * t
            // Eased in and out (a cubic: slow at each end, quickest in the middle) from
            // standing still; from a speed, the cubic that sets off at it and comes to rest
            if (speed === 0)
                return from + d * (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2)
            return from + (t * t * t - 2 * t * t + t) * span * speed + (3 * t * t - 2 * t * t * t) * d
        }

        function speedAt(now) {
            const t = Math.min(1, Math.max(0, (now - startedAt) / span))
            const d = to - from
            if (!slides.win.slideScrollEased)
                return d / span
            if (speed === 0)
                return d / span * (t < 0.5 ? 12 * t * t : 3 * Math.pow(-2 * t + 2, 2))
            return ((3 * t * t - 4 * t + 1) * span * speed + (6 * t - 6 * t * t) * d) / span
        }

        // To `target`: quickly, for a jump; at the pace of slides coming a row every `rowEvery`
        // milliseconds, if that is quicker than the time set (0 for no pace)
        function go(target, quick, rowEvery) {
            const now = clock.now()
            const going = moving()
            // (On its way there already: a slide live in the same row)
            if (going && Math.abs(target - to) < 0.5)
                return
            const here = going ? placeAt(now) : grid.contentY
            const distance = Math.abs(target - here)
            if (distance < 0.5) {
                stop()
                return
            }
            if (distance > grid.height * 3) {
                stop()
                grid.contentY = target
                return
            }
            let length = quick ? Math.min(duration, jumpDuration) : duration
            if (rowEvery > 0)
                length = Math.min(length, Math.max(50, 1.5 * rowEvery))
            let setOff = 0
            if (going) {
                length = Math.min(length, Math.max(50, 1.5 * (now - aimedAt)))
                if (slides.win.slideScrollEased) {
                    setOff = speedAt(now)
                    // (Never past the place and back: the cubic from a speed to rest does not
                    // turn if it is given no longer than three times the distance at that speed)
                    if (setOff * (target - here) > 0)
                        length = Math.min(length, 3 * distance / Math.abs(setOff))
                }
            }
            from = here
            to = target
            startedAt = now
            aimedAt = now
            span = Math.max(1, length)
            speed = setOff
            put = grid.contentY
            clock.running = true
            backstop.restart()
        }

        function step(now) {
            if (grid.dragging || grid.flicking || Math.abs(grid.contentY - put) > 1) {
                stop()
                return
            }
            put = placeAt(now)
            grid.contentY = put
            if (now - startedAt >= span)
                stop()
        }

        function stop() {
            clock.running = false
            backstop.stop()
        }

        FrameClock {
            id: clock

            onTicked: (now) => scroller.step(now)
        }

        // A scroll the window makes no frames for (minimised, say) ends where it was going
        Timer {
            id: backstop

            interval: scroller.duration + 1000
            onTriggered: {
                scroller.stop()
                grid.contentY = scroller.to
            }
        }

    }

    // Says when the pointer is over this pane, for Ctrl+= and Ctrl+- to make its thumbnails
    // larger or smaller (see Main's thumbnailsUnderPointer)
    HoverHandler {
        onHoveredChanged: {
            if (hovered)
                slides.win.thumbnailsUnderPointer = "slides"
            else if (slides.win.thumbnailsUnderPointer === "slides")
                slides.win.thumbnailsUnderPointer = ""
        }
    }

    // A selected slide's ring, where the live slide's is (made only for a selected slide
    // that is not live). The blue is a GUESS at ProPresenter's, which marks its selected
    // slides with the system's colour for a selection.
    Component {
        id: selectedRing

        Rectangle {
            objectName: "selectedRing"
            color: "transparent"
            radius: 8
            border.width: 3
            border.color: "#3d8bff"
        }
    }

    // A thin header over the slides. In its middle is the name of the presentation
    // being viewed. At its left is the key a song's chords are shown in, for a song
    // that has chords, and after that whatever there is to tell the user. At its right
    // are the button that opens the arrangement editor under this line
    // (ArrangementBar.qml), as ProPresenter has its Arrangements button in the
    // presentation's header, with the arrangement being viewed named beside it if that
    // is not Master; and after it the one that opens the timeline (TimelinePanel.qml),
    // as ProPresenter has its Timeline button there. The line is as high as it was
    // without them, and the editor's two rows and the timeline are added to it while
    // they are open.
    Item {
        id: gridHeader

        // Whether the arrangement editor is open under the line. Not in Simple View,
        // which is for running a show and has nothing to set things up with: the
        // button is not there either (it would be under that view's own button, where
        // the pane at the right is kept).
        // (Not for Bible mode's slides, which have no arrangements, destination or Slide
        // Show of their own to set: the maintainer, 2026-10-15)
        readonly property bool arranging: slides.win.arrangementsShown && slides.win.document !== null && !slides.win.simple && !slides.win.bibleMode
        // Whether the timeline is open under the line (and under the arrangement editor,
        // if that is open too): by the same rule (TimelinePanel.qml)
        readonly property bool timelining: slides.win.timelineShown && slides.win.document !== null && !slides.win.simple && !slides.win.bibleMode

        onArrangingChanged: {
            if (arranging)
                arrangementBar.wanted = true
        }
        onTimeliningChanged: {
            if (timelining)
                timelinePanel.wanted = true
        }

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        visible: slides.win.document !== null || slides.win.notice !== ""
        height: visible ? 30 + (arranging ? arrangementBar.height : 0) + (timelining ? timelinePanel.height + 4 : 0) : 6

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

            anchors.right: timelineToggle.left
            anchors.rightMargin: 4
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

        // The Timeline button, at the right of the Arrangements button, where
        // ProPresenter has its own: opens the presentation's timeline under this line
        // (TimelinePanel.qml), and shuts it; lit while it is open, as the Arrangements
        // button is. ProPresenter's pack has no picture of it, so its picture is drawn
        // here after ProPresenter's: two cues, and the playhead between them.
        Rectangle {
            id: timelineToggle

            objectName: "timelineToggle"
            readonly property bool on: gridHeader.timelining
            readonly property color ink: on ? "black" : "#e6e6e6"

            anchors.right: parent.right
            y: 4
            width: 30
            height: 22
            radius: 6
            visible: slides.win.document !== null && !slides.win.simple && !slides.win.bibleMode
            color: on ? "#4da3ff" : timelineMouse.pressed ? "#50535a" : timelineMouse.containsMouse ? "#45484e" : "#3a3c42"

            Rectangle {
                x: 6
                y: 9
                width: 6
                height: 7
                radius: 1
                color: timelineToggle.ink
            }

            Rectangle {
                x: 18
                y: 9
                width: 6
                height: 7
                radius: 1
                color: timelineToggle.ink
            }

            Rectangle {
                x: 14
                y: 5
                width: 2
                height: 14
                color: timelineToggle.ink
            }

            Rectangle {
                x: 12
                y: 4
                width: 6
                height: 3
                radius: 1
                color: timelineToggle.ink
            }

            MouseArea {
                id: timelineMouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    slides.win.takeFocus()
                    slides.win.timelineShown = !slides.win.timelineShown
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

        // The timeline, under the line and the arrangement editor. Made when it is first
        // opened, and kept, as the editor is.
        Loader {
            id: timelinePanel

            property bool wanted: false

            objectName: "timelinePanelLoader"
            anchors.left: parent.left
            anchors.right: parent.right
            y: 30 + (gridHeader.arranging ? arrangementBar.height : 0) + 2
            height: item ? item.height : 0
            active: wanted
            visible: gridHeader.timelining
            sourceComponent: TimelinePanel {
                objectName: "timelinePanel"
                win: slides.win
            }
            Component.onCompleted: wanted = gridHeader.timelining
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

        readonly property int columns: slides.table ? 1 : Math.max(1, Math.floor(width / slides.win.thumbnailWidth))
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
        // (A row of Table View is as high as the cell at its left, of the thumbnail's width)
        cellHeight: ((slides.table ? slides.win.tableThumbnailWidth : cellWidth - 12) - 2 * frameWidth) * 9 / 16 + frameWidth + labelHeight + 12
        // One row beyond what is in view is kept ready, so that scrolling does not wait for
        // thumbnails to be built, and no more: each is a slide drawn afresh.
        cacheBuffer: Math.max(0, cellHeight)
        model: slides.win.document ? slides.win.document.slides : []
        onModelChanged: slides.lastLive = -1

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
            // Selected, by a click with Ctrl or Shift held (see SlideSelection), for the Text
            // button's formats to go to
            readonly property bool selected: slides.win.selectedSlideIds.length > 0 && slides.win.document !== null
                                             && slides.win.selectedIn === slides.win.document.path
                                             && slides.win.selectedSlideIds.includes(modelData.id)
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
            //
            // A selected slide that is not live has a blue ring in the same place, made by
            // the same Loader, and only for it (selectedRing, below the grid). Live, it keeps
            // its own ring, being selected all the same.
            // (Round the whole cell, which is a row in Table View: in the grid it is where
            // the frame is, five pixels out)
            Loader {
                anchors.fill: parent
                anchors.margins: 1
                active: cell.live || cell.selected
                // (Each by its own case: see the trap of one Loader for two kinds)
                sourceComponent: cell.live ? liveRing : cell.selected ? selectedRing : null

                Component {
                    id: liveRing

                    LiveRing {
                        objectName: "liveRing"
                        color: slides.win.viewingLive && slides.win.cueLive && slides.win.liveIndex === cell.index ? "#ff8a1f" : "#3ddc68"
                    }
                }
            }

            Rectangle {
                id: frameRect

                objectName: "slideFrame:" + cell.index
                anchors.fill: parent
                anchors.margins: 6
                // (In Table View the row's left, as wide as its thumbnail)
                anchors.rightMargin: slides.table ? parent.width - 6 - slides.win.tableThumbnailWidth : 6
                color: cell.frame
                radius: 4
                // A disabled slide is dimmed (A GUESS at how ProPresenter marks one): it is
                // passed over by Next and Previous, and shown when it is clicked.
                opacity: cell.modelData.disabled === true ? 0.38 : 1

                Rectangle {
                    id: thumbnail

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: grid.frameWidth
                    height: width * 9 / 16
                    color: "black"

                    // The media the cue triggers alongside the slide: what the slide is drawn
                    // over, which its fills that are a blur or an invert of what is behind
                    // them take (Slide's `backdrop`, TextFills). (Each part shown by its own
                    // rule, and the two together by none, which would be a binding more for
                    // every thumbnail, for nothing.)
                    Item {
                        id: cellMedia

                        anchors.fill: parent

                        // (Not while Alt is held, when a click would show the slide
                        // without it.)
                        Image {
                            anchors.fill: parent
                            visible: cell.modelData.media !== undefined && !Cursors.altHeld && !lookedMedia.active
                                     && (!slides.easy || slides.viewFormat.showMedia)
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
                                    && (!slides.easy || slides.viewFormat.showMedia)

                            sourceComponent: MediaPicture {
                                source: slides.win.thumbnailUrl(cell.modelData.media.path, cell.modelData.media.settings)
                                settings: cell.modelData.media.settings
                                shaped: true
                                animated: false
                            }
                        }
                    }

                    // (Not in Easy View, where nothing of it is made: its words take its place)
                    Slide {
                        anchors.fill: parent
                        slide: slides.easy ? null : cell.modelData
                        effects: false
                        backdrop: cellMedia
                    }

                    // Easy View's words, in the slide's place; or Table View's, beside the
                    // thumbnail to the row's end (out past the thumbnail, which does not
                    // clip: in the frame, a disabled slide's words are dimmed with it). One
                    // Loader for the two, made only in those views, each chosen only for its
                    // own, the cell handed to what it makes as it is made (so that nothing
                    // of it is a binding in the grid): a Loader is a thing more for every
                    // slide, in the grid too.
                    Loader {
                        active: slides.easy || slides.table
                        sourceComponent: slides.table ? tableWordsMade : slides.easy ? easyWordsMade : null
                        onLoaded: {
                            item.cell = cell
                            item.box = thumbnail
                        }
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
                            // And under that, for a background video, whether it is heard: a
                            // speaker struck through while it is muted, which a background is
                            // unless its sound is turned on (this app's own, the maintainer's
                            // ask of 2026-11-08: see src/backgroundsoundfile.h), and sending
                            // out its sound while it is heard. (In the Loader the video's
                            // icon is in, so that a slide without a video is made with
                            // nothing more for it.)
                            Loader {
                                active: cell.modelData.mediaName !== "" && cell.modelData.mediaVideo
                                visible: active

                                sourceComponent: Column {
                                    spacing: 3

                                    ActionIcon {
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

                                    ActionIcon {
                                        id: soundIcon

                                        readonly property bool heard: cell.modelData.mediaSound === true

                                        objectName: "soundBadge"
                                        visible: !cell.modelData.mediaForeground
                                        strength: slides.win.actionIconOpacity

                                        // A plain click on this one turns the sound on or off,
                                        // as the maintainer asked, where a plain click on the
                                        // others is a click on the slide (see cellMouse)
                                        function press() {
                                            slides.win.setBackgroundSound(cell.index, !heard)
                                        }

                                        function open() {
                                            slides.win.showBackgroundSoundMenu(cell.index, soundIcon)
                                        }

                                        SpeakerGlyph {
                                            anchors.centerIn: parent
                                            muted: !soundIcon.heard
                                        }
                                    }
                                }
                            }
                        }

                        // Its Go To Next Timer, for a slide that has one: an arrowhead
                        // against a bar for the next slide, or an arrow going round
                        // for the first ("Loop to Beginning"), and the seconds. And its
                        // disable date, this app's own (see DisableDates), for a slide
                        // that has one: a calendar's page and the day. A right click on
                        // either has its panel. (One Loader for the two, which few slides
                        // have: a Loader is a thing more made for every slide, and one of
                        // its own for the date cost opening the benchmark's Shapes some
                        // 8 ms, 2026-10-23.)
                        Loader {
                            active: cell.modelData.goToNext !== undefined || slides.win.disableDates[cell.modelData.id] !== undefined
                            visible: active

                            sourceComponent: Row {
                                readonly property double disableAt: slides.win.disableDates[cell.modelData.id] ?? 0

                                spacing: 3

                                ActionIcon {
                                    id: nextIcon

                                    objectName: "goToNextBadge"
                                    visible: cell.modelData.goToNext !== undefined
                                    width: 21 + nextLabel.implicitWidth
                                    strength: slides.win.actionIconOpacity

                                    function open() {
                                        slides.win.editGoToNext(cell.index)
                                    }

                                    ActionGlyph {
                                        x: 3
                                        anchors.verticalCenter: parent.verticalCenter
                                        kind: cell.modelData.goToNext?.loop ? "loop" : "next"
                                    }

                                    Text {
                                        id: nextLabel

                                        x: 17
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#e3e5e9"
                                        font.pixelSize: 10
                                        text: cell.modelData.goToNext !== undefined ? Number(cell.modelData.goToNext.seconds.toFixed(2)) + "s" : ""
                                    }
                                }

                                ActionIcon {
                                    objectName: "disableDateBadge"
                                    visible: parent.disableAt > 0
                                    width: 25 + dateLabel.implicitWidth
                                    strength: slides.win.actionIconOpacity

                                    function open() {
                                        slides.win.editDisableDate(cell.index)
                                    }

                                    CalendarGlyph {
                                        x: 3
                                        anchors.verticalCenter: parent.verticalCenter
                                        ink: "#e3e5e9"
                                    }

                                    Text {
                                        id: dateLabel

                                        x: 23
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#e3e5e9"
                                        font.pixelSize: 10
                                        text: parent.parent.disableAt > 0 ? CalendarEvents.dateText(parent.parent.disableAt) : ""
                                    }
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
            // gap before this slide or after it (over it or under it, in Table View). The
            // last slide also shows it for a drag that is over the grid but past every slide.
            Rectangle {
                readonly property bool atEnd: endDrop.containsDrag && cell.index === grid.count - 1
                readonly property bool after: mediaDrop.zone === "after" || atEnd

                x: slides.table ? 6 : after ? parent.width - 1.5 : -1.5
                y: slides.table ? (after ? parent.height - 1.5 : -1.5) : 6
                width: slides.table ? parent.width - 12 : 3
                height: slides.table ? 3 : parent.height - 12
                visible: (mediaDrop.containsDrag && !mediaDrop.onto) || atEnd
                color: "white"
            }

            MouseArea {
                id: cellMouse

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                // While the timeline is open a slide can be carried onto it (see the top):
                // the grid would otherwise take the drag for itself, to scroll by.
                preventStealing: gridHeader.timelining

                // Whether the press has become a drag carrying this slide, at `at`, a
                // point of the window: from the first move that makes it one, on.
                function carry(at) {
                    if (!slides.carrying && gridHeader.timelining && DragStart.begins(slides.pressedAt, at, slides.pressedTime)) {
                        slides.carrying = true
                        slides.win.beginSlideDrag(cell.index, at)
                    }
                    if (slides.carrying)
                        slides.win.moveSlideDrag(at)
                    return slides.carrying
                }

                onPressed: (mouse) => {
                    if (mouse.button !== Qt.LeftButton)
                        return
                    slides.pressedAt = mapToItem(null, mouse.x, mouse.y)
                    slides.pressedTime = DragStart.now()
                    slides.carrying = false
                }
                onPositionChanged: (mouse) => {
                    if (gridHeader.timelining && (pressedButtons & Qt.LeftButton))
                        carry(mapToItem(null, mouse.x, mouse.y))
                }
                // Let go: dropped where it is, if it was carried; or, if it is let go
                // after the moment and the distance without having been moved since (a
                // quick move, then still), carried there and dropped all at once.
                onReleased: (mouse) => {
                    if (mouse.button === Qt.LeftButton && gridHeader.timelining && carry(mapToItem(null, mouse.x, mouse.y)))
                        slides.win.endSlideDrag(true)
                }
                onCanceled: {
                    if (slides.carrying)
                        slides.win.endSlideDrag(false)
                    slides.carrying = false
                }
                // The icon in the corner under the pointer, if there is one. (An icon may
                // be inside something that holds two of them, or more.)
                function iconAt(x, y) {
                    let hit = icons
                    let icon = null
                    while (hit && !icon) {
                        const at = mapToItem(hit, x, y)
                        hit = hit.childAt(at.x, at.y)
                        const inner = hit ? (hit.item ?? hit) : null
                        if (inner && inner.open)
                            icon = inner
                    }
                    return icon
                }

                onClicked: (mouse) => {
                    // (A slide that was carried, and let go on itself, is not clicked.)
                    if (mouse.button === Qt.LeftButton && slides.carrying)
                        return
                    if (mouse.button === Qt.LeftButton) {
                        // The keyboard comes back to the show, from whatever box of the
                        // window was being typed in.
                        slides.win.takeFocus()
                        // A plain click on an icon that does something of its own when
                        // clicked (a background's sound: see soundIcon) does that, and
                        // does not take the slide live. Only the corner is looked in, and
                        // only for a click with no key held.
                        const corner = mapToItem(icons, mouse.x, mouse.y)
                        const icon = mouse.modifiers === Qt.NoModifier && icons.contains(corner) ? iconAt(mouse.x, mouse.y) : null
                        if (icon && icon.press)
                            icon.press()
                        // With Ctrl held the slide is selected, or let go of; with Shift,
                        // every slide from the last one selected; neither takes it live
                        // (see SlideSelection)
                        else if (mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))
                            slides.win.selectSlide(cell.index, (mouse.modifiers & Qt.ShiftModifier) !== 0)
                        // With Alt held, the slide without the media it brings
                        else
                            slides.win.clickSlide(cell.index, (mouse.modifiers & Qt.AltModifier) !== 0)
                    } else {
                        // A right click on one of the icons in the corner is for that
                        // icon, which has a menu of its own; anywhere else it is for
                        // the slide. (A click with the other button is always the
                        // slide's: the icons are not in the way of showing it.)
                        const icon = iconAt(mouse.x, mouse.y)
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
            // controls, or a playlist of either bin or a message dragged out of its
            // list, and dropped anywhere on the slide gives the slide the action that
            // goes with it. (One drop area for both: two, one over the other,
            // would each have a say in what a drag of files is taken as, and a drag
            // from the file manager must only ever be taken as a copy.)
            DropArea {
                id: mediaDrop

                // Which of those the drag is over: "onto", "before" or "after"
                property string zone: "onto"
                readonly property bool onto: containsDrag && zone === "onto"
                // How far in from each side the edges reach (from the top and the bottom, in
                // Table View, where the slides are one over another)
                readonly property real edge: slides.table ? Math.max(10, Math.min(24, height * 0.2)) : Math.max(16, Math.min(30, width * 0.14))

                function ofAction(drag) {
                    return drag.keys.some(key => slides.win.actionDragKeys.includes(key))
                }

                function follow(drag) {
                    if (ofAction(drag)) {
                        zone = "onto"
                        // (A folder of playlists is dragged as a playlist is, and is none.)
                        drag.accepted = slides.win.draggedAction(drag.source) !== null
                        return
                    }
                    const along = slides.table ? drag.y : drag.x
                    const length = slides.table ? height : width
                    zone = along < edge ? "before" : along > length - edge ? "after" : "onto"
                    slides.win.acceptMediaDrag(drag)
                }

                anchors.fill: parent
                // (Media, files, and the drags that stand for actions, as actionDragKeys in
                // Main.qml lists them: a plain list, made with nothing for every slide.)
                keys: ["media", "text/uri-list", "action", "mediaPlaylist", "audioPlaylist", "message"]
                enabled: slides.win.takesDrops
                onEntered: (drag) => follow(drag)
                onPositionChanged: (drag) => follow(drag)
                onDropped: (drop) => {
                    if (ofAction(drop))
                        slides.win.dropActionOnSlide(cell.index, cell, drop.source)
                    else
                        slides.win.dropOnSlides(cell.index, zone, drop)
                }
            }
        }
    }

    // A thin footer under the slides, with a line over it: the transition at its left,
    // and the views and the size of the thumbnails at its right
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

        // The media transition, beside the slides' one, while the media bin, where it is
        // otherwise, is not shown: hidden (Ctrl+V), or in Simple View. Made only then.
        Loader {
            id: mediaControls

            anchors.left: transitionControls.right
            anchors.leftMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            active: !slides.win.mediaBinVisible || slides.win.simple
            visible: active

            sourceComponent: TransitionControls {
                media: true
                win: slides.win
            }
        }

        // Bible mode's ← Verse and Verse →: the passage grown by the verse before it or
        // after it (BiblePanel's step). Midway between the transition and the buttons at
        // the right (the views and the size of the thumbnails), where the maintainer asked
        // for them (2026-10-15).
        Loader {
            readonly property Item before: mediaControls.active ? mediaControls : transitionControls

            x: before.x + before.width + (viewButtons.x - before.x - before.width - width) / 2
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

        // The three views (see the top), and the … of the view's format
        // (ViewFormatPanel), at the left of the size buttons, where ProPresenter has its,
        // the … after the three
        Row {
            id: viewButtons

            objectName: "slideViewButtons"
            anchors.right: zoomButtons.left
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            spacing: 2

            ViewButton {
                view: "grid"
            }

            ViewButton {
                view: "easy"
            }

            ViewButton {
                view: "table"
            }

            // ProPresenter's circle with three dots in it. Always there, so that nothing
            // moves or comes and goes as the view changes (the maintainer's wish), and dimmed
            // and unclickable in the grid, which has no format here, as the size buttons
            // beside it are dimmed at their ends
            Rectangle {
                id: viewFormatButton

                readonly property bool available: slides.easy || slides.table

                objectName: "viewFormatButton"
                opacity: available ? 1 : 0.3
                width: 26
                height: 22
                radius: 5
                color: slides.win.viewFormatOpen ? "#50535a" : formatMouse.pressed ? "#50535a" : formatMouse.containsMouse ? "#45484e" : "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 17
                    height: 17
                    radius: 8.5
                    color: "transparent"
                    border.width: 1.5
                    border.color: "#c9ccd1"

                    Row {
                        anchors.centerIn: parent
                        spacing: 2

                        Repeater {
                            model: 3

                            Rectangle {
                                width: 2.5
                                height: 2.5
                                radius: 1.25
                                color: "#c9ccd1"
                            }
                        }
                    }
                }

                MouseArea {
                    id: formatMouse

                    anchors.fill: parent
                    enabled: viewFormatButton.available
                    hoverEnabled: true
                    onClicked: slides.win.viewFormatOpen ? slides.win.closeViewFormat() : slides.win.openViewFormat(viewFormatButton)
                }
            }
        }

        ZoomButtons {
            id: zoomButtons

            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            size: 22
            visible: slides.win.document !== null && slides.win.document.slides.length > 0
            // (In Table View, its thumbnails' width, a row being the whole width)
            canShrink: slides.table ? slides.win.tableThumbnailWidth > slides.win.smallestTableThumbnail
                                    : slides.win.thumbnailWidth > slides.win.smallestThumbnail
            canGrow: slides.table ? slides.win.tableThumbnailWidth < slides.win.largestTableThumbnail
                                  : slides.win.thumbnailWidth < slides.win.largestThumbnail && grid.columns > 1
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
