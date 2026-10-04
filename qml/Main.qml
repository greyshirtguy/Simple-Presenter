import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtMultimedia
import QtQuick.Window
import SimplePresenterApp

// The operator window. A toolbar across the top stands in for the title bar; below it
// are libraries and their presentations on the left, the selected presentation's slides
// as a grid of thumbnails in the middle, the preview and clear buttons on the right, and
// the media bin along the bottom. Owns the output and stage windows.
Window {
    id: win

    // Set from main.cpp
    required property Catalog catalog
    property int outputScreen: -1
    // Whether to restore the last session's selections and layout, and save this one's
    property bool remember: true
    // Set once the last session has been restored; nothing is saved before then.
    property bool restored: false

    // Paths of the library and media folder being browsed
    property string libraryPath: ""
    property string mediaFolder: ""
    // What is in them, re-read by refreshLists() whenever either path or the disk changes
    property var documents: []
    property var mediaFiles: []

    // The presentation shown in the grid: { name, path, slides, error } from Catalog.open()
    property var document: null
    // The presentation and slide on the slide layer. They stay set while the layer is
    // cleared, so stepping carries on from where it was.
    property var liveDocument: null
    property int liveIndex: -1
    property bool cleared: true
    // What is on the media layer: { name, path, source, video }, or null
    property var liveMedia: null

    readonly property var transitions: [
        { name: "Cut", shader: "" },
        { name: "Dissolve", shader: "qrc:/shaders/dissolve.frag.qsb" },
        { name: "Ripple", shader: "qrc:/shaders/ripple.frag.qsb" }
    ]
    property int transitionIndex: 1
    // Seconds
    property real transitionDuration: 0.6
    property bool mediaBinVisible: true
    property bool outputEnabled: true
    property bool stageEnabled: true
    property bool settingsOpen: false
    // Read by main.cpp at the next launch; see the Windows section of the settings screen.
    property bool useX11: false
    // An error to show above the slides, or ""
    property string notice: ""
    // [{ name, color }], edited on the settings screen
    property var groups: [
        { name: "Intro", color: "#fdd835" },
        { name: "Verse", color: "#1e88e5" },
        { name: "Pre-Chorus", color: "#00acc1" },
        { name: "Chorus", color: "#d81b60" },
        { name: "Bridge", color: "#8e24aa" },
        { name: "Tag", color: "#fb8c00" },
        { name: "Interlude", color: "#43a047" },
        { name: "Instrumental", color: "#43a047" },
        { name: "Ending", color: "#6d4c41" },
        { name: "Outro", color: "#6d4c41" },
        { name: "Background", color: "#757575" }
    ]
    // Pane sizes, changed by dragging the dividers between them
    property real sidebarWidth: 260
    property real sidePanelWidth: 360
    property real mediaBinHeight: 250

    // The slide on the slide layer and the one after it, or null
    readonly property var liveSlide: !cleared && liveDocument ? liveDocument.slides[liveIndex] : null
    readonly property var nextSlide: liveDocument && liveIndex + 1 < liveDocument.slides.length
                                     ? liveDocument.slides[liveIndex + 1] : null
    readonly property string stageCurrentText: liveSlide ? liveSlide.plainText : ""
    readonly property string stageNextText: nextSlide ? nextSlide.plainText : ""

    readonly property bool viewingLive: document !== null && liveDocument !== null
                                        && document.path === liveDocument.path
                                        && document.arrangement === liveDocument.arrangement

    readonly property color panelColor: "#1e1f22"
    readonly property color surfaceColor: "#2b2d31"
    readonly property color textColor: "#e6e6e6"
    readonly property color dimTextColor: "#9a9da3"
    readonly property color accentColor: "#ff8a1f"
    // Title colours of the three browsing areas
    readonly property color librariesColor: "#ff8a1f"
    readonly property color presentationsColor: "#4da3ff"
    readonly property color mediaBinColor: "#b388ff"

    function refreshLists() {
        documents = catalog.documentsIn(libraryPath)
        mediaFiles = catalog.mediaIn(mediaFolder)
    }

    function openLibrary(path) {
        libraryPath = path
        if (documents.length > 0)
            openDocument(documents[0].path)
        else
            document = null
    }

    function openDocument(path) {
        document = catalog.open(path)
        grid.positionViewAtBeginning()
    }

    // The frame colour for a slide: the configured group of the same name, else the one
    // matching without a trailing number, else the colour the document gives the group.
    function groupColor(slide) {
        const name = slide.group.trim().toLowerCase()
        if (name !== "") {
            const find = wanted => groups.find(g => g.name.trim().toLowerCase() === wanted)
            const group = find(name) ?? find(name.replace(/\s*\d+$/, ""))
            if (group)
                return group.color
        }
        return slide.groupColor !== "" ? slide.groupColor : surfaceColor
    }

    // Selects one of a presentation's arrangements ("" for Master), saves that in its
    // file, and re-lays out the presentation wherever it is showing.
    function setArrangement(path, name) {
        const error = catalog.setArrangement(path, name)
        if (error !== "") {
            notice = error
            return
        }
        notice = ""
        const rearranged = catalog.open(path)
        if (liveDocument && liveDocument.path === path && liveIndex >= 0) {
            // Follow the live slide to its first place in the new order. If the new
            // arrangement leaves it out, the output keeps it and nothing is marked live.
            const index = rearranged.slides.findIndex(s => s.id === liveDocument.slides[liveIndex].id)
            if (index >= 0) {
                liveDocument = rearranged
                liveIndex = index
            }
        }
        if (document && document.path === path) {
            document = rearranged
            grid.positionViewAtBeginning()
        }
    }

    // Makes a slide of the presentation being viewed trigger a media file, replacing any
    // media it triggered before, or with null stops it triggering media. Saved in the
    // presentation file.
    function assignMedia(index, media) {
        const path = document.path
        const id = document.slides[index].id
        const error = media ? catalog.setSlideMedia(path, id, media.path) : catalog.removeSlideMedia(path, id)
        if (error !== "") {
            notice = error
            return
        }
        notice = ""
        // Same slides in the same order, so the grid can stay where it is scrolled to
        // and the live slide keeps its index.
        const scrolledTo = grid.contentY
        const reloaded = catalog.open(path)
        if (viewingLive)
            liveDocument = reloaded
        document = reloaded
        grid.contentY = scrolledTo
    }

    // Selects the presentation `delta` places from the current one in its library.
    function stepDocument(delta) {
        if (documents.length === 0)
            return
        const current = document ? documents.findIndex(d => d.path === document.path) : -1
        const next = Math.max(0, Math.min(documents.length - 1, current + delta))
        if (next !== current)
            openDocument(documents[next].path)
    }

    // Puts a slide on the slide layer, and any media its cue triggers on the media layer.
    function goLive(index) {
        if (!document || index < 0 || index >= document.slides.length)
            return
        const slide = document.slides[index]
        liveDocument = document
        liveIndex = index
        cleared = false
        output.showSlide(slide)
        if (slide.media)
            showMedia(slide.media)
        grid.positionViewAtIndex(index, GridView.Contain)
    }

    function showMedia(media) {
        liveMedia = media
        output.showMedia(media)
    }

    // For the self-test: the first file of the first media folder that has any.
    function showFirstMedia() {
        for (const folder of catalog.mediaFolders) {
            const files = catalog.mediaIn(folder.path)
            if (files.length > 0) {
                mediaFolder = folder.path
                showMedia(files[0])
                return
            }
        }
    }

    // Moves the live slide within the presentation being viewed. From a cleared output
    // it brings the current slide back; from another presentation it starts at the top.
    function step(delta) {
        if (!viewingLive)
            goLive(0)
        else if (cleared)
            goLive(liveIndex)
        else
            goLive(liveIndex + delta)
    }

    function clearSlide() {
        if (cleared)
            return
        cleared = true
        output.showSlide(null)
    }

    function clearMedia() {
        if (liveMedia === null)
            return
        liveMedia = null
        output.showMedia(null)
    }

    function clearAll() {
        clearSlide()
        clearMedia()
    }

    width: 1400
    height: 880
    minimumWidth: 900
    minimumHeight: 500
    visible: true
    color: panelColor
    // The toolbar is the title bar: it drags the window and carries the window buttons.
    flags: Qt.Window | Qt.FramelessWindowHint
    title: document ? document.name + " — SimplePresenter" : "SimplePresenter"

    // Restores the last session where what it refers to is still on disk, and falls back
    // to the first library and presentation and the top media folder where it is not.
    Component.onCompleted: {
        const saved = (key, fallback) => remember ? settings.value(key, fallback) : fallback
        width = Number(saved("windowWidth", width))
        height = Number(saved("windowHeight", height))
        sidebarWidth = Number(saved("sidebarWidth", sidebarWidth))
        sidePanelWidth = Number(saved("sidePanelWidth", sidePanelWidth))
        mediaBinHeight = Number(saved("mediaBinHeight", mediaBinHeight))
        // Settings stores booleans as text.
        mediaBinVisible = String(saved("mediaBinVisible", true)) === "true"
        outputEnabled = String(saved("outputEnabled", true)) === "true"
        stageEnabled = String(saved("stageEnabled", true)) === "true"
        useX11 = String(saved("useX11", false)) === "true"
        // By name, so the choice survives transitions being added or reordered.
        const transition = transitions.findIndex(t => t.name === String(saved("transition", "")))
        if (transition >= 0)
            transitionIndex = transition
        const duration = Number(saved("transitionDuration", transitionDuration))
        if (isFinite(duration) && duration >= 0)
            transitionDuration = duration
        try {
            const stored = JSON.parse(String(saved("groups", "")))
            if (Array.isArray(stored))
                groups = stored.filter(g => typeof g.name === "string" && typeof g.color === "string")
        } catch (e) {
            // Nothing stored yet, or not readable: keep the defaults.
        }

        const library = String(saved("library", ""))
        const presentation = String(saved("presentation", ""))
        const folder = String(saved("mediaFolder", ""))
        if (catalog.libraries.some(l => l.path === library)) {
            libraryPath = library
            if (documents.some(d => d.path === presentation))
                openDocument(presentation)
            else
                openLibrary(library)
        } else if (catalog.libraries.length > 0) {
            openLibrary(catalog.libraries[0].path)
        }
        mediaFolder = catalog.mediaFolders.some(f => f.path === folder) ? folder : catalog.mediaDirectory
        restored = true
    }
    onClosing: Qt.quit()

    // Saved as they change, not on exit, so a crash or a kill loses nothing.
    function save(key, value) {
        if (remember && restored)
            settings.setValue(key, value)
    }

    onWidthChanged: save("windowWidth", width)
    onHeightChanged: save("windowHeight", height)
    onSidebarWidthChanged: save("sidebarWidth", sidebarWidth)
    onSidePanelWidthChanged: save("sidePanelWidth", sidePanelWidth)
    onMediaBinHeightChanged: save("mediaBinHeight", mediaBinHeight)
    onMediaBinVisibleChanged: save("mediaBinVisible", mediaBinVisible)
    onOutputEnabledChanged: save("outputEnabled", outputEnabled)
    onStageEnabledChanged: save("stageEnabled", stageEnabled)
    onGroupsChanged: save("groups", JSON.stringify(groups))
    onUseX11Changed: save("useX11", useX11)
    onTransitionIndexChanged: save("transition", transitions[transitionIndex].name)
    onTransitionDurationChanged: save("transitionDuration", transitionDuration)
    onLibraryPathChanged: {
        refreshLists()
        save("library", libraryPath)
    }
    onDocumentChanged: save("presentation", document ? document.path : "")
    onMediaFolderChanged: {
        refreshLists()
        save("mediaFolder", mediaFolder)
    }

    Settings {
        id: settings
    }

    Connections {
        target: win.catalog

        // Keep each selection if it is still on disk; otherwise fall back to the first.
        function onChanged() {
            win.refreshLists()
            if (!win.catalog.libraries.some(l => l.path === win.libraryPath))
                win.openLibrary(win.catalog.libraries.length > 0 ? win.catalog.libraries[0].path : "")
            else if (win.document && !win.documents.some(d => d.path === win.document.path))
                win.openLibrary(win.libraryPath)
            else if (!win.document && win.documents.length > 0)
                win.openDocument(win.documents[0].path)
            if (!win.catalog.mediaFolders.some(f => f.path === win.mediaFolder))
                win.mediaFolder = win.catalog.mediaDirectory
        }
    }

    Output {
        id: output

        owner: win
        remember: win.remember
        shown: win.outputEnabled
        fullScreenOn: win.outputScreen
        shader: win.transitions[win.transitionIndex].shader
        duration: Math.round(win.transitionDuration * 1000)
        keyTarget: keys
    }

    Stage {
        owner: win
        remember: win.remember
        shown: win.stageEnabled
        currentText: win.stageCurrentText
        nextText: win.stageNextText
        keyTarget: keys
    }

    // A draggable line between two panes. `vertical` is a vertical line dragged sideways.
    // Reports each movement in pixels; the handler decides which pane grows.
    component Divider: MouseArea {
        id: divider

        property bool vertical: true
        property real last: 0

        signal moved(real delta)

        function position(mouse) {
            const point = mapToItem(null, mouse.x, mouse.y)
            return vertical ? point.x : point.y
        }

        width: vertical ? 7 : undefined
        height: vertical ? undefined : 7
        z: 1
        hoverEnabled: true
        preventStealing: true
        cursorShape: vertical ? Qt.SplitHCursor : Qt.SplitVCursor
        onPressed: (mouse) => last = position(mouse)
        onPositionChanged: (mouse) => {
            if (!pressed)
                return
            const now = position(mouse)
            moved(now - last)
            last = now
        }
        onReleased: keys.forceActiveFocus()

        Rectangle {
            anchors.centerIn: parent
            width: divider.vertical ? 2 : parent.width
            height: divider.vertical ? parent.height : 2
            visible: divider.containsMouse || divider.pressed
            color: win.accentColor
        }
    }

    component WindowButton: AppButton {
        width: 34
        leftPadding: 0
        rightPadding: 0
        font.pixelSize: 15
    }

    // Marks a thumbnail as live: an orange ring outside its frame with a soft glow, set
    // off from the frame by a gap of the background colour so it reads even against an
    // orange frame. Fill the frame with this, behind it. The glow is a single cheap
    // shader, drawn only on the live thumbnail, not a blur pass.
    component LiveRing: Item {
        id: liveRing

        // The colour behind the thumbnail, for the gap
        property color background: win.panelColor

        anchors.margins: -5

        RectangularShadow {
            anchors.fill: parent
            radius: ring.radius
            blur: 12
            spread: 1
            color: win.accentColor
        }

        Rectangle {
            id: ring

            anchors.fill: parent
            radius: 8
            color: liveRing.background
            border.width: 3
            border.color: win.accentColor
        }
    }

    component SectionTitle: Text {
        leftPadding: 12
        topPadding: 12
        bottomPadding: 6
        color: win.dimTextColor
        font.pixelSize: 12
        font.capitalization: Font.AllUppercase
        // Coloured titles are also bold, to stand out from the plain ones.
        font.bold: color !== win.dimTextColor
    }

    component EmptyNote: Text {
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: win.dimTextColor
        font.pixelSize: 14
    }

    Item {
        id: keys

        anchors.fill: parent
        focus: true

        Keys.onPressed: (event) => {
            if (win.settingsOpen) {
                if (event.key === Qt.Key_Escape)
                    win.settingsOpen = false
                return
            }
            if (event.modifiers & Qt.ControlModifier) {
                switch (event.key) {
                case Qt.Key_V:
                    win.mediaBinVisible = !win.mediaBinVisible
                    break
                case Qt.Key_1:
                    win.outputEnabled = !win.outputEnabled
                    break
                case Qt.Key_2:
                    win.stageEnabled = !win.stageEnabled
                    break
                default:
                    return
                }
                event.accepted = true
                return
            }
            switch (event.key) {
            case Qt.Key_Right:
            case Qt.Key_Space:
                win.step(1)
                break
            case Qt.Key_Left:
                win.step(-1)
                break
            case Qt.Key_Down:
                win.stepDocument(1)
                break
            case Qt.Key_Up:
                win.stepDocument(-1)
                break
            case Qt.Key_F1:
                win.clearAll()
                break
            case Qt.Key_F2:
                win.clearSlide()
                break
            case Qt.Key_F3:
                win.clearMedia()
                break
            default:
                return
            }
            event.accepted = true
        }

        // Libraries and presentations
        Rectangle {
            id: sidebar

            anchors.left: parent.left
            anchors.top: toolbar.bottom
            anchors.bottom: mediaBin.top
            width: Math.max(160, Math.min(win.sidebarWidth, win.width - sidePanel.width - 260))
            color: win.surfaceColor

            SectionTitle {
                id: librariesTitle

                anchors.top: parent.top
                color: win.librariesColor
                text: "Libraries"
            }

            SidebarList {
                id: libraryList

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: librariesTitle.bottom
                height: Math.min(contentHeight, sidebar.height * 0.3)
                model: win.catalog.libraries
                selectedPath: win.libraryPath
                livePath: !win.cleared && win.liveDocument
                          ? win.liveDocument.path.substring(0, win.liveDocument.path.lastIndexOf("/")) : ""
                onPicked: (entry) => win.openLibrary(entry.path)
            }

            SectionTitle {
                id: documentsTitle

                anchors.top: libraryList.bottom
                color: win.presentationsColor
                text: "Presentations"
            }

            SidebarList {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: documentsTitle.bottom
                anchors.bottom: parent.bottom
                model: win.documents
                selectedPath: win.document ? win.document.path : ""
                livePath: !win.cleared && win.liveDocument ? win.liveDocument.path : ""
                onPicked: (entry) => win.openDocument(entry.path)
                onMenuRequested: (entry, item) => {
                    arrangementMenu.entry = entry
                    arrangementMenu.parent = item
                    arrangementMenu.open()
                }
            }

            // Right-click menu of a presentation: its arrangements
            Popup {
                id: arrangementMenu

                property var entry: null
                readonly property var names: entry ? [""].concat(entry.arrangements) : []

                x: 24
                y: parent ? parent.height - 2 : 0
                width: 200
                padding: 4
                onClosed: keys.forceActiveFocus()

                background: Rectangle {
                    radius: 6
                    color: win.surfaceColor
                    border.width: 1
                    border.color: "#45484e"
                }

                contentItem: Column {
                    Text {
                        leftPadding: 10
                        topPadding: 6
                        bottomPadding: 6
                        color: win.dimTextColor
                        font.pixelSize: 12
                        font.capitalization: Font.AllUppercase
                        text: "Arrangement"
                    }

                    Repeater {
                        model: arrangementMenu.names

                        delegate: Rectangle {
                            id: choice

                            required property string modelData
                            readonly property bool current: arrangementMenu.entry !== null
                                                            && arrangementMenu.entry.arrangement === modelData

                            width: arrangementMenu.availableWidth
                            height: 30
                            radius: 4
                            color: choiceMouse.containsMouse ? "#45484e" : "transparent"

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                color: choice.current ? win.accentColor : win.textColor
                                font.pixelSize: 14
                                text: (choice.current ? "✓  " : "     ") + (choice.modelData === "" ? "Master" : choice.modelData)
                            }

                            MouseArea {
                                id: choiceMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    const path = arrangementMenu.entry.path
                                    arrangementMenu.close()
                                    win.setArrangement(path, choice.modelData)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Toolbar, standing in for the title bar: drag it to move the window, double-click
        // to maximise.
        Rectangle {
            id: toolbar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 48
            color: "#15161a"

            DragHandler {
                target: null
                onActiveChanged: if (active) win.startSystemMove()
            }

            TapHandler {
                onDoubleTapped: win.visibility = win.visibility === Window.Maximized ? Window.Windowed : Window.Maximized
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: toolbarControls.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                color: win.textColor
                font.pixelSize: 15
                text: win.title
            }

            Row {
                id: toolbarControls

                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                AppComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 120
                    model: win.transitions.map(t => t.name)
                    currentIndex: win.transitionIndex
                    onActivated: (index) => win.transitionIndex = index
                }

                AppSlider {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 120
                    from: 0
                    to: 3
                    stepSize: 0.05
                    enabled: win.transitionIndex !== 0
                    value: Math.min(win.transitionDuration, to)
                    onMoved: win.transitionDuration = Math.round(value * 100) / 100
                }

                // Seconds; accepts any value from 0 up, beyond the slider's range.
                AppTextField {
                    id: durationField

                    function reset() {
                        text = Number(win.transitionDuration.toFixed(2)).toString()
                    }

                    anchors.verticalCenter: parent.verticalCenter
                    width: 45
                    horizontalAlignment: TextInput.AlignRight
                    enabled: win.transitionIndex !== 0
                    onEditingFinished: {
                        const seconds = Number(text.replace(",", "."))
                        if (text.trim() !== "" && isFinite(seconds) && seconds >= 0)
                            win.transitionDuration = seconds
                        reset()
                        keys.forceActiveFocus()
                    }
                    Keys.onEscapePressed: {
                        reset()
                        keys.forceActiveFocus()
                    }
                    Component.onCompleted: reset()

                    Connections {
                        target: win

                        function onTransitionDurationChanged() {
                            durationField.reset()
                        }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    rightPadding: 16
                    color: win.dimTextColor
                    font.pixelSize: 13
                    text: "s"
                }

                ToolbarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    kind: "bin"
                    label: "Media"
                    on: win.mediaBinVisible
                    onClicked: win.mediaBinVisible = !win.mediaBinVisible
                }

                ToolbarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    label: "Output"
                    on: win.outputEnabled
                    onClicked: win.outputEnabled = !win.outputEnabled
                }

                ToolbarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    label: "Stage"
                    on: win.stageEnabled
                    onClicked: win.stageEnabled = !win.stageEnabled
                }

                ToolbarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    kind: "settings"
                    label: "Settings"
                    onClicked: win.settingsOpen = true
                }

                Item {
                    width: 8
                    height: 1
                }

                WindowButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "–"
                    onClicked: win.showMinimized()
                }

                WindowButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: win.visibility === Window.Maximized ? "❐" : "□"
                    onClicked: win.visibility = win.visibility === Window.Maximized ? Window.Windowed : Window.Maximized
                }

                WindowButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "✕"
                    onClicked: win.close()
                }
            }
        }

        // Preview of the output, the clear buttons, and spare room below
        Rectangle {
            id: sidePanel

            anchors.right: parent.right
            anchors.top: toolbar.bottom
            anchors.bottom: mediaBin.top
            width: Math.max(250, Math.min(win.sidePanelWidth, win.width - 160 - 260))
            color: "black"

            // Both previews are 16:9 and as wide as the panel, unless the panel is too
            // short for that, in which case they shrink to fit and stay centred.
            readonly property real previewWidth: Math.max(80, Math.min(
                width - 24, (height - 2 * outputTitle.height - clearButtons.height - 48) / 2 * 16 / 9))

            SectionTitle {
                id: outputTitle

                anchors.top: parent.top
                text: "Output"
            }

            // Built from cheap parts instead of a second copy of the output: the slide is
            // drawn again at this small size, a still image comes from its cached
            // thumbnail, and video borrows a few frames a second from the output's decoder.
            Rectangle {
                id: preview

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: outputTitle.bottom
                width: sidePanel.previewWidth
                height: width * 9 / 16
                color: "black"
                border.width: 1
                border.color: "#3a3c42"

                Item {
                    anchors.fill: parent
                    anchors.margins: 1

                    Image {
                        anchors.fill: parent
                        visible: win.liveMedia !== null && !win.liveMedia.video
                        source: visible ? "image://thumbnail/" + encodeURIComponent(win.liveMedia.path) : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    VideoOutput {
                        id: previewVideo

                        anchors.fill: parent
                        visible: win.liveMedia !== null && win.liveMedia.video
                        fillMode: VideoOutput.PreserveAspectFit
                    }

                    FrameRelay {
                        source: output.liveVideoSink
                        target: previewVideo.videoSink
                        interval: 100
                    }

                    Slide {
                        anchors.fill: parent
                        slide: win.liveSlide
                    }
                }
            }

            SectionTitle {
                id: stageTitle

                anchors.top: preview.bottom
                text: "Stage"
            }

            Rectangle {
                id: stagePreview

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: stageTitle.bottom
                width: sidePanel.previewWidth
                height: width * 9 / 16
                color: "black"
                border.width: 1
                border.color: "#3a3c42"

                StageView {
                    anchors.fill: parent
                    anchors.margins: 1
                    currentText: win.stageCurrentText
                    nextText: win.stageNextText
                }
            }

            // Sized for five buttons.
            Row {
                id: clearButtons

                readonly property real buttonWidth: (width - 4 * spacing) / 5

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: stagePreview.bottom
                anchors.margins: 12
                spacing: 6

                // The key hints are dropped from all the buttons together when the widest
                // label would no longer fit.
                readonly property bool showHints: widestLabel.width + 8 <= buttonWidth

                TextMetrics {
                    id: widestLabel

                    font.pixelSize: 12
                    text: "F3 Media"
                }

                component ClearButton: AppButton {
                    property string hint
                    property string name

                    width: clearButtons.buttonWidth
                    leftPadding: 2
                    rightPadding: 2
                    font.pixelSize: 12
                    text: clearButtons.showHints ? hint + " " + name : name
                    // Red while its layer has something on it, grey once cleared.
                    alert: true
                }

                ClearButton {
                    enabled: !win.cleared || win.liveMedia !== null
                    hint: "F1"
                    name: "All"
                    onClicked: win.clearAll()
                }

                ClearButton {
                    enabled: !win.cleared
                    hint: "F2"
                    name: "Slide"
                    onClicked: win.clearSlide()
                }

                ClearButton {
                    enabled: win.liveMedia !== null
                    hint: "F3"
                    name: "Media"
                    onClicked: win.clearMedia()
                }
            }
        }

        // Arrangement of the presentation being viewed
        Item {
            id: gridHeader

            anchors.left: sidebar.right
            anchors.right: sidePanel.left
            anchors.top: toolbar.bottom
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            visible: (win.document !== null && win.document.arrangements.length > 0) || win.notice !== ""
            height: visible ? 46 : 6

            Text {
                anchors.left: parent.left
                anchors.right: arrangementLabel.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                color: "#ff6b6b"
                font.pixelSize: 13
                text: win.notice
            }

            Text {
                id: arrangementLabel

                anchors.right: arrangementBox.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                visible: arrangementBox.visible
                color: win.dimTextColor
                font.pixelSize: 13
                text: "Arrangement"
            }

            // "Master" is every group once, in the order the presentation stores them.
            // The choice is saved in the presentation file.
            AppComboBox {
                id: arrangementBox

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                visible: win.document !== null && win.document.arrangements.length > 0
                model: win.document ? ["Master"].concat(win.document.arrangements) : []
                currentIndex: win.document && win.document.arrangement !== ""
                              ? win.document.arrangements.indexOf(win.document.arrangement) + 1 : 0
                onActivated: (index) => win.setArrangement(win.document.path, index === 0 ? "" : model[index])
            }
        }

        // Slides
        GridView {
            id: grid

            objectName: "slideGrid"

            readonly property int columns: Math.max(1, Math.floor(width / 250))
            readonly property real labelHeight: 26
            readonly property int frameWidth: 4

            anchors.left: sidebar.right
            anchors.right: sidePanel.left
            anchors.top: gridHeader.bottom
            anchors.bottom: mediaBin.top
            anchors.leftMargin: 10
            anchors.rightMargin: 4
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: Math.floor((width - 12) / columns)
            cellHeight: (cellWidth - 12 - 2 * frameWidth) * 9 / 16 + frameWidth + labelHeight + 12
            model: win.document ? win.document.slides : []

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            delegate: Item {
                id: cell

                required property var modelData
                required property int index
                readonly property bool live: win.viewingLive && !win.cleared && win.liveIndex === index
                // The frame is the slide's group colour, and the label is drawn on it.
                readonly property color frame: win.groupColor(modelData)
                readonly property bool lightFrame: 0.299 * frame.r + 0.587 * frame.g + 0.114 * frame.b > 0.6

                width: grid.cellWidth
                height: grid.cellHeight
                // The live slide's glow spills a little over its neighbours.
                z: live ? 1 : 0

                LiveRing {
                    anchors.fill: frameRect
                    visible: cell.live
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
                            source: visible ? "image://thumbnail/" + encodeURIComponent(cell.modelData.media.path) : ""
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }

                        Slide {
                            anchors.fill: parent
                            slide: cell.modelData
                        }

                        // Marks a slide that triggers media: two stacked layers, the back
                        // one filled. Amber when the media file cannot be found, in which
                        // case there is no thumbnail behind the slide either.
                        Rectangle {
                            readonly property color ink: cell.modelData.media !== undefined ? "#e6e6e6" : "#ffb300"

                            x: 4
                            y: 4
                            width: 24
                            height: 20
                            radius: 4
                            visible: cell.modelData.mediaName !== ""
                            color: "#c0000000"

                            Rectangle {
                                x: 4
                                y: 4
                                width: 12
                                height: 9
                                radius: 1
                                color: parent.ink
                                opacity: 0.6
                            }

                            Rectangle {
                                x: 8
                                y: 7
                                width: 12
                                height: 9
                                radius: 1
                                color: "#c0000000"
                                border.width: 1.5
                                border.color: parent.ink
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
                        visible: (cellMouse.containsMouse && !cell.live) || mediaDrop.containsDrag
                        color: "transparent"
                        border.width: mediaDrop.containsDrag ? 3 : 1
                        border.color: mediaDrop.containsDrag ? "white" : "#c0ffffff"
                    }
                }

                MouseArea {
                    id: cellMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton) {
                            win.goLive(cell.index)
                        } else {
                            slideMenu.index = cell.index
                            slideMenu.parent = cell
                            slideMenu.x = mouse.x
                            slideMenu.y = mouse.y
                            slideMenu.open()
                        }
                    }
                }

                // A media bin file dropped here becomes the media this slide triggers.
                DropArea {
                    id: mediaDrop

                    anchors.fill: parent
                    keys: ["media"]
                    onDropped: (drop) => win.assignMedia(cell.index, drop.source.media)
                }
            }
        }

        // Right-click menu of a slide
        Popup {
            id: slideMenu

            property int index: -1
            readonly property var slide: win.document && index >= 0 && index < win.document.slides.length
                                         ? win.document.slides[index] : null
            readonly property bool hasMedia: slide !== null && slide.mediaName !== ""

            width: 200
            padding: 4
            onClosed: keys.forceActiveFocus()

            background: Rectangle {
                radius: 6
                color: win.surfaceColor
                border.width: 1
                border.color: "#45484e"
            }

            contentItem: Rectangle {
                implicitHeight: 30
                radius: 4
                color: slideMenu.hasMedia && removeMouse.containsMouse ? "#45484e" : "transparent"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    color: slideMenu.hasMedia ? win.textColor : "#6c6f75"
                    font.pixelSize: 14
                    text: "Remove Media"
                }

                MouseArea {
                    id: removeMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: slideMenu.hasMedia
                    onClicked: {
                        const index = slideMenu.index
                        slideMenu.close()
                        win.assignMedia(index, null)
                    }
                }
            }
        }

        EmptyNote {
            anchors.centerIn: grid
            width: grid.width - 80
            visible: text !== ""
            text: win.document ? win.document.error
                : win.catalog.libraries.length === 0 ? "No libraries yet. Add a folder of .pro files to\n" + win.catalog.librariesDirectory
                : "This library has no presentations."
        }

        // What is being dragged out of the media bin: a point that follows the pointer,
        // with a small picture of the file beside it.
        Item {
            id: mediaDrag

            property var media: null

            z: 50
            Drag.keys: ["media"]

            Rectangle {
                x: 10
                y: 10
                width: 112
                height: 63
                visible: mediaDrag.Drag.active
                color: "black"
                border.width: 2
                border.color: win.accentColor
                opacity: 0.9

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: mediaDrag.media ? "image://thumbnail/" + encodeURIComponent(mediaDrag.media.path) : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
            }
        }

        // Dividers. Each starts from the pane's current, clamped size, so dragging back
        // from a limit responds at once.
        Divider {
            anchors.horizontalCenter: sidebar.right
            anchors.top: sidebar.top
            anchors.bottom: sidebar.bottom
            onMoved: (delta) => win.sidebarWidth = sidebar.width + delta
        }

        Divider {
            anchors.horizontalCenter: sidePanel.left
            anchors.top: sidePanel.top
            anchors.bottom: sidePanel.bottom
            onMoved: (delta) => win.sidePanelWidth = sidePanel.width - delta
        }

        Divider {
            vertical: false
            anchors.verticalCenter: mediaBin.top
            anchors.left: parent.left
            anchors.right: parent.right
            visible: win.mediaBinVisible
            onMoved: (delta) => win.mediaBinHeight = mediaBin.height - delta
        }

        // Media bin: folder tree on the left, the selected folder's files on the right
        Rectangle {
            id: mediaBin

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: visible ? Math.max(120, Math.min(win.mediaBinHeight, win.height - toolbar.height - 160)) : 0
            visible: win.mediaBinVisible
            color: win.surfaceColor

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
                color: win.mediaBinColor
                text: "Media bin"
            }

            SidebarList {
                id: folderList

                anchors.left: parent.left
                anchors.top: mediaTitle.bottom
                anchors.bottom: parent.bottom
                width: sidebar.width
                model: win.catalog.mediaFolders
                selectedPath: win.mediaFolder
                livePath: win.liveMedia ? win.liveMedia.path.substring(0, win.liveMedia.path.lastIndexOf("/")) : ""
                onPicked: (entry) => win.mediaFolder = entry.path
            }

            GridView {
                id: mediaGrid

                readonly property real labelHeight: 24

                anchors.left: folderList.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 10
                anchors.rightMargin: 4
                anchors.topMargin: 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cellWidth: 180
                cellHeight: (cellWidth - 12) * 9 / 16 + labelHeight + 12
                model: win.mediaFiles

                ScrollBar.vertical: ScrollBar {}

                KineticWheel {}

                delegate: Item {
                    id: mediaCell

                    required property var modelData
                    readonly property bool live: win.liveMedia !== null && win.liveMedia.path === modelData.path

                    width: mediaGrid.cellWidth
                    height: mediaGrid.cellHeight
                    // The live file's glow spills a little over its neighbours.
                    z: live ? 1 : 0

                    LiveRing {
                        anchors.fill: mediaFrame
                        visible: mediaCell.live
                        background: win.surfaceColor
                    }

                    Rectangle {
                        id: mediaFrame

                        anchors.fill: parent
                        anchors.margins: 6
                        color: mediaMouse.containsMouse ? "#4a4d54" : win.panelColor
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
                                source: "image://thumbnail/" + encodeURIComponent(mediaCell.modelData.path)
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
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
                            color: win.textColor
                            font.pixelSize: 11
                            text: mediaCell.modelData.name
                        }
                    }

                    // Click to put the file on the media layer; drag it onto a slide to
                    // make that slide trigger it.
                    MouseArea {
                        id: mediaMouse

                        property point pressedAt
                        property bool dragging: false

                        function finish() {
                            dragging = false
                            mediaDrag.Drag.active = false
                        }

                        anchors.fill: parent
                        hoverEnabled: true
                        preventStealing: true
                        onPressed: (mouse) => {
                            pressedAt = Qt.point(mouse.x, mouse.y)
                            dragging = false
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return
                            const at = mapToItem(keys, mouse.x, mouse.y)
                            mediaDrag.x = at.x
                            mediaDrag.y = at.y
                            if (!dragging && Math.abs(mouse.x - pressedAt.x) + Math.abs(mouse.y - pressedAt.y) > 10) {
                                dragging = true
                                mediaDrag.media = mediaCell.modelData
                                mediaDrag.Drag.active = true
                            }
                        }
                        onReleased: {
                            if (dragging) {
                                mediaDrag.Drag.drop()
                                finish()
                            } else {
                                win.showMedia(mediaCell.modelData)
                            }
                        }
                        onCanceled: finish()
                    }
                }
            }

            EmptyNote {
                anchors.centerIn: mediaGrid
                width: mediaGrid.width - 80
                visible: win.mediaFiles.length === 0
                text: win.catalog.mediaFolders.length > 1 ? "No media files in this folder."
                    : "No media yet. Add images and videos to\n" + win.catalog.mediaDirectory
            }
        }
    }

    SettingsScreen {
        anchors.fill: parent
        anchors.topMargin: toolbar.height
        visible: win.settingsOpen
        groups: win.groups
        onGroupsEdited: (groups) => win.groups = groups
        useX11: win.useX11
        onUseX11Edited: (useX11) => win.useX11 = useX11
        onClosed: {
            win.settingsOpen = false
            keys.forceActiveFocus()
        }
    }

    ResizeGrips {
        anchors.fill: parent
        target: win
    }
}
