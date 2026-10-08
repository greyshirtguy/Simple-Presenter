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
// Simple View is the same window with everything round the slides taken away, the
// toolbar included, so that as many slides as will fit can be seen at once. Nothing
// about the show changes with it, only what is in the way of seeing it: the panes
// slide off the edges of the window and the grid then takes their room. It is switched
// by a button in the toolbar, by the small button that floats over the slides while it
// is on (SimpleViewToggle), and by holding the ~ key down: held, not pressed, because
// a view that takes everything familiar away must not be one that a stray key can
// land in. `simpleView` and the properties after it are all there is to it.
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
//
// Over the slides are the props: slides of the workspace's own (`Props`, src/props.h)
// that are turned on and off one by one and stay until they are turned off. Which are
// on, and in what order, is `liveProps`. The stage display shows either the plain view
// it has always had or one of the workspace's stage layouts (`StageLayouts`), which is
// a slide of text boxes linked to what is live; those boxes ask `Show` (src/show.h),
// which this window keeps told of the live slide and the next.
//
// What is done here goes into the session's log as it is done (`Log`,
// src/sessionlog.h): a line for a slide going live, for media, for a clear, for an
// error shown, and so on. That is all a line costs, so there is one wherever knowing
// what happened last could help, and none in anything that runs for every frame.
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
    // retriggers, volume }, or null, and the media playlist it was triggered from, "" if
    // a slide triggered it. The last four are how it behaves (see goLive() and
    // alreadyPlaying(), and for the sound MediaContent).
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
    // Simple View: whether it has been asked for, and whether it is what is showing,
    // which it is not while the editor or the settings screen is up: they need the
    // toolbar, and the view is there again when they are done with.
    property bool simpleView: false
    readonly property bool simple: simpleView && !editing && !settingsOpen
    // How far the panes round the slides are in their places: 1 when they are, 0 when
    // they are out of sight, and for a moment between the two as they slide off the
    // edges of the window or back on. That is all that is animated, and it is cheap:
    // the panes are only moved, nothing is laid out again for it. The slides are laid
    // out afresh once, when the panes have gone (or before they come back), since
    // doing that for every frame of the slide would cost more than the whole of it.
    property real chrome: simple ? 0 : 1
    // Milliseconds the panes take; 0 would be a plain cut
    readonly property int chromeDuration: 180
    // How far out of their places the panes are, from 0 to 1, for moving them by: they
    // gather speed as they leave and lose it as they come back
    readonly property real chromeAway: (1 - chrome) * (1 - chrome)
    // Whether the panes are there at all, in place or on their way. When they are not,
    // the slides have the whole window.
    readonly property bool chromeShown: !simple || chrome > 0
    // How long the ~ key has to be held to switch Simple View, in milliseconds: long
    // enough that brushing it does nothing, and short enough not to be a wait. And how
    // far a hold of it has got, from 0 to 1, for the buttons that show it.
    readonly property int simpleViewHold: 700
    property real holdProgress: 0
    // Set if the workspace changed on disk while the editor was up: the lists are
    // brought up to date when it comes down, not after every change it saves.
    property bool listsStale: false
    // Set when the editor has added or deleted slides of the presentation being viewed,
    // so that the show is brought up to date when the editor comes down
    property bool slidesRearranged: false
    // Read by main.cpp at the next launch; see the Windows section of the settings screen.
    property bool useX11: false
    // A message to show above the slides, or "", and whether it reports a failure
    property string notice: ""
    property bool noticeIsError: true
    // Whether anything can be dropped on the show's panes just now: not while the editor
    // or the settings screen is over them
    readonly property bool takesDrops: !editing && !settingsOpen
    // The files of the drag from another application that was last asked about, and
    // which of them are media (see draggedMedia())
    property string draggedFiles: ""
    property var draggedFilesMedia: []
    // [{ name, color, key }], edited on the settings screen. `key` is the group's
    // hotkey, a capital letter or a digit, and is left out or "" for none.
    //
    // The names and the colours are the app's own, kept in its settings, and these are
    // what a new installation starts with. The hotkeys are the workspace's: they are
    // in its list of groups, where ProPresenter keeps them (see GroupKeys), and are
    // read from there when the workspace is opened (readGroupKeys) and written there
    // when one is changed (setGroups).
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
    // The hotkeys a workspace has until it has a list of groups of its own: what a
    // new installation starts with. As GroupKeys.keys.
    readonly property var newGroupKeys: [{ name: "verse", label: "Verse", key: "V" }, { name: "chorus", label: "Chorus", key: "C" },
                                         { name: "bridge", label: "Bridge", key: "B" }]
    // Every hotkey there is: [{ name, key }], the group's name in lower case. Those of
    // the workspace's list of groups (or, if it has none, the ones above), and any
    // plain keys its key mappings give to groups besides.
    readonly property var hotkeys: (GroupKeys.exists ? GroupKeys.keys : newGroupKeys).concat(GroupKeys.mappedKeys)
    // The hotkeys of groups that are not among the groups of the settings, in words,
    // for the settings screen to say: they work, and are not its to change
    readonly property string otherHotkeys: hotkeys.filter(h => !groups.some(g => g.name.trim().toLowerCase() === h.name))
                                                  .map(h => h.key + " for " + h.label).join(", ")
    // Set when the settings held hotkeys, as an earlier version kept them: see
    // readGroupKeys
    property bool groupKeysInSettings: false
    // How solid the small icons are that say what comes with a slide (its hotkey, its
    // media): from 0.05, nearly gone, to 1. Set on the settings screen. This is the
    // app's own, and is kept in its settings.
    property real actionIconOpacity: 0.8
    // Pane sizes, changed by dragging the dividers between them
    property real sidebarWidth: 260
    // How wide the media bin's list of playlists is. It starts as wide as the lists
    // above it, and is its own from when it is first dragged.
    property real mediaListWidth: 260
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

    // The props that are on, by id, in the order they were turned on: the last is in
    // front. And the same as what the output is handed, [{ id, slide }], which follows
    // the props themselves: one that is edited changes where it is shown, and one that
    // is removed goes.
    property var liveProps: []
    readonly property var shownProps: {
        const all = []
        for (const collection of Props.collections) {
            for (const prop of collection.props)
                all.push(prop)
        }
        return liveProps.map(id => all.find(prop => prop.id === id)).filter(prop => prop !== undefined)
                        .map(prop => ({ id: prop.id, slide: prop.slide }))
    }
    // The stage layout the stage screen has, by id, and the layout itself; "" and null
    // for the plain view the app has of its own.
    property string stageLayoutId: ""
    readonly property var stageLayout: StageLayouts.layouts.find(layout => layout.id === stageLayoutId) ?? null

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
        if (!openable(entry)) {
            if (entry.kind === "presentation")
                Log.problem("The presentation " + quoted(entry.name) + " is listed in " + browsing() + " but its file is not in the workspace")
            return
        }
        documentKey = entry.path
        document = load(entry)
        grid.positionViewAtBeginning()
        Log.note("open", quoted(document.name) + (document.arrangement !== "" ? " [" + document.arrangement + "]" : "") + ", "
                 + counted(document.slides.length, "slide", "slides") + ", from " + browsing())
        if (document.error !== "")
            Log.problem("Opening " + quoted(document.name) + ": " + document.error)
    }

    // For the log: a name in quotes; what is being browsed; and media in a few words.
    function quoted(name) {
        return "\"" + name + "\""
    }

    function browsing() {
        const playlist = catalog.playlists.find(p => p.path === playlistId)
        const library = catalog.libraries.find(l => l.path === libraryPath)
        return playlist ? "the playlist " + quoted(playlist.name) : library ? "the library " + quoted(library.name) : "nowhere"
    }

    function mediaWords(media) {
        return (media.foreground ? "foreground " : "background ") + (media.video ? "video " : "picture ") + quoted(media.name)
               + (media.video && media.loops ? ", looping" : "")
    }

    function counted(count, one, many) {
        return count + " " + (count === 1 ? one : many)
    }

    // Switches Simple View on or off. `how` says what asked, for the log.
    function setSimpleView(on, how) {
        if (on === simpleView)
            return
        simpleView = on
        Log.note("view", "Simple View " + (on ? "on" : "off") + ", by " + how
                 + (on && !simple ? "; it shows when the " + (editing ? "editor" : "settings screen") + " is done with" : ""))
        // The keyboard is the show's, as after a click on a slide.
        takeFocus()
    }

    // Whether a key event is of the key that switches Simple View when held: the one
    // left of the 1, which on most keyboards gives ` and ~. It is known by either of
    // those, and also by where it is on the keyboard, for the keyboards on which it
    // gives something else.
    function isSimpleViewKey(event) {
        return (event.key === Qt.Key_QuoteLeft || event.key === Qt.Key_AsciiTilde || event.nativeScanCode === 49)
               && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
    }

    // The key has gone down, and has come up again (or the window has lost the
    // keyboard). Simple View is switched when it has been down for long enough, and
    // then not again until it has been let go and pressed afresh.
    function beginHold() {
        holdTimer.restart()
        holdShown.restart()
    }

    function endHold() {
        holdTimer.stop()
        holdShown.stop()
        holdProgress = 0
    }

    // What the workspace holds, and how the app is set, said once when each is known.
    function logWorkspace() {
        const props = Props.collections.reduce((count, collection) => count + collection.props.length, 0)
        Log.note("workspace", quoted(catalog.workspaceName) + " at " + catalog.workspacePath + ": "
                 + counted(catalog.libraries.length, "library", "libraries") + ", "
                 + counted(catalog.playlists.filter(p => !p.folder).length, "playlist", "playlists") + ", "
                 + counted(catalog.mediaPlaylists.filter(p => !p.folder).length, "media playlist", "media playlists") + ", "
                 + counted(Timers.timers.length, "timer", "timers") + ", " + counted(props, "prop", "props") + ", "
                 + counted(StageLayouts.layouts.length, "stage layout", "stage layouts"))
    }

    function logSettings() {
        Log.note("settings", "transition " + quoted(transition.name) + " over " + transitionDuration + " s; output window "
                 + (outputEnabled ? "on" : "off") + (outputScreen >= 0 ? ", fullscreen on screen " + outputScreen : "")
                 + "; stage window " + (stageEnabled ? "on" : "off") + ", with "
                 + (stageLayout ? "the layout " + quoted(stageLayout.name) : "the plain view") + "; media bin "
                 + (mediaBinVisible ? "shown" : "hidden") + "; this window " + width + "x" + height
                 + (useX11 ? "; set to run through X11" : "") + (remember ? "" : "; settings neither read nor saved"))
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
        const group = configuredGroup(slide)
        return group ? group.color : slide.groupColor !== "" ? slide.groupColor : surfaceColor
    }

    // Which of the groups set up on the settings screen a slide's group is: the one of
    // the same name, else the one matching without a trailing number, else none.
    function configuredGroup(slide) {
        const name = slide.group.trim().toLowerCase()
        if (name === "")
            return undefined
        const find = wanted => groups.find(g => g.name.trim().toLowerCase() === wanted)
        return find(name) ?? find(name.replace(/\s*\d+$/, ""))
    }

    // The slides of the presentation being viewed that the hotkeys go to: `at` is a
    // slide's place to its key, for marking the slide, and `of` a key to the place it
    // goes to. A key goes to the first slide of its group, where the group first comes
    // up; if several groups have the same key, as "Verse" and "Verse 1" have in
    // ProPresenter's own list, to whichever of them comes first. A group is the one of
    // a hotkey if it has the hotkey's name, or has it with a number after ("Verse 1"
    // for a hotkey of "Verse") and no hotkey under its own name.
    readonly property var groupKeys: {
        const at = {}
        const of = {}
        const slides = document ? document.slides : []
        for (let index = 0; index < slides.length; ++index) {
            if (!slides[index].groupStart)
                continue
            const name = slides[index].group.trim().toLowerCase()
            let found = hotkeys.filter(h => h.name === name)
            if (found.length === 0)
                found = hotkeys.filter(h => h.name === name.replace(/\s*\d+$/, ""))
            for (const hotkey of found) {
                if (of[hotkey.key] !== undefined)
                    continue
                of[hotkey.key] = index
                // (A slide that two keys go to is marked with the first.)
                at[index] = at[index] ?? hotkey.key
            }
        }
        return { at: at, of: of }
    }
    readonly property var groupKeyAt: groupKeys.at

    // The hotkey a group of the settings has, by its own name, as the settings screen
    // shows it.
    function keyOfGroup(name) {
        const wanted = name.trim().toLowerCase()
        const found = (GroupKeys.exists ? GroupKeys.keys : newGroupKeys).find(h => h.name === wanted)
        return found ? found.key : ""
    }

    // Gives the groups of the settings their hotkeys, when a workspace is opened: those
    // in its list of groups, or, for a workspace that has no list, the ones a new
    // installation starts with.
    function readGroupKeys() {
        if (groupKeysInSettings) {
            // An earlier version kept the hotkeys in the app's settings, with the
            // colours. A workspace that has a list of groups has hotkeys of its own
            // there, which stand. One that has none is given a list, with them in it.
            groupKeysInSettings = false
            if (GroupKeys.exists) {
                Log.note("settings", "the hotkeys an earlier version kept in the app's settings are left behind: the workspace's list of groups has its own")
            } else {
                Log.note("settings", "the hotkeys of the groups moved from the app's settings into a list of groups for the workspace")
                report(GroupKeys.setKeys(groups))
            }
        }
        groups = groups.map(group => ({ name: group.name, color: group.color, key: keyOfGroup(group.name) }))
    }

    // The groups as edited on the settings screen. A hotkey that has changed goes into
    // the workspace's list of groups: the file is not touched for a change of name or
    // colour alone.
    function setGroups(edited) {
        const keysOf = list => JSON.stringify(list.filter(g => g.key).map(g => [g.name.trim().toLowerCase(), g.key]).sort())
        const before = groups
        groups = edited
        if (keysOf(before) !== keysOf(edited))
            report(GroupKeys.setKeys(edited))
    }

    // Show mode and edit mode: the two things the window is for. Show mode comes out of
    // whichever editor is up (a presentation's, the props', the stage layouts'); edit
    // mode goes into the editor for the presentation being viewed.
    function showMode() {
        if (editing)
            stopEditing()
    }

    function editMode() {
        if (!editing && document !== null && currentEntry() !== undefined)
            startEditing(currentEntry())
    }

    // A hotkey was pressed: the first slide of its group goes live. Answers whether
    // the key was a hotkey, which is then all it is, whether or not the presentation
    // being viewed has such a group.
    function triggerGroupKey(key) {
        const hotkey = hotkeys.find(h => h.key === key)
        if (!hotkey)
            return false
        const index = groupKeys.of[key]
        Log.note("key", quoted(key) + ", the hotkey of the group "
                 + (index !== undefined ? quoted(document.slides[index].group)
                                        : quoted(hotkey.name) + ": the presentation being viewed has no such group"))
        if (index !== undefined)
            goLive(index)
        return true
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

    // Makes a slide of the presentation being viewed trigger a media file (by its
    // path), replacing any media it triggered before, or with "" stops it triggering
    // media. Saved in the presentation file.
    //
    // How the file is to play is settled by where it lands, not by where it came from.
    // On a slide that triggers no media it is a background: something to go behind the
    // slide's words. On a slide that does, it takes the place of what was there and
    // plays as that did, so that this week's video dropped on last week's is still the
    // foreground that was. (Media dropped between slides gets a slide of its own, as a
    // foreground: see insertMediaSlides().)
    function assignMedia(index, path) {
        const slide = document.slides[index]
        const error = path !== "" ? catalog.setSlideMedia(document.path, slide.id, path,
                                                          slide.mediaName !== "" && slide.mediaForeground)
                                  : catalog.removeSlideMedia(document.path, slide.id)
        if (report(error))
            reloadDocument()
    }

    // Gives each of these media files a slide of its own in the presentation being
    // viewed: a slide with nothing on it that triggers the file as a foreground, which
    // is how a video takes its turn in the run of a presentation. They go just before
    // the slide at `index`, or with `after` just after it; -1 is the end, which is
    // after the last slide as the slides are shown (which in an arrangement need not be
    // the last one in the file). Saved in the presentation file.
    function insertMediaSlides(index, after, paths) {
        const last = document.slides.length - 1
        const beside = index >= 0 && index <= last ? document.slides[index].id : last >= 0 ? document.slides[last].id : ""
        if (report(catalog.insertMediaSlides(document.path, beside, index < 0 || after, paths)))
            reloadDocument()
    }

    // Makes the media a slide triggers a background or a foreground.
    function setSlideMediaForeground(index, foreground) {
        if (report(catalog.setSlideMediaForeground(document.path, document.slides[index].id, foreground)))
            reloadDocument()
    }

    // Reads the presentation being viewed again after a change to it that leaves the
    // slides it had in the order they were in, though there may now be new ones among
    // them. The grid stays where it is scrolled to, and the slide that is live is still
    // the one marked: it is found again by its id, and by which of that id's places it
    // was at, since a group that comes up twice in an arrangement puts a slide in two
    // places.
    function reloadDocument() {
        notice = ""
        const scrolledTo = grid.contentY
        const reloaded = load(currentEntry())
        if (viewingLive) {
            const live = liveIndex >= 0 && liveIndex < liveDocument.slides.length ? liveDocument.slides[liveIndex].id : ""
            const place = liveDocument.slides.slice(0, liveIndex + 1).filter(slide => slide.id === live).length
            let passed = 0
            const index = reloaded.slides.findIndex(slide => slide.id === live && ++passed === place)
            liveDocument = reloaded
            if (index >= 0)
                liveIndex = index
        }
        document = reloaded
        grid.contentY = scrolledTo
    }

    // The media files in what is being dragged over the window, or has been dropped on
    // it, as paths: the one file of a drag out of the media bin, or whichever of the
    // files dragged in from another application are images and videos. For those the
    // answer is kept for as long as the drag is the same files, since it is asked for
    // with every move of the pointer.
    function draggedMedia(drag) {
        if (drag.source !== null)
            return drag.source.media && !drag.source.media.missing ? [drag.source.media.path] : []
        const files = drag.urls.join("\n")
        if (files !== draggedFiles) {
            draggedFiles = files
            draggedFilesMedia = catalog.mediaAmong(drag.urls)
        }
        return draggedFilesMedia
    }

    // Answers a drag that is over somewhere media can be dropped. Files from another
    // application are only ever referred to where they are, and the application is
    // told so: asked to copy, never to move, which a file manager takes as leave to
    // delete what it gave. A drag with no media in it is turned away, and the pointer
    // then shows that it cannot be dropped.
    function acceptMediaDrag(drag) {
        if (drag.source !== null)
            return true
        drag.action = Qt.CopyAction
        drag.accepted = draggedMedia(drag).length > 0
        return drag.accepted
    }

    // Takes the media out of a drop, saying to the application it came from, if it came
    // from one, that the drop was taken (and as a copy). The log gets a line for it,
    // which `where` finishes.
    function takeDroppedMedia(drop, where) {
        const paths = draggedMedia(drop)
        if (drop.source === null) {
            const left = drop.urls.length - paths.length
            if (paths.length > 0)
                drop.accept(Qt.CopyAction)
            Log.note("drop", counted(drop.urls.length, "file", "files") + " dragged in from another application and dropped " + where
                     + (paths.length === 0 ? ": none is an image or video this can show"
                        : left > 0 ? "; " + left + " left out, not being " + (left === 1 ? "an image or video" : "images or videos") : ""))
            draggedFiles = ""
            // The keyboard comes back to the show, as after a click.
            takeFocus()
        } else if (paths.length > 0) {
            Log.note("drop", quoted(drop.source.media.name) + " dragged out of the media bin and dropped " + where)
        }
        return paths
    }

    // Media dropped on the slides of the presentation being viewed: "onto" the slide at
    // `index`, which then triggers it, or "before" or "after" that slide, which is to
    // say between two slides, where it becomes a slide of its own. -1 is the end.
    function dropOnSlides(index, zone, drop) {
        if (document === null)
            return
        const where = index < 0 ? "after the last slide of " + quoted(document.name)
                    : (zone === "onto" ? "onto" : zone) + " slide " + (index + 1) + " of " + quoted(document.name)
        const paths = takeDroppedMedia(drop, where)
        if (paths.length === 0)
            return
        if (zone !== "onto") {
            insertMediaSlides(index, zone === "after", paths)
            return
        }
        assignMedia(index, paths[0])
        if (paths.length > 1 && notice === "") {
            notice = "A slide triggers one piece of media, so only the first of those " + paths.length + " files was put on it."
            noticeIsError = false
        }
    }

    // Files dragged in from another application and dropped on a media playlist: at its
    // end, or beside one of its rows (`target`, by id) if it is the playlist being
    // browsed and they were dropped among its thumbnails.
    function dropOnMediaPlaylist(playlist, drop, target = "", after = false) {
        const node = catalog.mediaPlaylists.find(p => p.path === playlist)
        if (drop.source !== null || !node)
            return
        const all = drop.urls
        const paths = takeDroppedMedia(drop, "on the media playlist " + quoted(node.name))
        if (paths.length === 0)
            return
        const scrolledTo = mediaBin.contentY
        if (!report(catalog.addMedia(playlist, all, target, after)))
            return
        if (playlist === mediaPlaylistId)
            mediaBin.contentY = scrolledTo
        // Said in words where it cannot be seen: another playlist than the one being
        // browsed, or files that were left out.
        const left = all.length - paths.length
        if (playlist !== mediaPlaylistId || left > 0) {
            notice = counted(paths.length, "file", "files") + " added to " + quoted(node.name)
                     + (left > 0 ? "; " + left + " left out, not being " + (left === 1 ? "an image or video" : "images or videos") : "")
            noticeIsError = false
        }
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
        if (error !== "")
            Log.problem("Shown to the user: " + error)
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
        Log.note("edit", "the editor opened on " + quoted(entry.name) + ", slide " + (editScreen.canvas.row + 1))
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
            { label: "Remove Media", disabled: none, run: () => assignMedia(index, "") },
            { header: "Slide" },
            // A copy goes after the slide whose menu Paste is chosen from, in this
            // presentation or another.
            { label: "Copy", run: () => copySlide(index) },
            { label: "Paste", disabled: !catalog.hasCopiedSlide, run: () => pasteSlide(index) },
            // Asked twice, since it is for good. (Not the last one there is: a
            // presentation with no slides is not one ProPresenter makes.)
            { label: "Delete Slide…", danger: true, disabled: new Set(document.slides.map(s => s.id)).size < 2,
              run: () => menu.show([
                { note: "Slide " + (index + 1) + " will be deleted from “" + document.name + "”. That cannot be undone." },
                { label: "Delete", danger: true, run: () => deleteSlide(index) },
                { label: "Cancel", run: () => {} }
            ], item, x, y) }
        ], item, x, y)
    }

    // Deletes a slide of the presentation being viewed from its file. If it is the
    // one on the output it is taken off first: what is shown should not be something
    // that is no longer there to go back to.
    function copySlide(index) {
        if (report(catalog.copySlide(document.path, document.slides[index].id)))
            Log.note("edit", "slide " + (index + 1) + " of " + quoted(document.name) + " copied")
    }

    function pasteSlide(index) {
        const made = catalog.pasteSlide(document.path, document.slides[index].id)
        if (report(made.error)) {
            Log.note("edit", "the copied slide pasted after slide " + (index + 1) + " of " + quoted(document.name))
            reloadDocument()
        }
    }

    function deleteSlide(index) {
        const slide = document.slides[index]
        if (viewingLive && !cleared && liveIndex >= 0 && liveDocument.slides[liveIndex].id === slide.id)
            clearSlide()
        Log.note("edit", "slide " + (index + 1) + " of " + quoted(document.name) + " deleted" + (slide.label !== "" ? " (" + slide.label + ")" : ""))
        if (report(catalog.removeSlide(document.path, slide.id)))
            reloadDocument()
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

    // Brings up the editor on the workspace's props, at the one with the given id, or on
    // its stage layouts: both are edited as a presentation's slides are.
    function startEditingProps(id) {
        if (editing || !report(editScreen.openProps(Props.path, catalog.workspacePath, id)))
            return
        notice = ""
        editing = true
        editScreen.takeFocus()
        Log.note("edit", "the editor opened on the props, prop " + (editScreen.canvas.row + 1) + " of " + editScreen.editor.count)
    }

    function startEditingStage(id) {
        if (editing || !report(editScreen.openStage(StageLayouts.path, catalog.workspacePath, id)))
            return
        notice = ""
        editing = true
        editScreen.takeFocus()
        Log.note("edit", "the editor opened on the stage layouts, layout " + (editScreen.canvas.row + 1) + " of " + editScreen.editor.count)
    }

    // Takes the editor down and shows the presentation as it now is. What is on the
    // output is left as it is until a slide is next shown; a prop that is on, or the
    // stage's layout, is shown as it now is at once.
    function stopEditing() {
        if (!editing)
            return
        editScreen.finish()
        const changed = editScreen.editor.changed
        const kind = editScreen.editor.kind
        editScreen.close()
        editing = false
        Log.note("edit", "the editor closed; " + (changed ? "what was changed was saved as it was made" : "nothing was changed"))
        keys.forceActiveFocus()
        if (listsStale) {
            listsStale = false
            followCatalog()
        }
        if (kind === "props") {
            report(Props.reload())
            return
        }
        if (kind === "stage") {
            report(StageLayouts.reload())
            return
        }
        const entry = currentEntry()
        if (slidesRearranged && entry) {
            // Slides were added or deleted in the editor: the one that is live is
            // found again.
            slidesRearranged = false
            reloadDocument()
        } else if (changed && entry) {
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
        Log.note("live", "slide " + (index + 1) + " of " + document.slides.length + " of " + quoted(document.name)
                 + (slide.label !== "" ? " (" + slide.label + ")" : "")
                 + (slide.media ? (alreadyPlaying(slide.media) ? "; its " + mediaWords(slide.media) + " is playing already"
                                                                : "; with its " + mediaWords(slide.media))
                    : slide.mediaName !== "" ? "; its media " + quoted(slide.mediaName) + " was not found"
                    : liveMedia !== null && liveMedia.foreground ? "; which takes off the foreground media" : "")
                 + (slide.timerActions.length > 0 ? "; and does " + slide.timerActions.length + " thing"
                                                    + (slide.timerActions.length === 1 ? "" : "s") + " to a timer" : ""))
        if (!slide.media && slide.mediaName !== "")
            Log.problem("The media " + quoted(slide.mediaName) + " of slide " + (index + 1) + " of " + quoted(document.name)
                        + " was not found in the workspace, so the slide is shown without it")
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
        Log.note("media", mediaWords(media) + (playing ? ", which is playing already and is left to" : ""))
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
        Log.note("clear", "the slide")
        cleared = true
        output.showSlide(null)
    }

    function clearMedia() {
        if (liveMedia === null)
            return
        Log.note("clear", "the media, " + quoted(liveMedia.name))
        liveMedia = null
        liveMediaPlaylistId = ""
        output.showMedia(null)
    }

    // Turns a prop on, over whatever else is on the output and in front of the props
    // that are on already, or off if it is on. A collection set to show one prop at a
    // time gives up whichever of its others is on.
    function toggleProp(id) {
        const prop = Props.find(id)
        if (liveProps.includes(id)) {
            liveProps = liveProps.filter(other => other !== id)
            Log.note("prop", quoted(prop.name ?? "") + " off; " + liveProps.length + " on")
            return
        }
        if (prop.id === undefined)
            return
        const collection = Props.collections.find(candidate => candidate.id === prop.collection)
        const rivals = prop.single && collection ? collection.props.map(other => other.id) : []
        const before = liveProps.length
        liveProps = liveProps.filter(other => !rivals.includes(other)).concat([id])
        Log.note("prop", quoted(prop.name) + " on" + (liveProps.length <= before ? ", in place of another of its collection" : "")
                 + "; " + liveProps.length + " on")
    }

    function clearProps() {
        if (liveProps.length === 0)
            return
        Log.note("clear", "the props, " + liveProps.length + " of them")
        liveProps = []
    }

    function clearAll() {
        Log.note("clear", "everything asked for")
        clearSlide()
        clearMedia()
        clearProps()
    }

    // Reads what else a workspace folder holds for the show: its timers, its props and
    // its stage layouts.
    function openShowControls(path) {
        for (const error of [Timers.open(path, clocksHeld), Props.open(path), StageLayouts.open(path), GroupKeys.open(path)]) {
            if (error !== "")
                report(error)
        }
        readGroupKeys()
    }

    width: 1400
    height: 880
    minimumWidth: 900
    minimumHeight: 500
    visible: true
    color: panelColor
    // The toolbar is the title bar: it drags the window and carries the window buttons.
    flags: Qt.Window | Qt.FramelessWindowHint
    // The app and its version, then in brackets what is open or being edited
    title: "Simple Presenter " + Qt.application.version

    // Restores the last session where what it refers to is still on disk, and falls back
    // to the first library and presentation and the top media folder where it is not.
    Component.onCompleted: {
        const saved = (key, fallback) => remember ? settings.value(key, fallback) : fallback
        width = Number(saved("windowWidth", width))
        height = Number(saved("windowHeight", height))
        sidebarWidth = Number(saved("sidebarWidth", sidebarWidth))
        mediaListWidth = Number(saved("mediaListWidth", sidebarWidth))
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
            if (Array.isArray(stored)) {
                groups = stored.filter(g => typeof g.name === "string" && typeof g.color === "string")
                groupKeysInSettings = groups.some(g => g.key)
            }
        } catch (e) {
            // Nothing stored yet, or not readable: keep the defaults.
        }
        const iconOpacity = Number(saved("actionIconOpacity", actionIconOpacity))
        if (isFinite(iconOpacity))
            actionIconOpacity = Math.max(0.05, Math.min(1, iconOpacity))

        const tab = String(saved("showControlTab", "timers"))
        if (["timers", "props", "stage"].includes(tab))
            sidePanel.showControlTab = tab
        openShowControls(catalog.workspacePath)
        Log.watch(win, "operator window")
        logWorkspace()
        restoreSelections()
        restored = true
        save("workspace", catalog.workspacePath)
        logSettings()
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
        // The stage has the layout it had, if the workspace still has that layout.
        const stageLayout = savedSelection("stageLayout")
        stageLayoutId = StageLayouts.layouts.some(layout => layout.id === stageLayout) ? stageLayout : ""
    }

    // Closes the open workspace and opens another: the output is cleared, everything
    // shown is reloaded from the other folder, and what was selected there last time is
    // selected again.
    function switchWorkspace(path) {
        if (path === catalog.workspacePath)
            return
        Log.note("workspace", "changing to " + path)
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
        stageLayoutId = ""
        notice = ""
        catalog.openWorkspace(path)
        // The timers, the props and the stage layouts are the workspace's too.
        openShowControls(path)
        logWorkspace()
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
    onMediaListWidthChanged: save("mediaListWidth", mediaListWidth)
    onSourcesHeightChanged: save("sourcesHeight", sourcesHeight)
    onThumbnailWidthChanged: save("thumbnailWidth", thumbnailWidth)
    onMediaThumbnailWidthChanged: save("mediaThumbnailWidth", mediaThumbnailWidth)
    onSidePanelWidthChanged: save("sidePanelWidth", sidePanelWidth)
    onMediaBinHeightChanged: save("mediaBinHeight", mediaBinHeight)
    onMediaBinVisibleChanged: save("mediaBinVisible", mediaBinVisible)
    onOutputEnabledChanged: save("outputEnabled", outputEnabled)
    onStageEnabledChanged: save("stageEnabled", stageEnabled)
    // (Without their hotkeys, which are the workspace's.)
    onGroupsChanged: save("groups", JSON.stringify(groups.map(group => ({ name: group.name, color: group.color }))))
    onRestoredChanged: {
        // The groups as they are now that everything has been read: without the
        // hotkeys an earlier version kept with them, which readGroupKeys has moved.
        if (restored)
            groupsChanged()
    }
    onActionIconOpacityChanged: save("actionIconOpacity", actionIconOpacity)
    onSimpleChanged: {
        if (simple)
            simpleToggle.announce()
    }

    Behavior on chrome {
        NumberAnimation { duration: win.chromeDuration }
    }

    Timer {
        id: holdTimer

        interval: win.simpleViewHold
        onTriggered: {
            win.endHold()
            win.setSimpleView(!win.simpleView, "the ~ key, held")
        }
    }

    // What shows a hold of the key shows nothing for the first moment of it, so that a
    // key only brushed does not set something flickering.
    SequentialAnimation {
        id: holdShown

        PauseAnimation { duration: 150 }
        NumberAnimation {
            target: win
            property: "holdProgress"
            from: 150 / win.simpleViewHold
            to: 1
            duration: win.simpleViewHold - 150
        }
    }

    onUseX11Changed: {
        save("useX11", useX11)
        if (restored)
            Log.note("settings", "set to run through " + (useX11 ? "X11" : "Wayland") + " from the next start")
    }
    // Not `transition.name`: that follows the index too, and may not have caught up yet.
    onTransitionIndexChanged: {
        save("transition", transitions[transitionIndex].name)
        if (restored)
            Log.note("transition", "chosen: " + transitions[transitionIndex].name)
    }
    onTransitionChoicesChanged: save("transitionOptions", JSON.stringify(transitionChoices))
    onTransitionDurationChanged: save("transitionDuration", transitionDuration)
    onLibraryPathChanged: {
        refreshLists()
        saveSelection("library", libraryPath)
    }
    onPlaylistIdChanged: saveSelection("playlist", playlistId)
    onDocumentKeyChanged: saveSelection("presentation", documentKey)
    onMediaPlaylistIdChanged: saveSelection("mediaPlaylist", mediaPlaylistId)
    onStageLayoutIdChanged: {
        saveSelection("stageLayout", stageLayoutId)
        // (Found here, and not read from `stageLayout`, which may not have followed yet.)
        const layout = StageLayouts.layouts.find(candidate => candidate.id === stageLayoutId)
        if (restored)
            Log.note("stage", layout ? "the stage has the layout " + quoted(layout.name) : "the stage has the plain view")
    }

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
            Log.note("import", "done: " + summary)
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
        transitionName: win.transition.name
        props: win.shownProps
        propsDuration: Math.round(Props.transitionDuration * 1000)
        keyTarget: keys
    }

    Stage {
        owner: win
        remember: win.remember
        shown: win.stageEnabled
        layout: win.stageLayout ? win.stageLayout.slide : null
        currentText: win.stageCurrentText
        nextText: win.stageNextText
        keyTarget: keys
    }

    // A prop that is no longer there is no longer on.
    Connections {
        target: Props

        function onChanged() {
            const there = id => Props.find(id).id !== undefined
            if (!win.liveProps.every(there))
                win.liveProps = win.liveProps.filter(there)
        }
    }

    // What is live, for the text boxes that show it: those of a stage layout, mostly
    // (see Show).
    Binding {
        target: Show
        property: "currentSlide"
        value: win.liveSlide ?? ({})
    }

    Binding {
        target: Show
        property: "nextSlide"
        value: win.nextSlide ?? ({})
    }

    // Show mode and edit mode, whichever of the app's windows has the keyboard and
    // whatever in it
    Shortcut {
        sequence: "Ctrl+S"
        context: Qt.ApplicationShortcut
        enabled: !win.settingsOpen
        onActivated: win.showMode()
    }

    Shortcut {
        sequence: "Ctrl+E"
        context: Qt.ApplicationShortcut
        enabled: !win.settingsOpen
        onActivated: win.editMode()
    }

    Item {
        id: keys

        anchors.fill: parent
        focus: true

        // A hold of the key is over when the keyboard goes elsewhere: its being let go
        // would not be heard of here.
        onActiveFocusChanged: {
            if (!activeFocus)
                win.endHold()
        }
        Keys.onReleased: (event) => {
            if (!win.isSimpleViewKey(event))
                return
            // (A key held down is reported as let go and pressed again, over and over.)
            if (!event.isAutoRepeat)
                win.endHold()
            event.accepted = true
        }
        Keys.onPressed: (event) => {
            if (win.settingsOpen) {
                if (event.key === Qt.Key_Escape)
                    win.settingsOpen = false
                return
            }
            if (win.isSimpleViewKey(event)) {
                if (!event.isAutoRepeat)
                    win.beginHold()
                event.accepted = true
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
            // A letter or a digit may be a group's hotkey.
            if (/^[a-z0-9]$/i.test(event.text) && !(event.modifiers & (Qt.AltModifier | Qt.MetaModifier))
                    && !event.isAutoRepeat && win.triggerGroupKey(event.text.toUpperCase())) {
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
            case Qt.Key_F4:
                win.clearProps()
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
            // For Simple View each pane goes off its own edge of the window, and is
            // not drawn once it is out of sight. Only where it is drawn changes: its
            // place in the layout is kept, for the slides to come back to.
            visible: win.chromeShown
            transform: Translate { x: -win.chromeAway * sidebar.width }
            win: win
            presentationDrag: presentationDrag
            playlistDrag: playlistDrag
        }

        Toolbar {
            id: toolbar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            visible: win.chromeShown
            transform: Translate { y: -win.chromeAway * toolbar.height }
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
            visible: win.chromeShown
            transform: Translate { x: win.chromeAway * sidePanel.width }
            win: win
            liveVideoSink: output.liveVideoSink
            livePlayer: win.clocksHeld ? null : output.livePlayer
            onShowControlTabChanged: win.save("showControlTab", showControlTab)
        }

        // Between the panes while they are there, and the whole window in Simple View
        // once they have gone.
        SlideGrid {
            id: grid

            anchors.left: win.chromeShown ? sidebar.right : parent.left
            anchors.right: win.chromeShown ? sidePanel.left : parent.right
            anchors.top: win.chromeShown ? toolbar.bottom : parent.top
            anchors.bottom: win.chromeShown ? mediaBin.top : parent.bottom
            win: win
        }

        // The way back out of Simple View, where the toolbar's button for it is: the
        // same place on the screen switches the view on and off.
        SimpleViewToggle {
            id: simpleToggle

            objectName: "simpleViewToggle"
            x: toolbar.simpleViewCentre + 15 - width
            y: 4
            z: 60
            visible: win.simple
            progress: win.holdProgress
            onClicked: win.setSimpleView(false, "the button over the slides")
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
        // (None of them is there while the panes are away, or on their way.)
        Divider {
            anchors.horizontalCenter: sidebar.right
            anchors.top: sidebar.top
            anchors.bottom: sidebar.bottom
            visible: win.chrome === 1
            onMoved: (delta) => win.sidebarWidth = sidebar.width + delta
            onReleased: win.takeFocus()
        }

        Divider {
            anchors.horizontalCenter: sidePanel.left
            anchors.top: sidePanel.top
            anchors.bottom: sidePanel.bottom
            visible: win.chrome === 1
            onMoved: (delta) => win.sidePanelWidth = sidePanel.width - delta
            onReleased: win.takeFocus()
        }

        Divider {
            vertical: false
            anchors.verticalCenter: mediaBin.top
            anchors.left: parent.left
            anchors.right: sidePanel.left
            visible: win.mediaBinVisible && win.chrome === 1
            onMoved: (delta) => win.mediaBinHeight = mediaBin.height - delta
            onReleased: win.takeFocus()
        }

        // Between the media bin's list of playlists and its thumbnails
        Divider {
            objectName: "mediaListDivider"
            x: mediaBin.x + mediaBin.listWidth - width / 2
            anchors.top: mediaBin.top
            anchors.bottom: mediaBin.bottom
            visible: win.mediaBinVisible && win.chrome === 1
            onMoved: (delta) => win.mediaListWidth = mediaBin.listWidth + delta
            onReleased: win.takeFocus()
        }

        MediaBin {
            id: mediaBin

            anchors.left: parent.left
            anchors.right: sidePanel.left
            anchors.bottom: parent.bottom
            height: visible ? Math.max(120, Math.min(win.mediaBinHeight, win.height - toolbar.height - 160)) : 0
            visible: win.mediaBinVisible && win.chromeShown
            transform: Translate { y: win.chromeAway * mediaBin.height }
            win: win
            listWidth: Math.max(140, Math.min(win.mediaListWidth, width - 220))
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
                if (!sidebar.renaming && !mediaBin.renaming && !sidePanel.renaming)
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
        mediaFilter: win.catalog.mediaDialogFilter
        mediaFolder: win.catalog.mediaDirectory
        canPaste: win.catalog.hasCopiedSlide
        removeSlide: (path, slideId) => {
            Log.note("edit", "a slide deleted in the editor")
            win.slidesRearranged = true
            return win.catalog.removeSlide(path, slideId)
        }
        insertSlide: (path, slideId) => {
            Log.note("edit", "a slide added in the editor")
            win.slidesRearranged = true
            return win.catalog.insertSlide(path, slideId)
        }
        copySlide: (path, slideId) => {
            Log.note("edit", "a slide copied in the editor")
            return win.catalog.copySlide(path, slideId)
        }
        pasteSlide: (path, slideId) => {
            Log.note("edit", "the copied slide pasted in the editor")
            win.slidesRearranged = true
            return win.catalog.pasteSlide(path, slideId)
        }
        // Whatever is on the output can still be cleared while editing.
        onKeyPassed: (event) => {
            if (event.key === Qt.Key_F1)
                win.clearAll()
            else if (event.key === Qt.Key_F2)
                win.clearSlide()
            else if (event.key === Qt.Key_F3)
                win.clearMedia()
            else if (event.key === Qt.Key_F4)
                win.clearProps()
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
        onGroupsEdited: (groups) => win.setGroups(groups)
        otherHotkeys: win.otherHotkeys
        actionIconOpacity: win.actionIconOpacity
        onActionIconOpacityEdited: (opacity) => win.actionIconOpacity = Math.max(0.05, Math.min(1, opacity))
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
