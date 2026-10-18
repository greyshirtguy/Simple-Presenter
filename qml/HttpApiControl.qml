import QtQuick
import QtMultimedia
import SimplePresenterApp

// The operator window's answers to the HTTP API (src/httpapiserver.h, which says how it
// works; src/httpapi.h has ProPresenter's rules). Made only while the API is listening,
// so that the window carries none of this otherwise.
//
// handle() is called with a route's pattern ("stage/message", or "stage/message#PUT" for
// another method than GET), the parts of the path in braces, the query and the body, and
// returns { status, data }; nothing is 404. What it reports it reads
// where the window and the show keep it; what it does it does with the window's own
// functions, the ones its keys and buttons use, so that a client operates the show as the
// person at the window would. What each answer is made of is shaped in C++
// (HttpApi.slideStatus(), HttpApi.presentation(), ...: httpapijson.h, httpapifiles.h);
// what is here is which of the window's things go into it.
//
// And it tells the API when anything it reports may have changed, so that streams are
// looked at again (HttpApi.changed(), which costs nothing while no stream is open).
Item {
    id: control

    // The operator window (qml/Main.qml)
    required property var window

    Component.onCompleted: HttpApi.handler = handle
    Component.onDestruction: HttpApi.handler = null

    // ---- answers

    function ok(data) {
        return { status: 200, data: data }
    }

    readonly property var done: ({ status: 204 })
    readonly property var notFound: ({ status: 404 })
    readonly property var bad: ({ status: 400 })

    // A body that is JSON, or undefined
    function parsed(body) {
        try {
            return JSON.parse(body)
        } catch (error) {
            return undefined
        }
    }

    // The presentation the show is at (the slide layer's), and the one the operator has
    // open: catalog.open's maps (see Catalog::open), or null
    readonly property var activeDocument: Show.atSlide ? window.liveDocument : null
    readonly property var focusedDocument: window.document ?? null

    // A presentation's place in the list of the library or playlist the operator is
    // looking through, or ProPresenter's "none" if it is not in it
    function listIndexOf(key) {
        const index = window.documents.findIndex(d => d.path === key)
        return index < 0 ? HttpApi.noIndex() : index
    }

    // The first slide of a group of a presentation, the group by UUID, name or index of
    // its groups (ProPresenter's); -1 for none
    function groupStart(document, groupId) {
        const at = HttpApi.resolve(document.groups ?? [], groupId, "id", "name")
        if (at < 0)
            return -1
        const name = document.groups[at].name
        return document.slides.findIndex(slide => slide.group === name)
    }

    // A slide of the presentation the show is at goes live, as a click on it would
    // had it been open: by its place, from 0
    function triggerActive(index) {
        const document = activeDocument
        if (!document || index < 0 || index >= document.slides.length)
            return notFound
        window.goLiveIn(document, Show.liveKey, Show.livePlaylistId, index)
        return done
    }

    // The same for the presentation on the announcements layer
    function triggerAnnouncement(index) {
        const document = Show.announcementAtSlide ? window.announcementDocument : null
        if (!document || index < 0 || index >= document.slides.length)
            return notFound
        window.goLiveIn(document, Show.announcementKey, Show.announcementPlaylistId, index)
        return done
    }

    // A slide of a presentation that need not be open here goes live, the presentation
    // as a row of a library or a playlist is (a library's rows are its files), leaving
    // what is open in the window as it is: a GUESS that ProPresenter's API, too, triggers
    // without moving the operator's focus, which is what its "focus" endpoints are for.
    function triggerRow(row, playlist, index) {
        if (!row || !window.openable(row))
            return notFound
        const document = window.load(row)
        if (document.error !== "" || index < 0 || index >= document.slides.length)
            return notFound
        window.goLiveIn(document, row.path, playlist, index)
        return done
    }

    // ---- the workspace's things: each found by UUID, name or index in its list

    function findIn(list, id) {
        return list[HttpApi.resolve(list, id, "id", "name")]
    }

    // Every prop, and every macro, in their collections' order
    function allProps() {
        let props = []
        for (const collection of Props.collections)
            props = props.concat(collection.props)
        return props
    }

    function allMacros() {
        let macros = []
        for (const collection of Macros.collections)
            macros = macros.concat(collection.macros)
        return macros
    }

    // The audience screens, by id, in their order: what a look's screens follow
    function audienceIds() {
        return Screens.audience.map(screen => screen.id)
    }

    // The live look, as ProPresenter gives it: an id of its own (not that of the saved
    // look it was made from, as ProPresenter's is not: the maintainer), the name of the
    // saved look it was made from
    function currentLook() {
        const live = Looks.live
        const origin = Looks.looks.find(look => look.id === (live.origin ?? ""))
        return HttpApi.look(live, audienceIds(), live.name ?? (origin ? origin.name : ""), 0)
    }

    function timerAt(id) {
        const index = HttpApi.resolve(Timers.timers, id, "id", "name")
        return index < 0 ? null : { timer: Timers.timers[index], index: index }
    }

    // A stage layout's id, by its UUID; an empty one for the plain view
    function layoutId(layoutUuid) {
        const index = StageLayouts.layouts.findIndex(layout => layout.id === layoutUuid)
        return index < 0 ? HttpApi.id("", "", 0) : HttpApi.id(layoutUuid, StageLayouts.layouts[index].name, index)
    }

    // The media playlists or the audio bin's: which, by UUID or name anywhere, by index
    // among those at the top, or by a path of names
    function binOf(nodes, id) {
        const exact = nodes.find(n => n.path.toUpperCase() === String(id).toUpperCase()) ?? nodes.find(n => n.name === id)
        if (exact)
            return exact
        if (/^[0-9]+$/.test(id))
            return nodes.filter(n => n.parent === "")[Number(id)]
        let parent = ""
        let node
        for (const name of String(id).split("/")) {
            node = nodes.find(n => n.parent === parent && n.name === name)
            if (!node)
                return undefined
            parent = node.path
        }
        return node
    }

    function binId(nodes, node) {
        const siblings = nodes.filter(n => n.parent === node.parent)
        return HttpApi.id(node.path, node.name, siblings.findIndex(n => n.path === node.path))
    }

    // Plays a row of a media playlist, or of the audio bin, as a click on it there does
    function playBinRow(audio, playlistPath, row) {
        if (!row || row.missing)
            return notFound
        if (audio)
            window.playAudio(row, playlistPath)
        else
            window.showMedia(row, playlistPath)
        return done
    }

    // The row after (or before) the one playing from a playlist, or the first
    function stepBin(rows, playingPath, delta) {
        const at = rows.findIndex(row => row.path === playingPath)
        return at < 0 ? (delta > 0 ? rows[0] : undefined) : rows[at + delta]
    }

    // ---- libraries and playlists

    readonly property var catalog: window.catalog

    // The library an id names (by UUID, name or index), or undefined
    function libraryOf(id) {
        const libraries = catalog.libraries.map(library => ({ uuid: HttpApi.libraryUuid(library.name), name: library.name }))
        return catalog.libraries[HttpApi.resolve(libraries, id, "uuid", "name")]
    }

    // A library's rows that are presentations, each with its UUID for resolving
    function presentationRowsOf(rows) {
        return rows.map(row => Object.assign({}, row, { uuid: row.kind === "presentation" ? HttpApi.presentationUuid(row.file) : "" }))
    }

    // The playlist (of presentations) an id names: by UUID or name anywhere in the tree,
    // by index among those at the top, or by a path of names through folders
    // ("Folder 2/Playlist3"), as ProPresenter's documentation says
    function playlistOf(id) {
        const all = catalog.playlists
        const exact = all.find(p => p.path.toUpperCase() === String(id).toUpperCase()) ?? all.find(p => p.name === id)
        if (exact)
            return exact
        if (/^[0-9]+$/.test(id))
            return all.filter(p => p.parent === "")[Number(id)]
        let parent = ""
        let node
        for (const name of String(id).split("/")) {
            node = all.find(p => p.parent === parent && p.name === name)
            if (!node)
                return undefined
            parent = node.path
        }
        return node
    }

    // A playlist's id as ProPresenter gives it: its index is its place among those beside it
    function playlistId(playlist) {
        const siblings = catalog.playlists.filter(p => p.parent === playlist.parent)
        return HttpApi.id(playlist.path, playlist.name, siblings.findIndex(p => p.path === playlist.path))
    }

    // {playlist, item, playlist_item} for a row of a playlist, nulls for none
    function placeIn(playlistPath, rowKey) {
        const playlist = catalog.playlists.find(p => p.path === playlistPath)
        if (!playlist)
            return { playlist: null, item: null, playlist_item: null }
        const rows = catalog.playlistItems(playlist.path)
        const at = rows.findIndex(row => row.path === rowKey)
        return { playlist: playlistId(playlist),
                 item: at < 0 ? null : HttpApi.id(rows[at].path, rows[at].name, at),
                 playlist_item: at < 0 ? null : HttpApi.playlistItem(rows[at], at) }
    }

    // The row of a playlist that is the next (or previous) presentation from a row, or
    // the first from none; undefined at either end
    function stepRow(rows, fromKey, delta) {
        const presentations = rows.filter(row => window.openable(row))
        const at = presentations.findIndex(row => row.path === fromKey)
        if (at < 0)
            return delta > 0 ? presentations[0] : undefined
        return presentations[at + delta]
    }

    // Every presentation file of the workspace's libraries
    function allFiles() {
        // (A list property of C++'s is not quite an array: it has no flatMap.)
        let files = []
        for (const library of catalog.libraries)
            files = files.concat(catalog.documentsIn(library.path).filter(row => window.openable(row)).map(row => row.file))
        return files
    }

    // The library row of the presentation with this UUID, and its library; or null
    function findPresentation(uuid) {
        const file = HttpApi.findPresentation(uuid, allFiles())
        if (file === "")
            return null
        for (const library of catalog.libraries) {
            const row = catalog.documentsIn(library.path).find(r => r.file === file)
            if (row)
                return { row: row, library: library }
        }
        return null
    }

    // Opens a row of a library or a playlist in the window, as clicking it there would
    function focusRow(libraryPath, playlistPath, rowKey) {
        if (playlistPath !== "") {
            if (window.playlistId !== playlistPath)
                window.openPlaylist(playlistPath)
        } else if (window.playlistId !== "" || window.libraryPath !== libraryPath) {
            window.openLibrary(libraryPath)
        }
        if (window.documentKey !== rowKey)
            window.openDocument(rowKey)
        return window.documentKey === rowKey ? done : notFound
    }

    // The next (or previous) playlist from the one open, folders passed over
    function stepPlaylist(delta) {
        const playlists = catalog.playlists.filter(p => !p.folder)
        const at = playlists.findIndex(p => p.path === window.playlistId)
        return playlists[at < 0 ? (delta > 0 ? 0 : -1) : at + delta]
    }

    // A slide of the presentation open in the window goes live, as a click on it does
    function triggerFocused(index) {
        const document = focusedDocument
        if (!document || index < 0 || index >= document.slides.length)
            return notFound
        window.goLive(index)
        return done
    }

    // Seconds of the media playing on the media layer: where it is, and how long it is
    function mediaPlayer() {
        return window.output ? window.output.livePlayer : null
    }

    function handle(route, params, query, body) {
        switch (route) {
        // ---- status
        case "status/layers":
            return ok({ video_input: false,
                        media: !!Show.liveMedia,
                        slide: Show.atSlide && !Show.cleared,
                        announcements: Show.announcementLive,
                        props: Show.liveProps.length > 0,
                        messages: Show.liveMessages.length > 0,
                        audio: !!Show.liveMusic || Show.liveSounds.length > 0 })
        case "status/slide":
            // (A GUESS: that ProPresenter says nothing is current while the slide layer
            // is cleared, as nothing is on it; the next slide is still the next.)
            return ok(HttpApi.slideStatus(Show.atSlide && !Show.cleared ? Show.currentSlide : {}, Show.nextSlide))
        case "status/screens":
            return ok(Screens.screens.map((screen, i) => ({ id: HttpApi.id(screen.id, screen.name, i),
                                                            size: { width: screen.width, height: screen.height },
                                                            screen_type: screen.kind })))
        case "status/audience_screens":
            return ok(window.outputEnabled)
        case "status/stage_screens":
            return ok(window.stageEnabled)
        case "status/audience_screens#PUT":
        case "status/stage_screens#PUT": {
            const on = parsed(body)
            if (typeof on !== "boolean")
                return bad
            if (route.startsWith("status/audience"))
                window.outputEnabled = on
            else
                window.stageEnabled = on
            return done
        }

        // ---- presentations
        case "presentation/active": {
            const document = activeDocument
            return ok(document ? HttpApi.presentation(document.path, HttpApi.noIndex()) : { presentation: null })
        }
        case "presentation/focused": {
            const document = focusedDocument
            return ok(document ? HttpApi.id(document.uuid ?? "", document.name, listIndexOf(window.documentKey)) : null)
        }
        case "presentation/slide_index": {
            const document = activeDocument
            return ok(document ? HttpApi.slideIndex("presentation_index", document.uuid ?? "", document.name, HttpApi.noIndex(),
                                                    Show.liveIndex, document.slides.length)
                               : HttpApi.slideIndex("presentation_index", "", "", 0, -1, 0))
        }
        case "announcement/slide_index": {
            const document = Show.announcementAtSlide ? window.announcementDocument : null
            return ok(document ? HttpApi.slideIndex("announcement_index", document.uuid ?? "", document.name, HttpApi.noIndex(),
                                                    Show.announcementIndex, document.slides.length)
                               : HttpApi.slideIndex("announcement_index", "", "", 0, -1, 0))
        }
        case "presentation/active/trigger":
            return triggerActive(0)
        case "presentation/active/{index}/trigger":
            return triggerActive(Number(params.index))
        case "presentation/active/next/trigger":
            return activeDocument ? triggerActive(Show.stepTarget(Show.liveKey, Show.livePlaylistId, 1, false)) : notFound
        case "presentation/active/previous/trigger":
            return activeDocument ? triggerActive(Show.stepTarget(Show.liveKey, Show.livePlaylistId, -1, false)) : notFound
        case "presentation/active/group/{group_id}/trigger":
            return activeDocument ? triggerActive(groupStart(activeDocument, params.group_id)) : notFound
        case "presentation/focused/trigger":
            return triggerFocused(0)
        case "presentation/focused/{index}/trigger":
            return triggerFocused(Number(params.index))
        case "presentation/focused/next/trigger":
        case "presentation/focused/previous/trigger":
            if (!focusedDocument)
                return notFound
            window.step(route.includes("/next/") ? 1 : -1)
            return done
        case "presentation/focused/group/{group_id}/trigger":
            return focusedDocument ? triggerFocused(groupStart(focusedDocument, params.group_id)) : notFound
        case "presentation/active/timeline":
        case "presentation/focused/timeline":
            // (The app plays no timeline: one that is not running, at its start.)
            return ok({ is_running: false, current_time: 0 })

        // ---- a presentation by its UUID
        case "presentation/{uuid}": {
            const found = findPresentation(params.uuid)
            return found ? ok(HttpApi.presentation(found.row.file, catalog.documentsIn(found.library.path).findIndex(r => r.file === found.row.file)))
                         : notFound
        }
        case "presentation/{uuid}/trigger":
        case "presentation/{uuid}/{index}/trigger": {
            const found = findPresentation(params.uuid)
            return found ? triggerRow(found.row, "", params.index === undefined ? 0 : Number(params.index)) : notFound
        }
        case "presentation/{uuid}/next/trigger":
        case "presentation/{uuid}/previous/trigger": {
            const found = findPresentation(params.uuid)
            if (!found)
                return notFound
            // From the slide the show is at, if it is at this presentation; from its top if not
            const at = Show.atSlide && Show.liveKey === found.row.path ? Show.liveIndex : -1
            return triggerRow(found.row, "", route.includes("/next/") ? at + 1 : Math.max(0, at - 1))
        }
        case "presentation/{uuid}/group/{group_id}/trigger": {
            const found = findPresentation(params.uuid)
            if (!found)
                return notFound
            const document = window.load(found.row)
            return triggerRow(found.row, "", groupStart(document, params.group_id))
        }
        case "presentation/{uuid}/focus": {
            const found = findPresentation(params.uuid)
            return found ? focusRow(found.library.path, "", found.row.path) : notFound
        }
        case "presentation/active/focus":
            if (!activeDocument)
                return notFound
            return Show.livePlaylistId !== "" ? focusRow("", Show.livePlaylistId, Show.liveKey)
                                              : focusRow(catalog.libraries.find(l => Show.liveKey.startsWith(l.path + "/"))?.path ?? window.libraryPath,
                                                         "", Show.liveKey)
        case "presentation/next/focus":
            window.stepDocument(1)
            return done
        case "presentation/previous/focus":
            window.stepDocument(-1)
            return done

        // ---- the announcements layer
        case "announcement/active": {
            const document = Show.announcementAtSlide ? window.announcementDocument : null
            return ok({ announcement: document ? HttpApi.presentation(document.path, HttpApi.noIndex()).presentation ?? null : null })
        }
        case "announcement/active/trigger":
            return triggerAnnouncement(0)
        case "announcement/active/{index}/trigger":
            return triggerAnnouncement(Number(params.index))
        case "announcement/active/next/trigger":
            return triggerAnnouncement(Show.announcementIndex + 1)
        case "announcement/active/previous/trigger":
            return triggerAnnouncement(Show.announcementIndex - 1)
        case "announcement/active/focus":
            if (!Show.announcementAtSlide)
                return notFound
            return Show.announcementPlaylistId !== "" ? focusRow("", Show.announcementPlaylistId, Show.announcementKey)
                                                      : focusRow(catalog.libraries.find(l => Show.announcementKey.startsWith(l.path + "/"))?.path
                                                                 ?? window.libraryPath, "", Show.announcementKey)
        case "announcement/active/timeline":
            return ok({ is_running: false, current_time: 0 })

        // ---- libraries
        case "libraries":
            return ok(HttpApi.libraries(catalog.libraries.map(library => library.name)))
        case "library/{library_id}": {
            const library = libraryOf(params.library_id)
            return library ? ok(HttpApi.library(catalog.documentsIn(library.path))) : notFound
        }
        case "library/{library_id}/{presentation_id}/trigger":
        case "library/{library_id}/{presentation_id}/{index}/trigger": {
            const library = libraryOf(params.library_id)
            if (!library)
                return notFound
            const rows = presentationRowsOf(catalog.documentsIn(library.path))
            const row = rows[HttpApi.resolve(rows, params.presentation_id, "uuid", "name")]
            return triggerRow(row, "", params.index === undefined ? 0 : Number(params.index))
        }

        // ---- playlists
        case "playlists":
            return ok(HttpApi.playlistTree(catalog.playlists))
        case "playlist/{playlist_id}":
        case "playlist/{playlist_id}/updates": {
            const playlist = playlistOf(params.playlist_id)
            return playlist ? ok({ id: playlistId(playlist), items: playlist.folder ? [] : HttpApi.playlistItems(catalog.playlistItems(playlist.path)) })
                            : notFound
        }
        case "playlist/active":
            return ok({ presentation: placeIn(Show.livePlaylistId, Show.liveKey),
                        announcements: placeIn(Show.announcementPlaylistId, Show.announcementKey) })
        case "playlist/focused":
            return ok(placeIn(window.playlistId, window.documentKey))
        case "playlist/focused/next":
        case "playlist/focused/previous": {
            if (window.playlistId === "")
                return notFound
            const rows = catalog.playlistItems(window.playlistId)
            const row = stepRow(rows, window.documentKey, route.endsWith("next") ? 1 : -1)
            return ok({ item: row ? HttpApi.id(row.path, row.name, rows.indexOf(row)) : null })
        }
        case "playlist/{playlist_id}/trigger":
        case "playlist/{playlist_id}/{index}/trigger":
        case "playlist/{playlist_id}/{index}/{cue_index}/trigger": {
            const playlist = playlistOf(params.playlist_id)
            if (!playlist || playlist.folder)
                return notFound
            const rows = catalog.playlistItems(playlist.path)
            const row = params.index === undefined ? rows.find(r => window.openable(r)) : rows[Number(params.index)]
            return triggerRow(row, playlist.path, params.cue_index === undefined ? 0 : Number(params.cue_index))
        }
        case "playlist/{playlist_id}/next/trigger":
        case "playlist/{playlist_id}/previous/trigger": {
            // From the item the show is at, if it is at one of this playlist; from the first if not
            const playlist = playlistOf(params.playlist_id)
            if (!playlist || playlist.folder)
                return notFound
            const rows = catalog.playlistItems(playlist.path)
            const from = Show.livePlaylistId === playlist.path ? Show.liveKey : ""
            return triggerRow(stepRow(rows, from, route.includes("/next/") ? 1 : -1), playlist.path, 0)
        }
        case "playlist/focused/trigger":
        case "playlist/focused/{index}/trigger":
        case "playlist/focused/{index}/{cue_index}/trigger":
        case "playlist/focused/next/trigger":
        case "playlist/focused/previous/trigger": {
            // The playlist open in the window: the item is opened there too, as the
            // operator stepping through it would, and its slide taken live
            if (window.playlistId === "")
                return notFound
            const rows = catalog.playlistItems(window.playlistId)
            const row = route === "playlist/focused/trigger" ? rows.find(r => window.openable(r))
                      : route.includes("/next/") ? stepRow(rows, window.documentKey, 1)
                      : route.includes("/previous/") ? stepRow(rows, window.documentKey, -1)
                      : rows[Number(params.index)]
            if (!row || !window.openable(row))
                return notFound
            window.openDocument(row.path)
            return triggerFocused(params.cue_index === undefined ? 0 : Number(params.cue_index))
        }
        case "playlist/active/presentation/trigger":
        case "playlist/active/presentation/{index}/trigger":
        case "playlist/active/presentation/{index}/{cue_index}/trigger":
        case "playlist/active/announcement/trigger":
        case "playlist/active/announcement/{index}/trigger":
        case "playlist/active/announcement/{index}/{cue_index}/trigger": {
            const playlistPath = route.includes("/announcement/") ? Show.announcementPlaylistId : Show.livePlaylistId
            if (playlistPath === "")
                return notFound
            const rows = catalog.playlistItems(playlistPath)
            const row = params.index === undefined ? rows.find(r => window.openable(r)) : rows[Number(params.index)]
            return triggerRow(row, playlistPath, params.cue_index === undefined ? 0 : Number(params.cue_index))
        }
        case "playlist/active/presentation/focus":
            return Show.livePlaylistId !== "" ? focusRow("", Show.livePlaylistId, Show.liveKey) : notFound
        case "playlist/active/announcement/focus":
            return Show.announcementPlaylistId !== "" ? focusRow("", Show.announcementPlaylistId, Show.announcementKey) : notFound
        case "playlist/{playlist_id}/focus": {
            const playlist = playlistOf(params.playlist_id)
            if (!playlist || playlist.folder)
                return notFound
            window.openPlaylist(playlist.path)
            return done
        }
        case "playlist/next/focus":
        case "playlist/previous/focus": {
            const playlist = stepPlaylist(route.startsWith("playlist/next") ? 1 : -1)
            if (!playlist)
                return notFound
            window.openPlaylist(playlist.path)
            return done
        }
        case "playlist/focused/{index}/timeline":
        case "playlist/active/presentation/{index}/timeline":
        case "playlist/active/announcement/{index}/timeline":
            return ok({ is_running: false, current_time: 0 })

        // ---- looks
        case "looks":
            return ok(Looks.looks.map((look, i) => HttpApi.look(look, audienceIds(), look.name, i)))
        case "look/current":
            return ok(currentLook())
        case "look/{id}": {
            const at = HttpApi.resolve(Looks.looks, params.id, "id", "name")
            return at < 0 ? notFound : ok(HttpApi.look(Looks.looks[at], audienceIds(), Looks.looks[at].name, at))
        }
        case "look/{id}/trigger": {
            const look = findIn(Looks.looks, params.id)
            if (!look)
                return notFound
            Show.lookId = look.id
            return done
        }

        // ---- props
        case "props":
            return ok(allProps().map((prop, i) => HttpApi.prop(prop, i, Show.liveProps.includes(prop.id))))
        case "prop/{id}": {
            const props = allProps()
            const at = HttpApi.resolve(props, params.id, "id", "name")
            return at < 0 ? notFound : ok(HttpApi.prop(props[at], at, Show.liveProps.includes(props[at].id)))
        }
        case "prop/{id}/trigger":
        case "prop/{id}/clear": {
            const prop = findIn(allProps(), params.id)
            if (!prop)
                return notFound
            window.setProp(prop.id, route.endsWith("trigger"))
            return done
        }
        case "prop_collections":
            return ok({ prop_collections: { collections: Props.collections.map((collection, i) => ({
                id: HttpApi.id(collection.id, collection.name, i),
                props: collection.props.map((prop, j) => HttpApi.prop(prop, j, Show.liveProps.includes(prop.id))) })) } })
        case "prop_collection/{id}": {
            const at = HttpApi.resolve(Props.collections, params.id, "id", "name")
            if (at < 0)
                return notFound
            const collection = Props.collections[at]
            return ok({ id: HttpApi.id(collection.id, collection.name, at),
                        props: collection.props.map((prop, j) => HttpApi.prop(prop, j, Show.liveProps.includes(prop.id))) })
        }

        // ---- messages
        case "messages":
            return ok(Messages.messages.map((message, i) => HttpApi.message(message, i, Show.liveMessages.includes(message.id))))
        case "message/{id}": {
            const at = HttpApi.resolve(Messages.messages, params.id, "id", "name")
            return at < 0 ? notFound : ok(HttpApi.message(Messages.messages[at], at, Show.liveMessages.includes(Messages.messages[at].id)))
        }
        case "message/{id}/trigger#POST": {
            const message = findIn(Messages.messages, params.id)
            if (!message)
                return notFound
            // The words of its text tokens, where the request gives them, as typed into
            // them in the Messages panel would be (which keeps them in the workspace's
            // file of messages, as ProPresenter keeps them)
            const given = body.trim() === "" ? [] : parsed(body)
            if (!Array.isArray(given))
                return bad
            for (const token of given) {
                const ours = (message.tokens ?? []).find(t => t.name === token.name && t.kind === "text")
                if (ours && token.text && typeof token.text.text === "string" && token.text.text !== ours.value)
                    window.report(Messages.setValue(message.id, ours.id, token.text.text))
            }
            window.showMessage(message.id)
            return done
        }
        case "message/{id}/clear": {
            const message = findIn(Messages.messages, params.id)
            if (!message)
                return notFound
            window.hideMessage(message.id)
            return done
        }

        // ---- timers
        case "timers":
            return ok(Timers.timers.map((timer, i) => HttpApi.timer(timer, i)))
        case "timers/current":
            return ok(Timers.timers.map((timer, i) => HttpApi.timerCurrent(timer, Timers.state(timer.id), i)))
        case "timer/{id}": {
            const found = timerAt(params.id)
            return found ? ok(HttpApi.timer(found.timer, found.index)) : notFound
        }
        case "timer/{id}/{operation}": {
            const found = timerAt(params.id)
            if (!found)
                return notFound
            switch (params.operation) {
            case "start": Timers.start(found.timer.id); return done
            case "stop": Timers.stop(found.timer.id); return done
            case "reset": Timers.reset(found.timer.id); return done
            }
            return notFound
        }
        case "timers/{operation}":
            if (!["start", "stop", "reset"].includes(params.operation))
                return notFound
            for (const timer of Timers.timers) {
                if (params.operation === "start")
                    Timers.start(timer.id)
                else if (params.operation === "stop")
                    Timers.stop(timer.id)
                else
                    Timers.reset(timer.id)
            }
            return done
        case "timer/{id}/increment/{time}": {
            const found = timerAt(params.id)
            const seconds = Number(params.time)
            if (!found || !isFinite(seconds))
                return notFound
            Timers.act({ action: 5, timerId: found.timer.id, timerName: found.timer.name, amount: seconds })
            return done
        }

        // ---- stage
        case "stage/screens":
            return ok(Screens.stage.map((screen, i) => HttpApi.id(screen.id, screen.name, i)))
        case "stage/layouts":
            return ok(StageLayouts.layouts.map((layout, i) => ({ id: HttpApi.id(layout.id, layout.name, i) })))
        case "stage/layout_map":
            return ok(Screens.stage.map((screen, i) => ({ screen: HttpApi.id(screen.id, screen.name, i),
                                                          layout: layoutId(Show.screenLayouts[screen.id] ?? "") })))
        case "stage/layout_map#PUT": {
            const asked = parsed(body)
            if (!Array.isArray(asked))
                return bad
            for (const entry of asked) {
                const screen = entry && entry.screen ? findIn(Screens.stage, entry.screen.uuid ?? entry.screen.name ?? "") : undefined
                const layout = entry && entry.layout ? findIn(StageLayouts.layouts, entry.layout.uuid ?? entry.layout.name ?? "") : undefined
                if (!screen || !layout)
                    return bad
                Show.setStageLayout(screen.id, layout.id)
            }
            return done
        }
        case "stage/screen/{id}/layout": {
            const screen = findIn(Screens.stage, params.id)
            return screen ? ok(layoutId(Show.screenLayouts[screen.id] ?? "")) : notFound
        }
        case "stage/screen/{id}/layout/{layout_id}": {
            const screen = findIn(Screens.stage, params.id)
            const layout = findIn(StageLayouts.layouts, params.layout_id)
            if (!screen || !layout)
                return notFound
            Show.setStageLayout(screen.id, layout.id)
            return done
        }

        // ---- clear groups, macros, masks, global groups
        case "clear/groups":
            return ok(ClearGroups.groups.map((group, i) => HttpApi.clearGroup(group, i)))
        case "clear/group/{id}": {
            const at = HttpApi.resolve(ClearGroups.groups, params.id, "id", "name")
            return at < 0 ? notFound : ok(HttpApi.clearGroup(ClearGroups.groups[at], at))
        }
        case "clear/group/{id}/trigger": {
            const group = findIn(ClearGroups.groups, params.id)
            if (!group)
                return notFound
            window.clearGroup(group)
            return done
        }
        case "macros":
            return ok(allMacros().map((macro, i) => HttpApi.macro(macro, i)))
        case "macro/{id}": {
            const macros = allMacros()
            const at = HttpApi.resolve(macros, params.id, "id", "name")
            return at < 0 ? notFound : ok(HttpApi.macro(macros[at], at))
        }
        case "macro/{id}/trigger": {
            const macro = findIn(allMacros(), params.id)
            if (!macro)
                return notFound
            window.runMacro(macro.id)
            return done
        }
        case "macro_collections":
            return ok({ collections: Macros.collections.map((collection, i) => ({
                id: HttpApi.id(collection.id, collection.name, i), macros: collection.macros.map((macro, j) => HttpApi.macro(macro, j)) })) })
        case "macro_collection/{id}": {
            const at = HttpApi.resolve(Macros.collections, params.id, "id", "name")
            if (at < 0)
                return notFound
            const collection = Macros.collections[at]
            return ok({ id: HttpApi.id(collection.id, collection.name, at), macros: collection.macros.map((macro, j) => HttpApi.macro(macro, j)) })
        }
        case "masks":
            return ok(Masks.masks.map((mask, i) => ({ id: HttpApi.id(mask.id, mask.name, i) })))
        case "mask/{id}": {
            const at = HttpApi.resolve(Masks.masks, params.id, "id", "name")
            return at < 0 ? notFound : ok({ id: HttpApi.id(Masks.masks[at].id, Masks.masks[at].name, at) })
        }
        case "groups":
            return ok(GroupKeys.groups.map((group, i) => ({ id: HttpApi.id(group.id, group.name, i), color: HttpApi.color(group.color) })))

        // ---- media playlists and the audio bin
        case "media/playlists":
            return ok(HttpApi.binTree(catalog.mediaPlaylists))
        case "audio/playlists":
            return ok(HttpApi.binTree(catalog.audioPlaylists))
        case "media/playlist/{playlist_id}":
        case "media/playlist/{playlist_id}/updates":
        case "audio/playlist/{playlist_id}":
        case "audio/playlist/{playlist_id}/updates": {
            const audio = route.startsWith("audio")
            const nodes = audio ? catalog.audioPlaylists : catalog.mediaPlaylists
            const node = binOf(nodes, params.playlist_id)
            if (!node)
                return notFound
            const rows = node.folder ? [] : (audio ? catalog.audioIn(node.path) : catalog.mediaIn(node.path))
            return ok({ id: binId(nodes, node), items: rows.map((row, i) => HttpApi.binItem(row, i, audio)) })
        }
        case "media/playlist/active":
        case "audio/playlist/active": {
            const audio = route.startsWith("audio")
            const nodes = audio ? catalog.audioPlaylists : catalog.mediaPlaylists
            const playlistPath = audio ? Show.liveMusicPlaylistId : Show.liveMediaPlaylistId
            const playing = audio ? Show.liveMusic : Show.liveMedia
            const node = nodes.find(n => n.path === playlistPath)
            if (!node || !playing)
                return ok({ playlist: null, item: null })
            const rows = audio ? catalog.audioIn(node.path) : catalog.mediaIn(node.path)
            const at = rows.findIndex(row => row.path === playing.path)
            return ok({ playlist: binId(nodes, node), item: at < 0 ? null : HttpApi.id(rows[at].id, rows[at].name, at) })
        }
        case "media/playlist/focused":
        case "audio/playlist/focused": {
            const audio = route.startsWith("audio")
            const nodes = audio ? catalog.audioPlaylists : catalog.mediaPlaylists
            const node = nodes.find(n => n.path === (audio ? window.audioPlaylistId : window.mediaPlaylistId))
            return ok(node ? binId(nodes, node) : null)
        }
        case "media/playlist/{playlist_id}/{media_id}/trigger":
        case "audio/playlist/{playlist_id}/{id}/trigger":
        case "media/playlist/{playlist_id}/trigger":
        case "audio/playlist/{playlist_id}/trigger":
        case "media/playlist/{playlist_id}/next/trigger":
        case "audio/playlist/{playlist_id}/next/trigger":
        case "media/playlist/{playlist_id}/previous/trigger":
        case "audio/playlist/{playlist_id}/previous/trigger":
        case "media/playlist/active/{media_id}/trigger":
        case "audio/playlist/active/{id}/trigger":
        case "media/playlist/active/trigger":
        case "audio/playlist/active/trigger":
        case "media/playlist/active/next/trigger":
        case "audio/playlist/active/next/trigger":
        case "media/playlist/active/previous/trigger":
        case "audio/playlist/active/previous/trigger":
        case "media/playlist/focused/{media_id}/trigger":
        case "audio/playlist/focused/{id}/trigger":
        case "media/playlist/focused/trigger":
        case "audio/playlist/focused/trigger":
        case "media/playlist/focused/next/trigger":
        case "audio/playlist/focused/next/trigger":
        case "media/playlist/focused/previous/trigger":
        case "audio/playlist/focused/previous/trigger":
        case "trigger/media/next":
        case "trigger/media/previous":
        case "trigger/audio/next":
        case "trigger/audio/previous": {
            const audio = route.startsWith("audio") || route.startsWith("trigger/audio")
            const nodes = audio ? catalog.audioPlaylists : catalog.mediaPlaylists
            const activePath = audio ? Show.liveMusicPlaylistId : Show.liveMediaPlaylistId
            const which = route.startsWith("trigger/") || route.includes("/active/") ? "active"
                        : route.includes("/focused/") ? "focused" : "given"
            const node = which === "active" ? nodes.find(n => n.path === activePath)
                       : which === "focused" ? nodes.find(n => n.path === (audio ? window.audioPlaylistId : window.mediaPlaylistId))
                       : binOf(nodes, params.playlist_id)
            if (!node || node.folder)
                return notFound
            const rows = audio ? catalog.audioIn(node.path) : catalog.mediaIn(node.path)
            const playing = audio ? Show.liveMusic : Show.liveMedia
            const playingPath = playing && node.path === activePath ? playing.path : ""
            const item = params.media_id ?? params.id
            const row = item !== undefined ? rows[HttpApi.resolve(rows, item, "id", "name")]
                      : route.includes("next") ? stepBin(rows, playingPath, 1)
                      : route.includes("previous") ? stepBin(rows, playingPath, -1)
                      : rows[0]
            return playBinRow(audio, node.path, row)
        }
        case "media/playlist/{playlist_id}/focus":
        case "audio/playlist/{playlist_id}/focus":
        case "media/playlist/active/focus":
        case "audio/playlist/active/focus":
        case "media/playlist/next/focus":
        case "audio/playlist/next/focus":
        case "media/playlist/previous/focus":
        case "audio/playlist/previous/focus": {
            const audio = route.startsWith("audio")
            const nodes = audio ? catalog.audioPlaylists : catalog.mediaPlaylists
            const open = audio ? window.audioPlaylistId : window.mediaPlaylistId
            const playlists = nodes.filter(n => !n.folder)
            const at = playlists.findIndex(n => n.path === open)
            const node = route.includes("/active/") ? nodes.find(n => n.path === (audio ? Show.liveMusicPlaylistId : Show.liveMediaPlaylistId))
                       : route.includes("/next/") ? playlists[at + 1]
                       : route.includes("/previous/") ? playlists[at < 0 ? 0 : at - 1]
                       : binOf(nodes, params.playlist_id)
            if (!node || node.folder)
                return notFound
            if (audio)
                window.openAudioPlaylist(node.path)
            else
                window.openMediaPlaylist(node.path)
            return done
        }

        // ---- the transport of what plays on the presentation layer (the media layer)
        case "transport/{layer}/play":
        case "transport/{layer}/pause":
        case "transport/{layer}/go_to_end":
        case "transport/{layer}/skip_forward/{time}":
        case "transport/{layer}/skip_backward/{time}":
        case "transport/{layer}/time#PUT": {
            // (Only the media layer's player can be worked here: the audio bin's music and
            // the announcements' media have no transport of their own in the app.)
            const player = mediaPlayer()
            if (params.layer !== "presentation" || !player || !Show.liveMedia)
                return notFound
            if (route.endsWith("/play")) {
                player.play()
            } else if (route.endsWith("/pause")) {
                player.pause()
            } else {
                let seconds
                if (route.includes("go_to_end"))
                    seconds = player.duration / 1000 - (Number(query.time) || 0)
                else if (route.includes("skip_forward"))
                    seconds = player.position / 1000 + Number(params.time)
                else if (route.includes("skip_backward"))
                    seconds = player.position / 1000 - Number(params.time)
                else
                    seconds = parsed(body)
                if (typeof seconds !== "number" || !isFinite(seconds))
                    return bad
                player.position = Math.max(0, Math.min(player.duration, seconds * 1000))
            }
            return done
        }
        case "transport/{layer}/auto_advance":
            if (params.layer === "presentation")
                return ok(Show.advanceWaiting)
            if (params.layer === "announcement")
                return ok(Show.announcementAdvanceWaiting)
            return notFound

        // ---- next and previous, as the arrow keys
        case "trigger/next":
            window.step(1)
            return done
        case "trigger/previous":
            window.step(-1)
            return done

        // ---- clearing
        case "clear/layer/{layer}":
            switch (params.layer) {
            case "audio": window.clearAudio(); return done
            case "props": window.clearProps(); return done
            case "messages": window.clearMessages(); return done
            case "announcements": window.clearAnnouncements(); return done
            case "slide": window.clearSlide(); return done
            case "media": window.clearMedia(); return done
            // (There is no video input here to clear.)
            case "video_input": return done
            }
            return notFound

        // ---- the stage message
        case "stage/message":
            return ok(Show.stageMessage)
        case "stage/message#PUT": {
            const text = parsed(body)
            if (typeof text !== "string")
                return bad
            window.stageMessageText = text
            Show.showStageMessage(text)
            return done
        }
        case "stage/message#DELETE":
            Show.hideStageMessage()
            return done

        // ---- clocks, and what plays
        case "timer/video_countdown": {
            const player = mediaPlayer()
            return ok(HttpApi.videoCountdown(player && Show.liveMedia ? (player.duration - player.position) / 1000 : 0))
        }
        case "transport/{layer}/current":
            if (params.layer === "presentation") {
                const player = mediaPlayer()
                const media = Show.liveMedia
                return ok({ is_playing: !!media && !!player && player.playbackState === MediaPlayer.PlayingState,
                            uuid: media ? (media.id ?? "") : "",
                            name: media ? media.name : "",
                            artist: "",
                            audio_only: false,
                            duration: player && media ? player.duration / 1000 : 0 })
            }
            if (params.layer === "audio") {
                const music = Show.liveMusic
                return ok({ is_playing: !!music, uuid: music ? (music.id ?? "") : "", name: music ? music.name : "", artist: "",
                            audio_only: true, duration: music ? AudioLayer.musicDuration / 1000 : 0 })
            }
            if (params.layer === "announcement")
                return ok({ is_playing: false, uuid: "", name: "", artist: "", audio_only: false, duration: 0 })
            return notFound
        case "transport/{layer}/time":
            if (params.layer === "presentation") {
                const player = mediaPlayer()
                return ok(player && Show.liveMedia ? player.position / 1000 : 0)
            }
            if (params.layer === "audio")
                return ok(Show.liveMusic ? AudioLayer.musicPosition / 1000 : 0)
            if (params.layer === "announcement")
                return ok(0)
            return notFound

        // ---- what the app has nothing of yet
        case "capture/status":
            return ok({ status: "inactive", capture_time: "0:00:00", status_text: "" })
        case "video_inputs":
            return ok([])
        }
        return undefined
    }

    // ---- telling the API when what it reports may have changed

    Connections {
        target: Show
        function onChanged() { HttpApi.changed() }
        function onSlideChanged() { HttpApi.changed() }
        function onAnnouncementChanged() { HttpApi.changed() }
        function onMediaChanged() { HttpApi.changed() }
        function onAudioChanged() { HttpApi.changed() }
        function onPropsChanged() { HttpApi.changed() }
        function onMessagesChanged() { HttpApi.changed() }
        function onStageMessageChanged() { HttpApi.changed() }
        function onScreenLayoutsChanged() { HttpApi.changed() }
        function onLookChanged() { HttpApi.changed() }
        function onWorkspaceChanged() { HttpApi.changed() }
    }

    Connections {
        target: control.window
        function onDocumentChanged() { HttpApi.changed() }
        function onDocumentKeyChanged() { HttpApi.changed() }
        function onDocumentsChanged() { HttpApi.changed() }
        function onPlaylistIdChanged() { HttpApi.changed() }
        function onLibraryPathChanged() { HttpApi.changed() }
        function onMediaPlaylistIdChanged() { HttpApi.changed() }
        function onAudioPlaylistIdChanged() { HttpApi.changed() }
        function onLiveDocumentChanged() { HttpApi.changed() }
        function onAnnouncementDocumentChanged() { HttpApi.changed() }
        function onOutputEnabledChanged() { HttpApi.changed() }
        function onStageEnabledChanged() { HttpApi.changed() }
    }

    Connections {
        target: Screens
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: control.catalog
        function onChanged() { HttpApi.changed() }
    }

    // The workspace's own lists; and the timers, whose running is said once a second
    Connections {
        target: Looks
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: Props
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: Messages
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: Timers
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: StageLayouts
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: ClearGroups
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: Macros
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: Masks
        function onChanged() { HttpApi.changed() }
    }

    Connections {
        target: control.window.output ? control.window.output.livePlayer : null
        ignoreUnknownSignals: true
        function onPlaybackStateChanged() { HttpApi.changed() }
        function onDurationChanged() { HttpApi.changed() }
    }
}
