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
// right the preview of a screen with the clear buttons beside it, the transport and the show controls
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
// land in. `simpleView` and the properties after it are all there is to it. A setting
// (`simpleKeepsSidePanel`) leaves the pane at the right where it is, for whoever wants
// the room without losing sight of what is live.
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
// TransitionLayer.qml); the preview here draws the same maps again, small.
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
    // The same for the audio bin: the playlist of it being browsed, by id, and the
    // playlist or folder that is picked in its list.
    property string audioPlaylistId: ""
    property string selectedAudioNode: ""
    // What is in them, re-read by refreshLists() whenever either path or the disk changes.
    // `documents` holds the rows of the library or playlist being browsed, in the shape
    // PlaylistFile describes; a row's `path` identifies the row, its `file` the presentation.
    property var documents: []
    property var mediaFiles: []
    // The pieces of audio of the playlist being browsed in the audio bin, and what is
    // typed in the box that narrows them
    property var audioFiles: []
    property string audioFilter: ""

    // The presentation shown in the grid: { name, path, slides, error } from Catalog.open()
    property var document: null
    // Which row of `documents` it was opened from
    property string documentKey: ""
    // What is typed in the box under the list of presentations, and the list as that
    // narrows it (Search.narrowed, which says how). Only what the list shows is
    // narrowed: `documents` stays the whole of the library or playlist, which is what
    // the arrow keys step through and a service runs down, and a box at the foot of a
    // list is not to change the order of a service. The box is emptied when another
    // library or playlist is opened, so that what is opened is seen whole.
    property string documentFilter: ""
    readonly property var shownDocuments: documentFilter.trim() === "" ? documents : Search.narrowed(documents, documentFilter)
    // What is live. It is kept by Show (src/show.h), which is also the only thing that
    // changes it, by the rules of src/showstate.h: the functions below that put a slide
    // or media on the output, turn a prop over or clear a layer only ask it to. What it
    // holds is read back here under the names the rest of the window goes by.
    //
    // The presentation the show is at, as it was read (Show is handed its slides one at
    // a time, as they go live), and the place in it of the slide on the slide layer.
    // They stay set while the layer is cleared, so stepping carries on from where it was.
    property var liveDocument: null
    // The same for the announcements layer, which is at a slide of a presentation of
    // its own (see show::State::announcement): the presentation, as it was read when
    // a slide of it went there, and the slide that is on the layer, if one is.
    property var announcementDocument: null
    // A newer reading of either, where the editor has changed the presentation while a
    // slide of it was live: null, or the presentation as it now is. What is on the
    // output is left as it is until a slide is next shown, so the newer one is taken
    // up then, by a slide that goes live by its timer as by one that is clicked.
    property var liveFresh: null
    property var announcementFresh: null
    readonly property var announcementSlide: announcementDocument && !Show.announcementCleared && Show.announcementIndex >= 0
                                             ? announcementDocument.slides[Show.announcementIndex] ?? null : null
    readonly property int liveIndex: Show.liveIndex
    // The row and playlist it went live from ("" for a library)
    readonly property string liveKey: Show.liveKey
    readonly property string livePlaylistId: Show.livePlaylistId
    readonly property bool cleared: Show.cleared
    // Set when the slide that is live took itself off the output: its cue has an action
    // that clears the slide, or everything. Nothing of it is on the slide layer, and
    // `cleared` says so; but it is still the slide the show is at, which is marked as
    // the live one and from which the arrow keys go on.
    readonly property bool clearedByCue: Show.clearedByCue
    // Whether the slide the show is at is to be marked as such: it is on the output, or
    // would be had it not taken itself off
    readonly property bool cueLive: Show.cueLive
    // What is on the media layer: { name, path, source, video, foreground, loops,
    // retriggers, volume }, or null, and the media playlist it was triggered from, "" if
    // a slide triggered it. The last four are how it behaves (see show::State::goLive
    // and alreadyPlaying, and for the sound MediaContent).
    readonly property var liveMedia: Show.liveMedia
    readonly property string liveMediaPlaylistId: Show.liveMediaPlaylistId
    // The audio layer: the music that is playing, as a row of the audio bin, or null;
    // the playlist of the bin it was played from; the sound effects that are playing;
    // and whether anything is, which is whether there is audio to clear.
    readonly property var liveMusic: Show.liveMusic
    readonly property string liveMusicPlaylistId: Show.liveMusicPlaylistId
    readonly property var liveSounds: Show.liveSounds
    readonly property bool audioLive: liveMusic !== null || liveSounds.length > 0
    // How long a fade of the audio is, in seconds, and whether one piece of music is
    // crossed into the next (see AudioLayer, which goes by them); the device sound is
    // played through, by its id as the system has it, "" for wherever the system sends
    // sound; and how high the list of playlists is in the audio bin. All four are this
    // app's own, and this computer's: kept in its settings.
    property real audioFade: 1
    property bool audioCrossfade: false
    property string audioDevice: ""
    property real audioSplit: 66
    // What is in the box of the stage message (see StagePanel), and how high the list
    // of messages is over what one of them is made of (see MessagesPanel): how each
    // was left, kept with the app's settings.
    property string stageMessageText: ""
    property real messagesSplit: 62
    // How high the preview's area at the top right has been dragged to by the line under
    // it (see PreviewPanel), 0 for not at all, which is a 16:9 preview as wide as there
    // is room for; kept with the app's settings, as the panes' widths are.
    property real previewHeight: 0

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
    // Whether the search is up (see SearchPanel)
    property bool searchOpen: false

    // Whether the themes are up (see ThemesPanel)
    property bool themesOpen: false

    function openThemes() {
        if (editing || settingsOpen)
            return
        themesOpen = true
        themesPanel.open()
    }

    function closeThemes() {
        themesOpen = false
        takeFocus()
    }

    // Dresses slides of the presentation being viewed in a theme slide, for good: the
    // slides with these ids, or with none named all of them (see Themes).
    function applyTheme(slideIds, place, themeSlideId) {
        if (!document || currentEntry() === undefined)
            return
        const themed = Themes.slide(place, themeSlideId)
        if (!report(Themes.apply(document.path, slideIds, place, themeSlideId)))
            return
        reloadDocument()
        notice = (slideIds.length === 1 ? "The slide is" : slideIds.length > 1 ? slideIds.length + " slides are" : "Every slide of " + quoted(document.name) + " is")
               + " now in " + quoted(themed ? themed.name : "the theme") + " of the theme " + quoted(place.split("/").pop())
               + ". The file as it was is kept with the editor's backups."
        noticeIsError = false
    }

    // The themes as a menu: folders, then themes by the look of their first slide, then
    // each theme's slides, a click on one dressing the slides with these ids in it.
    function themeMenuItems(slideIds) {
        return themePickItems((place, slideId) => applyTheme(slideIds, place, slideId))
    }

    // The workspace's themes as a menu to pick a theme slide from: folders by name,
    // themes by the look of their first slide, and inside a theme its slides. `pick` is
    // given the place of the theme and the id of the slide picked. It is the menu a
    // slide's Theme has, and the one a look's line for a screen has (see LooksPanel).
    function themePickItems(pick) {
        const of = entries => entries.map(entry => entry.kind === "folder"
            ? { label: entry.name, items: [{ header: entry.name }].concat(of(entry.items)) }
            : { label: entry.name, preview: entry.slides.length > 0 ? entry.slides[0].slide : null,
                items: [{ header: entry.name }].concat(entry.slides.map(slide => ({
                    label: slide.name, preview: slide.slide, run: () => pick(entry.place, slide.id) }))) })
        return [{ header: "Theme" }].concat(of(Themes.tree))
    }

    // Whether the Looks window is up (see LooksPanel)
    property bool looksOpen: false

    function openLooks() {
        if (editing)
            return
        // (The settings, if they are up, make way for it, and so does the window of the
        // clear groups.)
        settingsOpen = false
        clearGroupsOpen = false
        arrangementsOpen = false
        looksOpen = true
        looksPanel.open()
    }

    function closeLooks() {
        looksOpen = false
        takeFocus()
    }

    // The notice that the guard is holding hardware video decoding back (see
    // DecodingNotice.qml), made when it is wanted, which on most computers is never.
    readonly property var decodingNotice: decodingNoticeLoader.item
    readonly property bool decodingNoticeOpen: decodingNoticeLoader.item !== null && decodingNoticeLoader.item.open

    function showDecodingNotice() {
        decodingNoticeLoader.active = true
        decodingNoticeLoader.item.show()
        Log.note("notice", "told that hardware video decoding is held back on this computer")
    }

    // The settings, at the part for video: where the notice leads.
    function openVideoSettings() {
        looksOpen = false
        clearGroupsOpen = false
        arrangementsOpen = false
        settingsOpen = true
        settingsScreen.section = "video"
    }

    // Whether the arrangement editor is open over the slides (see ArrangementBar.qml):
    // this computer's own, and kept from one start of the app to the next. And
    // whether the window that lists a presentation's arrangements is up (see
    // ArrangementsPanel.qml), which is made when it is first wanted.
    property bool arrangementsShown: false
    property bool arrangementsOpen: false

    // Whether the window the clear groups are set up in is up (see ClearGroupsPanel).
    // It is made when it is first wanted, which in most sessions is never.
    property bool clearGroupsOpen: false
    readonly property var clearGroupsPanel: clearGroupsLoader.item

    // Opens it, on the group whose id is given, or with "" on the first.
    function openClearGroups(groupId) {
        if (editing)
            return
        settingsOpen = false
        looksOpen = false
        arrangementsOpen = false
        clearGroupsLoader.active = true
        clearGroupsOpen = true
        clearGroupsLoader.item.open(groupId)
    }

    function closeClearGroups() {
        clearGroupsOpen = false
        takeFocus()
    }

    // The menu a right click on any of the clear buttons has: the way to the window
    // the clear groups are set up in, which opens on the group whose button it was
    // ("" for the button of a layer).
    function showClearMenu(item, groupId) {
        menu.show([{ label: "Configure Clear Groups…", run: () => openClearGroups(groupId) }], item)
    }

    function openSearch() {
        if (editing || settingsOpen)
            return
        searchOpen = true
        searchPanel.open()
    }
    // Whether the editor is up, in place of the slides, working on the presentation
    // being viewed
    property bool editing: false
    // Bible mode (BiblePanel.qml, src/bibles.h): a bar over the slides for making slides
    // of a passage of a Bible, which are then the slides shown in the grid. ProPresenter's
    // Bibles, one of its modes beside Show and Edit, and by its key, Ctrl+Alt+B.
    property bool bibleMode: false
    // How Bible slides are made (see BiblePanel's `settings`), kept from one start to
    // the next, and the person's own api.bible key, "" for the one built in
    property var bibleSettings: ({})
    property string apiBibleKey: ""
    // A timer link's Color Triggers kept as the default by the editor's Save as Default
    // ([{ time, color }]), for Recall Default to put on another box. The app's own, and
    // the same in every workspace: ProPresenter has no such thing, nor a file for it.
    property var colorTriggerDefault: []
    // The presentation the Bible's slides are, while they are what the grid shows; and
    // Bible mode's target: the presentation that was being viewed when it was come to, or
    // the one clicked since (which is not shown: the grid in Bible mode shows Bible
    // slides alone), which they can be copied into, after the slide of it that is live
    // if one is, and which leaving Bible mode goes to (the maintainer, 2026-10-15)
    property string bibleKey: ""
    property var bibleReturnEntry: null
    property string bibleReturnSlide: ""
    // The presentation marked in the list at the left: the one being viewed, or in Bible
    // mode its target
    readonly property string selectedPresentation: bibleMode ? (bibleReturnEntry ? bibleReturnEntry.path : "") : documentKey
    // Simple View: whether it has been asked for, and whether it is what is showing,
    // which it is not while the editor or the settings screen is up: they need the
    // toolbar, and the view is there again when they are done with.
    property bool simpleView: false
    readonly property bool simple: simpleView && !editing && !settingsOpen
    // A setting, this app's own: Simple View leaves the pane at the right, with the
    // previews, the clears and the show controls, and takes only the toolbar, the lists
    // and the media bin away. The pane then stays where it is while the others slide
    // off, and has the height of the window once the toolbar has gone.
    property bool simpleKeepsSidePanel: false
    // The HTTP API (src/httpapiserver.h), as the settings' Network section sets it:
    // whether it listens, the Network Name it gives ("" for the computer's name), and
    // its port. This computer's, kept with the app's own settings. Off until switched
    // on, as ProPresenter's is.
    property bool networkEnabled: false
    property string networkName: ""
    property int networkPort: HttpApi.defaultPort
    // Whether the pane at the right is there, in its place or on its way
    readonly property bool sidePanelShown: chromeShown || simpleKeepsSidePanel
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
    // The panel a label is made in, once it has been wanted (see labelPanel)
    readonly property var slideLabelPanel: slideLabelLoader.item
    // Set for as long as the catalog takes to rename or delete a presentation. The
    // catalog says that the files have changed before it comes back from doing it, and
    // following that then would find the presentation being viewed gone and open the
    // first of the list in its place. The function that asked knows what became of the
    // file, and follows the catalog itself when it has put that right.
    property bool holdingLists: false
    // Set when the editor has added or deleted slides of the presentation being viewed,
    // so that the show is brought up to date when the editor comes down
    property bool slidesRearranged: false
    // Read by main.cpp at the next launch; see the Windows section of the settings screen.
    property bool useX11: false
    // Who decodes video: "auto" (the graphics chip where it can be trusted to), or
    // "processor" (the processor alone), or "hardware" (the graphics chip where it can,
    // whatever the guard says of this computer). It is acted on as the app starts,
    // before anything is played (src/decodingchoice.h), so a change here is for the
    // next start; what this run is doing is VideoDecoding's to say.
    property string videoDecoding: "auto"
    // Whether the notice has been shown that says the guard is holding hardware
    // decoding back on this computer (see DecodingNotice.qml): it is shown once.
    property bool decodingNoticeSeen: false
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
    // The chord editors (Editor.qml): how many times its usual size the chord sheet is
    // drawn and the ChordPro text is set, each one of the same steps and kept apart (a
    // size that suits a grid of slides is not the one that suits a page of text); and
    // the colour of a chord in each, "" for the one it comes with. They are this
    // app's, kept from one start to the next: nothing in any song's file says them.
    property real chordSheetZoom: 0.8
    // How wide the reflow editor draws a slide, as it was left
    property real reflowThumbnail: 220
    property real chordTextZoom: 1
    property string chordSheetColor: ""
    property string chordTextColor: ""
    // And the colour of the chord sheet's highlighted line ("" for the one it comes
    // with), and whether the chord sheet draws sharps and flats as ♯ and ♭ (on as it
    // comes): the same, the app's own.
    property string chordLineColor: ""
    property bool chordSigns: true
    // Whether the editor has the pane for a slide's notes up under the slide, and how
    // tall that pane is: how the editor was left, kept with the app's settings.
    property bool editorNotes: false
    property real editorNotesHeight: 150
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
    // When the builds of the slide on the slide layer run (ProPresenter's, see
    // src/builds.h): the moment each of its steps began, as Date.now() has it, which
    // every screen and the preview draw them by (see Slide.buildTimes); null for a
    // slide with none, either of its own or in the theme a screen's look gives it, and
    // for none. (A screen can have them begin later: see buildStarts.) The first is as the slide's transition ends: the
    // builds that go with the slide are timed from then. (A GUESS at ProPresenter, which
    // the maintainer describes as making the slide's transition "first".) Each On Click
    // build that Next runs adds one, the moment it was asked for.
    property var liveBuildTimes: null
    readonly property var nextSlide: liveDocument && liveIndex + 1 < liveDocument.slides.length
                                     ? liveDocument.slides[liveIndex + 1] : null
    readonly property string stageCurrentText: liveSlide ? liveSlide.plainText : ""
    readonly property string stageNextText: nextSlide ? nextSlide.plainText : ""

    // The props that are on, by id, in the order they were turned on: the last is in
    // front. And the same as what the output is handed, [{ id, slide }], which follows
    // the props themselves: one that is edited changes where it is shown, and one that
    // is removed goes.
    readonly property var liveProps: Array.from(Show.liveProps)
    readonly property var shownProps: {
        const all = []
        for (const collection of Props.collections) {
            for (const prop of collection.props)
                all.push(prop)
        }
        return liveProps.map(id => all.find(prop => prop.id === id)).filter(prop => prop !== undefined)
                        .map(prop => ({ id: prop.id, slide: prop.slide }))
    }
    // The messages that are shown, by id, in the order they were shown: the last is in
    // front. And the same as what the output is handed, [{ id, slide }], each a message
    // as a slide in its theme (see Messages.slideOf).
    //
    // What is drawn of a message is what it was when it was shown: `messagesAsShown`
    // has each as it was then, by id, taken when the show says a message has been
    // shown (or shown again). So words typed into a message while it is on the screens
    // are not on the screens until it is shown again.
    readonly property var liveMessages: Array.from(Show.liveMessages)
    property var messagesAsShown: ({})
    readonly property var shownMessages: liveMessages.map(id => messagesAsShown[id]).filter(message => message !== undefined)
                                                     .map(message => ({ id: message.id, slide: messageSlide(message) }))

    // A message as a slide: the slide of its theme, with the message's words to come
    // in its first text box. The theme is found by its name, which is all a message's
    // file has of it (so the first of that name, whatever folder it is in), and the
    // slide by its id, or failing that by its name, or by its place.
    function messageSlide(message) {
        let slide = null
        const theme = message.theme === "" ? undefined : Themes.themes.find(candidate => candidate.name === message.theme)
        if (theme) {
            const found = theme.slides.find(candidate => candidate.id === message.themeSlide)
                       ?? theme.slides.find(candidate => candidate.name === message.themeSlideName)
                       ?? theme.slides[message.themeSlideIndex] ?? theme.slides[0]
            if (found)
                slide = found.slide
        }
        return Messages.slideOf(message.id, slide)
    }

    // The words a message is showing: its words, with what each of its tokens was
    // given put in. A token that is the clock or a timer is asked for as it is at the
    // moment, and reading the second and the tick is what has this asked again as
    // they go on: only by a message that has such a token, and only while it is shown.
    function messageWords(message) {
        let words = ""
        for (const piece of message.pieces) {
            const token = piece.token ? message.tokens.find(candidate => candidate.id === piece.text) : undefined
            if (!piece.token)
                words += piece.text
            else if (!token)
                continue
            else if (token.kind === "clock")
                words += Clocks.second >= 0 ? Clocks.clockText(token.date, token.time, token.twentyFour, "${clock}") : ""
            else if (token.kind === "timer")
                words += Timers.tick >= 0 ? Timers.linkedText(token.timerId, token.name, token.hours, token.minutes, token.seconds,
                                                              token.hundredths, token.hundredthsUnderMinute, "${timer}") : ""
            else
                words += token.value
        }
        return words
    }

    // Shows a message, or hides it if it is shown; and clears them all. (Whatever was
    // being typed into a message is kept first: see MessagesPanel, which calls these.)
    function toggleMessage(id) {
        Show.toggleMessage(id)
    }

    function showMessage(id) {
        Show.setMessage(id, true)
    }

    function hideMessage(id) {
        Show.setMessage(id, false)
    }

    function clearMessages() {
        Show.clearMessages()
    }

    // The stage layout the stage screen has, by id, and the layout itself; "" and null
    // for the plain view the app has of its own.
    readonly property string stageLayoutId: Show.stageLayoutId
    readonly property var stageLayout: StageLayouts.layouts.find(layout => layout.id === stageLayoutId) ?? null

    // The playlist the slides being viewed are a row of: the one being browsed, but for
    // Bible mode's slides, which are no row of one (they go live with none, and are
    // stepped through and marked live as such)
    readonly property string viewedPlaylistId: documentKey !== "" && documentKey === bibleKey ? "" : playlistId
    readonly property bool viewingLive: document !== null && liveDocument !== null
                                        && documentKey === liveKey && viewedPlaylistId === livePlaylistId
                                        && document.arrangement === liveDocument.arrangement
    // Whether the presentation being viewed is the one the announcements are in, and
    // whether it is one that is sent to the announcements at all
    readonly property bool viewingAnnouncements: document !== null && announcementDocument !== null
                                                 && documentKey === Show.announcementKey && viewedPlaylistId === Show.announcementPlaylistId
                                                 && document.arrangement === announcementDocument.arrangement
    readonly property bool viewedGoesToAnnouncements: document !== null && document.destination === "announcements"
    // The item the pop-up menu is open for, or null while it is shut
    readonly property var menuItem: menu.opened ? menu.opener : null

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
        audioFiles = catalog.audioIn(audioPlaylistId)
    }

    function openLibrary(path) {
        if (playlistId !== "" || libraryPath !== path)
            documentFilter = ""
        selectedNode = ""
        playlistId = ""
        libraryPath = path
        refreshLists()
        openFirst()
    }

    function openPlaylist(id) {
        if (playlistId !== id)
            documentFilter = ""
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
        // (In Bible mode a presentation clicked is not shown: it is Bible mode's target.)
        if (bibleMode) {
            aimBibles(entry)
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
                 + (useX11 ? "; set to run through X11" : "")
                 + (videoDecoding !== "auto" ? "; video decoding set to " + (videoDecoding === "processor" ? "the processor only" : "the graphics chip") : "")
                 + (remember ? "" : "; settings neither read nor saved"))
    }

    function openDocument(key) {
        const entry = documents.find(d => d.path === key)
        if (entry)
            openEntry(entry)
    }

    function openFirst() {
        const first = documents.find(openable)
        if (bibleMode) {
            aimBibles(first ?? null)
            return
        }
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
        if (bibleMode)
            leaveBibles()
    }

    // Bible mode, come to and left: from the editor, which is left first, as Show mode
    // leaves it; and back to the presentation that was being viewed, when left.
    function bibleModeToggle() {
        if (bibleMode) {
            leaveBibles()
            return
        }
        if (editing)
            stopEditing()
        bibleReturnEntry = currentEntry() ?? null
        bibleReturnSlide = liveDocument !== null && bibleReturnEntry !== null && liveKey === documentKey && livePlaylistId === playlistId
                         && Show.currentSlide.id !== undefined ? Show.currentSlide.id : ""
        bibleMode = true
        // The grid shows the slides Bible mode made last, this run; or until it has made
        // some, nothing: never the presentation that was being viewed, whose slides are
        // not Bible mode's. (Not kept from one run to the next: the slides are in a
        // folder that goes when the app closes.)
        if (bibleKey !== "") {
            documentKey = bibleKey
            document = catalog.open(bibleKey)
        } else {
            documentKey = ""
            document = null
        }
        Log.note("bibles", "Bible mode come to")
    }

    function leaveBibles() {
        bibleMode = false
        if (documentKey === "" || documentKey === bibleKey) {
            const back = bibleReturnEntry !== null ? documents.find(d => d.path === bibleReturnEntry.path) : undefined
            if (back)
                openEntry(back)
            else
                openFirst()
        }
        Log.note("bibles", "Bible mode left")
        takeFocus()
    }

    // Bible mode's target set to a presentation (or none): what Copy to Current
    // Presentation puts the slides into, after its live slide if it has the one that is
    // live, and what leaving Bible mode goes to
    function aimBibles(entry) {
        bibleReturnEntry = entry
        bibleReturnSlide = entry !== null && liveDocument !== null && liveKey === entry.path && livePlaylistId === playlistId
                         && Show.currentSlide.id !== undefined ? Show.currentSlide.id : ""
        if (entry !== null)
            Log.note("bibles", quoted(entry.name) + " chosen, for after Bible mode")
    }

    // The presentation Edit and Reflow go to: the one being viewed, or in Bible mode its
    // target, Bible mode being left for it
    function editTarget() {
        return bibleMode ? (bibleReturnEntry !== null ? documents.find(d => d.path === bibleReturnEntry.path) : undefined) : currentEntry()
    }

    // Bible mode's ← Verse and Verse → (under the slides, SlideGrid.qml): the passage
    // grown by the verse before it or after it
    readonly property bool bibleCanStep: bibleMode && biblePanel.item !== null && biblePanel.item.passage !== null && biblePanel.item.passage.error === ""

    function bibleStep(delta) {
        if (biblePanel.item)
            biblePanel.item.step(delta)
    }

    // The slides Bible mode made (Bibles.make's answer), shown in the grid as any
    // presentation's are. Where the passage grew round a slide of it that was live, that
    // slide was kept as it was and is at `live` among the new ones: the show follows it
    // into the new file without showing it again, so that it is still the one marked
    // live and the one the arrow keys go on from.
    function showBibleSlides(made) {
        const was = bibleKey
        bibleKey = made.path
        documentKey = made.path
        document = catalog.open(made.path)
        if (made.live >= 0 && was !== "" && liveDocument !== null && liveKey === was && livePlaylistId === "") {
            Show.renamed(was, made.path)
            liveDocument = document
            Show.follow(Object.assign(cueAt(document, made.path, "", made.live), { destination: "" }))
            grid.positionViewAtIndex(made.live, GridView.Contain)
        } else {
            grid.positionViewAtBeginning()
        }
        Log.note("bibles", quoted(document.name) + ", " + counted(document.slides.length, "slide", "slides"))
    }

    // Bible mode's menu of ways to keep the slides it made, from the Save button under
    // the slides (SlideGrid.qml)
    function bibleKeepMenu(item) {
        if (biblePanel.item)
            biblePanel.item.keepMenu(item)
    }

    // Something said that is no fault: on the line where faults are said
    function tell(message) {
        notice = message
        noticeIsError = false
    }

    function editMode() {
        if (reflowing)
            editScreen.setMode("slides")
        else if (!editing && editTarget() !== undefined)
            startEditing(editTarget())
    }

    // The reflow editor (ReflowEditor.qml): the editor in another of its ways of
    // working, for the presentation being viewed, or for the one the editor is on.
    readonly property bool reflowing: editing && editScreen.mode === "reflow"
    readonly property bool canReflow: editing ? editScreen.kind === "presentation"
                                              : editTarget() !== undefined && openable(editTarget())
    function reflowMode() {
        if (reflowing || !canReflow)
            return
        if (!editing)
            startEditing(editTarget())
        if (editing && editScreen.kind === "presentation") {
            editScreen.setMode("reflow")
            Log.note("edit", "the reflow editor come to")
        }
    }

    // ---- A slide asked for by its number, typed (src/slidenumber.h says how the whole
    // of it goes, and what of it is a guess; TypedNumber.qml is what is shown)

    // What is typed so far of a slide's number: "" when none is being typed
    property string typedNumber: ""

    // A digit typed while showing: the number begins, or goes on. Where there is no
    // slide to ask for, nothing is begun.
    function typeDigit(digit) {
        if (!document || document.slides.length === 0)
            return
        typedNumber = NumberTyping.typed(typedNumber, digit)
        numberQuiet.restart()
        // (What shows it is made now, if this is the first number of the session.)
        typedNumberLoader.active = true
    }

    // Backspace: the last digit comes off, and with the only one the number goes.
    function eraseTypedDigit() {
        typedNumber = NumberTyping.erased(typedNumber)
        if (typedNumber === "")
            numberQuiet.stop()
        else
            numberQuiet.restart()
    }

    // Enter: the slide that has the number goes live, as a click on it would have it.
    // A number that no slide of the presentation has does nothing.
    function takeTypedNumber() {
        const asked = typedNumber
        const index = NumberTyping.slideFor(asked, document ? document.slides.length : 0)
        dropTypedNumber()
        if (index < 0) {
            Log.note("key", "slide " + asked + " asked for by its number: "
                     + (document ? quoted(document.name) + " has " + document.slides.length : "no presentation is being viewed"))
            return
        }
        Log.note("key", "slide " + asked + " of " + quoted(document.name) + " asked for by its number")
        goLive(index)
    }

    // The number is given up: by Esc, by a second without a key, by any key that is
    // not part of typing one, or because the keyboard has gone elsewhere.
    function dropTypedNumber() {
        typedNumber = ""
        numberQuiet.stop()
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
        if (announcementDocument && Show.announcementKey === key && Show.announcementPlaylistId === playlistId && Show.announcementIndex >= 0) {
            // (The announcements follow theirs the same way.)
            const index = rearranged.slides.findIndex(s => s.id === announcementDocument.slides[Show.announcementIndex].id)
            if (index >= 0) {
                announcementDocument = rearranged
                Show.follow(announcementCue(rearranged, index))
            }
        }
        if (liveDocument && liveKey === key && livePlaylistId === playlistId && liveIndex >= 0) {
            // Follow the live slide to its first place in the new order. If the new
            // arrangement leaves it out, the output keeps it and nothing is marked live.
            const index = rearranged.slides.findIndex(s => s.id === liveDocument.slides[liveIndex].id)
            if (index >= 0) {
                liveDocument = rearranged
                Show.follow(liveCue(rearranged, index))
            }
        }
        if (documentKey === key) {
            document = rearranged
            grid.positionViewAtBeginning()
        }
    }

    // ---- Making and changing the arrangements of the presentation being viewed
    // (ArrangementBar.qml over the slides, and ArrangementsPanel.qml). Each is saved in
    // the presentation's file at once, by the catalog; src/arrangementedit.h has the
    // rules, and what of them is a guess.

    // The arrangement being viewed, as the presentation's list of them has it: null for
    // Master.
    function viewedArrangement() {
        return document && document.arrangement !== ""
               ? document.arrangementList.find(candidate => candidate.name === document.arrangement) ?? null : null
    }

    // Reads the presentation being viewed again after a change to its arrangements.
    // `wasLive` is whether it was what is live before the change: an arrangement that
    // has been renamed is no longer the one the live presentation is known by, and
    // the show is to follow it all the same.
    function rearranged(wasLive) {
        notice = ""
        const scrolledTo = grid.contentY
        const reloaded = load(currentEntry())
        if (wasLive && liveDocument) {
            const index = Show.placeAfterReload(liveDocument.slides.map(slide => slide.id), liveIndex, reloaded.slides.map(slide => slide.id))
            liveDocument = reloaded
            Show.follow(liveCue(reloaded, index >= 0 ? index : liveIndex))
        }
        // (The announcements go by the row and the playlist alone, which a change to
        // the arrangements leaves as they were.)
        followAnnouncements(reloaded, document !== null && announcementDocument !== null && documentKey === Show.announcementKey
                                      && playlistId === Show.announcementPlaylistId)
        document = reloaded
        grid.contentY = scrolledTo
    }

    // A new arrangement, of every group once in Master's order, which is then the one
    // viewed, so that it can be worked on at once.
    function newArrangement() {
        if (!document)
            return ""
        const made = catalog.addArrangement(document.path)
        if (!report(made.error))
            return ""
        Log.note("edit", "an arrangement made for " + quoted(document.name) + ": " + quoted(made.name))
        setArrangement(currentEntry(), made.name)
        return made.name
    }

    function duplicateArrangement(id) {
        if (!document)
            return ""
        const from = document.arrangementList.find(candidate => candidate.id === id)
        const wasLive = viewingLive
        const made = catalog.duplicateArrangement(document.path, id)
        if (!report(made.error))
            return ""
        Log.note("edit", "the arrangement " + quoted(from ? from.name : "") + " of " + quoted(document.name) + " copied as " + quoted(made.name))
        rearranged(wasLive)
        return made.name
    }

    function renameArrangement(id, name) {
        if (!document)
            return
        const from = document.arrangementList.find(candidate => candidate.id === id)
        if (!from || name.trim() === "" || name.trim() === from.name)
            return
        const wasLive = viewingLive
        if (!report(catalog.renameArrangement(document.path, id, name)))
            return
        Log.note("edit", "the arrangement " + quoted(from.name) + " of " + quoted(document.name) + " renamed " + quoted(name.trim()))
        rearranged(wasLive)
    }

    // Takes an arrangement out of the presentation. Where it was the one viewed, the
    // presentation is viewed as Master after.
    function removeArrangement(id) {
        if (!document)
            return
        const from = document.arrangementList.find(candidate => candidate.id === id)
        const wasLive = viewingLive
        if (!report(catalog.removeArrangement(document.path, id)))
            return
        Log.note("edit", "the arrangement " + quoted(from ? from.name : "") + " of " + quoted(document.name) + " removed")
        rearranged(wasLive)
    }

    // A group put into the arrangement being viewed at a landing, the group at a place
    // of it moved to a landing, and the group at a place taken out of it. (A landing is
    // the place a thing goes before, or the number of places there are for after the
    // last.) The log is told with the names, and not the place alone: which
    // arrangement was changed, and how, is what is wanted after a service that ran in
    // an order nobody expected.
    function arrangementInsert(groupId, landing) {
        const arrangement = viewedArrangement()
        if (!arrangement)
            return
        const group = document.groups.find(candidate => candidate.id === groupId)
        const wasLive = viewingLive
        if (!report(catalog.insertArrangementGroup(document.path, arrangement.id, groupId, landing)))
            return
        Log.note("edit", "the arrangement " + quoted(arrangement.name) + " of " + quoted(document.name) + ": the group "
                 + quoted(group ? group.name : "") + " put in at place " + (landing + 1))
        rearranged(wasLive)
    }

    function arrangementMove(from, landing) {
        const arrangement = viewedArrangement()
        if (!arrangement)
            return
        // (Let go where it already is, there is nothing to write or to say.)
        if (landing === from || landing === from + 1)
            return
        const group = document.groups.find(candidate => candidate.id === arrangement.groups[from])
        const wasLive = viewingLive
        if (!report(catalog.moveArrangementGroup(document.path, arrangement.id, from, landing)))
            return
        Log.note("edit", "the arrangement " + quoted(arrangement.name) + " of " + quoted(document.name) + ": the group "
                 + quoted(group ? group.name : "") + " moved from place " + (from + 1) + " to place " + ((landing > from ? landing - 1 : landing) + 1))
        rearranged(wasLive)
    }

    function arrangementTakeOut(place) {
        const arrangement = viewedArrangement()
        if (!arrangement)
            return
        const group = document.groups.find(candidate => candidate.id === arrangement.groups[place])
        const wasLive = viewingLive
        if (!report(catalog.removeArrangementGroup(document.path, arrangement.id, place)))
            return
        Log.note("edit", "the arrangement " + quoted(arrangement.name) + " of " + quoted(document.name) + ": the group "
                 + quoted(group ? group.name : "") + " taken out of place " + (place + 1))
        rearranged(wasLive)
    }

    // Opens the window that lists the arrangements of the presentation being viewed.
    function openArrangements() {
        if (editing || !document)
            return
        settingsOpen = false
        looksOpen = false
        clearGroupsOpen = false
        arrangementsLoader.active = true
        arrangementsOpen = true
        arrangementsLoader.item.open()
    }

    function closeArrangements() {
        arrangementsOpen = false
        takeFocus()
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

    // The menu of the icon that says whether a slide's media is a background or a
    // foreground: the one it is is ticked, and the other changes it. (How a video
    // plays on from its end is its own setting, with its own icon.)
    function showMediaBehaviourMenu(index, item) {
        const slide = document.slides[index]
        menu.show([
            { header: "Media" },
            { label: "Background", current: !slide.mediaForeground, run: () => setSlideMediaForeground(index, false) },
            { label: "Foreground", current: slide.mediaForeground, run: () => setSlideMediaForeground(index, true) },
            { label: "Inspector", disabled: !slide.media, run: () => inspectSlideMedia(index) }
        ], item, 0, item.height + 2)
    }

    // How a video plays on from its end, in a few words: for the icon and the menu.
    // `playback` is as the file numbers them: 0 it stops, 1 it goes round, 2 it plays
    // a number of times, 3 it goes round for a length of time.
    function playbackWords(playback, count, seconds) {
        const time = seconds >= 60 && seconds % 60 === 0 ? seconds / 60 + " min" : Math.round(seconds) + " s"
        return playback === 2 ? "×" + Math.max(1, count) : playback === 3 ? time : ""
    }

    // The menu of the icon that says how a slide's video plays on from its end.
    function showMediaPlaybackMenu(index, item) {
        const slide = document.slides[index]
        const mode = slide.mediaPlayback
        const show = items => menu.show(items, item, 0, item.height + 2)
        const set = (playback, count, seconds) => {
            Log.note("edit", "the video of slide " + (index + 1) + " of " + quoted(document.name) + " set to "
                     + ["stop at its end", "loop", "play " + count + " times", "loop for " + seconds + " seconds"][playback])
            if (report(catalog.setSlideMediaPlayback(document.path, slide.id, playback, count, seconds)))
                reloadDocument()
        }
        show([
            { header: "At Its End" },
            { label: "Stop", current: mode === 0, run: () => set(0, 0, 0) },
            { label: "Loop", current: mode === 1, run: () => set(1, 0, 0) },
            { label: "Loop for Play Count" + (mode === 2 ? ": " + slide.mediaLoopCount : ""), current: mode === 2,
              items: [{ header: "Play Count" }].concat([2, 3, 4, 5, 10, 20].map(count => ({
                  label: count + " times", current: mode === 2 && slide.mediaLoopCount === count, run: () => set(2, count, 0) }))) },
            { label: "Loop for Time" + (mode === 3 ? ": " + playbackWords(3, 0, slide.mediaLoopSeconds) : ""), current: mode === 3,
              items: [{ header: "Time" }].concat([10, 30, 60, 120, 300, 600, 1800].map(seconds => ({
                  label: playbackWords(3, 0, seconds), current: mode === 3 && slide.mediaLoopSeconds === seconds,
                  run: () => set(3, 0, seconds) }))) }
        ])
    }

    // ---- Actions: what a slide's cue, or a macro, does besides (see src/actions.h)

    // Runs a macro by hand, from the show controls.
    function runMacro(id) {
        Show.runMacro(id)
    }

    // What an action names in this workspace (see Show): the stage screen this app has,
    // which of a stage action's screens that is, the layout the action gives it, the
    // action's list of screens with this app's given a layout, and the prop a prop
    // action is for.
    function stageScreen() {
        return Show.stageScreen()
    }

    function stageAssignmentOf(action) {
        return Show.stageAssignmentOf(action)
    }

    function stageLayoutOf(action) {
        return Show.stageLayoutOf(action)
    }

    function stageAssignments(existing, layout) {
        return Show.stageAssignments(existing ?? null, layout ?? null)
    }

    function propOf(action) {
        return Show.propOf(action)
    }

    // What an action is added to, changed in or taken from: `target` is { slide } for
    // the slide at that place in the presentation being viewed, or { macro } for the
    // macro with that id. An action is a map as src/actions.h describes; with
    // `existingId` it takes the place of the action that has that id.
    function commitAction(target, action, existingId) {
        if (target.macro !== undefined) {
            Log.note("edit", "an action " + (existingId ? "changed in" : "added to") + " a macro: " + action.kind)
            return report(existingId ? Macros.changeAction(target.macro, existingId, action) : Macros.addAction(target.macro, action))
        }
        const slide = document.slides[target.slide]
        Log.note("edit", "an action " + (existingId ? "changed on" : "added to") + " slide " + (target.slide + 1) + " of "
                 + quoted(document.name) + ": " + action.kind)
        const done = report(existingId ? catalog.changeSlideAction(document.path, slide.id, existingId, action)
                                       : catalog.addSlideAction(document.path, slide.id, action))
        if (done)
            reloadDocument()
        return done
    }

    function removeAction(target, action) {
        if (target.macro !== undefined)
            return report(Macros.removeAction(target.macro, action.id))
        const slide = document.slides[target.slide]
        Log.note("edit", "an action taken off slide " + (target.slide + 1) + " of " + quoted(document.name) + ": " + action.title)
        const done = report(catalog.removeSlideAction(document.path, slide.id, action.id))
        if (done)
            reloadDocument()
        return done
    }

    // The kinds of action there are to add, as the rows of a menu, each leading to what
    // there is to choose for it: a menu of its own to pick from, or a small panel to
    // fill in. Each has the picture that kind of action has everywhere else, which is
    // ProPresenter's own (see ActionGlyph), as ProPresenter's menu of actions has.
    function addActionItems(target) {
        const add = action => commitAction(target, action, "")
        const clear = (label, layer, picture) => ({ label: label, glyph: "clear", picture: picture, run: () => add({ kind: "clear", layer: layer }) })
        const clearAnnouncements = { label: "The Announcements", glyph: "clear", picture: "ClearAnnouncements",
                                     run: () => add({ kind: "clear", layer: 0, announcements: true }) }
        const props = Props.collections.filter(collection => collection.props.length > 0)
        // (Not the macro itself, for an action of a macro.)
        const macros = Macros.collections.map(collection => Object.assign({}, collection, {
            macros: collection.macros.filter(macro => macro.id !== target.macro) })).filter(collection => collection.macros.length > 0)
        const mediaLists = binPlaylistItems(catalog.mediaPlaylists, "", playlist => add(playlistAction("mediaPlaylist", playlist)))
        const audioLists = binPlaylistItems(catalog.audioPlaylists, "", playlist => add(playlistAction("audioPlaylist", playlist)))
        return [
            { header: "Add Action" },
            { label: "Timer…", glyph: "timer", disabled: Timers.timers.length === 0, run: () => actionDialog.openTimer(target, null, "") },
            { label: "Clear", glyph: "clear", items: [{ header: "Clear" }, clear("Everything", 0, "Clear"), clear("The Slide", 5, "ClearPresentation"),
                                                     clear("The Media", 2, "ClearMedia"), clear("The Props", 4, "ClearProps"),
                                                     clear("The Messages", 7, "ClearMessages"), clearAnnouncements,
                                                     clear("The Video Input", 3, "ClearVideoInput")] },
            { label: "Clear Group", glyph: "clear", disabled: ClearGroups.groups.length === 0, items: [{ header: "Clear Group" }].concat(
                ClearGroups.groups.map(group => ({ label: group.name, run: () => add(clearGroupAction(group.id, group.name)) }))) },
            { label: "Stage…", glyph: "stage", run: () => actionDialog.openStage(target, null) },
            { label: "Audience Look", glyph: "look", disabled: Looks.looks.length === 0, items: [{ header: "Audience Look" }].concat(Looks.looks.map(look => ({
                label: look.name, run: () => add({ kind: "look", lookId: look.id, lookName: look.name }) }))) },
            { label: "Prop", glyph: "prop", disabled: props.length === 0, items: [{ header: "Prop" }].concat(props.map(collection => ({
                label: collection.name, items: [{ header: collection.name }].concat(collection.props.map(prop => ({
                    label: prop.name, items: propActionItems(target, prop, collection) }))) }))) },
            { label: "Macro", glyph: "macro", disabled: macros.length === 0, items: [{ header: "Macro" }].concat(macros.map(collection => ({
                label: collection.name, items: [{ header: collection.name }].concat(collection.macros.map(macro => ({
                    label: macro.name, run: () => add(macroAction(macro, collection)) }))) }))) },
            { label: "Media Playlist", glyph: "mediaPlaylist", disabled: mediaLists.length === 0, items: [{ header: "Media Playlist" }].concat(mediaLists) },
            { label: "Video Input", glyph: "videoInput", disabled: VideoInputs.inputs.length === 0, items: [{ header: "Video Input" }].concat(
                VideoInputs.inputs.map(input => ({ label: input.name, run: () => add({ kind: "videoInput", inputIndex: input.index }) }))) },
            { label: "Audio Playlist", glyph: "audioPlaylist", disabled: audioLists.length === 0, items: [{ header: "Audio Playlist" }].concat(audioLists) },
            { label: "Message", glyph: "message", disabled: Messages.messages.length === 0, items: [{ header: "Message" }].concat(Messages.messages.map(message => ({
                label: message.name + (givenWords(message) ? "…" : ""), run: () => addMessageAction(target, message) }))) },
            { label: "Communication", glyph: "other", disabled: Devices.connections.length === 0, items: communicationItems(target) }
        ]
    }

    // The playlists of a bin as the rows of a menu, in their folders, each leading to
    // `make` with the playlist ({ path, name }, as the bin's list has it). A folder with
    // no playlist in it, however deep, is left out.
    function binPlaylistItems(nodes, parent, make) {
        const items = []
        for (const node of nodes.filter(candidate => candidate.parent === parent)) {
            if (!node.folder) {
                items.push({ label: node.name, run: () => make(node) })
                continue
            }
            const inner = binPlaylistItems(nodes, node.path, make)
            if (inner.length > 0)
                items.push({ label: node.name, items: [{ header: node.name }].concat(inner) })
        }
        return items
    }

    // An action that plays a playlist of the media bin or of the audio bin (see
    // src/playlistactions.h): new, it is the playlist alone, with Always Retrigger off.
    function playlistAction(kind, playlist) {
        return { kind: kind, playlistId: playlist.path, playlistName: playlist.name }
    }

    // The row of an action's menu that turns its Always Retrigger on or off: off, a
    // playlist that is playing already is left to play on when the action is run again.
    function retriggerItem(target, action) {
        return { label: "Always Retrigger", current: action.retrigger, run: () => commitAction(target, {
            kind: action.kind, playlistId: action.playlistId, playlistName: action.playlistName, retrigger: !action.retrigger }, action.id) }
    }

    // Whether a message has tokens that are given words (Name, Room and the like),
    // which an action that shows it can give words of its own
    function givenWords(message) {
        return message.tokens.some(token => token.kind === "text")
    }

    // An action that shows a message, with the words it gives the message's text tokens
    // (as ActionDialog sets them up: { tokenId, name, kind, value } each)
    function messageAction(message, values) {
        return { kind: "message", messageId: message.id, messageName: message.name, values: values }
    }

    // A message picked for an action, or dropped on a slide or a macro: one with tokens
    // that are given words brings up the panel to give them, starting from what they
    // were last given on the messages' tab; one with none needs nothing more.
    function addMessageAction(target, message) {
        if (givenWords(message))
            actionDialog.openMessage(target, null, message)
        else
            commitAction(target, messageAction(message, []), "")
    }

    // The message an action shows, as Messages has it: by its id, or failing that by its
    // name; undefined for none.
    function messageOf(action) {
        return Messages.messages.find(message => message.id === action.messageId)
            ?? Messages.messages.find(message => message.name === action.messageName)
    }

    // The panel in which the words a message action gives its tokens are changed, for
    // the macros' tab, which lists actions and has no way to it of its own.
    function openMessageAction(target, action) {
        const message = messageOf(action)
        if (message)
            actionDialog.openMessage(target, action, message)
    }

    // The communication devices' commands, as ProPresenter's menu has them: a kind of
    // device, then the device (only where there is more than one of the kind), then
    // what it can be sent. Each opens the panel that sets it up (ActionDialog).
    function communicationItems(target) {
        const items = [{ header: "Communication" }]
        const commands = device => device.kind === "midi"
            ? [{ label: "MIDI Note On…", run: () => actionDialog.openCommunication(target, null, device, "midi", true) },
               { label: "MIDI Note Off…", run: () => actionDialog.openCommunication(target, null, device, "midi", false) }]
            : device.kind === "rosstalk"
            ? [{ label: "Custom Control…", run: () => actionDialog.openCommunication(target, null, device, "customControl") }]
            : [{ label: "HTTP Request…", run: () => actionDialog.openCommunication(target, null, device, "http") }]
        for (const kind of ["midi", "rosstalk", "http"]) {
            const devices = Devices.connections.filter(device => device.kind === kind)
            if (devices.length === 0)
                continue
            const name = devices[0].kindName
            items.push({ label: name, items: [{ header: name }].concat(devices.length === 1 ? commands(devices[0])
                : devices.map(device => ({ label: device.name, items: [{ header: device.name }].concat(commands(device)) }))) })
        }
        return items
    }

    function showAddActionMenu(target, item, x, y) {
        menu.show(addActionItems(target), item, x, y)
    }

    // A prop is put on by an action, or taken off: the rows that ask which.
    function propActionItems(target, prop, collection) {
        const make = clear => ({ kind: "prop", propId: prop.id, propName: prop.name, collectionId: collection.id,
                                 collectionName: collection.id === "" ? "" : collection.name, clear: clear })
        return [
            { header: prop.name },
            { label: "Trigger", run: () => commitAction(target, make(false), "") },
            { label: "Clear", run: () => commitAction(target, make(true), "") }
        ]
    }

    function showPropActionMenu(target, prop, collection, item, x, y) {
        menu.show(propActionItems(target, prop, collection), item, x, y)
    }

    function macroAction(macro, collection) {
        return { kind: "macro", macroId: macro.id, macroName: macro.name, collectionId: collection.id,
                 collectionName: collection.id === "" ? "" : collection.name }
    }

    // The menu of an action that is there: what it is, and changing it or taking it away.
    function showActionMenu(target, action, item, x, y) {
        const items = [{ note: action.title + (action.done ? "" : ". This kind of action is kept in the file, and is not done here.") }]
        // (An action that adds time to a timer is not one the panel can show.)
        if (action.kind === "timer" && action.action !== Timers.Increment)
            items.push({ label: "Change…", run: () => actionDialog.openTimer(target, action, "") })
        if (action.kind === "stage")
            items.push({ label: "Change…", run: () => actionDialog.openStage(target, action) })
        // A communication action this app makes: its panel, for the device it names (or,
        // where that is not here, as the action has it)
        if (action.kind === "communication" && (action.command === "midi" || action.command === "http" || action.customControl)) {
            const device = Devices.find(action.deviceId, action.deviceName)
            const known = device.id !== undefined ? device : { id: action.deviceId, name: action.deviceName,
                                                               kind: action.command === "midi" ? "midi" : action.command === "http" ? "http" : "rosstalk" }
            items.push({ label: "Change…", run: () => actionDialog.openCommunication(target, action, known,
                                                                                    action.command === "words" ? "customControl" : action.command) })
        }
        // A video input: which of the workspace's it is, by its place
        if (action.kind === "videoInput" && VideoInputs.inputs.length > 0)
            items.push({ label: "Video Input", items: [{ header: "Video Input" }].concat(VideoInputs.inputs.map(input => ({
                label: input.name, current: input.index === action.inputIndex,
                run: () => commitAction(target, { kind: "videoInput", inputIndex: input.index }, action.id) }))) })
        // A playlist: whether it starts again when it is playing already
        if (action.kind === "mediaPlaylist" || action.kind === "audioPlaylist")
            items.push(retriggerItem(target, action))
        // A message whose tokens are given words: the words this action gives them
        if (action.kind === "message" && messageOf(action) && givenWords(messageOf(action)))
            items.push({ label: "Change…", run: () => openMessageAction(target, action) })
        // A piece of audio: how it plays on from its end (there is no playlist here
        // for a next to follow), and whether it is music or a sound effect.
        if (action.kind === "audio") {
            const change = (how) => commitAction(target, Object.assign({ kind: "audio" }, how), action.id)
            items.push({ header: "Playback" })
            items.push({ label: "Stop", glyph: "stop", current: action.playback !== "loop", run: () => change({ playback: "stop" }) })
            items.push({ label: "Loop", glyph: "loop", current: action.playback === "loop", run: () => change({ playback: "loop" }) })
            items.push({ header: "Type" })
            items.push({ label: "Music", glyph: "music", current: !action.effect, run: () => change({ effect: false }) })
            items.push({ label: "Sound Effect", glyph: "sound", current: action.effect, run: () => change({ effect: true }) })
            items.push({ header: "Action" })
        }
        items.push({ label: "Remove", danger: true, run: () => removeAction(target, action) })
        menu.show(items, item, x, y)
    }

    // The panel in which a timer action or a stage action is changed, for whoever lists
    // actions and has no way to it of its own.
    function openTimerAction(target, action) {
        actionDialog.openTimer(target, action, "")
    }

    function openStageAction(target, action) {
        actionDialog.openStage(target, action)
    }

    // Something is being dragged that stands for an action: a timer, a prop, a macro or
    // a stage screen out of the show controls, or one of the clear buttons, to be
    // dropped on a slide or on a macro, which gives that the action that goes with it.
    // (A playlist of either bin, and a message, are dragged as a row of their lists is,
    // and stand for an action too: see draggedAction.)
    // `payload` says what: { kind, id, name } and, for a prop or a macro, its
    // collection, or for a clear button the layer it clears. (A prop dropped on another
    // prop of its collection is moved there instead: see PropsPanel.)
    function beginActionDrag(payload) {
        actionDrag.payload = payload
        actionDrag.Drag.active = true
    }

    function moveActionDrag(point) {
        const at = actionDrag.parent.mapFromItem(null, point.x, point.y)
        actionDrag.x = at.x
        actionDrag.y = at.y
    }

    function endActionDrag(dropped) {
        if (dropped)
            actionDrag.Drag.drop()
        actionDrag.Drag.active = false
    }

    // What is being dragged, as an action, by the item that is (a drag's or a drop's
    // `source`): the payload of a drag out of the show controls, or a playlist of either
    // bin, or a message, being dragged out of its list ({ kind, id, name }); null for
    // anything else. A folder of playlists is no playlist. (By the item and not by which
    // drag is active: a drag is no longer active by the time its drop arrives.)
    function draggedAction(source) {
        if (!source)
            return null
        if (source === actionDrag)
            return actionDrag.payload
        const kind = source === mediaPlaylistDrag ? "mediaPlaylist" : source === audioPlaylistDrag ? "audioPlaylist"
                   : source === messageDrag ? "message" : ""
        return kind !== "" && source.entry && !source.entry.folder ? { kind: kind, id: source.entry.path, name: source.entry.name } : null
    }

    // The keys of the drags that stand for actions, for whatever takes them; and the
    // drag the messages' list moves (see MessagesPanel)
    readonly property var actionDragKeys: ["action", "mediaPlaylist", "audioPlaylist", "message"]
    readonly property Item messageDragProxy: messageDrag

    // What was being dragged has been dropped on a slide, or on a macro (`target` is
    // as commitAction takes it; `source` the item that was being dragged, the drop's
    // own, which is the show controls' drag if it is not said). A timer and a stage
    // screen bring up the panel that says what is to be done to them; a prop asks
    // whether it is to be put on or taken off; a message whose tokens are given words
    // brings up the panel for them; a macro, a playlist, a message with no such tokens
    // and one of the clear buttons (a layer's or a group's) need nothing more.
    function dropAction(target, item, source) {
        const dropped = draggedAction(source ?? actionDrag)
        if (!dropped || (target.slide !== undefined && !document) || (dropped.kind === "macro" && dropped.id === target.macro))
            return
        Log.note("drop", "a " + (dropped.kind === "stage" ? "stage screen" : dropped.kind === "clear" ? "clear button"
                                 : dropped.kind === "clearGroup" ? "clear group's button" : dropped.kind === "mediaPlaylist" ? "media playlist"
                                 : dropped.kind === "audioPlaylist" ? "audio playlist" : dropped.kind === "videoInput" ? "video input"
                                 : dropped.kind) + " dropped on "
                 + (target.macro !== undefined ? "a macro" : "slide " + (target.slide + 1) + " of " + quoted(document.name)))
        if (dropped.kind === "timer")
            actionDialog.openTimer(target, null, dropped.id)
        else if (dropped.kind === "stage")
            actionDialog.openStage(target, null)
        else if (dropped.kind === "prop")
            showPropActionMenu(target, dropped, { id: dropped.collectionId, name: dropped.collectionName }, item, 20, 20)
        else if (dropped.kind === "macro")
            commitAction(target, macroAction(dropped, { id: dropped.collectionId, name: dropped.collectionName }), "")
        else if (dropped.kind === "clear")
            commitAction(target, dropped.announcements ? { kind: "clear", layer: 0, announcements: true } : { kind: "clear", layer: dropped.layer }, "")
        else if (dropped.kind === "clearGroup")
            commitAction(target, clearGroupAction(dropped.id, dropped.groupName), "")
        else if (dropped.kind === "audio")
            commitAction(target, { kind: "audio", item: dropped.id }, "")
        else if (dropped.kind === "mediaPlaylist" || dropped.kind === "audioPlaylist")
            commitAction(target, playlistAction(dropped.kind, { path: dropped.id, name: dropped.name }), "")
        else if (dropped.kind === "videoInput")
            commitAction(target, { kind: "videoInput", inputIndex: dropped.inputIndex }, "")
        else if (dropped.kind === "message") {
            const message = Messages.messages.find(candidate => candidate.id === dropped.id)
            if (message)
                addMessageAction(target, message)
        }
    }

    // The piece of audio that is being dragged out of the audio bin, as what the drag
    // carries ({ id, name, playlist }), or null. (As a property for what is to follow
    // it, and as a function for what asks once.)
    readonly property var audioDragged: actionDrag.Drag.active && actionDrag.payload && actionDrag.payload.kind === "audio" ? actionDrag.payload : null

    function draggedAudio() {
        return audioDragged
    }

    // A piece of audio dropped on the bare list of the macros, on no macro: it is given
    // a macro of its own, called what the piece is called, whose one action plays it.
    function dropAudioAsMacro() {
        const dropped = draggedAudio()
        if (!dropped)
            return
        Log.note("drop", "a piece of audio dropped on the macros, on none of them: a macro is made for it")
        const made = Macros.add("")
        if (!report(made.error))
            return
        report(Macros.rename(made.id, dropped.name))
        commitAction({ macro: made.id }, { kind: "audio", item: dropped.id }, "")
    }

    function dropActionOnSlide(index, item, source) {
        dropAction({ slide: index }, item, source)
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
            // The show follows its slide to wherever it now is.
            const index = Show.placeAfterReload(liveDocument.slides.map(slide => slide.id), liveIndex, reloaded.slides.map(slide => slide.id))
            liveDocument = reloaded
            Show.follow(liveCue(reloaded, index >= 0 ? index : liveIndex))
        }
        followAnnouncements(reloaded, viewingAnnouncements)
        document = reloaded
        grid.contentY = scrolledTo
    }

    // The presentation being viewed has been read again, and is the one the
    // announcements are in (`mine`): they follow their slide to wherever it now is.
    function followAnnouncements(reloaded, mine) {
        if (!mine || !announcementDocument)
            return
        const index = Show.placeAfterReload(announcementDocument.slides.map(slide => slide.id), Show.announcementIndex,
                                            reloaded.slides.map(slide => slide.id))
        announcementDocument = reloaded
        Show.follow(announcementCue(reloaded, index >= 0 ? index : Show.announcementIndex))
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

    // A size of the chord editors as it was kept: the nearest of the steps they go by,
    // or `usual` for what is no size at all.
    readonly property var chordZooms: [0.5, 0.6, 0.7, 0.8, 0.9, 1, 1.1, 1.25, 1.5]

    function chordZoom(kept, usual) {
        const size = Number(kept)
        if (!(size > 0))
            return usual
        return chordZooms.reduce((best, step) => Math.abs(step - size) < Math.abs(best - size) ? step : best, chordZooms[0])
    }

    // A colour of the chord editors as it is kept: "#rrggbb" in small letters, and ""
    // (the colour it comes with) for anything else.
    function chordColorKept(kept) {
        const written = String(kept).toLowerCase()
        return /^#[0-9a-f]{6}$/.test(written) ? written : ""
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
    // The address of a media file's thumbnail; with the media's settings (as
    // mediasettings::describe has them, if there are any), of the media as it is set to
    // look: at the moment chosen for its picture, cropped, turned and flipped (see
    // src/thumbnailprovider.h).
    function thumbnailUrl(path, settings) {
        return "image://thumbnail/" + encodeURIComponent(path) + "?" + catalog.thumbnailRevision + thumbnailLook(settings)
    }

    function thumbnailLook(s) {
        if (!s)
            return ""
        let look = ""
        if (s.thumbnail > 0)
            look += "&t=" + s.thumbnail
        if (s.cropOn && (s.cropLeft > 0 || s.cropRight > 0 || s.cropTop > 0 || s.cropBottom > 0))
            look += "&c=" + [s.cropLeft, s.cropRight, s.cropTop, s.cropBottom].join(",")
        if (s.rotation)
            look += "&r=" + s.rotation
        if (s.flipH || s.flipV)
            look += "&f=" + (s.flipH ? "h" : "") + (s.flipV ? "v" : "")
        return look
    }

    // The media files the slides of a presentation trigger, as paths.
    function mediaOf(file) {
        return catalog.open(file).slides.filter(s => s.media !== undefined).map(s => s.media.path)
    }

    // Gives the keyboard back to whatever drives the show: the editor while it is up,
    // and otherwise the item that takes the arrow and function keys.
    function takeFocus() {
        // The Looks window keeps the keys while it is up (a menu opened from it hands
        // them back here when it closes): Esc is then still the way out of it.
        if (decodingNoticeOpen)
            decodingNoticeLoader.item.takeKeys()
        else if (looksOpen)
            looksPanel.takeKeys()
        else if (clearGroupsOpen)
            clearGroupsLoader.item.takeKeys()
        else if (arrangementsOpen)
            arrangementsLoader.item.takeKeys()
        else if (editing)
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

    // ---- The audio bin (AudioPanel.qml)

    // (Only its own pieces are read again, and not the other lists with them: this is
    // done as the app starts, and every list read again is every row of it made again.)
    function openAudioPlaylist(id) {
        selectedAudioNode = id
        audioPlaylistId = id
        audioFilter = ""
        audioFiles = catalog.audioIn(id)
    }

    // Plays a piece of audio, a row of the audio bin: music in place of the music that
    // is playing, a sound effect over whatever is (see show::State::playAudio).
    function playAudio(audio, playlist = "") {
        Show.playAudio(audio, playlist)
    }

    function clearAudio() {
        Show.clearAudio()
    }

    function newAudioNodeParent() {
        const selected = catalog.audioPlaylists.find(p => p.path === selectedAudioNode)
        return !selected ? "" : selected.folder ? selected.path : selected.parent
    }

    function newAudioNode(folder, parent) {
        const names = catalog.audioPlaylists.map(p => p.name)
        const base = folder ? "New Folder" : "New Playlist"
        let name = base
        for (let n = 2; names.includes(name); ++n)
            name = base + " " + n
        const created = folder ? catalog.createAudioFolder(name, parent) : catalog.createAudioPlaylist(name, parent)
        if (!report(created.error))
            return
        if (folder)
            selectedAudioNode = created.id
        else
            openAudioPlaylist(created.id)
        sidePanel.renameAudioPlaylist(created.id)
    }

    function browseForAudio() {
        audioDialog.open()
    }

    function showAudioAddMenu(item) {
        const items = [
            { label: "Add Folder", run: () => newAudioNode(true, newAudioNodeParent()) },
            { label: "Add Playlist", run: () => newAudioNode(false, newAudioNodeParent()) }
        ]
        if (audioPlaylistId !== "")
            items.push({ label: "Add Audio…", run: () => browseForAudio() })
        menu.show(items, item)
    }

    function showAudioNodeMenu(node, item) {
        const items = []
        if (node.folder) {
            items.push({ label: "New Playlist Here", run: () => newAudioNode(false, node.path) })
            items.push({ label: "New Folder Here", run: () => newAudioNode(true, node.path) })
        }
        items.push({ label: "Rename", run: () => sidePanel.renameAudioPlaylist(node.path) })
        items.push({
            label: node.folder ? "Remove Folder…" : "Remove Playlist…",
            run: () => menu.show([
                { header: "Remove “" + node.name + "”?" },
                { note: node.folder ? "The playlists in it go too. Sound files stay on disk."
                                    : "Its sound files stay on disk." },
                { label: "Remove", danger: true, run: () => report(catalog.removeAudioPlaylist(node.path)) },
                { label: "Cancel", run: () => {} }
            ], item)
        })
        menu.show(items, item)
    }

    // What can be changed of a piece of audio: how it plays on from its end, and
    // whether it is music or a sound effect (see audiocue.h). Both are the row's own,
    // kept in ProPresenter's file of the audio bin.
    function showAudioItemMenu(audio, item) {
        const play = (how) => {
            Log.note("edit", "the audio " + quoted(audio.name) + " set to " + how)
            report(catalog.setAudioPlayback(audio.id, how))
        }
        const type = (effect) => {
            Log.note("edit", "the audio " + quoted(audio.name) + " made " + (effect ? "a sound effect" : "music"))
            report(catalog.setAudioEffect(audio.id, effect))
        }
        menu.show([
            { header: "Playback" },
            { label: "Stop", glyph: "stop", current: audio.playback === "stop", run: () => play("stop") },
            { label: "Loop", glyph: "loop", current: audio.playback === "loop", run: () => play("loop") },
            { label: "Next", glyph: "next", current: audio.playback === "next", run: () => play("next") },
            { header: "Type" },
            { label: "Music", glyph: "music", current: !audio.effect, run: () => type(false) },
            { label: "Sound Effect", glyph: "sound", current: audio.effect, run: () => type(true) },
            { header: "Playlist" },
            { label: "Remove from Playlist", run: () => report(catalog.removeAudioItem(audio.id)) }
        ], item)
    }

    function moveAudio(sourceId, targetId, after) {
        report(catalog.moveAudioItem(sourceId, targetId, after))
    }

    // Answers a drag of files from another application that is over somewhere audio can
    // be dropped: as acceptMediaDrag does, for sound files.
    function acceptAudioDrag(drag) {
        if (drag.source !== null)
            return false
        drag.action = Qt.CopyAction
        const files = drag.urls.join("\n")
        if (files !== draggedFiles) {
            draggedFiles = files
            draggedFilesMedia = catalog.audioAmong(drag.urls)
        }
        drag.accepted = draggedFilesMedia.length > 0
        return drag.accepted
    }

    // Sound files dragged in from another application and dropped on a playlist of the
    // audio bin: at its end, or beside one of its rows (`target`, by id).
    function dropOnAudioPlaylist(playlist, drop, target = "", after = false) {
        const node = catalog.audioPlaylists.find(p => p.path === playlist)
        if (drop.source !== null || !node)
            return
        const all = drop.urls
        const paths = catalog.audioAmong(all)
        const left = all.length - paths.length
        if (paths.length > 0)
            drop.accept(Qt.CopyAction)
        Log.note("drop", counted(all.length, "file", "files") + " dragged in from another application and dropped on the audio playlist "
                 + quoted(node.name) + (paths.length === 0 ? ": none is a sound file this can play"
                                        : left > 0 ? "; " + left + " left out, not being " + (left === 1 ? "a sound file" : "sound files") : ""))
        draggedFiles = ""
        takeFocus()
        if (paths.length === 0 || !report(catalog.addAudio(playlist, all, target, after)))
            return
        if (playlist !== audioPlaylistId || left > 0) {
            notice = counted(paths.length, "file", "files") + " added to " + quoted(node.name)
                     + (left > 0 ? "; " + left + " left out, not being " + (left === 1 ? "a sound file" : "sound files") : "")
            noticeIsError = false
        }
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

    // ---- Chords

    // The key each presentation's chords are being shown in, by its path, where one has
    // been picked. It is a matter of this sitting: nothing is written to the file, in
    // which the chords stay in the song's own key (see chords.h).
    property var chordKeys: ({})

    // The key a presentation's chords are shown in: the one picked here, else the one
    // its file says it was last shown in, else C, which is what a song that names no
    // key is taken to be in.
    function keyOf(shown) {
        return chordKeys[shown.path] ?? (shown.userKey ? shown.userKey : "C")
    }

    function setChordKey(shown, key) {
        const keys = Object.assign({}, chordKeys)
        keys[shown.path] = key
        chordKeys = keys
        Log.note("chords", quoted(shown.name) + " is shown in " + key)
    }

    // ---- Libraries

    // The menu of the + beside Libraries.
    function showLibraryMenu(item) {
        menu.show([
            { label: "New Presentation…", disabled: libraryPath === "", run: () => newPresentationPanel.show() },
            { label: "New Library", run: () => newLibrary() },
            { label: "Import ChordPro File…", disabled: libraryPath === "", run: () => songDialog.open() }
        ], item)
    }

    // Makes an empty library, opens it, and starts naming it in place.
    function newLibrary() {
        const names = catalog.libraries.map(library => library.name)
        let name = "New Library"
        for (let n = 2; names.includes(name); ++n)
            name = "New Library " + n
        const created = catalog.createLibrary(name)
        if (!report(created.error))
            return
        openLibrary(created.path)
        sidebar.renameLibrary(created.path)
    }

    function renameLibrary(path, name) {
        const renamed = catalog.renameLibrary(path, name)
        if (report(renamed.error) && renamed.path !== path && libraryPath === path)
            openLibrary(renamed.path)
    }

    // Gives a presentation another name: its file's and the one inside it, with the
    // rows of playlists that are it brought along (Catalog::renamePresentation). What
    // here goes by the file is then pointed at the file as it now is: the list, the
    // presentation being viewed if it is that one, and the show if it is the one that
    // is live, which goes on as it was.
    function renamePresentation(entry, name) {
        const was = entry.file
        const viewed = document !== null && document.path === was
        // (The lists are not followed while the catalog does it: see holdingLists.)
        holdingLists = true
        const renamed = catalog.renamePresentation(was, name)
        holdingLists = false
        report(renamed.error)
        if (renamed.path === was)
            return
        Log.note("edit", "the presentation " + quoted(was.replace(/^.*\//, "")) + " renamed " + quoted(renamed.path.replace(/^.*\//, "")))
        // In a library a row goes by its file; in a playlist it has an id of its own.
        if (playlistId === "" && documentKey === was)
            documentKey = renamed.path
        Show.renamed(was, renamed.path)
        followCatalog()
        if (liveDocument !== null && liveDocument.path === was) {
            const live = livePlaylistId !== "" ? catalog.openArranged(renamed.path, liveDocument.arrangement) : catalog.open(renamed.path)
            liveDocument = live
            Show.follow(liveCue(live, liveIndex))
        }
        if (announcementDocument !== null && announcementDocument.path === was) {
            const theirs = Show.announcementPlaylistId !== "" ? catalog.openArranged(renamed.path, announcementDocument.arrangement)
                                                              : catalog.open(renamed.path)
            announcementDocument = theirs
            Show.follow(announcementCue(theirs, Show.announcementIndex))
        }
        if (viewed && currentEntry())
            document = load(currentEntry())
    }

    // Takes a presentation out of its library: its file goes to the Trash, and no
    // further (Catalog::deletePresentation). If it is the one on the output its slide
    // is taken off first, as when a slide that is live is deleted: what is shown should
    // not be something that is no longer there to go back to. If it is the one being
    // viewed, the one that takes its place in the list is opened.
    function deletePresentation(entry) {
        const file = entry.file
        const viewed = document !== null && document.path === file
        const place = documents.findIndex(d => d.path === entry.path)
        if (liveDocument !== null && liveDocument.path === file && !cleared)
            clearSlide()
        if (announcementDocument !== null && announcementDocument.path === file)
            clearAnnouncements()
        // (The lists are not followed while the catalog does it: see holdingLists.)
        holdingLists = true
        const deleted = catalog.deletePresentation(file)
        holdingLists = false
        if (!report(deleted.error))
            return
        Log.note("edit", "the presentation " + quoted(file.replace(/^.*\//, "")) + " moved to the Trash")
        refreshLists()
        if (viewed) {
            const next = documents.slice(Math.max(0, place)).find(openable) ?? documents.slice(0, Math.max(0, place)).reverse().find(openable)
            if (next) {
                openEntry(next)
            } else {
                documentKey = ""
                document = null
            }
        }
        followCatalog()
    }

    // Makes a presentation called `name` in the library that is open, with one slide
    // with nothing on it, and opens it.
    function newPresentation(name) {
        const made = catalog.createPresentation(libraryPath, name)
        if (!report(made.error))
            return
        selectedNode = ""
        playlistId = ""
        refreshLists()
        openDocument(made.path)
        Search.read(catalog.librariesDirectory)
        Log.note("edit", "a new presentation, " + quoted(made.path.replace(/^.*\//, "")) + ", made in the library "
                 + quoted(libraryPath.replace(/^.*\//, "")))
    }

    // Makes a presentation of a ChordPro file in the library that is open, with so many
    // lines of its words on a slide, and opens it.
    function importSong(file, lines) {
        const made = catalog.importSong(file, libraryPath, lines)
        if (!report(made.error))
            return
        selectedNode = ""
        playlistId = ""
        refreshLists()
        openDocument(made.path)
        Search.read(catalog.librariesDirectory)
        Log.note("import", "A song came in from " + quoted(String(file).replace(/^.*\//, "")) + " as " + quoted(made.path.replace(/^.*\//, "")))
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

    // Where a presentation is sent when one of its slides is triggered, which is
    // ProPresenter's Destination: "announcements", or "" for the slide and media
    // layers ("Presentation", as ProPresenter calls it). It is the presentation's own,
    // kept in its file and on its rows in the playlists, so every row of it in every
    // list goes by it.
    //
    // What is on the output is left as it is: a slide of it that is live stays on the
    // layer it went to, and the next one triggered goes to the new one. (A GUESS, that
    // ProPresenter leaves what is on the output too.)
    function setDestination(entry, destination) {
        if (!report(catalog.setDestination(entry.file, destination)))
            return
        notice = ""
        Log.note("edit", "the presentation " + quoted(entry.name) + " is now sent to "
                         + (destination === "announcements" ? "the announcements layer" : "the slide and media layers"))
        if (document !== null && document.path === entry.file)
            reloadDocument()
    }

    // The two there are to choose from, as rows of a menu, the one it has ticked.
    function destinationItems(entry) {
        return [
            { header: "Destination" },
            { label: "Presentation", current: (entry.destination ?? "") === "", run: () => setDestination(entry, "") },
            { label: "Announcements", current: entry.destination === "announcements", run: () => setDestination(entry, "announcements") }
        ]
    }

    // The same menu by itself, for the button over the slides.
    function showDestinationMenu(item) {
        const entry = currentEntry()
        if (entry && openable(entry))
            menu.show(destinationItems(entry), item)
    }

    // Opens the menu for a row of the presentations list.
    function showPresentationMenu(entry, item) {
        let items = []
        if (openable(entry)) {
            items.push({ header: "Arrangement" })
            for (const name of [""].concat(entry.arrangements)) {
                items.push({
                    label: name === "" ? "Master" : name,
                    current: entry.arrangement === name,
                    run: () => setArrangement(entry, name)
                })
            }
            items = items.concat(destinationItems(entry))
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
            // In place, in the list, as a library and a playlist are renamed
            items.push({ label: "Rename", run: () => sidebar.renamePresentation(entry.path) })
            items.push({ label: "Rebuild Thumbnails", run: () => catalog.rebuildThumbnails(mediaOf(entry.file)) })
            // Only where a library is being browsed. In a playlist "Remove" is the word,
            // and only the row goes: a presentation is not to be lost from there.
            // Asked twice, though the file goes no further than the Trash.
            if (!entry.playlistItem) {
                items.push({ label: "Delete…", danger: true, run: () => menu.show([
                    { note: "“" + entry.name + "” will be moved to the Trash, where the file manager has it to put back. "
                            + "A playlist that has it will say it is missing." },
                    { label: "Move to Trash", danger: true, run: () => deletePresentation(entry) },
                    { label: "Cancel", run: () => {} }
                ], item) })
            }
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
        // (From Bible mode, which is left for the presentation, as Show mode leaves it)
        if (bibleMode)
            leaveBibles()
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
        const target = { slide: index }
        menu.show([
            { label: "Edit", run: () => startEditing(currentEntry(), slide.id) },
            // Its words alone, put right over its thumbnail: see quickEdit
            { label: "Quick Edit", run: () => quickEdit(index, item) },
            // What else the slide does when it is shown: see src/actions.h
            { label: "Add Action", items: () => addActionItems(target) },
            { label: "Remove Action", disabled: slide.actions.length === 0, items: [{ header: "Remove Action" }].concat(
                slide.actions.map(action => ({ label: action.title, glyph: action.kind, picture: action.picture,
                                               run: () => removeAction(target, action) }))) },
            // When it goes on to the next slide by itself: see editGoToNext
            { label: "Go To Next Timer…", current: slide.goToNext !== undefined, run: () => editGoToNext(index) },
            // The look of the slide: see Themes
            { label: "Theme", disabled: Themes.themes.length === 0, items: () => themeMenuItems([slide.id]) },
            // What it is called: see labelMenuItems
            { label: "Label", items: () => labelMenuItems(index) },
            // Which group it is in, from here on in its group: see setSlideGroup
            { label: "Group", disabled: groupChoices().length === 0, items: () => [{ header: "Group" }].concat(
                groupChoices().map(group => ({ label: group.name, swatch: group.color, current: slide.group.trim() === group.name,
                                               run: () => setSlideGroup(index, group) }))) },
            // The caption itself is something to click, where the slide has media that
            // can be played: it plays the media and leaves the slide layer alone.
            { header: "Media", run: slide.media ? () => playSlideMedia(index) : undefined },
            { label: "Background", current: !none && !slide.mediaForeground, disabled: none,
              run: () => setSlideMediaForeground(index, false) },
            { label: "Foreground", current: !none && slide.mediaForeground, disabled: none,
              run: () => setSlideMediaForeground(index, true) },
            { label: "Remove Media", disabled: none, run: () => assignMedia(index, "") },
            // All that can be set about it: see inspectSlideMedia
            { label: "Inspector", disabled: !slide.media, run: () => inspectSlideMedia(index) },
            { header: "Slide" },
            // A copy goes after the slide whose menu Paste is chosen from, in this
            // presentation or another.
            { label: "Copy", run: () => copySlide(index) },
            { label: "Paste", disabled: !catalog.hasCopiedSlide, run: () => pasteSlide(index) },
            // The look of its text, kept to be put on another slide's, here or in the
            // editor (see Themes.copyTextStyle)
            { label: "Copy Text Style", disabled: slide.plainText.trim() === "", run: () => copyTextStyle(index) },
            { label: "Paste Text Style", disabled: !Themes.hasCopiedTextStyle, run: () => pasteTextStyle(index) },
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

    // ---- Going on by itself: a slide's Go To Next Timer and a presentation's Slide
    // Show, which are ProPresenter's (show::State::advance in src/showstate.h has what
    // they do, and src/prodocument.h how the file says them). Both are kept in the
    // presentation's file at once.

    // A number of seconds as it is said: "5 s", "2.5 s"
    function secondsWords(seconds) {
        return Number(seconds.toFixed(2)) + " s"
    }

    // The panel the seconds are typed in, made now if it has not been wanted before
    function timePanel() {
        goToNextLoader.active = true
        return goToNextLoader.item
    }

    // Puts up the panel for a slide's Go To Next Timer: the one it has, or one of five
    // seconds to start from. (A GUESS at what ProPresenter starts a new one at.)
    function editGoToNext(index) {
        const slide = document.slides[index]
        const has = slide.goToNext !== undefined
        timePanel().show("Go To Next Timer", "Slide " + (index + 1) + " of “" + document.name + "”", has ? slide.goToNext.seconds : 5,
                         has && slide.goToNext.loop, true, has, (seconds, loop) => setGoToNext(index, seconds, loop))
    }

    // Gives a slide of the presentation being viewed a Go To Next Timer, or with less
    // than no seconds takes its timer off. It starts when the slide is next triggered.
    function setGoToNext(index, seconds, loop) {
        const slide = document.slides[index]
        if (!report(catalog.setSlideGoToNext(document.path, slide.id, seconds >= 0, Math.max(0, seconds), loop)))
            return
        Log.note("edit", "slide " + (index + 1) + " of " + quoted(document.name)
                 + (seconds < 0 ? " has no Go To Next Timer now"
                                : " goes on to the " + (loop ? "first slide" : "next slide") + " after " + secondsWords(seconds)))
        reloadDocument()
    }

    // The menu of the Slide Show button over the slides, as ProPresenter has it: None,
    // four ready times, and Custom… for any other. (A GUESS: how its rows are worded.)
    function showSlideShowMenu(item) {
        if (!document)
            return
        const now = document.slideShow ?? 0
        const ready = [5, 10, 15, 20]
        const custom = now > 0 && ready.indexOf(now) < 0
        const items = [
            { header: "Slide Show" },
            { label: "None", current: now === 0, run: () => setSlideShow(0) },
            { divider: true }
        ]
        for (const seconds of ready)
            items.push({ label: seconds + " Seconds", current: now === seconds, run: () => setSlideShow(seconds) })
        items.push({ label: custom ? "Custom… (" + secondsWords(now) + ")" : "Custom…", current: custom,
                     run: () => timePanel().show("Slide Show", "Each slide of “" + document.name + "” stays this long", now > 0 ? now : 5,
                                                 false, false, false, (seconds) => setSlideShow(seconds)) })
        menu.show(items, item)
    }

    // Gives the presentation being viewed a Slide Show of that many seconds a slide,
    // or with none takes it off. It starts when a slide of it is next triggered.
    function setSlideShow(seconds) {
        if ((document.slideShow ?? 0) === seconds || !report(catalog.setSlideShow(document.path, seconds)))
            return
        Log.note("edit", "the presentation " + quoted(document.name)
                 + (seconds > 0 ? " has a Slide Show of " + secondsWords(seconds) + " a slide" : " has no Slide Show now"))
        reloadDocument()
    }

    // The menu a slide's label is given from, as ProPresenter has it: Edit…, which puts
    // up the panel a label is made in; None; and the labels the workspace keeps ready,
    // each with its colour, the slide's own ticked if it is one of them.
    function labelMenuItems(index) {
        const slide = document.slides[index]
        const items = [
            { label: "Edit…", run: () => editSlideLabel(index) },
            { divider: true },
            { label: "None", current: slide.label === "" && slide.labelColor === "", run: () => setSlideLabel(index, "", "") }
        ]
        if (Labels.presets.length > 0)
            items.push({ divider: true })
        for (const preset of Labels.presets) {
            items.push({ label: preset.text, swatch: preset.color, current: slide.label === preset.text && slide.labelColor === preset.color,
                         run: () => setSlideLabel(index, preset.text, preset.color) })
        }
        return items
    }

    // The panel a label is made in, made now if it has not been wanted before
    function labelPanel() {
        slideLabelLoader.active = true
        return slideLabelLoader.item
    }

    function editSlideLabel(index) {
        const slide = document.slides[index]
        labelPanel().show("Slide Label", "Slide " + (index + 1) + " of “" + document.name + "”", slide.label, slide.labelColor,
                             (text, color) => setSlideLabel(index, text, color))
    }

    // Gives a slide of the presentation being viewed a label, words and a colour ("" for
    // none), or with neither takes its label off. The show goes on from where it is.
    function setSlideLabel(index, text, color) {
        const slide = document.slides[index]
        if (slide.label === text && slide.labelColor === color)
            return
        if (!report(catalog.setSlideLabel(document.path, slide.id, text, color)))
            return
        Log.note("edit", "slide " + (index + 1) + " of " + quoted(document.name)
                 + (text === "" && color === "" ? " has no label now" : " labelled " + quoted(text) + (color !== "" ? ", in " + color : "")))
        reloadDocument()
    }

    // The labels the workspace keeps ready, from the settings: a new one, one changed,
    // one taken out of the list. Slides that were given one keep what they were given.
    function editLabelPreset(index) {
        if (index < 0) {
            labelPanel().show("New Label", "Kept ready in this workspace, for a slide's Label menu", "", "",
                                 (text, color) => report(Labels.add(text, color)))
        } else {
            const preset = Labels.presets[index]
            labelPanel().show("Label", "Kept ready in this workspace, for a slide's Label menu", preset.text, preset.color,
                                 (text, color) => report(Labels.change(index, text, color)))
        }
    }

    // The groups a slide can be put in: the workspace's list of them, which is what
    // ProPresenter offers, each { name, color, id }; or, in a workspace that has no
    // list, the groups set up on the settings screen here, which have no ids.
    function groupChoices() {
        if (GroupKeys.groups.length > 0)
            return GroupKeys.groups
        return groups.filter(group => group.name.trim() !== "").map(group => ({ name: group.name.trim(), color: group.color, id: "" }))
    }

    // Puts a slide of the presentation being viewed into a group, and with it the slides
    // after it that are in its group now, which is what ProPresenter does: the whole
    // group, from its first slide; from a later one the group is split there (the rules
    // are in src/groupedit.h). The show goes on from the slide it is at.
    function setSlideGroup(index, group) {
        const slide = document.slides[index]
        const done = catalog.setSlideGroup(document.path, slide.id, group)
        if (!report(done.error) || !done.changed)
            return
        Log.note("edit", "slide " + (index + 1) + " of " + quoted(document.name) + ", and those after it in its group, put in the group " + quoted(group.name))
        reloadDocument()
    }

    // Deletes a slide of the presentation being viewed from its file. If it is the
    // one on the output it is taken off first: what is shown should not be something
    // that is no longer there to go back to.
    function copySlide(index) {
        if (report(catalog.copySlide(document.path, document.slides[index].id)))
            Log.note("edit", "slide " + (index + 1) + " of " + quoted(document.name) + " copied")
    }

    // The look of a slide's text boxes, kept to be put on others; and put on one, for
    // good, as a theme is (see Themes), the file as it was being kept first.
    function copyTextStyle(index) {
        if (report(Themes.copyTextStyle(document.path, document.slides[index].id)))
            Log.note("edit", "the text style of slide " + (index + 1) + " of " + quoted(document.name) + " copied")
    }

    function pasteTextStyle(index) {
        if (!report(Themes.pasteTextStyle(document.path, [document.slides[index].id])))
            return
        reloadDocument()
        notice = "Slide " + (index + 1) + " has the copied text style now. The file as it was is kept with the editor's backups."
        noticeIsError = false
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
        // (The same for one that is on the announcements layer.)
        if (viewingAnnouncements && Show.announcementLive && announcementSlide !== null && announcementSlide.id === slide.id)
            clearAnnouncements()
        Log.note("edit", "slide " + (index + 1) + " of " + quoted(document.name) + " deleted" + (slide.label !== "" ? " (" + slide.label + ")" : ""))
        if (report(catalog.removeSlide(document.path, slide.id)))
            reloadDocument()
    }

    // The menu of a file in the media bin.
    function showMediaItemMenu(media, item) {
        const items = [
            { header: "Behaviour" },
            { label: "Background", current: !media.foreground, run: () => setMediaItemForeground(media.id, false) },
            { label: "Foreground", current: media.foreground, run: () => setMediaItemForeground(media.id, true) },
            { label: "Inspector", disabled: media.missing, run: () => inspectMediaItem(media) }
        ]
        // What follows a video when it has played to its end, played from this playlist:
        // ProPresenter's Next Cue (see src/mediacue.h). (Not for a picture, which never
        // comes to an end.)
        if (media.video)
            items.push({ label: "Next Cue", items: mediaNextItems(media) })
        items.push({ header: "Playlist" })
        items.push({ label: "Remove from Playlist", run: () => report(catalog.removeMediaItem(media.id)) })
        menu.show(items, item)
    }

    // The rows of Next Cue: nothing, the next, one at random, the first, and then each of
    // the playlist's other rows, the one it has ticked. Any but nothing makes the row stop
    // at its end, or it would never get there.
    function mediaNextItems(media) {
        const next = media.next ?? ""
        const choose = (kind, target, name) => {
            Log.note("edit", quoted(media.name) + " in the media bin goes on to " + (kind === "" ? "nothing" : name))
            report(catalog.setMediaItemNext(media.id, kind, target))
        }
        return [
            { header: "Next Cue" },
            { label: "None", current: next === "", run: () => choose("", "", "") },
            { label: "Next", current: next === "next", run: () => choose("next", "", "the next") },
            { label: "Random", current: next === "random", run: () => choose("random", "", "one at random") },
            { label: "First", current: next === "first", run: () => choose("first", "", "the first") },
            { divider: true }
        ].concat(mediaFiles.filter(other => other.id !== media.id).map(other => ({
            label: other.name, disabled: other.missing, current: next === "cue" && media.nextCue === other.cueId,
            run: () => choose("cue", other.id, quoted(other.name))
        })))
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

    // The editor on the workspace's masks, at the one with the given id (or the
    // first): a mask is a slide, and is edited as one. It is come to from the Looks
    // window, where a screen is given its mask, as in ProPresenter.
    function startEditingMasks(id) {
        if (editing || !report(editScreen.openMasks(Masks.path, catalog.workspacePath, id)))
            return
        notice = ""
        editing = true
        editScreen.takeFocus()
        Log.note("edit", "the editor opened on the masks, " + (editScreen.editor.count > 0 ? "mask " + (editScreen.canvas.row + 1) + " of " + editScreen.editor.count
                                                                                              : "of which there are none"))
    }

    // The editor on a theme, at one of its slides (or its first): the same editor that
    // edits a presentation's slides, since a theme's slides are slides.
    function startEditingTheme(place, slideId) {
        const path = catalog.workspacePath + "/Themes/" + place + "/Theme"
        if (editing || !report(editScreen.openTheme(path, catalog.workspacePath, slideId)))
            return
        notice = ""
        editing = true
        editScreen.takeFocus()
        Log.note("edit", "the editor opened on the theme " + quoted(place) + ", slide " + (editScreen.canvas.row + 1) + " of " + editScreen.editor.count)
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
        if (kind === "masks") {
            // A screen that is given one of them shows it as it now is.
            report(Masks.reload())
            return
        }
        if (kind === "theme") {
            // A screen dressed in the theme by the look that is live shows it as it now is.
            Themes.reload()
            eachAudienceScene((scene, id) => redress(id))
            return
        }
        const entry = currentEntry()
        if (slidesRearranged && entry) {
            // Slides were added or deleted in the editor: the one that is live is
            // found again.
            slidesRearranged = false
            reloadDocument()
        } else if (changed && entry) {
            reloadEdited(entry)
        }
    }

    // Reads the presentation being viewed again after an editor has changed it and left
    // the same slides in the same order, so the grid can stay where it is. What is on
    // the output is left as it is until a slide is next shown.
    function reloadEdited(entry) {
        const scrolledTo = grid.contentY
        document = load(entry)
        grid.contentY = scrolledTo
        // (And a slide of it that goes live by itself is to be the slide as edited.)
        if (viewingLive)
            liveFresh = document
        if (viewingAnnouncements)
            announcementFresh = document
    }

    // ---- The Media Inspector (qml/MediaInspector.qml), for a slide's media, a row of the
    // media bin, or an element filled with media (from the editor: see Editor.inspectFill).
    // `keeper` says how its changes are kept, and `after` is called when it is put away.
    property var afterInspector: null

    function inspectMedia(media, about, keeper, after) {
        if (!media)
            return
        mediaInspectorLoader.active = true
        afterInspector = after ?? null
        mediaInspectorLoader.item.show(media, about, keeper)
    }

    // What a slide's media, or a row's, is shown in, for the inspector's preview: the
    // first audience screen, or a 1080p one
    function mediaBox() {
        const screen = Screens.audience.length > 0 ? Screens.audience[0] : null
        return screen && screen.width > 0 && screen.height > 0 ? { width: screen.width, height: screen.height } : { width: 1920, height: 1080 }
    }

    function inspectSlideMedia(index) {
        const slide = document.slides[index]
        if (!slide.media)
            return
        const file = document.path
        const id = slide.id
        const box = mediaBox()
        // (The slide's map has the media's settings only when it is set to anything: the
        // inspector has them all, as the file has them.)
        const mediaOf = () => {
            const again = catalog.open(file).slides.find(s => s.id === id)
            return again && again.media ? Object.assign({}, again.media, { box: box, missing: false, settings: catalog.slideMediaSettings(file, id) })
                                        : null
        }
        inspectMedia(mediaOf(), "Slide " + (index + 1) + " of “" + document.name + "”", {
            save: (changes) => catalog.setSlideMediaSettings(file, id, changes),
            replace: (path) => catalog.replaceSlideMediaFile(file, id, path),
            reread: mediaOf
        }, () => {
            // The grid shows the media as it is now set: its thumbnail, and what it is.
            if (document && document.path === file)
                reloadDocument()
        })
    }

    function inspectMediaItem(media) {
        const playlist = mediaPlaylistId
        const id = media.id
        const box = mediaBox()
        const node = catalog.mediaPlaylists.find(p => p.path === playlist)
        const mediaOf = () => {
            const row = catalog.mediaIn(playlist).find(r => r.id === id)
            return row ? Object.assign({}, row, { box: box, settings: catalog.mediaItemSettings(id) }) : null
        }
        inspectMedia(mediaOf(), "The media bin" + (node ? ": " + node.name : ""), {
            save: (changes) => catalog.setMediaItemSettings(id, changes),
            replace: (path) => catalog.replaceMediaItemFile(id, path),
            reread: mediaOf
        }, () => {
            const scrolledTo = mediaBin.contentY
            if (mediaPlaylistId === playlist)
                mediaFiles = catalog.mediaIn(playlist)
            mediaBin.contentY = scrolledTo
        })
    }

    // Quick Edit: the words of a slide of the grid put right over its thumbnail (`cell`,
    // the grid's item for it, if there is one), without the editor (see
    // qml/QuickEdit.qml). When it is put away the presentation is read again, as after
    // the editor.
    function quickEdit(index, cell) {
        const entry = currentEntry()
        if (editing || !entry || !openable(entry))
            return
        const slide = document.slides[index]
        quickEditLoader.active = true
        const panel = quickEditLoader.item
        const about = "Slide " + (index + 1) + " of “" + document.name + "”"
        const at = cell ? cell.mapToItem(quickEditLoader, 0, 0) : null
        const near = at ? { x: at.x, y: at.y, width: cell.width, height: cell.width * 9 / 16 } : null
        if (!report(panel.show(entry.file, catalog.workspacePath, slide.id, about, slideBackdrop(slide.id), near, (changed) => {
            Log.note("edit", "quick edit of slide " + (index + 1) + " of " + quoted(entry.name) + " put away; "
                     + (changed ? "what was typed was saved as it was typed" : "nothing was changed"))
            if (changed && currentEntry() && currentEntry().path === entry.path)
                reloadEdited(entry)
        })))
            return
        Log.note("edit", "quick edit opened on slide " + (index + 1) + " of " + quoted(entry.name))
    }

    // The url of the thumbnail of the media a slide of the presentation being viewed
    // triggers, or "": what the editor shows behind that slide.
    function slideBackdrop(slideId) {
        const slide = document ? document.slides.find(s => s.id === slideId) : undefined
        return slide && slide.media ? thumbnailUrl(slide.media.path, slide.media.settings) : ""
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
    // ---- The screens' scenes
    //
    // The ids of the screens of each kind that are drawn, in the workspace's order: all
    // but those sent out through nothing. What draws them is made from the two models,
    // which are changed a screen at a time, so that a screen coming or going leaves the
    // others, and whatever is playing on them, as they were.
    property var audienceIds: []
    property var stageIds: []
    readonly property ListModel audienceDrawn: ListModel {}
    readonly property ListModel stageDrawn: ListModel {}
    // What draws each audience screen, by the screen's id: its Output window, or the
    // OutputScene of one sent over NDI. All are told what to show; the first also plays
    // the media, and is what the preview and the transport go by.
    property var scenes: ({})
    readonly property var output: scenes[audienceIds[0]] ?? null
    // The screen the preview at the top right shows (see PreviewPanel), by id: the one
    // picked under it, kept for each workspace; with none picked, or one picked that is
    // no longer there, the first audience screen that is drawn, which is what the
    // preview always was.
    property string pickedPreviewScreen: ""
    readonly property string previewScreenId: Screens.screens.some(screen => screen.id === pickedPreviewScreen) ? pickedPreviewScreen
                                            : audienceIds[0] ?? (Screens.audience.length > 0 ? Screens.audience[0].id : "")

    function pickPreviewScreen(id) {
        const screen = Screens.screens.find(candidate => candidate.id === id)
        Log.note("preview", "the preview shows " + (screen ? quoted(screen.name) + ", " + (screen.kind === "stage" ? "a stage screen" : "an audience screen") : "nothing"))
        pickedPreviewScreen = id
        saveSelection("previewScreen", id)
    }
    // What the preview in this window shows of the look: what the live look gives the
    // screen the preview stands for, which is that first screen. So the preview has
    // the layers that screen has, and its slide in the theme the look gives that
    // screen. (Until it went by this, a layer switched off in a look was gone from
    // the screen and still in the preview, which read as the look not having worked.)
    // With no audience screen drawn, it shows everything as it is.
    readonly property var previewLook: audienceIds.length > 0 && Looks.live !== undefined ? Looks.liveOf(audienceIds[0])
                                                                                          : ({ slide: true, media: true, props: true, theme: "" })
    readonly property var previewSlide: previewLook.theme === "" || audienceIds.length === 0 ? liveSlide : slideFor(audienceIds[0], liveSlide)
    // The mask the live look gives that screen, as its slide; null for none
    readonly property var previewMask: maskSlide(previewLook.mask)

    // The slide of the mask of that id, which is what a screen given the mask lays
    // over everything else it shows; null for no id, and for one no mask has.
    function maskSlide(id) {
        if (!id)
            return null
        const mask = Masks.masks.find(candidate => candidate.id === id)
        return mask ? mask.slide : null
    }
    // The media the scenes were last told to play, with the number that playing was
    // given (see MediaFeeds), or null: what a scene made later is told to show.
    property var lastPlayed: null
    property int plays: 0
    // What sends each screen that goes over NDI, by the screen's id
    property var senders: ({})

    function senderMade(id, sender) {
        const all = Object.assign({}, senders)
        all[id] = sender
        senders = all
    }

    function senderGone(id, sender) {
        if (senders[id] !== sender)
            return
        const all = Object.assign({}, senders)
        delete all[id]
        senders = all
    }

    function followScreens() {
        const drawn = kind => Screens.screens.filter(screen => screen.kind === kind && screen.output !== "none").map(screen => screen.id)
        // Brings a model into line with a list of ids: what is gone taken out, what is
        // new put in at its place.
        const follow = (model, ids) => {
            for (let i = model.count - 1; i >= 0; --i) {
                if (!ids.includes(model.get(i).screenId))
                    model.remove(i)
            }
            for (let i = 0; i < ids.length; ++i) {
                if (i >= model.count || model.get(i).screenId !== ids[i]) {
                    let at = -1
                    for (let j = i + 1; j < model.count; ++j) {
                        if (model.get(j).screenId === ids[i])
                            at = j
                    }
                    if (at >= 0)
                        model.move(at, i, 1)
                    else
                        model.insert(i, { screenId: ids[i] })
                }
            }
        }
        const audience = drawn("audience")
        const stage = drawn("stage")
        if (audience.join("\n") !== audienceIds.join("\n")) {
            audienceIds = audience
            follow(audienceDrawn, audience)
        }
        if (stage.join("\n") !== stageIds.join("\n")) {
            stageIds = stage
            follow(stageDrawn, stage)
        }
    }

    function eachAudienceScene(act) {
        for (const id of audienceIds) {
            if (scenes[id])
                act(scenes[id], id)
        }
    }

    // ---- Looks
    //
    // How long a layer takes to come or go when the look changes, in milliseconds: what
    // the look that is live says.
    readonly property int lookFade: Math.round((Looks.live.transition ?? 0) * 1000)

    // What the toolbar's Looks button says under its picture: the name of the look
    // that is live, as ProPresenter's has it. A workspace with no live look yet (none
    // has ever been made live) just says what the button is.
    readonly property string lookName: Looks.live.name !== undefined && Looks.live.name !== "" ? Looks.live.name : "Looks"

    // The saved looks as a menu, to make one live, with the one the live look came from
    // ticked; and under a line, the way to the Looks window. Under the toolbar's Looks
    // button, as in ProPresenter. (It says nothing of the live look having been changed
    // from that look. It did once, in a note, and the note was there at the first start
    // on a workspace that ProPresenter had left with a changed live look, where it read
    // as a warning about something nobody had done. The Looks window is where that is
    // shown, with Save beside it to do something about it.)
    function looksMenuItems() {
        return [{ header: "Look" }].concat(Looks.looks.map(look => ({ label: look.name, current: Looks.live.origin === look.id,
                                                                    run: () => Show.lookId = look.id })))
            .concat(Looks.looks.length === 0 ? [{ note: "This workspace has no saved looks: every audience screen gets everything." }] : [])
            .concat([{ header: "" }, { label: "Edit Looks…", run: () => openLooks() }])
    }

    function showLooksMenu(item) {
        menu.show(looksMenuItems(), item, 0, item.height + 2)
    }

    // A slide as an audience screen shows it: as it is, or, where the look that is live
    // gives that screen a theme, dressed in the theme (see Themes), with nothing about
    // the presentation changed. Dressed, it has the theme's builds (see
    // builds::takeThemeBuilds) in place of its own. `given` is what the live look gives
    // the screen, for a binding that has read it, and so follows it (see ScreenPicture);
    // left out, it is asked for.
    function slideFor(screenId, slide, given) {
        if (!slide || !liveDocument)
            return slide
        const look = given ?? Looks.liveOf(screenId)
        if (look.theme === "")
            return slide
        const dressed = Themes.dressed(liveDocument.path, slide.id, look.theme, look.themeSlide)
        return dressed.elements !== undefined ? Object.assign({}, slide, { elements: dressed.elements, backgroundColor: dressed.backgroundColor,
                                                                           drawsBackground: dressed.drawsBackground, builds: dressed.builds,
                                                                           buildSteps: dressed.buildSteps }) : slide
    }

    // When the builds of the slide on show began on each screen where that is not when
    // they began on the rest (see liveBuildTimes): a screen the live look gave a theme
    // with builds while the slide was on show, which runs them from then. By the screens'
    // ids, as Date.now() has it; empty again when the next slide is shown.
    property var buildStarts: ({})

    // The times of the slide's steps on a screen: those of all of them, but for one that
    // began later (a step can be no earlier than the slide is there)
    function buildTimesFor(screenId) {
        const own = buildStarts[screenId]
        if (own === undefined)
            return liveBuildTimes
        return [own].concat((liveBuildTimes ?? []).slice(1).map(time => Math.max(time, own)))
    }

    // The slide that is live, shown again on a screen whose theme has changed. In a
    // theme with builds, they run from now there, as they would have had the slide come
    // on in that theme: a name in a lower third is wiped in when the look gives the
    // stream that theme, and not found already wiped away. (A GUESS at ProPresenter, and
    // what the maintainer's use of such a theme wants.) Otherwise its builds go on where
    // they are, with the other screens': their times are the screens' clock, not the
    // change's.
    function redress(screenId) {
        if (!liveSlide || !scenes[screenId])
            return
        const dressed = slideFor(screenId, liveSlide)
        const starts = Object.assign({}, buildStarts)
        if (Looks.liveOf(screenId).theme !== "" && dressed.builds !== undefined)
            starts[screenId] = Date.now() + (transitionCatalogue.shaderUrl(transition) !== "" ? Math.round(transitionDuration * 1000) : 0)
        else
            delete starts[screenId]
        buildStarts = starts
        scenes[screenId].showSlide(dressed, null, buildTimesFor(screenId))
    }

    // The transition a slide of the presentation on show is brought in with, where it
    // is not the one in use: its own, or failing that its presentation's, as
    // TransitionCatalogue.drawn gives it (with `from`, whose it is); null for the one in
    // use. ProPresenter goes by them in that order: the slide's, the presentation's,
    // and the one for every presentation.
    function slideTransitionOf(slide) {
        if (!slide)
            return null
        const own = slide.transition
        const presentation = liveDocument ? liveDocument.transition : undefined
        const asked = own !== undefined ? own : presentation !== undefined && presentation.renderId !== undefined ? presentation : undefined
        if (asked === undefined)
            return null
        return Object.assign(transitionCatalogue.drawn(asked), { from: own !== undefined ? "slide" : "presentation" })
    }

    // Media as the scenes are told to play it: with a number for this playing of it.
    function playing(media) {
        lastPlayed = Object.assign({}, media, { playId: ++plays })
        return lastPlayed
    }

    // The same for media started under a slide on the announcements layer, which is
    // another playing, of its own; null where the slide goes there with none.
    property var lastAnnouncementPlayed: null
    function playingAnnouncement(media) {
        lastAnnouncementPlayed = media.path !== undefined ? Object.assign({}, media, { playId: ++plays }) : null
        return lastAnnouncementPlayed
    }

    // A scene has been made: it is told what is live, which the others were told as it
    // happened.
    function sceneMade(id, scene) {
        const all = Object.assign({}, scenes)
        all[id] = scene
        scenes = all
        if (lastPlayed !== null)
            scene.showMedia(lastPlayed)
        if (liveSlide)
            scene.showSlide(slideFor(id, liveSlide), null, buildTimesFor(id))
        if (announcementSlide || lastAnnouncementPlayed !== null)
            scene.showAnnouncement(announcementSlide, Show.WithMedia, lastAnnouncementPlayed)
    }

    function sceneGone(id, scene) {
        if (scenes[id] !== scene)
            return
        const all = Object.assign({}, scenes)
        delete all[id]
        scenes = all
    }

    // The pictures of screens there are (see ScreenPicture): the operator's preview,
    // and every shape on show whose fill is a Screen Preview. Each says when it is made
    // and when it goes, and is told what the screens are told of the announcements,
    // which are not something it can look up; and, made while an announcement is up,
    // is told of it at once, as a scene is. (Kept in a plain object, which nothing
    // watches: a picture coming or going is no change to anything that is drawn.)
    readonly property var pictureList: ({ all: [] })
    function pictureMade(picture) {
        pictureList.all.push(picture)
        if (announcementSlide || lastAnnouncementPlayed !== null)
            picture.showAnnouncement(announcementSlide, Show.WithMedia, lastAnnouncementPlayed)
    }

    function pictureGone(picture) {
        const at = pictureList.all.indexOf(picture)
        if (at >= 0)
            pictureList.all.splice(at, 1)
    }

    function eachPicture(act) {
        for (const picture of pictureList.all.slice())
            act(picture)
    }

    // The slide of the stage layout a stage screen has, or null for the plain view
    function stageSlideOf(screenId) {
        const id = Show.screenLayouts[screenId] ?? ""
        const found = id === "" ? undefined : StageLayouts.layouts.find(candidate => candidate.id === id)
        return found ? found.slide : null
    }

    // A slide as Show is handed it when it goes live: which presentation it is of, by
    // the row it was opened from and the playlist that row is in, where it is in it,
    // and the slide itself with the one after it.
    function cueAt(presentation, key, playlist, index) {
        return { key: key, playlistId: playlist, presentation: presentation.name, index: index, count: presentation.slides.length,
                 slide: presentation.slides[index] ?? ({}), next: presentation.slides[index + 1] ?? ({}),
                 destination: presentation.destination ?? "", slideShow: presentation.slideShow ?? 0 }
    }

    // The same for a presentation that has been read again, to tell the show where
    // the slide it is at now is (Show.follow): the place the show itself is at, and
    // the place the announcements are at. Which of the two is said outright and not
    // taken from the presentation, whose destination may have been changed since its
    // slide went live: the slide is on the layer it went to.
    function liveCue(presentation, index) {
        return Object.assign(cueAt(presentation, liveKey, livePlaylistId, index), { destination: "" })
    }

    function announcementCue(presentation, index) {
        return Object.assign(cueAt(presentation, Show.announcementKey, Show.announcementPlaylistId, index), { destination: "announcements" })
    }

    // Puts a slide of the presentation being viewed on the output: the slide, the media
    // it brings, and then what its actions do (see show::State::goLive for the rules).
    // With `withoutMedia`, the slide and its actions and not the media its cue triggers:
    // what a click with Alt held does, as in ProPresenter.
    function goLive(index, withoutMedia) {
        if (!document || index < 0 || index >= document.slides.length)
            return
        // (The Bible's slides are no row of a playlist being browsed: stepping on from
        // their last goes nowhere.)
        goLiveIn(document, documentKey, viewedPlaylistId, index, withoutMedia)
        grid.positionViewAtIndex(index, GridView.Contain)
    }

    // The same for a slide of any presentation, open here or not (as catalog.open
    // reads it), by the row it is listed in and that row's playlist: what the HTTP API
    // triggers a presentation with (see HttpApiControl), which leaves what is open here
    // as it is.
    function goLiveIn(presentation, key, playlist, index, withoutMedia) {
        if (!presentation || index < 0 || index >= presentation.slides.length)
            return
        // A presentation that is sent to the announcements goes to their layer, and
        // the show stays at the slide it was at (see show::State::announcement).
        if (presentation.destination === "announcements") {
            announcementDocument = presentation
            announcementFresh = null
        } else {
            liveDocument = presentation
            liveFresh = null
        }
        Show.goLive(cueAt(presentation, key, playlist, index), withoutMedia === true)
    }

    function alreadyPlaying(media) {
        return Show.alreadyPlaying(media)
    }

    // Plays the media a slide brings, and that alone: the slide layer is left as it is.
    // As a click on the file in the media bin would. (The app's own: in ProPresenter a
    // slide's media comes with the slide or not at all.)
    function playSlideMedia(index) {
        const slide = document.slides[index]
        if (!slide.media)
            return
        Log.note("live", "the media of slide " + (index + 1) + " of " + quoted(document.name) + ", without the slide")
        showMedia(slide.media)
    }

    // Puts media on the media layer. `playlist` is the media playlist it was picked
    // from, if it was.
    function showMedia(media, playlist = "") {
        Show.showMedia(media, playlist)
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
        if (mediaPlaylistId === videoInputsNode)
            items.push({ label: "Add Video Input…", run: () => openInputSettings() })
        else if (mediaPlaylistId !== "")
            items.push({ label: "Add Media…", run: () => mediaDialog.open() })
        menu.show(items, item)
    }

    // ---- The media bin's Video Inputs (see MediaBin): ProPresenter's playlist of the
    // workspace's video inputs, which is not a playlist of the file's but the inputs
    // themselves, at the top of the bin's list; and the settings they are set up in.
    readonly property string videoInputsNode: "videoInputs"

    function openVideoInputs() {
        selectedMediaNode = videoInputsNode
        mediaPlaylistId = videoInputsNode
        refreshLists()
    }

    function openInputSettings() {
        settingsOpen = true
        settingsScreen.section = "inputs"
    }

    function showVideoInputMenu(input, item, x, y) {
        menu.show([{ header: input.name }, { label: "Show", run: () => showVideoInput(input.index) },
                   { label: "Video Inputs…", run: () => openInputSettings() }], item, x, y)
    }

    function showMediaNodeMenu(node, item) {
        if (node.path === videoInputsNode) {
            menu.show([{ label: "Video Inputs…", run: () => openInputSettings() }], item)
            return
        }
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
    // it brings the current slide back (unless that slide is one that clears itself,
    // which would only clear itself again); from another presentation it starts at
    // the top.
    //
    // Next on a slide on show that has an On Click build still to run runs that build
    // instead (see show::State::stepBuild).
    function step(delta) {
        if (delta > 0 && !viewedGoesToAnnouncements && Show.stepBuild(documentKey, viewedPlaylistId, -1))
            return
        goLive(Show.stepTarget(documentKey, viewedPlaylistId, delta, viewedGoesToAnnouncements))
    }

    // A click on a slide of the presentation being viewed: it goes live, unless it is
    // the slide on show and has an On Click build still to run, which is run instead.
    function clickSlide(index, withoutMedia) {
        if (!viewedGoesToAnnouncements && Show.stepBuild(documentKey, viewedPlaylistId, index))
            return
        goLive(index, withoutMedia)
    }

    function clearSlide() {
        Show.clearSlide()
    }

    // The announcements layer: its slide, and the media under it.
    function clearAnnouncements() {
        Show.clearAnnouncements()
    }

    function clearMedia() {
        Show.clearMedia()
    }

    // The video input layer: an input put on it by its place in the workspace's list (a
    // click on it in the media bin's Video Inputs, unless `from` says otherwise), and the
    // layer cleared
    function showVideoInput(index, from = "the media bin") {
        const input = VideoInputs.at(index)
        Log.note("live", "the video input " + quoted(input.name ?? "") + ", from " + from)
        Show.showVideoInput(index)
    }

    function clearVideoInput() {
        Show.clearVideoInput()
    }

    // The video on the media layer has played to its end, in the scene that plays it.
    // If it is the playing that is on the layer now, and was played from a media
    // playlist, what follows it there, if anything, is played (ProPresenter's Next Cue:
    // see Show.mediaFinished).
    function mediaEnded(playId) {
        if (lastPlayed === null || lastPlayed.playId !== playId || Show.liveMediaPlaylistId === "")
            return
        Show.mediaFinished(lastPlayed.id ?? "", catalog.mediaIn(Show.liveMediaPlaylistId))
    }

    // Turns a prop on, over whatever else is on the output and in front of the props
    // that are on already, or off if it is on. A collection set to show one prop at a
    // time gives up whichever of its others is on.
    function toggleProp(id) {
        Show.toggleProp(id)
    }

    // Puts a prop on or takes it off, whichever way it was: what an action asks, where
    // a click turns it over.
    function setProp(id, on) {
        Show.setProp(id, on)
    }

    function clearProps() {
        Show.clearProps()
    }

    function clearAll() {
        Show.clearAll()
    }

    // Clears what a clear group of the workspace clears: those of its layers that there
    // are here (see show::State::clearGroup).
    function clearGroup(group) {
        Show.clearGroup(group.id)
    }

    // What F1 clears: the first of the workspace's clear groups, which in a workspace as
    // ProPresenter starts one is Clear All, as that group is set up; or everything, in
    // a workspace that has been left with no group at all.
    //
    // A GUESS: that ProPresenter's F1 is for the first group of its list. It is for its
    // Clear All, which is first in the one file looked at; how it knows that group, if
    // the list is put in another order there, has not been looked at.
    function clearFirstGroup() {
        if (ClearGroups.groups.length > 0)
            clearGroup(ClearGroups.groups[0])
        else
            clearAll()
    }

    // The action that clears a clear group, as an action's map (see src/actions.h). A
    // workspace that has no file of clear groups yet is given one first, so that the
    // group the action names by its id is there for ProPresenter to find too.
    function clearGroupAction(id, name) {
        report(ClearGroups.keep())
        return { kind: "clearGroup", groupId: id, groupName: name }
    }

    // Reads what else a workspace folder holds for the show: its timers, its props and
    // its stage layouts.
    function openShowControls(path) {
        Screens.remember = remember
        Devices.remember = remember
        VideoInputs.remember = remember
        Search.read(catalog.librariesDirectory)
        for (const error of [Timers.open(path, clocksHeld), Props.open(path), StageLayouts.open(path), GroupKeys.open(path),
                             Macros.open(path), Screens.open(path), Looks.open(path), Labels.open(path), ClearGroups.open(path),
                             Messages.open(path), Masks.open(path)]) {
            if (error !== "")
                report(error)
        }
        Themes.open(path)
        // The communication devices (see src/devices.h): the workspace's, as this
        // computer keeps them
        Devices.open(path)
        // And the video inputs (see src/videoinputs.h): ProPresenter's list, with this
        // computer's devices for them
        const inputs = VideoInputs.open(path)
        if (inputs !== "")
            report(inputs)
        followScreens()
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
        const fade = Number(saved("audioFade", audioFade))
        audioFade = isFinite(fade) ? Math.max(0, Math.min(30, fade)) : 1
        audioCrossfade = String(saved("audioCrossfade", false)) === "true"
        audioDevice = String(saved("audioDevice", ""))
        const split = Number(saved("audioSplit", audioSplit))
        audioSplit = isFinite(split) ? Math.max(36, split) : 66
        stageMessageText = String(saved("stageMessage", ""))
        arrangementsShown = String(saved("arrangementsShown", false)) === "true"
        const messagesHeight = Number(saved("messagesSplit", messagesSplit))
        messagesSplit = isFinite(messagesHeight) ? Math.max(36, messagesHeight) : 62
        const previewArea = Number(saved("previewHeight", 0))
        previewHeight = isFinite(previewArea) ? Math.max(0, previewArea) : 0
        editorNotes = String(saved("editorNotes", false)) === "true"
        const notesHeight = Number(saved("editorNotesHeight", editorNotesHeight))
        editorNotesHeight = isFinite(notesHeight) ? Math.max(60, notesHeight) : 150
        chordSheetZoom = chordZoom(saved("chordSheetZoom", chordSheetZoom), 0.8)
        const reflowWidth = Number(saved("reflowThumbnail", reflowThumbnail))
        reflowThumbnail = isFinite(reflowWidth) ? Math.max(120, Math.min(480, reflowWidth)) : 220
        chordTextZoom = chordZoom(saved("chordTextZoom", chordTextZoom), 1)
        chordSheetColor = chordColorKept(saved("chordSheetColor", ""))
        chordTextColor = chordColorKept(saved("chordTextColor", ""))
        chordLineColor = chordColorKept(saved("chordLineColor", ""))
        chordSigns = String(saved("chordSigns", true)) !== "false"
        try {
            const kept = JSON.parse(String(saved("bibleSettings", "{}")))
            bibleSettings = kept !== null && typeof kept === "object" ? kept : {}
        } catch (error) {
            bibleSettings = {}
        }
        apiBibleKey = String(saved("apiBibleKey", ""))
        try {
            const triggers = JSON.parse(String(saved("colorTriggerDefault", "[]")))
            colorTriggerDefault = Array.isArray(triggers) ? triggers.filter(t => typeof t.time === "number" && typeof t.color === "string") : []
        } catch (error) {
            colorTriggerDefault = []
        }
        const decoding = String(saved("videoDecoding", "auto"))
        videoDecoding = ["processor", "hardware"].includes(decoding) ? decoding : "auto"
        decodingNoticeSeen = String(saved("decodingNoticeSeen", false)) === "true"
        simpleKeepsSidePanel = String(saved("simpleKeepsSidePanel", false)) === "true"
        // (The port and name before the switch, so that it listens once, on the right port.)
        networkPort = Number(saved("networkPort", HttpApi.defaultPort)) || HttpApi.defaultPort
        networkName = String(saved("networkName", ""))
        networkEnabled = String(saved("networkEnabled", false)) === "true"
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
        if (["timers", "props", "macros", "stage", "audio"].includes(tab))
            sidePanel.showControlTab = tab
        openShowControls(catalog.workspacePath)
        Log.watch(win, "operator window")
        logWorkspace()
        restoreSelections()
        restored = true
        save("workspace", catalog.workspacePath)
        logSettings()
        // Told once, where the guard is holding hardware video decoding back on this
        // computer: something has been decided for whoever runs it, and they should
        // know, and know where to change it. (Not in a run that keeps no settings, which
        // could not remember having said it: the self-test, the benchmark.)
        if (remember && VideoDecoding.guardInForce && !decodingNoticeSeen)
            showDecodingNotice()
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
        if (mediaPlaylist === videoInputsNode)
            openVideoInputs()
        else
            openMediaPlaylist(catalog.mediaPlaylists.some(p => p.path === mediaPlaylist && !p.folder) ? mediaPlaylist
                              : firstMediaPlaylist ? firstMediaPlaylist.path : "")
        // (A screen that is not there now is the first audience screen: see previewScreenId.)
        pickedPreviewScreen = savedSelection("previewScreen")
        const audioPlaylist = savedSelection("audioPlaylist")
        const firstAudioPlaylist = catalog.audioPlaylists.find(p => !p.folder)
        openAudioPlaylist(catalog.audioPlaylists.some(p => p.path === audioPlaylist && !p.folder) ? audioPlaylist
                          : firstAudioPlaylist ? firstAudioPlaylist.path : "")
        // Each stage screen has the layout it had, if the workspace still has that
        // layout: as it was left here, or, for a screen nothing is remembered of, as
        // ProPresenter's own file has it. (What was remembered before there could be
        // several stage screens is the first one's.)
        let had = {}
        try {
            had = JSON.parse(savedSelection("stageLayouts") || "{}")
        } catch (error) {
            had = {}
        }
        const before = savedSelection("stageLayout")
        Screens.stage.forEach((screen, at) => {
            const layout = had[screen.id] !== undefined ? had[screen.id]
                         : at === 0 && before !== "" ? before
                         : Object.keys(had).length === 0 && before === "" ? (Screens.layouts[screen.id] ?? "") : ""
            Show.setStageLayout(screen.id, StageLayouts.layouts.some(candidate => candidate.id === layout) ? layout : "")
        })
        // The live look is the workspace's own, in its file, and is what the screens
        // show from the start. Which saved look it came from is only said here:
        // nothing is made live by opening a workspace.
        Show.adoptLook(Looks.live.origin ?? "")
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
        announcementDocument = null
        liveFresh = null
        announcementFresh = null
        Show.leavePresentation()
        document = null
        documentKey = ""
        playlistId = ""
        selectedNode = ""
        libraryPath = ""
        mediaPlaylistId = ""
        selectedMediaNode = ""
        audioPlaylistId = ""
        selectedAudioNode = ""
        for (const screen of Screens.stage)
            Show.setStageLayout(screen.id, "")
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
    onChordSheetZoomChanged: save("chordSheetZoom", chordSheetZoom)
    onReflowThumbnailChanged: save("reflowThumbnail", reflowThumbnail)
    onEditorNotesChanged: save("editorNotes", editorNotes)
    onEditorNotesHeightChanged: save("editorNotesHeight", editorNotesHeight)
    onChordTextZoomChanged: save("chordTextZoom", chordTextZoom)
    onChordSheetColorChanged: save("chordSheetColor", chordSheetColor)
    onChordTextColorChanged: save("chordTextColor", chordTextColor)
    onChordLineColorChanged: save("chordLineColor", chordLineColor)
    onChordSignsChanged: save("chordSigns", chordSigns)
    onBibleSettingsChanged: save("bibleSettings", JSON.stringify(bibleSettings))
    onColorTriggerDefaultChanged: save("colorTriggerDefault", JSON.stringify(colorTriggerDefault))
    onApiBibleKeyChanged: save("apiBibleKey", apiBibleKey)
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
    onSimpleKeepsSidePanelChanged: save("simpleKeepsSidePanel", simpleKeepsSidePanel)
    onNetworkEnabledChanged: save("networkEnabled", networkEnabled)
    onNetworkNameChanged: save("networkName", networkName)
    onNetworkPortChanged: save("networkPort", networkPort)
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

    // A second without a key, and a slide's number that was being typed is given up.
    Timer {
        id: numberQuiet

        interval: 1000
        onTriggered: win.dropTypedNumber()
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
    onVideoDecodingChanged: {
        save("videoDecoding", videoDecoding)
        if (restored)
            Log.note("settings", "video decoding set to " + ({ processor: "the processor only", hardware: "the graphics chip where it can, whatever the guard says" }[videoDecoding]
                                                             ?? "automatic") + ", from the next start")
    }
    onDecodingNoticeSeenChanged: save("decodingNoticeSeen", decodingNoticeSeen)
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
    // (Not in Bible mode, nor Bible mode's slides: theirs is a file that goes when the
    // app closes, and the grid in Bible mode shows nothing else.)
    onDocumentKeyChanged: if (!bibleMode && documentKey !== bibleKey) saveSelection("presentation", documentKey)
    onMediaPlaylistIdChanged: saveSelection("mediaPlaylist", mediaPlaylistId)
    onAudioPlaylistIdChanged: saveSelection("audioPlaylist", audioPlaylistId)
    onAudioFadeChanged: save("audioFade", audioFade)
    onAudioCrossfadeChanged: save("audioCrossfade", audioCrossfade)
    onAudioDeviceChanged: save("audioDevice", audioDevice)
    onAudioSplitChanged: save("audioSplit", audioSplit)
    onStageMessageTextChanged: save("stageMessage", stageMessageText)
    onMessagesSplitChanged: save("messagesSplit", messagesSplit)
    onPreviewHeightChanged: save("previewHeight", previewHeight)
    onArrangementsShownChanged: save("arrangementsShown", arrangementsShown)
    // What each stage screen has, as last noted in the log
    property var notedLayouts: ({})

    Connections {
        target: Looks

        function onFailed(error) {
            win.report(error)
        }
    }

    Connections {
        target: Show

        // A saved look is to be made live, by hand or by an action: it is copied into
        // the live look, which is what the screens show (see Looks).
        function onLookAsked(id) {
            const error = Looks.makeLive(id)
            if (error !== "")
                win.report(error)
        }

        function onScreenLayoutsChanged() {
            const layouts = Show.screenLayouts
            win.saveSelection("stageLayouts", JSON.stringify(layouts))
            win.saveSelection("stageLayout", Show.stageLayoutId)
            for (const screen of Screens.stage) {
                const id = layouts[screen.id] ?? ""
                if ((win.notedLayouts[screen.id] ?? "") === id)
                    continue
                const layout = StageLayouts.layouts.find(candidate => candidate.id === id)
                if (win.restored)
                    Log.note("stage", (Screens.stage.length > 1 ? "the stage screen " + win.quoted(screen.name) : "the stage")
                             + (layout ? " has the layout " + win.quoted(layout.name) : " has the plain view"))
            }
            win.notedLayouts = Object.assign({}, layouts)
        }
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
        // (Only what has changed is read again.)
        Search.read(catalog.librariesDirectory)
        const firstLibrary = catalog.libraries.length > 0 ? catalog.libraries[0].path : ""
        if (playlistId !== "" && !catalog.playlists.some(p => p.path === playlistId))
            openLibrary(firstLibrary)
        else if (playlistId === "" && !catalog.libraries.some(l => l.path === libraryPath))
            openLibrary(firstLibrary)
        else if (!documents.some(d => d.path === selectedPresentation && openable(d)))
            openFirst()
        if (!catalog.playlists.some(p => p.path === selectedNode))
            selectedNode = playlistId
        if (!catalog.mediaPlaylists.some(p => p.path === mediaPlaylistId && !p.folder)) {
            const first = catalog.mediaPlaylists.find(p => !p.folder)
            openMediaPlaylist(first ? first.path : "")
        } else if (!catalog.mediaPlaylists.some(p => p.path === selectedMediaNode)) {
            selectedMediaNode = mediaPlaylistId
        }
        if (!catalog.audioPlaylists.some(p => p.path === audioPlaylistId && !p.folder)) {
            const first = catalog.audioPlaylists.find(p => !p.folder)
            openAudioPlaylist(first ? first.path : "")
        } else if (!catalog.audioPlaylists.some(p => p.path === selectedAudioNode)) {
            selectedAudioNode = audioPlaylistId
        }
    }

    Connections {
        target: win.catalog

        // Every change the editor saves is a change on disk; reading the lists again
        // for each would be a lot of work for nothing, so that waits until it is done.
        function onChanged() {
            if (win.editing)
                win.listsStale = true
            else if (!win.holdingLists)
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
        id: songDialog

        title: "Import ChordPro File"
        nameFilters: ["ChordPro files (*.cho *.chopro *.chordpro *.crd *.pro *.txt)", "All files (*)"]
        onAccepted: importSongPanel.show(decodeURIComponent(String(selectedFile).replace(/^file:\/\//, "")))
    }

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

    // Sound files are added to the playlist of the audio bin being browsed, where they
    // are on disk.
    FileDialog {
        id: audioDialog

        title: "Add Audio"
        fileMode: FileDialog.OpenFiles
        currentFolder: "file://" + win.catalog.mediaDirectory
        nameFilters: [win.catalog.audioDialogFilter, "All files (*)"]
        onAccepted: {
            Log.note("edit", win.counted(selectedFiles.length, "file", "files") + " chosen to add to the audio bin")
            win.report(win.catalog.addAudio(win.audioPlaylistId, selectedFiles))
        }
    }

    // The screens. Each one that is sent out through something (see Screens) has a scene
    // of its own, which draws what that screen shows: in a window, or out of sight for
    // NDI. Every audience scene is told the same things (see the Connections to Show
    // below) and every stage scene follows what is live, so they all show the same; but
    // each draws for itself, at its own size.
    Instantiator {
        model: win.audienceDrawn
        delegate: QtObject {
            id: audienceHost

            required property string screenId
            required property int index
            readonly property string modelData: screenId
            readonly property var screen: Screens.screens.find(candidate => candidate.id === audienceHost.modelData) ?? ({ output: "none", name: "" })
            // The first plays the media, for all of them (see MediaContent).
            readonly property bool leads: index === 0
            // What the look that is live gives this screen: which layers, and in what theme
            // (Reading `live` is what has this follow the live look as it changes.)
            readonly property var look: Looks.live !== undefined ? Looks.liveOf(audienceHost.modelData) : null
            readonly property string themeKey: look.theme === "" ? "" : look.theme + "\n" + look.themeSlide
            // The look has given this screen another theme, or none: the slide that is
            // live is shown again as it now looks here.
            onThemeKeyChanged: win.redress(audienceHost.modelData)
            readonly property bool overNdi: screen.output === "ndi"
            readonly property bool overDeckLink: screen.output === "decklink"

            property Instantiator inWindow: Instantiator {
                model: audienceHost.overNdi || audienceHost.overDeckLink ? 0 : 1
                delegate: Output {
                    objectName: audienceHost.leads ? "output" : "output-" + audienceHost.modelData
                    title: audienceHost.leads ? "Output" : "Output: " + audienceHost.screen.name
                    owner: win
                    remember: win.remember
                    shown: win.outputEnabled
                    display: audienceHost.screen.output === "display" ? audienceHost.screen.display : ""
                    fullScreenOn: audienceHost.leads && audienceHost.screen.output === "window" ? win.outputScreen : -1
                    leads: audienceHost.leads
                    slideOn: audienceHost.look.slide
                    mediaOn: audienceHost.look.media
                    announcementsOn: audienceHost.look.announcements
                    propsOn: audienceHost.look.props
                    messagesOn: audienceHost.look.messages
                    videoInputOn: audienceHost.look.videoInput !== false
                    videoInput: Show.liveVideoInput
                    lookFade: win.lookFade
                    onMediaEnded: (playId) => win.mediaEnded(playId)
                    shader: transitionCatalogue.shaderUrl(win.transition)
                    options: win.transitionUniforms.options
                    tint: win.transitionUniforms.tint
                    direction: win.transitionUniforms.direction
                    duration: Math.round(win.transitionDuration * 1000)
                    transitionName: win.transition.name
                    props: win.shownProps
                    propsDuration: Math.round(Props.transitionDuration * 1000)
                    messages: win.shownMessages
                    mask: win.maskSlide(audienceHost.look.mask)
                    pictures: win
                    keyTarget: keys
                    Component.onCompleted: win.sceneMade(audienceHost.modelData, this, true)
                    Component.onDestruction: win.sceneGone(audienceHost.modelData, this)
                }
            }

            property Instantiator sentOverNdi: Instantiator {
                model: audienceHost.overNdi ? 1 : 0
                delegate: NdiScreen {
                    objectName: "ndi-" + audienceHost.modelData
                    name: audienceHost.screen.ndiName
                    width: audienceHost.screen.ndiWidth
                    height: audienceHost.screen.ndiHeight
                    rateNumerator: Screens.rateOf(audienceHost.screen.ndiRate).numerator
                    rateDenominator: Screens.rateOf(audienceHost.screen.ndiRate).denominator
                    active: win.outputEnabled
                    Component.onCompleted: win.senderMade(audienceHost.modelData, this)
                    Component.onDestruction: win.senderGone(audienceHost.modelData, this)

                    OutputScene {
                        id: ndiScene

                        leads: audienceHost.leads
                        slideOn: audienceHost.look.slide
                        mediaOn: audienceHost.look.media
                        announcementsOn: audienceHost.look.announcements
                        propsOn: audienceHost.look.props
                        messagesOn: audienceHost.look.messages
                        videoInputOn: audienceHost.look.videoInput !== false
                        videoInput: Show.liveVideoInput
                        lookFade: win.lookFade
                        onMediaEnded: (playId) => win.mediaEnded(playId)
                        shader: transitionCatalogue.shaderUrl(win.transition)
                        options: win.transitionUniforms.options
                        tint: win.transitionUniforms.tint
                        direction: win.transitionUniforms.direction
                        duration: Math.round(win.transitionDuration * 1000)
                        transitionName: win.transition.name
                        props: win.shownProps
                        propsDuration: Math.round(Props.transitionDuration * 1000)
                        messages: win.shownMessages
                        mask: win.maskSlide(audienceHost.look.mask)
                        pictures: win
                        Component.onCompleted: win.sceneMade(audienceHost.modelData, ndiScene, true)
                        Component.onDestruction: win.sceneGone(audienceHost.modelData, ndiScene)
                    }
                }
            }

            // Out through a Blackmagic card's output (see DeckLinkScreen): the scene again,
            // as it is for NDI
            property Instantiator sentOverDeckLink: Instantiator {
                model: audienceHost.overDeckLink ? 1 : 0
                delegate: DeckLinkScreen {
                    objectName: "decklink-" + audienceHost.modelData
                    output: audienceHost.screen.decklinkOutput
                    outputName: audienceHost.screen.decklinkName
                    format: audienceHost.screen.decklinkFormat
                    alpha: audienceHost.screen.decklinkAlpha
                    key: audienceHost.screen.decklinkKey
                    blend: audienceHost.screen.decklinkBlend
                    active: win.outputEnabled
                    Component.onCompleted: win.senderMade(audienceHost.modelData, this)
                    Component.onDestruction: win.senderGone(audienceHost.modelData, this)

                    OutputScene {
                        id: cardScene

                        leads: audienceHost.leads
                        slideOn: audienceHost.look.slide
                        mediaOn: audienceHost.look.media
                        announcementsOn: audienceHost.look.announcements
                        propsOn: audienceHost.look.props
                        messagesOn: audienceHost.look.messages
                        videoInputOn: audienceHost.look.videoInput !== false
                        videoInput: Show.liveVideoInput
                        lookFade: win.lookFade
                        onMediaEnded: (playId) => win.mediaEnded(playId)
                        shader: transitionCatalogue.shaderUrl(win.transition)
                        options: win.transitionUniforms.options
                        tint: win.transitionUniforms.tint
                        direction: win.transitionUniforms.direction
                        duration: Math.round(win.transitionDuration * 1000)
                        transitionName: win.transition.name
                        props: win.shownProps
                        propsDuration: Math.round(Props.transitionDuration * 1000)
                        messages: win.shownMessages
                        mask: win.maskSlide(audienceHost.look.mask)
                        pictures: win
                        Component.onCompleted: win.sceneMade(audienceHost.modelData, cardScene, true)
                        Component.onDestruction: win.sceneGone(audienceHost.modelData, cardScene)
                    }
                }
            }
        }
    }

    Instantiator {
        model: win.stageDrawn
        delegate: QtObject {
            id: stageHost

            required property string screenId
            required property int index
            readonly property string modelData: screenId
            readonly property var screen: Screens.screens.find(candidate => candidate.id === stageHost.modelData) ?? ({ output: "none", name: "" })
            readonly property bool overNdi: screen.output === "ndi"
            readonly property bool overDeckLink: screen.output === "decklink"
            // The slide of the layout this stage screen has, or null for the plain view
            readonly property var layout: win.stageSlideOf(stageHost.modelData)

            property Instantiator inWindow: Instantiator {
                model: stageHost.overNdi || stageHost.overDeckLink ? 0 : 1
                delegate: Stage {
                    objectName: stageHost.index === 0 ? "stage" : "stage-" + stageHost.modelData
                    title: stageHost.index === 0 ? "Stage" : "Stage: " + stageHost.screen.name
                    owner: win
                    remember: win.remember
                    shown: win.stageEnabled
                    display: stageHost.screen.output === "display" ? stageHost.screen.display : ""
                    layout: stageHost.layout
                    currentText: win.stageCurrentText
                    nextText: win.stageNextText
                    pictures: win
                    keyTarget: keys
                }
            }

            property Instantiator sentOverNdi: Instantiator {
                model: stageHost.overNdi ? 1 : 0
                delegate: NdiScreen {
                    objectName: "ndi-" + stageHost.modelData
                    name: stageHost.screen.ndiName
                    width: stageHost.screen.ndiWidth
                    height: stageHost.screen.ndiHeight
                    rateNumerator: Screens.rateOf(stageHost.screen.ndiRate).numerator
                    rateDenominator: Screens.rateOf(stageHost.screen.ndiRate).denominator
                    active: win.stageEnabled
                    Component.onCompleted: win.senderMade(stageHost.modelData, this)
                    Component.onDestruction: win.senderGone(stageHost.modelData, this)

                    StageScene {
                        layout: stageHost.layout
                        currentText: win.stageCurrentText
                        nextText: win.stageNextText
                        pictures: win
                    }
                }
            }

            property Instantiator sentOverDeckLink: Instantiator {
                model: stageHost.overDeckLink ? 1 : 0
                delegate: DeckLinkScreen {
                    objectName: "decklink-" + stageHost.modelData
                    output: stageHost.screen.decklinkOutput
                    outputName: stageHost.screen.decklinkName
                    format: stageHost.screen.decklinkFormat
                    alpha: stageHost.screen.decklinkAlpha
                    key: stageHost.screen.decklinkKey
                    blend: stageHost.screen.decklinkBlend
                    active: win.stageEnabled
                    Component.onCompleted: win.senderMade(stageHost.modelData, this)
                    Component.onDestruction: win.senderGone(stageHost.modelData, this)

                    StageScene {
                        layout: stageHost.layout
                        currentText: win.stageCurrentText
                        nextText: win.stageNextText
                        pictures: win
                    }
                }
            }
        }
    }

    Connections {
        target: Screens

        function onChanged() {
            win.followScreens()
        }
    }

    // While the show is on a screen, the screens are not to go to sleep (see Awake).
    Binding {
        target: Awake
        property: "wanted"
        value: win.outputEnabled || win.stageEnabled
    }

    // What a change to the show asks of the output and the timers (see Show): the
    // output window knows nothing of presentations, and is handed what to show.
    Connections {
        target: Show

        function onSlideShown(showing, media) {
            const slide = win.liveDocument.slides[Show.liveIndex]
            const played = showing === Show.WithMedia ? win.playing(media) : null
            const transition = win.slideTransitionOf(slide)
            // The slide as each screen has it: as it is, or in the theme its look gives it
            const dressings = {}
            win.eachAudienceScene((scene, id) => dressings[id] = win.slideFor(id, slide))
            // (Only a slide with builds, here or in a screen's theme, has times: one
            // without draws as it always has.)
            const built = slide.builds !== undefined || Object.keys(dressings).some(id => dressings[id].builds !== undefined)
            const shader = transition !== null ? transition.shader : transitionCatalogue.shaderUrl(win.transition)
            const length = transition !== null ? transition.duration : Math.round(win.transitionDuration * 1000)
            // (Each only when there is something to change: a new value is a change to
            // what the previews are bound to, which they would work out again for
            // nothing at every slide of a show without builds.)
            if (Object.keys(win.buildStarts).length > 0)
                win.buildStarts = ({})
            if (built || win.liveBuildTimes !== null)
                win.liveBuildTimes = built ? [Date.now() + (shader !== "" ? length : 0)] : null
            win.eachAudienceScene((scene, id) => {
                const dressed = dressings[id]
                if (showing === Show.WithMedia)
                    scene.showSlideWithMedia(dressed, played, transition, win.liveBuildTimes)
                else if (showing === Show.OverMedia)
                    scene.showSlideOverMedia(dressed, transition, win.liveBuildTimes)
                else
                    scene.showSlide(dressed, transition, win.liveBuildTimes)
            })
        }

        function onSlideCleared() {
            if (win.liveBuildTimes !== null)
                win.liveBuildTimes = null
            if (Object.keys(win.buildStarts).length > 0)
                win.buildStarts = ({})
            win.eachAudienceScene(scene => scene.showSlide(null))
        }

        // The next of its steps, from now: never before the one before it, which may not
        // have begun, the slide's transition still running
        function onBuildStepped(step) {
            if (win.liveBuildTimes === null)
                return
            const times = win.liveBuildTimes.slice(0, step)
            times.push(Math.max(Date.now(), times[times.length - 1]))
            win.liveBuildTimes = times
            win.eachAudienceScene((scene, id) => scene.stepBuilds(win.buildTimesFor(id)))
        }

        function onMediaShown(media) {
            const played = win.playing(media)
            win.eachAudienceScene(scene => scene.showMedia(played))
        }

        function onMediaCleared() {
            win.lastPlayed = null
            win.eachAudienceScene(scene => scene.showMedia(null))
        }

        // The announcements layer: the slide the announcements are at, on every screen
        // (which of them show the layer is the live look's to say, in the scene). A
        // look's theme is for the slides of the presentation layer, and is not put on
        // these. (A GUESS: whether ProPresenter dresses the announcements in a look's
        // theme has not been looked at.)
        function onAnnouncementShown(showing, media) {
            const slide = win.announcementDocument.slides[Show.announcementIndex]
            const played = showing === Show.WithMedia ? win.playingAnnouncement(media) : null
            win.eachAudienceScene(scene => scene.showAnnouncement(slide, showing, played))
            win.eachPicture(picture => picture.showAnnouncement(slide, showing, played))
        }

        // A slide's Go To Next Timer, or its presentation's Slide Show, has run out:
        // the slide it leads to goes live, on the layer its presentation is on,
        // whatever is being viewed. (The show says which slide; the presentation is
        // the one that was read when its slide went live, and followed since.)
        function onAdvanceDue(announcements, index) {
            // (As the editor left it, if it has been edited since its slide went live.)
            if (announcements && win.announcementFresh) {
                win.announcementDocument = win.announcementFresh
                win.announcementFresh = null
            } else if (!announcements && win.liveFresh) {
                win.liveDocument = win.liveFresh
                win.liveFresh = null
            }
            const presentation = announcements ? win.announcementDocument : win.liveDocument
            if (!presentation || index < 0 || index >= presentation.slides.length)
                return
            Show.goLive(announcements ? win.announcementCue(presentation, index) : win.liveCue(presentation, index), false, true)
            if (announcements ? win.viewingAnnouncements : win.viewingLive)
                grid.positionViewAtIndex(index, GridView.Contain)
        }

        function onAnnouncementsCleared() {
            win.lastAnnouncementPlayed = null
            win.eachAudienceScene(scene => scene.clearAnnouncements())
            win.eachPicture(picture => picture.clearAnnouncements())
        }

        // The audio layer is played by AudioLayer, which is told what the show says.
        function onAudioStarted(audio) {
            AudioLayer.start(audio)
        }

        function onMusicCleared() {
            AudioLayer.stopMusic()
        }

        function onSoundsCleared() {
            AudioLayer.stopSounds()
        }

        function onTimerAction(action) {
            Timers.act(action)
        }

        // A slide's or a macro's action for a communication device
        function onCommunicate(action) {
            Devices.send(action)
        }
    }

    // The clocks a text box can show besides a timer (see Clocks). The time of day
    // stands still where the timers do, for pictures that are to come out the same at
    // any hour. And how long the video that is playing has left is told from here,
    // which has the output's player: as the player says where it has got to, and only
    // while some text box is showing it (Clocks knows that from being asked).
    Binding {
        target: Clocks
        property: "held"
        value: win.clocksHeld
    }

    readonly property var countedPlayer: Clocks.videoWatched && !clocksHeld && output ? output.livePlayer : null

    // (A video trimmed in its Media Inspector ends at its out point.)
    function tellVideoClock() {
        const player = countedPlayer
        if (player && player.duration > 0 && liveMedia !== null) {
            const end = liveMedia.settings && liveMedia.settings.trimmedOut ? Math.min(player.duration, liveMedia.settings.outPoint * 1000)
                                                                          : player.duration
            Clocks.setVideo(Math.max(0, end - player.position) / 1000, liveMedia.loops === true)
        } else {
            Clocks.noVideo()
        }
    }

    onCountedPlayerChanged: tellVideoClock()

    Connections {
        target: win.countedPlayer

        function onPositionChanged() {
            win.tellVideoClock()
        }

        function onDurationChanged() {
            win.tellVideoClock()
        }
    }

    // A piece of audio has come to its end: the show says what follows, which for
    // music that the next of its playlist follows is that, going by the playlist as it
    // is now.
    Connections {
        target: AudioLayer

        function onFinished(play) {
            Show.audioFinished(play, win.catalog.audioIn(Show.liveMusicPlaylistId))
        }
    }

    // How the audio is faded, and what it is played through, are the window's to keep
    // and AudioLayer's to go by.
    Binding {
        target: AudioLayer
        property: "fade"
        value: win.audioFade
    }

    Binding {
        target: AudioLayer
        property: "crossfade"
        value: win.audioCrossfade
    }

    Binding {
        target: AudioLayer
        property: "device"
        value: win.audioDevice
    }

    // What of the workspace a slide's actions can name, which Show goes by: a prop that
    // is no longer there, for one thing, is no longer on.
    Binding {
        target: Show
        property: "props"
        value: Props.collections
    }

    // And where it finds a playlist an action plays, when one does (see Show)
    Binding {
        target: Show
        property: "catalog"
        value: win.catalog
    }

    // A video input's sound is played where the app's audio is (see VideoInputs)
    Binding {
        target: VideoInputs
        property: "audioDevice"
        value: win.audioDevice
    }

    // The key the live song's chords are written in, and the one they are shown in
    Binding {
        target: Show
        property: "originalKey"
        value: win.liveDocument ? (win.liveDocument.originalKey ? win.liveDocument.originalKey : "C") : ""
    }

    Binding {
        target: Show
        property: "chordKey"
        value: win.liveDocument ? win.keyOf(win.liveDocument) : ""
    }

    Binding {
        target: Show
        property: "macros"
        value: Macros.collections
    }

    Binding {
        target: Show
        property: "stageLayouts"
        value: StageLayouts.layouts
    }

    Binding {
        target: Show
        property: "looks"
        value: Looks.looks.map(look => ({ id: look.id, name: look.name }))
    }

    Binding {
        target: Show
        property: "stageScreens"
        value: Screens.stage.map(screen => ({ id: screen.id, name: screen.name }))
    }

    Binding {
        target: Show
        property: "clearGroups"
        value: ClearGroups.groups.map(group => ({ id: group.id, name: group.name, layers: group.layers }))
    }

    // (How long a message is shown is said here: only one that goes after a time has
    // any, and ProPresenter's third way, when its timers have run out, is by hand here.)
    Binding {
        target: Show
        property: "messages"
        value: Messages.messages.map(message => ({ id: message.id, name: message.name, seconds: message.clear === 1 ? message.seconds : 0 }))
    }

    // A message that has been shown is taken as it is at that moment (see
    // messagesAsShown), and one that is shown no longer is let go of.
    Connections {
        target: Show

        function onMessageShown(id, values) {
            const message = Messages.messages.find(candidate => candidate.id === id)
            if (!message)
                return
            // Shown by an action that gives its text tokens words of its own, it is
            // taken with those in place of what the tokens were last given here.
            const given = values ?? {}
            const shown = Object.keys(given).length === 0 ? message : Object.assign({}, message, {
                tokens: message.tokens.map(token => token.kind === "text" && given[token.id] !== undefined
                                                    ? Object.assign({}, token, { value: given[token.id] }) : token) })
            const kept = Object.assign({}, win.messagesAsShown)
            kept[id] = shown
            win.messagesAsShown = kept
        }

        function onMessagesChanged() {
            const gone = Object.keys(win.messagesAsShown).filter(id => !Show.liveMessages.includes(id))
            if (gone.length === 0)
                return
            const kept = Object.assign({}, win.messagesAsShown)
            for (const id of gone) {
                delete kept[id]
                Messages.forget(id)
            }
            win.messagesAsShown = kept
        }
    }

    // The words of each message that is shown, worked out here once for every screen
    // and handed to Messages, where the text box that draws them reads them. One of
    // these is made for each message that is shown and for no other, so a clock in a
    // message is only asked for while that message is on the screens.
    Instantiator {
        model: win.shownMessages.map(shown => shown.id)

        delegate: QtObject {
            required property string modelData
            readonly property var message: win.messagesAsShown[modelData]
            readonly property string words: message !== undefined ? win.messageWords(message) : ""

            onWordsChanged: Messages.setWords(modelData, words)
            Component.onCompleted: Messages.setWords(modelData, words)
        }
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

    Shortcut {
        sequence: "Ctrl+R"
        context: Qt.ApplicationShortcut
        enabled: !win.settingsOpen
        onActivated: win.reflowMode()
    }

    // Bible mode, by ProPresenter's key for it on Windows (on the Mac it is Ctrl+B,
    // which here is the editor's bold)
    Shortcut {
        sequence: "Ctrl+Alt+B"
        context: Qt.ApplicationShortcut
        enabled: !win.settingsOpen
        onActivated: win.bibleModeToggle()
    }

    // The settings, by the key that most apps have for theirs. It does what the
    // toolbar's button does, and like it is there in the editor too. (Not under the
    // notice of the guard on video decoding, which has its own way to them.)
    Shortcut {
        sequence: "Ctrl+,"
        context: Qt.ApplicationShortcut
        enabled: !win.settingsOpen && !win.decodingNoticeOpen
        onActivated: win.settingsOpen = true
    }

    Item {
        id: keys

        anchors.fill: parent
        focus: true

        // A hold of the key is over when the keyboard goes elsewhere: its being let go
        // would not be heard of here.
        onActiveFocusChanged: {
            if (!activeFocus) {
                win.endHold()
                // (Nor can a number being typed be gone on with.)
                win.dropTypedNumber()
            }
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
            // A slide's number being typed (see src/slidenumber.h): the keys that finish
            // it, give it up and mend it.
            if (win.typedNumber !== "") {
                switch (event.key) {
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    win.takeTypedNumber()
                    event.accepted = true
                    return
                case Qt.Key_Escape:
                    win.dropTypedNumber()
                    event.accepted = true
                    return
                case Qt.Key_Backspace:
                    win.eraseTypedDigit()
                    event.accepted = true
                    return
                }
            }
            // A digit goes on with a number that is being typed, and begins one unless it
            // is a group's hotkey, which it then stays: it was that first.
            const plain = !(event.modifiers & (Qt.AltModifier | Qt.MetaModifier))
            if (plain && /^[0-9]$/.test(event.text) && (win.typedNumber !== "" || !win.hotkeys.some(h => h.key === event.text))) {
                // (A key held down is one digit, and not a row of them.)
                if (!event.isAutoRepeat)
                    win.typeDigit(event.text)
                event.accepted = true
                return
            }
            // Any other key gives a number up, and then does what it does. (But for the
            // keys that only go with others, which do nothing by themselves.)
            if (win.typedNumber !== ""
                    && ![Qt.Key_Shift, Qt.Key_Control, Qt.Key_Alt, Qt.Key_AltGr, Qt.Key_Meta, Qt.Key_CapsLock, Qt.Key_NumLock].includes(event.key))
                win.dropTypedNumber()
            // A letter or a digit may be a group's hotkey.
            if (/^[a-z0-9]$/i.test(event.text) && plain && !event.isAutoRepeat && win.triggerGroupKey(event.text.toUpperCase())) {
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
                win.clearFirstGroup()
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
            case Qt.Key_F5:
                win.clearAudio()
                break
            case Qt.Key_F6:
                win.clearMessages()
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
            // Kept in Simple View, it has the toolbar's room too once the toolbar has
            // gone: laid out again once, then, as the slides are, and not on the way.
            anchors.top: win.simpleKeepsSidePanel && !win.chromeShown ? parent.top : toolbar.bottom
            anchors.bottom: parent.bottom
            width: Math.max(250, Math.min(win.sidePanelWidth, win.width - 160 - 260))
            visible: win.sidePanelShown
            transform: Translate { x: win.simpleKeepsSidePanel ? 0 : win.chromeAway * sidePanel.width }
            win: win
            liveVideoSink: win.output ? win.output.liveVideoSink : null
            livePlayer: win.clocksHeld || !win.output ? null : win.output.livePlayer
            audioPlaylistDrag: audioPlaylistDrag
            onShowControlTabChanged: win.save("showControlTab", showControlTab)
        }

        // Bible mode's bar, over the slides. Made when Bible mode is first come to, and
        // kept: a passage looked up stays there to go back to.
        Loader {
            id: biblePanel

            // (The operator window, for the panel: asked for out here, where `win` is still
            // the window's name. In the panel it is the panel's own property.)
            readonly property var operatorWindow: win

            objectName: "biblePanel"
            anchors.left: win.chromeShown ? sidebar.right : parent.left
            anchors.right: win.sidePanelShown ? sidePanel.left : parent.right
            anchors.top: win.chromeShown ? toolbar.bottom : parent.top
            active: false
            visible: win.bibleMode
            // (By the panel's implicit height: a Loader whose height is set makes its item
            // that height, so a height of 0 while hidden would stay the panel's.)
            height: visible && item ? item.implicitHeight : 0

            Connections {
                target: win

                function onBibleModeChanged() {
                    if (win.bibleMode)
                        biblePanel.active = true
                }
            }

            sourceComponent: BiblePanel {
                win: biblePanel.operatorWindow
                settings: biblePanel.operatorWindow.bibleSettings
                onSettingsEdited: (settings) => biblePanel.operatorWindow.bibleSettings = settings
            }
        }

        // Between the panes while they are there, and the whole window in Simple View
        // once they have gone.
        SlideGrid {
            id: grid

            anchors.left: win.chromeShown ? sidebar.right : parent.left
            anchors.right: win.sidePanelShown ? sidePanel.left : parent.right
            anchors.top: win.bibleMode ? biblePanel.bottom : win.chromeShown ? toolbar.bottom : parent.top
            anchors.bottom: win.chromeShown ? mediaBin.top : parent.bottom
            win: win
        }

        // A slide's number as it is typed, over the middle of the slides (see
        // TypedNumber.qml). Made when a number is first typed, and not with the
        // window: a session may never type one.
        Loader {
            id: typedNumberLoader

            anchors.centerIn: grid
            z: 5
            active: false

            sourceComponent: TypedNumber {
                typed: win.typedNumber
            }
        }

        // The way back out of Simple View, where the toolbar's button for it is: the
        // same place on the screen switches the view on and off.
        SimpleViewToggle {
            id: simpleToggle

            objectName: "simpleViewToggle"
            // Over the slides, also when the pane at the right is kept and is a wide one
            x: Math.min(toolbar.simpleViewCentre + 15, win.simpleKeepsSidePanel ? sidePanel.x - 8 : win.width) - width
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
                    source: mediaDrag.media ? win.thumbnailUrl(mediaDrag.media.path, mediaDrag.media.settings) : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
            }
        }

        // What is being dragged out of the show controls, to be dropped on a slide (see
        // beginActionDrag): a point that follows the pointer, with what it is beside it.
        Item {
            id: actionDrag

            property var payload: null

            z: 50
            Drag.keys: ["action"]

            Rectangle {
                x: 12
                y: 8
                width: dragLabel.implicitWidth + 40
                height: 26
                radius: 6
                visible: actionDrag.Drag.active
                color: "#e623252b"
                border.width: 1.5
                border.color: win.accentColor

                ActionGlyph {
                    x: 9
                    anchors.verticalCenter: parent.verticalCenter
                    kind: actionDrag.payload ? actionDrag.payload.kind : ""
                    picture: actionDrag.payload ? (actionDrag.payload.picture ?? "") : ""
                }

                Text {
                    id: dragLabel

                    x: 29
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#e6e6e6"
                    font.pixelSize: 13
                    text: actionDrag.payload ? actionDrag.payload.name : ""
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

        RowDrag {
            id: audioPlaylistDrag

            Drag.keys: ["audioPlaylist"]
        }

        // A message dragged out of the messages' tab, to give a slide or a macro the
        // action that shows it
        RowDrag {
            id: messageDrag

            Drag.keys: ["message"]
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
            // (This one is there in Simple View too, where the pane is kept.)
            visible: win.chrome === 1 || (win.chrome === 0 && win.simpleKeepsSidePanel)
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
        inspectMedia: (media, about, keeper, after) => win.inspectMedia(media, about, keeper, after)
        colorTriggerDefault: win.colorTriggerDefault
        onColorTriggerDefaultKept: (list) => win.colorTriggerDefault = list
        mediaFilter: win.catalog.mediaDialogFilter
        mediaFolder: win.catalog.mediaDirectory
        canPaste: win.catalog.hasCopiedSlide
        sheetZoom: win.chordSheetZoom
        textZoom: win.chordTextZoom
        sheetChordColor: win.chordSheetColor
        textChordColor: win.chordTextColor
        sheetLineColor: win.chordLineColor
        chordSigns: win.chordSigns
        onSheetZoomEdited: (zoom) => win.chordSheetZoom = win.chordZoom(zoom, 0.8)
        reflowThumbnail: win.reflowThumbnail
        onReflowThumbnailEdited: (width) => win.reflowThumbnail = Math.max(120, Math.min(480, width))
        onTextZoomEdited: (zoom) => win.chordTextZoom = win.chordZoom(zoom, 1)
        notesShown: win.editorNotes
        notesHeight: win.editorNotesHeight
        onNotesShownEdited: (shown) => {
            win.editorNotes = shown
            Log.note("edit", "the slide's notes " + (shown ? "shown" : "put away") + " in the editor")
        }
        onNotesHeightEdited: (height) => win.editorNotesHeight = height
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
                win.clearFirstGroup()
            else if (event.key === Qt.Key_F2)
                win.clearSlide()
            else if (event.key === Qt.Key_F3)
                win.clearMedia()
            else if (event.key === Qt.Key_F4)
                win.clearProps()
            else if (event.key === Qt.Key_F5)
                win.clearAudio()
            else if (event.key === Qt.Key_F6)
                win.clearMessages()
            else
                return
            event.accepted = true
        }
    }

    // The Looks window, over the operator window
    LooksPanel {
        id: looksPanel

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 20
        visible: win.looksOpen
        win: win
        onClosed: win.closeLooks()
    }

    // The notice of the guard on hardware video decoding: over everything else in the
    // window, the settings among it, since it is the first thing to be read.
    Loader {
        id: decodingNoticeLoader

        // (The operator window, for the panel: asked for out here, where `win` is still
        // the window's name.)
        readonly property var operatorWindow: win

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 40
        active: false

        sourceComponent: DecodingNotice {
            onClosed: (settingsWanted) => {
                decodingNoticeLoader.operatorWindow.decodingNoticeSeen = true
                if (settingsWanted)
                    decodingNoticeLoader.operatorWindow.openVideoSettings()
                // (The keys go back to the show, which is also where the settings' Esc
                // is heard.)
                decodingNoticeLoader.operatorWindow.takeFocus()
            }
        }
    }

    // The window the clear groups are set up in. It is made when it is first asked
    // for, and not with the window: it has a list, a table of layers and thirty-two
    // pictures to choose from, and most sessions never open it.
    Loader {
        id: clearGroupsLoader

        // (The operator window, for the panel: asked for out here, where `win` is still
        // the window's name. In the panel it is the panel's own property.)
        readonly property var operatorWindow: win

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 20
        active: false
        visible: win.clearGroupsOpen

        sourceComponent: ClearGroupsPanel {
            win: clearGroupsLoader.operatorWindow
            onClosed: clearGroupsLoader.operatorWindow.closeClearGroups()
        }
    }

    // The window that lists the arrangements of the presentation being viewed (see
    // ArrangementsPanel.qml): made when it is first wanted.
    Loader {
        id: arrangementsLoader

        readonly property var operatorWindow: win

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 20
        active: false
        visible: win.arrangementsOpen

        sourceComponent: ArrangementsPanel {
            win: arrangementsLoader.operatorWindow
            onClosed: arrangementsLoader.operatorWindow.closeArrangements()
        }
    }

    // A label's words and colour: a slide's, or one of those kept ready (over the
    // settings screen too, which is where those are looked after). It is made when a
    // label is first to be edited, which in most sessions is never, and not with the
    // window: there are some thirty things in it.
    // The seconds of a Go To Next Timer or of a Slide Show: made when one is first to
    // be typed, as the label's panel is.
    Loader {
        id: goToNextLoader

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 30
        active: false

        sourceComponent: GoToNextPanel {
            onClosed: win.takeFocus()
        }
    }

    Loader {
        id: mediaInspectorLoader

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 40
        active: false

        sourceComponent: MediaInspector {
            owner: win
            effectChoices: win.catalog.mediaEffects()
            mediaFilter: win.catalog.mediaDialogFilter
            mediaFolder: win.catalog.mediaDirectory
            onClosed: {
                const after = win.afterInspector
                win.afterInspector = null
                if (after)
                    after()
                if (win.editing)
                    editScreen.takeFocus()
                else
                    win.takeFocus()
            }
        }
    }

    Loader {
        id: quickEditLoader

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 30
        active: false

        sourceComponent: QuickEdit {
            onClosed: win.takeFocus()
        }
    }

    Loader {
        id: slideLabelLoader

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 30
        active: false

        // (The panel has nothing called `win` of its own, on purpose: in here that
        // would be found before the window's name, and hide it.)
        sourceComponent: SlideLabel {
            onClosed: {
                if (!win.settingsOpen)
                    win.takeFocus()
            }
        }
    }

    // What a new presentation is to be called
    NewPresentation {
        id: newPresentationPanel

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 20
        win: win
        onClosed: win.takeFocus()
    }

    // What a ChordPro file holds, and how it is to come in
    ImportSong {
        id: importSongPanel

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        z: 20
        win: win
        onClosed: win.takeFocus()
    }

    // The small panel in which a timer action or a stage action is set up
    ActionDialog {
        id: actionDialog

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        win: win
        onClosed: win.takeFocus()
    }

    SettingsScreen {
        id: settingsScreen

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        visible: win.settingsOpen
        groups: win.groups
        onGroupsEdited: (groups) => win.setGroups(groups)
        onScreenMenuWanted: (rows, item, x, y) => menu.show(rows, item, x, y)
        otherHotkeys: win.otherHotkeys
        actionIconOpacity: win.actionIconOpacity
        onActionIconOpacityEdited: (opacity) => win.actionIconOpacity = Math.max(0.05, Math.min(1, opacity))
        useX11: win.useX11
        onUseX11Edited: (useX11) => win.useX11 = useX11
        videoDecoding: win.videoDecoding
        onVideoDecodingEdited: (mode) => win.videoDecoding = mode
        audioDevice: win.audioDevice
        onAudioDeviceEdited: (id) => {
            win.audioDevice = id
            Log.note("settings", id === "" ? "sound is to be played wherever the system sends it" : "a device was chosen for sound")
        }
        chordSheetColor: win.chordSheetColor
        chordTextColor: win.chordTextColor
        chordLineColor: win.chordLineColor
        chordSigns: win.chordSigns
        onChordSheetColorEdited: (color) => win.chordSheetColor = win.chordColorKept(color)
        onChordTextColorEdited: (color) => win.chordTextColor = win.chordColorKept(color)
        onChordLineColorEdited: (color) => win.chordLineColor = win.chordColorKept(color)
        onChordSignsEdited: (on) => win.chordSigns = on
        apiBibleKey: win.apiBibleKey
        onApiBibleKeyEdited: (key) => win.apiBibleKey = key
        simpleKeepsSidePanel: win.simpleKeepsSidePanel
        onSimpleKeepsSidePanelEdited: (keeps) => win.simpleKeepsSidePanel = keeps
        networkEnabled: win.networkEnabled
        networkName: win.networkName
        networkPort: win.networkPort
        midiNotHere: midiControl.notHere
        onNetworkEnabledEdited: (on) => win.networkEnabled = on
        onNetworkNameEdited: (name) => win.networkName = name
        onNetworkPortEdited: (port) => win.networkPort = port
        onLabelEdited: (index) => win.editLabelPreset(index)
        onLabelRemoved: (index) => win.report(Labels.remove(index))
        senders: win.senders
        onScreensFailed: (text) => win.report(text)
        onClosed: {
            win.settingsOpen = false
            win.takeFocus()
        }
    }

    // The HTTP API: the settings it is given, and the window's answers to it while it
    // listens (made only then: qml/HttpApiControl.qml)
    Binding {
        target: HttpApi
        property: "port"
        value: win.networkPort
    }
    Binding {
        target: HttpApi
        property: "name"
        value: win.networkName
    }
    Binding {
        target: HttpApi
        property: "enabled"
        value: win.networkEnabled
    }
    Loader {
        active: HttpApi.listening
        sourceComponent: HttpApiControl {
            window: win
        }
    }

    // What a MIDI note heard from a device's sources does, by the MIDI Map
    MidiControl {
        id: midiControl

        window: win
    }

    // The themes, let down from the toolbar's button; a click anywhere else puts them away.
    MouseArea {
        anchors.fill: parent
        anchors.topMargin: toolbar.height
        visible: win.themesOpen
        onClicked: win.closeThemes()
    }

    ThemesPanel {
        id: themesPanel

        objectName: "themesPanel"
        x: 8
        y: toolbar.height + 4
        width: Math.min(720, win.width - 16)
        height: Math.min(470, win.height - toolbar.height - 16)
        visible: win.themesOpen
        win: win
        onClosed: win.closeThemes()
    }

    SearchPanel {
        id: searchPanel

        anchors.fill: parent
        anchors.topMargin: toolbar.height
        visible: win.searchOpen
        win: win
        onClosed: {
            win.searchOpen = false
            win.takeFocus()
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        context: Qt.ApplicationShortcut
        enabled: !win.settingsOpen && !win.editing
        onActivated: win.searchOpen ? searchPanel.closed() : win.openSearch()
    }

    ResizeGrips {
        anchors.fill: parent
        target: win
    }
}
