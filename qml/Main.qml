import QtCore
import QtQuick
import QtQuick.Dialogs
import QtQuick.Window
import SimplePresenterApp

// The operator window, and the heart of the app.
//
// What is on screen. A toolbar across the top stands in for the title bar (Toolbar).
// Below it are libraries, playlists and their presentations on the left (Sidebar); the
// slides of the presentation being viewed as a grid of thumbnails in the middle
// (SlideGrid), with the media bin under those two (MediaBin); and down the whole of the
// right the previews, the clear buttons, the transport and the show controls
// (PreviewPanel). In editor mode everything below the toolbar gives way to the editor
// (Editor). The output and stage windows belong to this one too.
//
// How it is organised. This file is the state and the logic; the files named above are
// the views. Everything the app knows about the show is a property here: what is being
// browsed, which presentation is open, what is live on each layer of the output, the
// chosen transition, the sizes of the panes. Everything that can be done is a function
// here: goLive(), showMedia(), clearSlide(), openPlaylist(), and so on. Each view is
// handed this window as `win`, shows what its properties say and calls its functions;
// no view holds state that another needs, and none changes the output itself.
//
// Where the data comes from. `catalog` (src/catalog.h) is the workspace on disk: its
// lists are plain arrays of maps, and a presentation it opens is a map with a `slides`
// array, each slide a map of everything needed to draw it (src/proconvert.h). Nothing
// here parses a file. A change the user makes goes to the catalog as a function call
// that writes the file and answers with an error message, empty if it worked.
//
// Going live is small. goLive(index) records which slide of which presentation is live
// and hands that slide's map to the output window's slide layer; showMedia() does the
// same for the media layer. The output draws what it is handed (Output.qml,
// TransitionLayer.qml); the previews here draw the same maps again, small.
//
// What a slide's cue does besides showing the slide happens in goLive() too. Its media
// goes to the media layer as a background, which stays while other slides come and go
// and is not started again by a slide that brings the same one, or as a foreground,
// which the next slide takes off (alreadyPlaying() and goLive() are all there is to
// that). And what it does to a timer is passed to the timers (`Timers`, src/timers.h),
// which belong to no window: whatever shows a timer asks them.
Window {
    id: win

    // Set from main.cpp
    required property Catalog catalog
    property int outputScreen: -1
    // Whether to restore the last session's selections and layout, and save this one's
    property bool remember: true
    // Whether what shows a time is held still: the timers, and the transport. For the
    // self-test, whose pictures must be the same whenever it is run and however long
    // it takes over it.
    property bool clocksHeld: false
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
    // What is on the media layer: { name, path, source, video, foreground, loops,
    // retriggers }, or null, and the media playlist it was triggered from, "" if a slide
    // triggered it. The last three are how it behaves (see goLive() and alreadyPlaying()).
    property var liveMedia: null
    property string liveMediaPlaylistId: ""

    // The transitions there are to choose from, with what can be adjusted about each
    // (see TransitionCatalogue), the one chosen, and what has been chosen for its
    // options and those of the others: a map of transition names to maps of option names
    // to values. An option that is not in it is as the catalogue has it.
    readonly property var transitions: transitionCatalogue.transitions
    readonly property var transitionCategories: transitionCatalogue.categories
    property int transitionIndex: 1
    property var transitionChoices: ({})
    readonly property var transition: transitions[transitionIndex]
    // What the chosen transition's shader is handed for its options
    readonly property var transitionUniforms: transitionCatalogue.uniforms(transition, transitionChoices[transition.name])
    // Seconds
    property real transitionDuration: 0.6
    property bool mediaBinVisible: true
    property bool outputEnabled: true
    property bool stageEnabled: true
    property bool settingsOpen: false
    // Whether the editor is up, in place of the slides, working on the presentation
    // being viewed
    property bool editing: false
    // Set if the workspace changed on disk while the editor was up: the lists are
    // brought up to date when it comes down, not after every change it saves.
    property bool listsStale: false
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
    // The item the pop-up menu is open for, or null while it is shut
    readonly property var menuItem: menu.opened ? menu.parent : null

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
        // It behaves on the slide as it did where it was dragged from, until changed.
        const error = media ? catalog.setSlideMedia(path, id, media.path, media.foreground === true)
                            : catalog.removeSlideMedia(path, id)
        if (report(error))
            reloadDocument()
    }

    // Makes the media a slide triggers a background or a foreground.
    function setSlideMediaForeground(index, foreground) {
        if (report(catalog.setSlideMediaForeground(document.path, document.slides[index].id, foreground)))
            reloadDocument()
    }

    // Reads the presentation being viewed again after a change to it that leaves it the
    // same slides in the same order: the grid can stay where it is scrolled to, and the
    // live slide keeps its index.
    function reloadDocument() {
        notice = ""
        const scrolledTo = grid.contentY
        const reloaded = load(currentEntry())
        if (viewingLive)
            liveDocument = reloaded
        document = reloaded
        grid.contentY = scrolledTo
    }

    // Makes a row of the media playlist being browsed a background or a foreground. The
    // media bin stays scrolled where it was, as when a row is moved.
    function setMediaItemForeground(id, foreground) {
        const scrolledTo = mediaBin.contentY
        report(catalog.setMediaItemForeground(id, foreground))
        mediaBin.contentY = scrolledTo
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
        thumbnailWidth = steppedThumbnail(thumbnailWidth, direction, grid.gridWidth, grid.columns,
                                          smallestThumbnail, largestThumbnail)
    }

    function zoomMediaThumbnails(direction) {
        mediaThumbnailWidth = steppedThumbnail(mediaThumbnailWidth, direction, mediaBin.gridWidth, mediaBin.columns,
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

    // Gives the keyboard back to whatever drives the show: the editor while it is up,
    // and otherwise the item that takes the arrow and function keys.
    function takeFocus() {
        if (editing)
            editScreen.takeFocus()
        else
            keys.forceActiveFocus()
    }

    // Opens the pop-up menu by an item, or at a point of it if one is given. `items` is
    // as PopupMenu describes.
    function showMenu(items, item, x, y) {
        menu.show(items, item, x, y)
    }

    // Moves a file of the media playlist being browsed to just before another, or with
    // `after` just after it. The media bin stays scrolled where it was: the change
    // rebuilds its grid, which would otherwise jump back to the top.
    function moveMedia(sourceId, targetId, after) {
        const scrolledTo = mediaBin.contentY
        report(catalog.moveMediaItem(sourceId, targetId, after))
        mediaBin.contentY = scrolledTo
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
        sidebar.renamePlaylist(created.id)
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
            items.push({ label: "Edit", run: () => startEditing(entry) })
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
        items.push({ label: "Rename", run: () => sidebar.renamePlaylist(node.path) })
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

    // Brings up the editor on a presentation: at the slide with the given id, if one is
    // given, and otherwise at the slide that is live if that presentation is the live one.
    function startEditing(entry, slideId) {
        if (editing || !entry || !openable(entry))
            return
        if (documentKey !== entry.path)
            openEntry(entry)
        const slide = slideId ?? (viewingLive && liveIndex >= 0 && liveIndex < document.slides.length
                                  ? document.slides[liveIndex].id : "")
        if (!report(editScreen.open(entry.file, catalog.workspacePath, slide)))
            return
        notice = ""
        editing = true
        editScreen.takeFocus()
        // A cue that only triggers media has no slide to work on.
        if (slide !== "" && editScreen.editor.rowOf(slide) < 0)
            editScreen.tell(editScreen.editor.count === 0
                            ? "This presentation's slides only trigger media, so there is nothing on them to edit."
                            : "That slide only triggers media, so there is nothing on it to edit. This is the first slide that has something.")
    }

    // Opens the menu for a slide of the grid, at a point of its cell.
    function showSlideMenu(index, item, x, y) {
        const slide = document.slides[index]
        const none = slide.mediaName === ""
        menu.show([
            { label: "Edit", run: () => startEditing(currentEntry(), slide.id) },
            { header: "Media" },
            { label: "Background", current: !none && !slide.mediaForeground, disabled: none,
              run: () => setSlideMediaForeground(index, false) },
            { label: "Foreground", current: !none && slide.mediaForeground, disabled: none,
              run: () => setSlideMediaForeground(index, true) },
            { label: "Remove Media", disabled: none, run: () => assignMedia(index, null) }
        ], item, x, y)
    }

    // The menu of a file in the media bin.
    function showMediaItemMenu(media, item) {
        menu.show([
            { header: "Behaviour" },
            { label: "Background", current: !media.foreground, run: () => setMediaItemForeground(media.id, false) },
            { label: "Foreground", current: media.foreground, run: () => setMediaItemForeground(media.id, true) },
            { header: "Playlist" },
            { label: "Remove from Playlist", run: () => report(catalog.removeMediaItem(media.id)) }
        ], item)
    }

    // Takes the editor down and shows the presentation as it now is. What is on the
    // output is left as it is until a slide is next shown.
    function stopEditing() {
        if (!editing)
            return
        editScreen.finish()
        const changed = editScreen.editor.changed
        editScreen.close()
        editing = false
        keys.forceActiveFocus()
        if (listsStale) {
            listsStale = false
            followCatalog()
        }
        const entry = currentEntry()
        if (changed && entry) {
            // The same slides in the same order, so the grid can stay where it is.
            const scrolledTo = grid.contentY
            document = load(entry)
            grid.contentY = scrolledTo
        }
    }

    // The url of the thumbnail of the media a slide of the presentation being viewed
    // triggers, or "": what the editor shows behind that slide.
    function slideBackdrop(slideId) {
        const slide = document ? document.slides.find(s => s.id === slideId) : undefined
        return slide && slide.media ? thumbnailUrl(slide.media.path) : ""
    }

    // For the self-test: steps through bringing the editor up, picking the first
    // element with text, editing that text, and taking the editor down. Nothing is
    // changed, so nothing is written.
    function selfTestEditor(step) {
        if (step === 0) {
            startEditing(currentEntry())
        } else if (step === 3) {
            stopEditing()
        } else if (editing) {
            // On the first slide that has one, if the one being shown has none.
            const pickable = () => editScreen.canvas.elements.find(e => e.hasText && !e.locked && !e.hidden)
            for (let row = 0; !pickable() && row < editScreen.editor.count; ++row)
                editScreen.showRow(row)
            const first = pickable()
            if (first && step === 1)
                editScreen.canvas.pick(first.id)
            else if (first)
                editScreen.canvas.editText(first.id)
        }
    }

    // Chooses a transition by name. Two have changed their names to the ones ProPresenter
    // knows them by, and are still found by the old.
    function selectTransition(name) {
        const renamed = { "Cross Warp": "Warp Fade", "Dreamy": "Wave Dissolve" }
        const index = transitions.findIndex(t => t.name === (renamed[name] ?? name))
        if (index >= 0)
            transitionIndex = index
    }

    // The menu of transitions: Cut, then the rest under their categories, with the
    // chosen one ticked.
    function showTransitionMenu(item) {
        const items = []
        const rowsOf = category => transitions.forEach((t, index) => {
            if (t.category === category)
                items.push({ label: t.name, current: index === transitionIndex, run: () => transitionIndex = index })
        })
        rowsOf("")
        for (const category of transitionCategories) {
            items.push({ header: category })
            rowsOf(category)
        }
        // Upwards, as far as the toolbar: the transition's controls are at the bottom of
        // the slides.
        menu.showAbove(items, item, toolbar.height)
    }

    // What an option of the chosen transition is set to.
    function transitionOption(option) {
        return transitionCatalogue.valueOf(option, transitionChoices[transition.name])
    }

    // Sets an option of the chosen transition, for the next change and from then on.
    function setTransitionOption(option, value) {
        const choices = Object.assign({}, transitionChoices)
        choices[transition.name] = Object.assign({}, choices[transition.name])
        choices[transition.name][option.name] = value
        transitionChoices = choices
    }

    // Puts the options of the chosen transition back as the catalogue has them.
    function resetTransitionOptions() {
        const choices = Object.assign({}, transitionChoices)
        delete choices[transition.name]
        transitionChoices = choices
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
        // What the slide's cue does to timers, such as starting the countdown it shows
        for (const action of slide.timerActions)
            Timers.act(action)
        if (!slide.media) {
            // A foreground is for the moment it was triggered in: a slide that brings no
            // media of its own ends it. A background plays on.
            if (liveMedia !== null && liveMedia.foreground)
                clearMedia()
            output.showSlide(slide)
        } else if (alreadyPlaying(slide.media)) {
            output.showSlideOverMedia(slide)
        } else {
            liveMedia = slide.media
            liveMediaPlaylistId = ""
            output.showSlideWithMedia(slide, slide.media)
        }
        grid.positionViewAtIndex(index, GridView.Contain)
    }

    // Whether this media is a background that is the one already playing, which is then
    // left to play on and not started again (unless it is set always to start again).
    function alreadyPlaying(media) {
        return liveMedia !== null && !liveMedia.foreground && !media.foreground && !media.retriggers
               && media.path === liveMedia.path
    }

    // Puts media on the media layer. `playlist` is the media playlist it was picked
    // from, if it was.
    function showMedia(media, playlist = "") {
        const playing = alreadyPlaying(media)
        liveMedia = media
        liveMediaPlaylistId = playlist
        if (!playing)
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
        mediaBin.renamePlaylist(created.id)
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
        items.push({ label: "Rename", run: () => mediaBin.renamePlaylist(node.path) })
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
    title: (editing ? "Editing " : "") + (document ? document.name + " — SimplePresenter" : "SimplePresenter")

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
        selectTransition(String(saved("transition", "")))
        try {
            const stored = JSON.parse(String(saved("transitionOptions", "")))
            if (stored !== null && typeof stored === "object" && !Array.isArray(stored))
                transitionChoices = stored
        } catch (e) {
            // Nothing stored yet, or not readable: every option is as the catalogue has it.
        }
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

        const tab = String(saved("showControlTab", "timers"))
        if (["timers", "props", "stage"].includes(tab))
            sidePanel.showControlTab = tab
        report(Timers.open(catalog.workspacePath, clocksHeld))
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
        // The timers are the workspace's too.
        report(Timers.open(path, clocksHeld))
        restoreSelections()
        restored = true
        save("workspace", path)
    }

    // Whatever is still being typed in the editor goes into the file first.
    onClosing: {
        if (editing)
            editScreen.finish()
        Qt.quit()
    }

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
    // Not `transition.name`: that follows the index too, and may not have caught up yet.
    onTransitionIndexChanged: save("transition", transitions[transitionIndex].name)
    onTransitionChoicesChanged: save("transitionOptions", JSON.stringify(transitionChoices))
    onTransitionDurationChanged: save("transitionDuration", transitionDuration)
    onLibraryPathChanged: {
        refreshLists()
        saveSelection("library", libraryPath)
    }
    onPlaylistIdChanged: saveSelection("playlist", playlistId)
    onDocumentKeyChanged: saveSelection("presentation", documentKey)
    onMediaPlaylistIdChanged: saveSelection("mediaPlaylist", mediaPlaylistId)

    TransitionCatalogue {
        id: transitionCatalogue
    }

    Settings {
        id: settings
    }

    // Brings the lists up to date with the workspace on disk, keeping each selection if
    // it is still there and otherwise falling back to the first.
    function followCatalog() {
        refreshLists()
        const firstLibrary = catalog.libraries.length > 0 ? catalog.libraries[0].path : ""
        if (playlistId !== "" && !catalog.playlists.some(p => p.path === playlistId))
            openLibrary(firstLibrary)
        else if (playlistId === "" && !catalog.libraries.some(l => l.path === libraryPath))
            openLibrary(firstLibrary)
        else if (!documents.some(d => d.path === documentKey && openable(d)))
            openFirst()
        if (!catalog.playlists.some(p => p.path === selectedNode))
            selectedNode = playlistId
        if (!catalog.mediaPlaylists.some(p => p.path === mediaPlaylistId && !p.folder)) {
            const first = catalog.mediaPlaylists.find(p => !p.folder)
            openMediaPlaylist(first ? first.path : "")
        } else if (!catalog.mediaPlaylists.some(p => p.path === selectedMediaNode)) {
            selectedMediaNode = mediaPlaylistId
        }
    }

    Connections {
        target: win.catalog

        // Every change the editor saves is a change on disk; reading the lists again
        // for each would be a lot of work for nothing, so that waits until it is done.
        function onChanged() {
            if (win.editing)
                win.listsStale = true
            else
                win.followCatalog()
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
        nameFilters: [win.catalog.mediaDialogFilter, "All files (*)"]
        onAccepted: win.report(win.catalog.addMedia(win.mediaPlaylistId, selectedFiles))
    }

    Output {
        id: output

        owner: win
        remember: win.remember
        shown: win.outputEnabled
        fullScreenOn: win.outputScreen
        shader: transitionCatalogue.shaderUrl(win.transition)
        options: win.transitionUniforms.options
        tint: win.transitionUniforms.tint
        direction: win.transitionUniforms.direction
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

        Sidebar {
            id: sidebar

            anchors.left: parent.left
            anchors.top: toolbar.bottom
            anchors.bottom: mediaBin.top
            width: Math.max(160, Math.min(win.sidebarWidth, win.width - sidePanel.width - 260))
            win: win
            presentationDrag: presentationDrag
            playlistDrag: playlistDrag
        }

        Toolbar {
            id: toolbar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            win: win
        }

        // Down the whole of the right, to the bottom of the window: the media bin stops
        // at it, and does not take from it.
        PreviewPanel {
            id: sidePanel

            anchors.right: parent.right
            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            width: Math.max(250, Math.min(win.sidePanelWidth, win.width - 160 - 260))
            win: win
            liveVideoSink: output.liveVideoSink
            livePlayer: win.clocksHeld ? null : output.livePlayer
            onShowControlTabChanged: win.save("showControlTab", showControlTab)
        }

        SlideGrid {
            id: grid

            anchors.left: sidebar.right
            anchors.right: sidePanel.left
            anchors.top: toolbar.bottom
            anchors.bottom: mediaBin.top
            win: win
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

        // What is being dragged out of each of the lists. They live here, over all the
        // panes, because a drag starts in one pane and ends in another.
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
            onReleased: win.takeFocus()
        }

        Divider {
            anchors.horizontalCenter: sidePanel.left
            anchors.top: sidePanel.top
            anchors.bottom: sidePanel.bottom
            onMoved: (delta) => win.sidePanelWidth = sidePanel.width - delta
            onReleased: win.takeFocus()
        }

        Divider {
            vertical: false
            anchors.verticalCenter: mediaBin.top
            anchors.left: parent.left
            anchors.right: sidePanel.left
            visible: win.mediaBinVisible
            onMoved: (delta) => win.mediaBinHeight = mediaBin.height - delta
            onReleased: win.takeFocus()
        }

        MediaBin {
            id: mediaBin

            anchors.left: parent.left
            anchors.right: sidePanel.left
            anchors.bottom: parent.bottom
            height: visible ? Math.max(120, Math.min(win.mediaBinHeight, win.height - toolbar.height - 160)) : 0
            visible: win.mediaBinVisible
            win: win
            listWidth: sidebar.width
            playlistDrag: mediaPlaylistDrag
            mediaDrag: mediaDrag
        }

        // The one pop-up menu: every right-click menu, and the menus of the + buttons.
        PopupMenu {
            id: menu

            limit: win.height - 40
            // Back to the keys that drive the show, unless a rename in place has just
            // been started from the menu and has the keyboard.
            onClosed: {
                if (!sidebar.renaming && !mediaBin.renaming)
                    win.takeFocus()
            }
        }
    }

    Editor {
        id: editScreen

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        visible: win.editing
        sidebarWidth: sidebar.width
        groupColor: (slide) => win.groupColor(slide)
        backdropFor: (slideId) => win.slideBackdrop(slideId)
        showMenu: (items, item, x, y) => menu.show(items, item, x, y)
        onDone: win.stopEditing()
        // Whatever is on the output can still be cleared while editing.
        onKeyPassed: (event) => {
            if (event.key === Qt.Key_F1)
                win.clearAll()
            else if (event.key === Qt.Key_F2)
                win.clearSlide()
            else if (event.key === Qt.Key_F3)
                win.clearMedia()
            else
                return
            event.accepted = true
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
            win.takeFocus()
        }
    }

    ResizeGrips {
        anchors.fill: parent
        target: win
    }
}
