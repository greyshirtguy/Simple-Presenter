import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
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

    // Paths of the library and media folder being browsed. While a playlist is being
    // browsed instead of a library, playlistId is its id; "" otherwise.
    property string libraryPath: ""
    property string playlistId: ""
    // The playlist or playlist folder selected in the tree, "" while a library is. It is
    // the playlist being browsed unless a folder has been selected since: selecting a
    // folder leaves the presentations of whatever was browsed before on show.
    property string selectedNode: ""
    // The same pair for the media bin: the media playlist being browsed, and the media
    // playlist or folder selected in its tree.
    property string mediaPlaylistId: ""
    property string selectedMediaNode: ""
    // What is in them, re-read by refreshLists() whenever either path or the disk changes.
    // `documents` holds the rows of the library or playlist being browsed, in the shape
    // PlaylistFile describes; a row's `path` identifies the row, its `file` the presentation.
    property var documents: []
    property var mediaFiles: []

    // The presentation shown in the grid: { name, path, slides, error } from Catalog.open()
    property var document: null
    // Which row of `documents` it was opened from
    property string documentKey: ""
    // The presentation and slide on the slide layer. They stay set while the layer is
    // cleared, so stepping carries on from where it was.
    property var liveDocument: null
    property int liveIndex: -1
    // The row and playlist it went live from ("" for a library)
    property string liveKey: ""
    property string livePlaylistId: ""
    property bool cleared: true
    // What is on the media layer: { name, path, source, video }, or null, and the media
    // playlist it was triggered from, "" if a slide triggered it
    property var liveMedia: null
    property string liveMediaPlaylistId: ""

    // Cut and Dissolve first, then the rest, which are ported from gl-transitions.
    readonly property var transitions: [
        { name: "Cut", shader: "" },
        { name: "Dissolve", shader: "qrc:/shaders/dissolve.frag.qsb" },
        { name: "Ripple", shader: "qrc:/shaders/ripple.frag.qsb" },
        { name: "Wipe Left", shader: "qrc:/shaders/gl-transitions/wipeLeft.frag.qsb" },
        { name: "Wipe Right", shader: "qrc:/shaders/gl-transitions/wipeRight.frag.qsb" },
        { name: "Wipe Up", shader: "qrc:/shaders/gl-transitions/wipeUp.frag.qsb" },
        { name: "Wipe Down", shader: "qrc:/shaders/gl-transitions/wipeDown.frag.qsb" },
        { name: "Circle Open", shader: "qrc:/shaders/gl-transitions/circleopen.frag.qsb" },
        { name: "Cross Warp", shader: "qrc:/shaders/gl-transitions/crosswarp.frag.qsb" },
        { name: "Directional Warp", shader: "qrc:/shaders/gl-transitions/directionalwarp.frag.qsb" },
        { name: "Dreamy", shader: "qrc:/shaders/gl-transitions/Dreamy.frag.qsb" },
        { name: "Swirl", shader: "qrc:/shaders/gl-transitions/Swirl.frag.qsb" },
        { name: "Water Drop", shader: "qrc:/shaders/gl-transitions/WaterDrop.frag.qsb" },
        { name: "Window Slice", shader: "qrc:/shaders/gl-transitions/windowslice.frag.qsb" },
        { name: "Pinwheel", shader: "qrc:/shaders/gl-transitions/pinwheel.frag.qsb" },
        { name: "Radial", shader: "qrc:/shaders/gl-transitions/Radial.frag.qsb" },
        { name: "Cross Zoom", shader: "qrc:/shaders/gl-transitions/CrossZoom.frag.qsb" },
        { name: "Simple Zoom", shader: "qrc:/shaders/gl-transitions/SimpleZoom.frag.qsb" },
        { name: "Linear Blur", shader: "qrc:/shaders/gl-transitions/LinearBlur.frag.qsb" },
        { name: "Pixelize", shader: "qrc:/shaders/gl-transitions/pixelize.frag.qsb" },
        { name: "Random Squares", shader: "qrc:/shaders/gl-transitions/randomsquares.frag.qsb" },
        { name: "Wind", shader: "qrc:/shaders/gl-transitions/wind.frag.qsb" },
        { name: "Heart", shader: "qrc:/shaders/gl-transitions/heart.frag.qsb" }
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
    // A message to show above the slides, or "", and whether it reports a failure
    property string notice: ""
    property bool noticeIsError: true
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
    // Height of the libraries and playlists pane at the top of the sidebar
    property real sourcesHeight: 260
    // The width slide thumbnails aim for. The grid fits as many columns of at least this
    // width as it can and stretches them to fill the row.
    property real thumbnailWidth: 250
    readonly property real smallestThumbnail: 180
    readonly property real largestThumbnail: 400
    // The same for the media bin's thumbnails
    property real mediaThumbnailWidth: 180
    readonly property real smallestMediaThumbnail: 110
    readonly property real largestMediaThumbnail: 320
    property real sidePanelWidth: 360
    property real mediaBinHeight: 250

    // The slide on the slide layer and the one after it, or null
    readonly property var liveSlide: !cleared && liveDocument ? liveDocument.slides[liveIndex] : null
    readonly property var nextSlide: liveDocument && liveIndex + 1 < liveDocument.slides.length
                                     ? liveDocument.slides[liveIndex + 1] : null
    readonly property string stageCurrentText: liveSlide ? liveSlide.plainText : ""
    readonly property string stageNextText: nextSlide ? nextSlide.plainText : ""

    readonly property bool viewingLive: document !== null && liveDocument !== null
                                        && documentKey === liveKey && playlistId === livePlaylistId
                                        && document.arrangement === liveDocument.arrangement

    readonly property color panelColor: "#1e1f22"
    readonly property color surfaceColor: "#2b2d31"
    readonly property color textColor: "#e6e6e6"
    readonly property color dimTextColor: "#9a9da3"
    readonly property color accentColor: "#ff8a1f"
    // Title colours of the three browsing areas
    readonly property color librariesColor: "#ff8a1f"
    readonly property color playlistsColor: "#5fd38d"
    readonly property color presentationsColor: "#4da3ff"
    readonly property color mediaBinColor: "#b388ff"

    function refreshLists() {
        documents = playlistId !== "" ? catalog.playlistItems(playlistId) : catalog.documentsIn(libraryPath)
        mediaFiles = catalog.mediaIn(mediaPlaylistId)
    }

    function openLibrary(path) {
        selectedNode = ""
        playlistId = ""
        libraryPath = path
        refreshLists()
        openFirst()
    }

    function openPlaylist(id) {
        selectedNode = id
        playlistId = id
        refreshLists()
        openFirst()
    }

    // Whether a row is a presentation that is actually here to open.
    function openable(entry) {
        return entry.kind === "presentation" && !entry.missing
    }

    // A playlist row brings its own choice of arrangement; a library row follows the
    // one saved in the presentation.
    function load(entry) {
        return entry.playlistItem ? catalog.openArranged(entry.file, entry.arrangement) : catalog.open(entry.file)
    }

    function currentEntry() {
        return documents.find(d => d.path === documentKey)
    }

    function openEntry(entry) {
        if (!openable(entry))
            return
        documentKey = entry.path
        document = load(entry)
        grid.positionViewAtBeginning()
    }

    function openDocument(key) {
        const entry = documents.find(d => d.path === key)
        if (entry)
            openEntry(entry)
    }

    function openFirst() {
        const first = documents.find(openable)
        if (first) {
            openEntry(first)
        } else {
            documentKey = ""
            document = null
        }
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

    // Selects one of a presentation's arrangements ("" for Master) for a row and re-lays
    // out the presentation wherever that row is showing. For a library row the choice is
    // saved in the presentation file; for a playlist row, in the playlist.
    function setArrangement(entry, name) {
        const key = entry.path
        const error = entry.playlistItem ? catalog.setPlaylistItemArrangement(key, entry.file, name)
                                         : catalog.setArrangement(entry.file, name)
        if (!report(error))
            return
        notice = ""
        const rearranged = entry.playlistItem ? catalog.openArranged(entry.file, name) : catalog.open(entry.file)
        if (liveDocument && liveKey === key && livePlaylistId === playlistId && liveIndex >= 0) {
            // Follow the live slide to its first place in the new order. If the new
            // arrangement leaves it out, the output keeps it and nothing is marked live.
            const index = rearranged.slides.findIndex(s => s.id === liveDocument.slides[liveIndex].id)
            if (index >= 0) {
                liveDocument = rearranged
                liveIndex = index
            }
        }
        if (documentKey === key) {
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
        if (!report(error))
            return
        notice = ""
        // Same slides in the same order, so the grid can stay where it is scrolled to
        // and the live slide keeps its index.
        const scrolledTo = grid.contentY
        const reloaded = load(currentEntry())
        if (viewingLive)
            liveDocument = reloaded
        document = reloaded
        grid.contentY = scrolledTo
    }

    // A thumbnail width one step larger (+1) or smaller (-1) than `current`, for a grid
    // of `columns` columns and the given width, within the limits. Since thumbnails
    // stretch to fill their row, the size only visibly changes when the number of
    // columns does, so this steps until it has, or until the limit.
    function steppedThumbnail(current, direction, gridWidth, columns, smallest, largest) {
        const columnsAt = width => Math.max(1, Math.floor(gridWidth / width))
        let target = current
        do {
            target = Math.max(smallest, Math.min(largest, target + direction * 10))
        } while (columnsAt(target) === columns && target > smallest && target < largest)
        return target
    }

    function zoomThumbnails(direction) {
        thumbnailWidth = steppedThumbnail(thumbnailWidth, direction, grid.width, grid.columns,
                                          smallestThumbnail, largestThumbnail)
    }

    function zoomMediaThumbnails(direction) {
        mediaThumbnailWidth = steppedThumbnail(mediaThumbnailWidth, direction, mediaGrid.width, mediaGrid.columns,
                                               smallestMediaThumbnail, largestMediaThumbnail)
    }

    // Selects the presentation `delta` places from the current one in its library or
    // playlist, passing over headers and presentations that are missing.
    function stepDocument(delta) {
        const rows = documents.filter(openable)
        if (rows.length === 0)
            return
        const current = rows.findIndex(d => d.path === documentKey)
        const next = Math.max(0, Math.min(rows.length - 1, current + delta))
        if (next !== current)
            openEntry(rows[next])
    }

    // The url of the thumbnail of a media file.
    function thumbnailUrl(path) {
        return "image://thumbnail/" + encodeURIComponent(path) + "?" + catalog.thumbnailRevision
    }

    // The media files the slides of a presentation trigger, as paths.
    function mediaOf(file) {
        return catalog.open(file).slides.filter(s => s.media !== undefined).map(s => s.media.path)
    }

    // Shows an error from a change to the playlists, if there was one.
    function report(error) {
        notice = error
        noticeIsError = true
        return error === ""
    }

    // Where a new playlist or folder goes: inside the selected folder, or beside the
    // selected playlist, or at the top level if a library is selected.
    function newNodeParent() {
        const selected = catalog.playlists.find(p => p.path === selectedNode)
        return !selected ? "" : selected.folder ? selected.path : selected.parent
    }

    function showAddMenu(item) {
        menu.show([
            { label: "Add Folder", run: () => newPlaylistNode(true, newNodeParent()) },
            { label: "Add Playlist", run: () => newPlaylistNode(false, newNodeParent()) },
            { label: "Import Playlist…", run: () => importDialog.open() }
        ], item)
    }

    // Adds an empty playlist, or with `folder` an empty playlist folder, inside the
    // folder `parent` or at the top level for "", and starts renaming it in place. A new
    // playlist is also browsed.
    function newPlaylistNode(folder, parent) {
        const names = catalog.playlists.map(p => p.name)
        const base = folder ? "New Folder" : "New Playlist"
        let name = base
        for (let n = 2; names.includes(name); ++n)
            name = base + " " + n
        const created = folder ? catalog.createPlaylistFolder(name, parent) : catalog.createPlaylist(name, parent)
        if (!report(created.error))
            return
        if (folder)
            selectedNode = created.id
        else
            openPlaylist(created.id)
        playlistList.editingPath = created.id
        // Bring the new row into view once the list has grown to hold it.
        Qt.callLater(() => {
            const index = catalog.playlists.findIndex(p => p.path === created.id)
            const bottom = playlistList.y + (index + 1) * 30
            if (bottom > sourcesView.contentY + sourcesView.height)
                sourcesView.contentY = Math.max(0, Math.min(bottom - sourcesView.height + 8,
                                                            sourcesView.contentHeight - sourcesView.height))
        })
    }

    function addToPlaylist(playlist, file) {
        report(catalog.addToPlaylist(playlist, file))
    }

    // Opens the menu for a row of the presentations list.
    function showPresentationMenu(entry, item) {
        const items = []
        if (openable(entry)) {
            items.push({ header: "Arrangement" })
            for (const name of [""].concat(entry.arrangements)) {
                items.push({
                    label: name === "" ? "Master" : name,
                    current: entry.arrangement === name,
                    run: () => setArrangement(entry, name)
                })
            }
            const playlists = catalog.playlists.filter(p => !p.folder)
            if (playlists.length > 0)
                items.push({ header: "Add to playlist" })
            for (const playlist of playlists)
                items.push({ label: playlist.name, run: () => addToPlaylist(playlist.path, entry.file) })
        }
        if (entry.playlistItem) {
            items.push({ header: "Playlist" })
            items.push({ label: "Remove from Playlist", run: () => report(catalog.removePlaylistItem(entry.path)) })
        }
        if (openable(entry)) {
            items.push({ header: "Presentation" })
            items.push({ label: "Rebuild Thumbnails", run: () => catalog.rebuildThumbnails(mediaOf(entry.file)) })
        }
        if (items.length > 0)
            menu.show(items, item)
    }

    // Opens the menu for a playlist or playlist folder. "Remove" is the word throughout:
    // only the list goes, never the presentations it refers to.
    function showPlaylistMenu(node, item) {
        const items = []
        if (node.folder) {
            items.push({ label: "New Playlist Here", run: () => newPlaylistNode(false, node.path) })
            items.push({ label: "New Folder Here", run: () => newPlaylistNode(true, node.path) })
        }
        items.push({ label: "Rename", run: () => playlistList.editingPath = node.path })
        if (!node.folder) {
            // Every media file that any of the playlist's presentations triggers
            items.push({
                label: "Rebuild Thumbnails",
                run: () => catalog.rebuildThumbnails(catalog.playlistItems(node.path).filter(openable)
                                                            .flatMap(row => mediaOf(row.file)))
            })
        }
        // Removing asks once more, by swapping the menu for a confirmation.
        items.push({
            label: node.folder ? "Remove Folder…" : "Remove Playlist…",
            run: () => menu.show([
                { header: "Remove “" + node.name + "”?" },
                { note: node.folder ? "The playlists in it go too. Presentations stay in their libraries."
                                    : "Its presentations stay in their libraries." },
                { label: "Remove", danger: true, run: () => report(catalog.removePlaylist(node.path)) },
                { label: "Cancel", run: () => {} }
            ], item)
        })
        menu.show(items, item)
    }

    // For the self-test: the playlist with the most rows, if there are any playlists.
    function openBusiestPlaylist() {
        let busiest = ""
        let most = 0
        for (const node of catalog.playlists) {
            const rows = node.folder ? 0 : catalog.playlistItems(node.path).length
            if (rows > most) {
                most = rows
                busiest = node.path
            }
        }
        if (busiest !== "")
            openPlaylist(busiest)
    }

    // Puts a slide on the slide layer, and any media its cue triggers on the media layer.
    function goLive(index) {
        if (!document || index < 0 || index >= document.slides.length)
            return
        const slide = document.slides[index]
        liveDocument = document
        liveIndex = index
        liveKey = documentKey
        livePlaylistId = playlistId
        cleared = false
        output.showSlide(slide)
        if (slide.media)
            showMedia(slide.media)
        grid.positionViewAtIndex(index, GridView.Contain)
    }

    // Puts media on the media layer. `playlist` is the media playlist it was picked
    // from, if it was.
    function showMedia(media, playlist = "") {
        liveMedia = media
        liveMediaPlaylistId = playlist
        output.showMedia(media)
    }

    function openMediaPlaylist(id) {
        selectedMediaNode = id
        mediaPlaylistId = id
        refreshLists()
    }

    // Where a new media playlist or folder goes, by the same rule as newNodeParent().
    function newMediaNodeParent() {
        const selected = catalog.mediaPlaylists.find(p => p.path === selectedMediaNode)
        return !selected ? "" : selected.folder ? selected.path : selected.parent
    }

    function newMediaNode(folder, parent) {
        const names = catalog.mediaPlaylists.map(p => p.name)
        const base = folder ? "New Folder" : "New Playlist"
        let name = base
        for (let n = 2; names.includes(name); ++n)
            name = base + " " + n
        const created = folder ? catalog.createMediaFolder(name, parent) : catalog.createMediaPlaylist(name, parent)
        if (!report(created.error))
            return
        if (folder)
            selectedMediaNode = created.id
        else
            openMediaPlaylist(created.id)
        mediaList.editingPath = created.id
    }

    function showMediaAddMenu(item) {
        const items = [
            { label: "Add Folder", run: () => newMediaNode(true, newMediaNodeParent()) },
            { label: "Add Playlist", run: () => newMediaNode(false, newMediaNodeParent()) }
        ]
        if (mediaPlaylistId !== "")
            items.push({ label: "Add Media…", run: () => mediaDialog.open() })
        menu.show(items, item)
    }

    function showMediaNodeMenu(node, item) {
        const items = []
        if (node.folder) {
            items.push({ label: "New Playlist Here", run: () => newMediaNode(false, node.path) })
            items.push({ label: "New Folder Here", run: () => newMediaNode(true, node.path) })
        }
        items.push({ label: "Rename", run: () => mediaList.editingPath = node.path })
        if (!node.folder) {
            items.push({
                label: "Rebuild Thumbnails",
                run: () => catalog.rebuildThumbnails(catalog.mediaIn(node.path).filter(m => !m.missing).map(m => m.path))
            })
        }
        items.push({
            label: node.folder ? "Remove Folder…" : "Remove Playlist…",
            run: () => menu.show([
                { header: "Remove “" + node.name + "”?" },
                { note: node.folder ? "The playlists in it go too. Media files stay on disk."
                                    : "Its media files stay on disk." },
                { label: "Remove", danger: true, run: () => report(catalog.removeMediaPlaylist(node.path)) },
                { label: "Cancel", run: () => {} }
            ], item)
        })
        menu.show(items, item)
    }

    // For the self-test: the first media of the first media playlist that has any here.
    function showFirstMedia() {
        for (const node of catalog.mediaPlaylists) {
            const first = node.folder ? undefined : catalog.mediaIn(node.path).find(m => !m.missing)
            if (first) {
                openMediaPlaylist(node.path)
                showMedia(first, node.path)
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
        liveMediaPlaylistId = ""
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
        sourcesHeight = Number(saved("sourcesHeight", sourcesHeight))
        thumbnailWidth = Math.max(smallestThumbnail, Math.min(largestThumbnail,
                                  Number(saved("thumbnailWidth", thumbnailWidth))))
        mediaThumbnailWidth = Math.max(smallestMediaThumbnail, Math.min(largestMediaThumbnail,
                                       Number(saved("mediaThumbnailWidth", mediaThumbnailWidth))))
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

        restoreSelections()
        restored = true
        save("workspace", catalog.workspacePath)
    }

    // What is selected is remembered for each workspace by name. Paths inside the
    // workspace are kept relative to it, so they still hold if its folder is moved.
    function selectionKey(key) {
        return "workspaces/" + catalog.workspaceName + "/" + key
    }

    function saveSelection(key, value) {
        const inside = catalog.workspacePath + "/"
        save(selectionKey(key), value.startsWith(inside) ? value.substring(inside.length) : value)
    }

    function savedSelection(key) {
        if (!remember)
            return ""
        const value = String(settings.value(selectionKey(key), ""))
        return value.includes("/") && !value.startsWith("/") ? catalog.workspacePath + "/" + value : value
    }

    // Selects what was selected last time in the open workspace, where it is still
    // there, and otherwise the first library, presentation and media playlist.
    function restoreSelections() {
        const library = savedSelection("library")
        const playlist = savedSelection("playlist")
        const presentation = savedSelection("presentation")
        const mediaPlaylist = savedSelection("mediaPlaylist")
        libraryPath = catalog.libraries.some(l => l.path === library) ? library
                    : catalog.libraries.length > 0 ? catalog.libraries[0].path : ""
        if (catalog.playlists.some(p => p.path === playlist && !p.folder)) {
            playlistId = playlist
            selectedNode = playlist
        }
        refreshLists()
        if (documents.some(d => d.path === presentation && openable(d)))
            openDocument(presentation)
        else
            openFirst()
        const firstMediaPlaylist = catalog.mediaPlaylists.find(p => !p.folder)
        openMediaPlaylist(catalog.mediaPlaylists.some(p => p.path === mediaPlaylist && !p.folder) ? mediaPlaylist
                          : firstMediaPlaylist ? firstMediaPlaylist.path : "")
    }

    // Closes the open workspace and opens another: the output is cleared, everything
    // shown is reloaded from the other folder, and what was selected there last time is
    // selected again.
    function switchWorkspace(path) {
        if (path === catalog.workspacePath)
            return
        clearAll()
        // Nothing is saved while the selections are in between the two workspaces.
        restored = false
        liveDocument = null
        liveIndex = -1
        liveKey = ""
        livePlaylistId = ""
        document = null
        documentKey = ""
        playlistId = ""
        selectedNode = ""
        libraryPath = ""
        mediaPlaylistId = ""
        selectedMediaNode = ""
        notice = ""
        catalog.openWorkspace(path)
        restoreSelections()
        restored = true
        save("workspace", path)
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
    onSourcesHeightChanged: save("sourcesHeight", sourcesHeight)
    onThumbnailWidthChanged: save("thumbnailWidth", thumbnailWidth)
    onMediaThumbnailWidthChanged: save("mediaThumbnailWidth", mediaThumbnailWidth)
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
        saveSelection("library", libraryPath)
    }
    onPlaylistIdChanged: saveSelection("playlist", playlistId)
    onDocumentKeyChanged: saveSelection("presentation", documentKey)
    onMediaPlaylistIdChanged: saveSelection("mediaPlaylist", mediaPlaylistId)

    Settings {
        id: settings
    }

    Connections {
        target: win.catalog

        // Keep each selection if it is still on disk; otherwise fall back to the first.
        function onChanged() {
            win.refreshLists()
            const firstLibrary = win.catalog.libraries.length > 0 ? win.catalog.libraries[0].path : ""
            if (win.playlistId !== "" && !win.catalog.playlists.some(p => p.path === win.playlistId))
                win.openLibrary(firstLibrary)
            else if (win.playlistId === "" && !win.catalog.libraries.some(l => l.path === win.libraryPath))
                win.openLibrary(firstLibrary)
            else if (!win.documents.some(d => d.path === win.documentKey && win.openable(d)))
                win.openFirst()
            if (!win.catalog.playlists.some(p => p.path === win.selectedNode))
                win.selectedNode = win.playlistId
            if (!win.catalog.mediaPlaylists.some(p => p.path === win.mediaPlaylistId && !p.folder)) {
                const first = win.catalog.mediaPlaylists.find(p => !p.folder)
                win.openMediaPlaylist(first ? first.path : "")
            } else if (!win.catalog.mediaPlaylists.some(p => p.path === win.selectedMediaNode)) {
                win.selectedMediaNode = win.mediaPlaylistId
            }
        }
    }

    Connections {
        target: win.catalog

        function onImportingChanged() {
            if (win.catalog.importing) {
                win.notice = "Importing the playlist…"
                win.noticeIsError = false
            }
        }

        function onImportFinished(error, summary, playlist) {
            if (!win.report(error))
                return
            win.notice = "Imported " + summary + "."
            win.noticeIsError = false
            if (playlist !== "")
                win.openPlaylist(playlist)
        }
    }

    // Presentations go into the library last browsed, playlists where a new one would.
    FileDialog {
        id: importDialog

        title: "Import Playlist"
        nameFilters: ["ProPresenter playlists (*.proplaylist)", "All files (*)"]
        onAccepted: win.catalog.importPlaylist(selectedFile, win.libraryPath, win.newNodeParent())
    }

    // Files are added to the media playlist being browsed, where they are on disk.
    FileDialog {
        id: mediaDialog

        title: "Add Media"
        fileMode: FileDialog.OpenFiles
        currentFolder: "file://" + win.catalog.mediaDirectory
        nameFilters: ["Images and videos (*.mp4 *.mov *.m4v *.mkv *.webm *.avi *.jpg *.jpeg *.png *.webp *.bmp *.gif)",
                      "All files (*)"]
        onAccepted: win.report(win.catalog.addMedia(win.mediaPlaylistId, selectedFiles))
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

        // Always drawn, so the panes' edges read as something to drag, but quietly.
        Rectangle {
            anchors.centerIn: parent
            width: divider.vertical ? 3 : parent.width
            height: divider.vertical ? parent.height : 3
            color: divider.containsMouse || divider.pressed ? "#a0a3aa" : "#5d6068"
        }
    }

    // The body of a pop-up menu: a rounded panel a shade lighter than the panes, with a
    // bright edge and a shadow, so it stands clear of whatever it opens over.
    component MenuBackground: Item {
        RectangularShadow {
            anchors.fill: panel
            radius: panel.radius
            blur: 24
            spread: 2
            offset: Qt.vector2d(0, 6)
            color: "#b0000000"
        }

        Rectangle {
            id: panel

            anchors.fill: parent
            radius: 9
            color: "#33363d"
            border.width: 1.5
            border.color: "#8b8f98"
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

            // Libraries and playlists share one pane, a shade lighter than the
            // presentations below it, that scrolls as a whole. Drag the Presentations
            // header to resize it.
            Rectangle {
                id: sources

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.max(90, Math.min(win.sourcesHeight, sidebar.height - presentationsHeader.height - 90))
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
                            color: win.librariesColor
                            text: "Libraries"
                        }

                        SidebarList {
                            id: libraryList

                            width: parent.width
                            height: contentHeight
                            interactive: false
                            model: win.catalog.libraries
                            selectedPath: win.selectedNode === "" ? win.libraryPath : ""
                            livePath: !win.cleared && win.liveDocument && win.livePlaylistId === ""
                                      ? win.liveDocument.path.substring(0, win.liveDocument.path.lastIndexOf("/")) : ""
                            onPicked: (entry) => win.openLibrary(entry.path)
                        }

                        // Playlists and the folders they are kept in, in ProPresenter's
                        // own playlists file. Drop a presentation on a playlist to add it.
                        Item {
                            width: parent.width
                            height: playlistsTitle.height

                            SectionTitle {
                                id: playlistsTitle

                                color: win.playlistsColor
                                text: "Playlists"
                            }

                            // Add a folder or a playlist, or import a playlist
                            Rectangle {
                                id: addButton

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
                                    color: win.textColor
                                    font.pixelSize: 16
                                    text: "+"
                                }

                                MouseArea {
                                    id: addMouse

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: win.showAddMenu(addButton)
                                }
                            }
                        }

                        SidebarList {
                            id: playlistList

                            width: parent.width
                            height: contentHeight
                            interactive: false
                            model: win.catalog.playlists
                            selectedPath: win.selectedNode
                            livePath: win.cleared ? "" : win.livePlaylistId
                            dragProxy: playlistDrag
                            dropKeys: ["presentation", "playlist"]
                            // A presentation goes onto a playlist. A playlist or folder
                            // goes between the others, or onto a folder to go inside it.
                            dropZone: (node, source) => source === presentationDrag ? (node.folder ? "" : "onto")
                                                      : node.folder ? "both" : "between"
                            onPicked: (entry) => {
                                if (entry.folder)
                                    win.selectedNode = entry.path
                                else
                                    win.openPlaylist(entry.path)
                            }
                            onMenuRequested: (entry, item) => win.showPlaylistMenu(entry, item)
                            onRenamed: (entry, name) => win.report(win.catalog.renamePlaylist(entry.path, name))
                            onEditingEnded: keys.forceActiveFocus()
                            onDropped: (node, source, where) => {
                                if (source === playlistDrag)
                                    win.report(win.catalog.movePlaylistNode(source.entry.path, node.path, where))
                                else if (win.openable(source.entry))
                                    win.addToPlaylist(node.path, source.entry.file)
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
                        const source = win.playlistId !== ""
                            ? win.catalog.playlists.find(p => p.path === win.playlistId)
                            : win.catalog.libraries.find(l => l.path === win.libraryPath)
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
                onMoved: (delta) => win.sourcesHeight = sources.height + delta
            }

            // In a playlist, rows can be dragged up and down to reorder them.
            SidebarList {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: presentationsHeader.bottom
                anchors.bottom: parent.bottom
                model: win.documents
                selectedPath: win.documentKey
                livePath: !win.cleared && win.livePlaylistId === win.playlistId ? win.liveKey : ""
                dragProxy: presentationDrag
                draggable: (entry) => entry.playlistItem || win.openable(entry)
                dropKeys: win.playlistId !== "" ? ["presentation"] : []
                dropZone: (entry, source) => "between"
                onPicked: (entry) => {
                    if (entry.missing)
                        win.report("“" + entry.name + "” is in the playlist but not in the libraries here.")
                    else
                        win.openEntry(entry)
                }
                onMenuRequested: (entry, item) => win.showPresentationMenu(entry, item)
                onDropped: (target, source, where) => {
                    if (source.entry.playlistItem && source.entry.path !== target.path)
                        win.report(win.catalog.movePlaylistItem(source.entry.path, target.path, where === "after"))
                }
            }

            // The one right-click menu of the sidebar. show() takes its rows: a row with
            // `header` is a caption; any other has a `label` and a `run` function, and
            // may be marked `current` (ticked) or `danger` (red).
            Popup {
                id: menu

                property var items: []
                // Whether the rows come in sections under captions. If so the rows sit in
                // from the captions, with room at their left for the tick on a current one.
                readonly property bool sectioned: items.some(item => item.header !== undefined)

                function show(items, item) {
                    close()
                    menu.items = items
                    parent = item
                    open()
                }

                // Under the row it was opened from, or under a small button, ending at the
                // button's right edge. `margins` then keeps the whole menu inside the
                // window whatever that works out to, and a menu taller than the window
                // scrolls.
                x: parent && parent.width < 60 ? parent.width - width : 24
                y: parent ? parent.height - 2 : 0
                width: 250
                margins: 6
                padding: 6
                // Takes the keyboard while open, so that Esc closes it.
                focus: true
                onClosed: {
                    if (playlistList.editingPath === "" && mediaList.editingPath === "")
                        keys.forceActiveFocus()
                }

                background: MenuBackground {}

                contentItem: Flickable {
                    implicitHeight: Math.min(menuRows.height, win.height - 40)
                    contentHeight: menuRows.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: menuRows

                        Repeater {
                            model: menu.items

                            delegate: Item {
                                id: row

                                required property var modelData
                                required property int index
                                readonly property bool note: modelData.note !== undefined
                                readonly property bool heading: modelData.header !== undefined
                                readonly property bool caption: heading || note
                                // A gap above each section after the first sets it apart.
                                readonly property real gap: heading && index > 0 ? 6 : 0
                                readonly property real indent: menu.sectioned && !caption ? 30 : 12

                                width: menu.availableWidth
                                height: gap + (note ? noteText.implicitHeight + 10 : caption ? 26 : 30)

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.topMargin: row.gap
                                    radius: 4
                                    color: row.heading ? "#484b53"
                                         : !row.caption && rowMouse.containsMouse ? "#565962" : "transparent"
                                }

                                Text {
                                    id: noteText

                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 10
                                    visible: row.note
                                    verticalAlignment: Text.AlignVCenter
                                    wrapMode: Text.Wrap
                                    color: win.dimTextColor
                                    font.pixelSize: 12
                                    text: row.note ? row.modelData.note : ""
                                }

                                // The tick of the current row, in the space the indent leaves
                                Text {
                                    x: 12
                                    anchors.verticalCenter: label.verticalCenter
                                    visible: !row.caption && row.modelData.current === true
                                    color: win.accentColor
                                    font.pixelSize: 13
                                    text: "✓"
                                }

                                Text {
                                    id: label

                                    anchors.fill: parent
                                    anchors.topMargin: row.gap
                                    anchors.leftMargin: row.indent
                                    anchors.rightMargin: 10
                                    visible: !row.note
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    color: row.heading ? "#d5d7dc"
                                         : row.modelData.danger ? "#ff6b6b"
                                         : row.modelData.current ? win.accentColor : win.textColor
                                    font.pixelSize: row.heading ? 11 : 14
                                    font.bold: row.heading
                                    font.capitalization: row.heading ? Font.AllUppercase : Font.MixedCase
                                    text: row.note ? "" : row.heading ? row.modelData.header : row.modelData.label
                                }

                                MouseArea {
                                    id: rowMouse

                                    anchors.fill: parent
                                    anchors.topMargin: row.gap
                                    hoverEnabled: true
                                    enabled: !row.caption
                                    onClicked: {
                                        const run = row.modelData.run
                                        menu.close()
                                        run()
                                    }
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

            // The workspace: the folder everything shown comes from. Picking another
            // reloads the app from that one.
            Rectangle {
                id: workspacePicker

                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: workspaceRow.width + 14
                height: 38
                radius: 8
                color: "#23252b"
                border.width: 1
                border.color: "#3a3c42"

                Row {
                    id: workspaceRow

                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        leftPadding: 4
                        color: win.dimTextColor
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        text: "Workspace"
                    }

                    AppComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 180
                        height: 28
                        font.pixelSize: 13
                        model: win.catalog.workspaces.map(w => w.name)
                        currentIndex: win.catalog.workspaces.findIndex(w => w.path === win.catalog.workspacePath)
                        onActivated: (index) => win.switchWorkspace(win.catalog.workspaces[index].path)
                    }
                }
            }

            Text {
                anchors.left: workspacePicker.right
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

                // The transition and its length, grouped on a panel of their own
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: transitionControls.width + 12
                    height: 38
                    radius: 8
                    color: "#23252b"
                    border.width: 1
                    border.color: "#3a3c42"

                    Row {
                        id: transitionControls

                        anchors.centerIn: parent
                        spacing: 6

                        AppComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 150
                            height: 28
                            font.pixelSize: 13
                            model: win.transitions.map(t => t.name)
                            currentIndex: win.transitionIndex
                            onActivated: (index) => win.transitionIndex = index
                        }

                        AppSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 100
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
                            width: 42
                            height: 28
                            leftPadding: 4
                            rightPadding: 6
                            font.pixelSize: 13
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
                            rightPadding: 2
                            color: win.dimTextColor
                            font.pixelSize: 12
                            text: "s"
                        }
                    }
                }

                // Sets the transition controls apart from the buttons that follow
                Item {
                    width: 14
                    height: 1
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
                        source: visible ? win.thumbnailUrl(win.liveMedia.path) : ""
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
                color: win.noticeIsError ? "#ff6b6b" : win.dimTextColor
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
            // The choice is saved in the presentation file, or for a playlist row in the
            // playlist.
            AppComboBox {
                id: arrangementBox

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                visible: win.document !== null && win.document.arrangements.length > 0
                model: win.document ? ["Master"].concat(win.document.arrangements) : []
                currentIndex: win.document && win.document.arrangement !== ""
                              ? win.document.arrangements.indexOf(win.document.arrangement) + 1 : 0
                onActivated: (index) => win.setArrangement(win.currentEntry(), index === 0 ? "" : model[index])
            }
        }

        // Slides
        GridView {
            id: grid

            objectName: "slideGrid"

            readonly property int columns: Math.max(1, Math.floor(width / win.thumbnailWidth))
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
                            source: visible ? win.thumbnailUrl(cell.modelData.media.path) : ""
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
                    onDropped: (drop) => {
                        if (!drop.source.media.missing)
                            win.assignMedia(cell.index, drop.source.media)
                    }
                }
            }
        }

        // Thumbnail size, over the bottom right corner of the slides
        Row {
            anchors.right: grid.right
            anchors.bottom: grid.bottom
            anchors.rightMargin: 22
            anchors.bottomMargin: 12
            spacing: 8
            visible: win.document !== null && win.document.slides.length > 0

            component ZoomButton: Rectangle {
                id: zoomButton

                property alias text: zoomLabel.text
                property bool available: true

                signal clicked

                width: 26
                height: 26
                radius: 13
                color: zoomMouse.pressed ? "#6a6d75" : "#3a3c42"
                border.width: 1
                border.color: "#6c6f75"
                opacity: !available ? 0.3 : zoomMouse.containsMouse ? 1 : 0.7

                Text {
                    id: zoomLabel

                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -1
                    color: win.textColor
                    font.pixelSize: 17
                }

                MouseArea {
                    id: zoomMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: zoomButton.available
                    onClicked: zoomButton.clicked()
                }
            }

            ZoomButton {
                text: "−"
                available: win.thumbnailWidth > win.smallestThumbnail
                onClicked: win.zoomThumbnails(-1)
            }

            ZoomButton {
                text: "+"
                available: win.thumbnailWidth < win.largestThumbnail && grid.columns > 1
                onClicked: win.zoomThumbnails(1)
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
            margins: 6
            padding: 6
            focus: true
            onClosed: keys.forceActiveFocus()

            background: MenuBackground {}

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
                    source: mediaDrag.media ? win.thumbnailUrl(mediaDrag.media.path) : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
            }
        }

        // What is being dragged out of a sidebar list: a point that follows the pointer,
        // with the name of the dragged entry beside it.
        component RowDrag: Item {
            id: rowDrag

            property var entry: null

            z: 50

            Rectangle {
                x: 12
                y: 8
                width: dragLabel.implicitWidth + 20
                height: 28
                radius: 6
                visible: rowDrag.Drag.active
                color: win.surfaceColor
                border.width: 1
                border.color: win.accentColor
                opacity: 0.95

                Text {
                    id: dragLabel

                    anchors.centerIn: parent
                    color: win.textColor
                    font.pixelSize: 13
                    text: rowDrag.entry ? rowDrag.entry.name : ""
                }
            }
        }

        RowDrag {
            id: presentationDrag

            Drag.keys: ["presentation"]
        }

        RowDrag {
            id: playlistDrag

            Drag.keys: ["playlist"]
        }

        RowDrag {
            id: mediaPlaylistDrag

            Drag.keys: ["mediaPlaylist"]
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

            // Add a media folder or playlist, or media to the playlist being browsed
            Rectangle {
                id: mediaAddButton

                x: sidebar.width - width - 12
                anchors.bottom: mediaTitle.bottom
                anchors.bottomMargin: 3
                width: 26
                height: 22
                radius: 5
                color: mediaAddMouse.pressed ? "#5c5f67" : mediaAddMouse.containsMouse ? "#53565e" : "#474a51"

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -1
                    color: win.textColor
                    font.pixelSize: 16
                    text: "+"
                }

                MouseArea {
                    id: mediaAddMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: win.showMediaAddMenu(mediaAddButton)
                }
            }

            // Media playlists and the folders they are kept in, in ProPresenter's own
            // media playlists file. They behave as the playlists of presentations do:
            // picking a playlist shows its media, picking a folder only selects it.
            SidebarList {
                id: mediaList

                anchors.left: parent.left
                anchors.top: mediaTitle.bottom
                anchors.bottom: parent.bottom
                width: sidebar.width
                model: win.catalog.mediaPlaylists
                selectedPath: win.selectedMediaNode
                livePath: win.liveMedia ? win.liveMediaPlaylistId : ""
                dragProxy: mediaPlaylistDrag
                dropKeys: ["mediaPlaylist"]
                dropZone: (node, source) => node.folder ? "both" : "between"
                onPicked: (entry) => {
                    if (entry.folder)
                        win.selectedMediaNode = entry.path
                    else
                        win.openMediaPlaylist(entry.path)
                }
                onMenuRequested: (entry, item) => win.showMediaNodeMenu(entry, item)
                onRenamed: (entry, name) => win.report(win.catalog.renameMediaPlaylist(entry.path, name))
                onEditingEnded: keys.forceActiveFocus()
                onDropped: (node, source, where) => win.report(win.catalog.moveMediaPlaylist(source.entry.path, node.path, where))
            }

            GridView {
                id: mediaGrid

                readonly property real labelHeight: 24
                readonly property int columns: Math.max(1, Math.floor(width / win.mediaThumbnailWidth))

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
                                source: mediaCell.modelData.missing ? ""
                                      : win.thumbnailUrl(mediaCell.modelData.path)
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
                            color: mediaCell.modelData.missing ? win.dimTextColor : win.textColor
                            font.pixelSize: 11
                            text: mediaCell.modelData.name
                        }
                    }

                    // Dropping another of the playlist's media here moves it to this
                    // place: before this one from the left half, after it from the right.
                    DropArea {
                        id: reorderDrop

                        property bool after: false
                        readonly property bool moving: containsDrag && mediaDrag.media !== null
                                                       && mediaDrag.media.id !== mediaCell.modelData.id

                        anchors.fill: parent
                        keys: ["media"]
                        onEntered: (drag) => after = drag.x > width / 2
                        onPositionChanged: (drag) => after = drag.x > width / 2
                        onDropped: (drop) => {
                            if (drop.source.media.id === mediaCell.modelData.id)
                                return
                            // The grid would otherwise jump back to the top.
                            const scrolledTo = mediaGrid.contentY
                            win.report(win.catalog.moveMediaItem(drop.source.media.id, mediaCell.modelData.id, after))
                            mediaGrid.contentY = scrolledTo
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
                            mediaDrag.Drag.active = false
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
                            const at = mapToItem(keys, mouse.x, mouse.y)
                            mediaDrag.x = at.x
                            mediaDrag.y = at.y
                            if (!dragging && Math.abs(mouse.x - pressedAt.x) + Math.abs(mouse.y - pressedAt.y) > 10) {
                                dragging = true
                                mediaDrag.media = mediaCell.modelData
                                mediaDrag.Drag.active = true
                            }
                        }
                        onReleased: (mouse) => {
                            if (dragging) {
                                mediaDrag.Drag.drop()
                                finish()
                            } else if (mouse.button === Qt.RightButton) {
                                const id = mediaCell.modelData.id
                                menu.show([{ label: "Remove from Playlist",
                                             run: () => win.report(win.catalog.removeMediaItem(id)) }], mediaCell)
                            } else if (!mediaCell.modelData.missing) {
                                win.showMedia(mediaCell.modelData, win.mediaPlaylistId)
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
                text: win.mediaPlaylistId !== "" ? "This media playlist is empty. Add media to it from the + above."
                    : "No media playlists yet. Add one from the + beside Media bin."
            }

            // Thumbnail size, over the bottom right corner of the media
            Row {
                anchors.right: mediaGrid.right
                anchors.bottom: mediaGrid.bottom
                anchors.rightMargin: 22
                anchors.bottomMargin: 10
                spacing: 8
                visible: win.mediaFiles.length > 0

                ZoomButton {
                    text: "−"
                    available: win.mediaThumbnailWidth > win.smallestMediaThumbnail
                    onClicked: win.zoomMediaThumbnails(-1)
                }

                ZoomButton {
                    text: "+"
                    available: win.mediaThumbnailWidth < win.largestMediaThumbnail && mediaGrid.columns > 1
                    onClicked: win.zoomMediaThumbnails(1)
                }
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
