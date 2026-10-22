import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Playback markers (src/playbackmarkers.h, the show's rule in show::State::runMarker), on the test's own media bin and
// audio bin (prepare_markers in run.py): a white video of twelve seconds that stops at its end, a still, and a tone.
//
// In the Media Inspector: the video's Playback Markers tab (a still has none); a marker added at the player with Add
// Marker, and one with a right click on the bar; the marker's menu: Rename (typed), Add Action (Clear → The Slide, and
// a prop's Trigger), Color, Duplicate, Delete, Move to Playhead; the marks on the bar. On the output: the video played
// over a live slide, the first marker clearing the slide as playback reaches it and the second putting the prop on;
// the transport's marks and its Marker menu, which takes the video to a marker and runs it once. On the stage: a text
// box of a stage layout linked, in the editor, to the presentation's previous marker's name in its colour, showing
// nothing before the first and "Lights" after it. The tone: its Inspector with Playback Markers alone, a marker
// clearing the props as it plays. after_markers in run.py reads the bins' files.
QtObject {
    id: t

//COMMON
    readonly property string runList: "0E0E0E0E-0000-0000-0000-000000000001"
    readonly property string tunes: "0A0A0A0A-0000-0000-0000-000000000001"

    function panel() {
        return mediaInspectorLoader.item
    }

    function texts(root) {
        return Lib.findAll(root, item => item.text !== undefined && item.font !== undefined && item.visible && typeof item.text === "string" && item.text !== "").map(item => item.text)
    }

    function hover(label) {
        const row = menuRow(label)
        testInput.mouse(4, row.x, row.y)
    }

    function cell(name) {
        return binCell(m => m.name === name)
    }

    function row(name) {
        return mediaFiles.find(m => m.name === name)
    }

    function transport() {
        return Lib.find(sidePanel, i => i.objectName === "transport")
    }

    // The flags on the transport
    function transportFlags() {
        return Lib.findAll(transport(), i => i.objectName === "transportMarkerMark" && i.visible)
    }

    // A marker's flag over the Inspector's bar
    function flagOf(id) {
        return Lib.find(box(), i => i.objectName === "inspectorMarkerMark" && i.modelData !== undefined && i.modelData.id === id)
    }

    function markerRow(index) {
        return named("inspectorMarker:" + index)
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // Runs again a tenth of a second later until `ready()` holds, or for at most `ms`; then `then()`
    function until(ready, ms, then) {
        return () => {
            if (kept.since === undefined)
                kept.since = testInput.now()
            if (!ready() && testInput.now() - kept.since < ms) {
                --t.at
                return 100
            }
            kept.waited = testInput.now() - kept.since
            kept.since = undefined
            return then()
        }
    }

    // The box on the stage screen, as drawn there, and how much of it is red
    function stageBox() {
        return Lib.find(testInput.windowContent("stage"), item => item.source !== undefined && item.liveLinkText !== undefined && item.source !== null
                        && item.source.id === kept.box.id && item.visible)
    }

    function stageRed() {
        const box = stageBox()
        const root = testInput.windowContent("stage")
        const at = box.mapToItem(root, 0, 0)
        return testInput.red(true, at.x / root.width, at.y / root.height, box.width / root.width, box.height / root.height)
    }

    function run() {
        steps = [
            () => {
                kept.prop = Props.collections[0].props[0]
                kept.collection = Props.collections[0]
                openMediaPlaylist(t.runList)
                return 800
            },
            // ---- The Inspector's Playback Markers tab
            () => {
                check("(the test's playlist: the video and the still, with no markers)", mediaFiles.map(m => m.name).join() === "Count.mp4,Still.png"
                      && row("Count.mp4").markers === undefined, mediaFiles.map(m => m.name).join())
                click(centre(cell("Still.png")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Inspector"))
                return 800
            },
            () => {
                check("a still's Inspector has no Playback Markers", panel().open && named("inspectorTab-markers") === null
                      && named("inspectorTab-effects") !== null)
                panel().close()
                return 400
            },
            () => {
                click(centre(cell("Count.mp4")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Inspector"))
                return 1200
            },
            () => {
                check("a video's has it, fourth", panel().open && named("inspectorTab-markers") !== null
                      && Lib.findAll(box(), i => String(i.objectName).startsWith("inspectorTab-")).map(i => i.objectName.slice(13)).join() === "information,properties,effects,markers",
                      Lib.findAll(box(), i => String(i.objectName).startsWith("inspectorTab-")).map(i => i.objectName).join())
                click(centre(named("inspectorTab-markers")))
                return 400
            },
            () => {
                check("which has no markers yet, and Add Marker", texts(box()).includes("No markers") && named("inspectorAddMarker").visible && named("inspectorAddMarker").enabled)
                panel().player.pause()
                panel().player.position = 2000
                return 600
            },
            () => {
                click(centre(named("inspectorAddMarker")))
                return 800
            },
            () => {
                const markers = panel().markers
                check("Add Marker puts one where the player is", markers.length === 1 && Math.abs(markers[0].time - 2.0) < 0.15 && markers[0].name === "",
                      JSON.stringify(markers))
                check("it is listed, and drawn on the bar", markerRow(0) !== null && markerRow(0).visible
                      && Lib.findAll(box(), i => i.objectName === "inspectorMarkerMark" && i.visible).length === 1)
                // A right click halfway along the bar
                const bar = named("inspectorScrub")
                const at = bar.mapToItem(null, bar.leftPadding + bar.availableWidth / 2, bar.height / 2)
                click(at, Qt.RightButton)
                return 400
            },
            () => {
                check("a right click on the bar offers a marker there", labels() === "Add Marker Here", labels())
                click(menuRow("Add Marker Here"))
                return 800
            },
            () => {
                const markers = panel().markers
                check("and makes it, in the order of their times", markers.length === 2 && Math.abs(markers[1].time - 6.0) < 0.4, JSON.stringify(markers.map(m => m.time)))
                check("each new one a colour of the palette the other has not, and not white", markers[0].color !== markers[1].color
                      && markers.every(m => m.color !== "#ffffff" && catalog.markerPalette().some(c => c.color === m.color)), markers.map(m => m.color).join())
                click(centre(markerRow(0)), Qt.RightButton)
                return 400
            },
            () => {
                check("a marker's menu, as ProPresenter's has it", labels() === "[Marker at 2.00 s], Rename, Color, Add Action, --, Duplicate, Move to Playhead, Delete"
                      || /^\[Marker at [0-9.]+ s\], Rename, Color, Add Action, --, Duplicate, Move to Playhead, Delete$/.test(labels()), labels())
                click(menuRow("Rename"))
                return 400
            },
            () => {
                check("Rename has its name typed in its row", named("inspectorMarkerName") !== null && named("inspectorMarkerName").visible
                      && named("inspectorMarkerName").activeFocus)
                testInput.type("Lights")
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                check("and the name kept", panel().markers[0].name === "Lights" && texts(markerRow(0)).includes("Lights"), panel().markers[0].name)
                click(centre(markerRow(0)), Qt.RightButton)
                return 300
            },
            () => {
                hover("Add Action")
                return 500
            },
            () => {
                check("Add Action has the kinds a slide's actions are", labels().includes("Clear") && labels().includes("Prop") && labels().includes("Macro"), labels())
                hover("Clear")
                return 500
            },
            () => {
                click(menuRow("The Slide"))
                return 800
            },
            () => {
                const actions = panel().markers[0].actions
                check("a Clear of the slide on it, its icon in the row", actions.length === 1 && actions[0].kind === "clear" && actions[0].layer === 5
                      && Lib.findAll(markerRow(0), i => i.objectName === "inspectorMarkerAction").length === 1, JSON.stringify(actions))
                click(centre(markerRow(1)), Qt.RightButton)
                return 300
            },
            () => {
                hover("Add Action")
                return 500
            },
            () => {
                hover("Prop")
                return 500
            },
            () => {
                hover(kept.collection.name)
                return 500
            },
            () => {
                hover(kept.prop.name)
                return 500
            },
            () => {
                click(menuRow("Trigger"))
                return 800
            },
            () => {
                const actions = panel().markers[1].actions
                check("and the second puts the prop on", actions.length === 1 && actions[0].kind === "prop" && actions[0].propId === kept.prop.id && !actions[0].clear,
                      JSON.stringify(actions))
                click(centre(markerRow(0)), Qt.RightButton)
                return 300
            },
            () => {
                check("(a marker with actions has Remove Action)", labels().includes("Remove Action"), labels())
                hover("Color")
                return 500
            },
            () => {
                const now = panel().markers[0].color
                check("(the colour picker not made until it is wanted)", markerColorPicker.item === null)
                check("Color has the palette, the marker's own colour ticked, and Other…",
                      labels() === "[Color], " + catalog.markerPalette().map(c => (c.color === now ? "*" : "") + c.name).join(", ") + ", --, Other…", labels())
                click(menuRow("Other…"))
                return 600
            },
            () => {
                check("Other… opens the system's colour picker, made then", markerColorPicker.item !== null && markerColorPicker.item.visible)
                markerColorPicker.item.selectedColor = "#123456"
                markerColorPicker.item.accept()
                return 700
            },
            () => {
                check("any colour picked there is the marker's, in the list and on the bar", panel().markers[0].color === "#123456"
                      && flagOf(panel().markers[0].id) !== null && String(flagOf(panel().markers[0].id).color) === "#123456", panel().markers[0].color)
                click(centre(markerRow(0)), Qt.RightButton)
                return 300
            },
            () => {
                hover("Color")
                return 500
            },
            () => {
                check("(Other… ticked for a colour of no name)", labels().endsWith("--, *Other…") && !labels().includes("*Red"), labels())
                click(menuRow("Red"))
                return 700
            },
            () => {
                check("made red, in the list and on the bar", panel().markers[0].color === "#e53935"
                      && String(flagOf(panel().markers[0].id).color) === "#e53935", panel().markers[0].color)
                click(centre(markerRow(1)), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Duplicate"))
                return 700
            },
            () => {
                check("Duplicate makes another, at the same time", panel().markers.length === 3 && panel().markers[2].time === panel().markers[1].time
                      && panel().markers[2].id !== panel().markers[1].id && panel().markers[2].actions.length === 1)
                click(centre(markerRow(2)), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Delete"))
                return 700
            },
            () => {
                check("and Delete takes it away", panel().markers.length === 2)
                // The first marker's flag dragged a quarter of the way along the bar (3 s)
                kept.dragged = panel().markers[0].id
                const flag = flagOf(kept.dragged)
                check("a marker's flag is over the bar, the transport's shape larger", flag !== null && flag.width === 14 && flag.height === 16
                      && Math.abs(flag.mapToItem(null, 0, flag.height - 1).y - named("inspectorScrub").handle.mapToItem(null, 0, 0).y) <= 1.5,
                      flag ? flag.width + "x" + flag.height : "none")
                const bar = named("inspectorScrub")
                kept.from = centre(flag)
                kept.to = { x: bar.mapToItem(null, bar.leftPadding + bar.availableWidth * 0.25, 0).x, y: kept.from.y }
                testInput.mouse(0, kept.from.x, kept.from.y)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.from.x + (kept.to.x - kept.from.x) * i / 10, kept.from.y)
                return 400
            },
            () => {
                check("dragged, it follows the pointer, and the player with it, to show where it is going", panel().draggedMarker === kept.dragged
                      && Math.abs(panel().draggedTime - 3.0) < 0.3 && Math.abs(panel().player.position - panel().draggedTime * 1000) < 300
                      && Math.abs(centre(flagOf(kept.dragged)).x - kept.to.x) < 2, panel().draggedTime + " " + panel().player.position)
                check("with the time it is going to shown over it", texts(flagOf(kept.dragged)).includes(panel().clock(panel().draggedTime * 1000)),
                      texts(flagOf(kept.dragged)).join())
                testInput.grab("0-dragging")
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 800
            },
            () => {
                const marker = panel().markers.find(m => m.id === kept.dragged)
                check("let go, it is kept there, with its name, colour and actions, and the player stays", marker !== undefined && Math.abs(marker.time - 3.0) < 0.3
                      && marker.name === "Lights" && marker.color === "#e53935" && marker.actions.length === 1 && panel().draggedMarker === ""
                      && Math.abs(panel().player.position - marker.time * 1000) < 400, JSON.stringify(marker) + " " + panel().player.position)
                click(centre(flagOf(panel().markers[1].id)))
                return 500
            },
            () => {
                check("a click on a flag takes the player to it", Math.abs(panel().player.position - panel().markers[1].time * 1000) < 300,
                      panel().player.position + " " + panel().markers[1].time)
                panel().player.position = 2500
                return 500
            },
            () => {
                click(centre(markerRow(0)), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Move to Playhead"))
                return 700
            },
            () => {
                check("Move to Playhead takes it to where the player is", Math.abs(panel().markers[0].time - 2.5) < 0.1 && panel().markers[0].name === "Lights",
                      panel().markers[0].time)
                testInput.grab("1-inspector")
                click(centre(named("inspectorDone")))
                return 600
            },
            // ---- On the output: a slide live, then the video
            () => {
                check("(the Inspector put away, the row has its markers)", !panel().open && row("Count.mp4").markers.length === 2)
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    const found = documents.find(d => openable(d) && catalog.open(d.file).slides.length > 2)
                    if (found) {
                        kept.entry = found
                        break
                    }
                }
                openEntry(kept.entry)
                return 600
            },
            () => {
                goLive(1)
                return 600
            },
            () => {
                check("(a slide is live)", !cleared && liveSlide !== null)
                openMediaPlaylist(t.runList)
                return 500
            },
            () => {
                click(centre(cell("Count.mp4")))
                kept.started = testInput.now()
                return 100
            },
            until(() => cleared, 6000, () => 200),
            () => {
                const after = (testInput.now() - kept.started) / 1000
                check("playback reaching the first marker runs it: the slide is cleared, at about two and a half seconds", cleared && after > 2.2 && after < 4.5,
                      after.toFixed(2) + " s")
                check("as the log says", logged(/the marker "Lights" of the video "Count.mp4": 1 action/).length === 1,
                      logged(/marker/).join(" | "))
                return 100
            },
            until(() => liveProps.length === 1, 8000, () => 200),
            () => {
                const after = (testInput.now() - kept.started) / 1000
                check("and the second puts the prop on, at about six", liveProps.length === 1 && liveProps[0] === kept.prop.id && after > 5.4 && after < 8,
                      after.toFixed(2) + " s")
                // ---- The transport's marks and its Marker menu
                const marks = Lib.findAll(sidePanel, i => i.objectName === "transportMarkerMark" && i.visible)
                check("the transport draws the markers on its bar, the first red", marks.length === 2 && marks.some(m => String(m.color) === "#e53935"), marks.length)
                check("and has its Marker menu", named("transportMarkers") !== null && named("transportMarkers").visible)
                testInput.grab("1-transport")
                toggleProp(kept.prop.id)
                return 400
            },
            () => {
                check("(the prop taken off)", liveProps.length === 0)
                kept.runs = logged(/the marker at [0-9.]+ s of the video "Count.mp4"/).length
                click(centre(named("transportMarkers")))
                return 400
            },
            () => {
                check("the Marker menu lists them by time and name", /^\[Markers\], 0:02   Lights, 0:0[5-6]$/.test(labels()), labels())
                const second = menu.shown.find(i => i.label !== undefined && i.label.startsWith("0:0") && !i.label.includes("Lights"))
                click(menuRow(second.label))
                return 900
            },
            () => {
                check("one picked takes the video there and runs it", liveProps.length === 1 && Math.abs(output.livePlayer.position / 1000 - row("Count.mp4").markers[1].time) < 1.0,
                      liveProps.length + " " + output.livePlayer.position)
                check("once", logged(/from the Marker menu/).length === 1 && logged(/the marker at [0-9.]+ s of the video "Count.mp4"/).length === kept.runs,
                      logged(/marker/).slice(-3).join(" | "))
                // ---- The transport's flags, with the video paused so that nothing runs by itself
                output.livePlayer.pause()
                Show.clearProps()
                return 400
            },
            () => {
                const tr = transport()
                check("(the transport's rename field not made until it is wanted)", named("transportMarkerName") === null)
                kept.slider = Lib.find(tr, i => i.handle !== undefined && i.availableWidth !== undefined)
                const flags = transportFlags()
                const point = f => f.mapToItem(null, f.width / 2, f.height - 1).y
                const handleTop = kept.slider.handle.mapToItem(null, 0, 0).y
                check("on the transport each marker is a small flag, its point on the top of the handle", flags.length === 2
                      && flags.every(f => f.width === 9 && f.height === 10 && Math.abs(point(f) - handleTop) <= 1.5),
                      flags.map(f => f.width + "x" + f.height + " at " + point(f).toFixed(1)).join(", ") + "; handle " + handleTop.toFixed(1))
                check("and they can be changed there: the video is a row of the media bin", tr.editable && liveProps.length === 0)
                // A click on the bar itself under the first flag is the slider's: the video goes there.
                const first = flags.find(f => f.modelData.name === "Lights")
                click({ x: first.mapToItem(null, first.width / 2, 0).x, y: kept.slider.mapToItem(null, 0, kept.slider.height / 2).y })
                return 700
            },
            () => {
                const lights = row("Count.mp4").markers[0]
                check("a click on the bar under a flag is still the slider's: the video goes there", Math.abs(output.livePlayer.position / 1000 - lights.time) < 0.6
                      && transport().draggedMarker === "" && row("Count.mp4").markers[0].time === lights.time, output.livePlayer.position + " " + lights.time)
                // The second marker's flag dragged to nine seconds
                const tr = transport()
                const flag = transportFlags().find(f => f.modelData.name !== "Lights")
                kept.second = flag.modelData.id
                kept.from = centre(flag)
                kept.to = { x: kept.slider.mapToItem(null, tr.markerX(9), 0).x, y: kept.from.y }
                kept.position = output.livePlayer.position
                testInput.mouse(0, kept.from.x, kept.from.y)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.from.x + (kept.to.x - kept.from.x) * i / 10, kept.from.y)
                return 400
            },
            () => {
                const tr = transport()
                const flag = transportFlags().find(f => f.modelData.id === kept.second)
                check("dragged on the transport, it follows the pointer, with its time over it", tr.draggedMarker === kept.second && Math.abs(tr.draggedTime - 9) < 0.3
                      && Math.abs(centre(flag).x - kept.to.x) < 2 && texts(flag).some(t => t.startsWith("0:09.")), tr.draggedTime + " " + texts(flag).join())
                check("and the live video is not moved with it", Math.abs(output.livePlayer.position - kept.position) < 100,
                      output.livePlayer.position + " " + kept.position)
                testInput.grab("1-transport-drag")
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 800
            },
            () => {
                const marker = row("Count.mp4").markers.find(m => m.id === kept.second)
                const live = (Show.liveMedia.markers ?? []).find(m => m.id === kept.second)
                const flag = transportFlags().find(f => f.modelData.id === kept.second)
                check("let go, it is kept there, in the media bin's file, with its action", marker !== undefined && Math.abs(marker.time - 9) < 0.3 && marker.actions.length === 1,
                      JSON.stringify(marker))
                check("and the live video goes by it: the transport draws it there", live !== undefined && Math.abs(live.time - marker.time) < 0.001
                      && flag !== undefined && Math.abs(centre(flag).x - kept.to.x) < 2 && transport().draggedMarker === "", live ? live.time : "none")
                check("(the log says so)", logged(/a playback marker dragged on the transport to 9\./).length === 1, logged(/dragged/).join(" | "))
                // Played from eight seconds, it runs where it now is.
                output.livePlayer.position = 8000
                output.livePlayer.play()
                return 100
            },
            until(() => liveProps.length === 1, 4000, () => 100),
            () => {
                check("played from eight seconds, the moved marker runs at nine, and not at six", liveProps.length === 1
                      && output.livePlayer.position / 1000 > 8.7 && output.livePlayer.position / 1000 < 10.3, output.livePlayer.position)
                output.livePlayer.pause()
                Show.clearProps()
                click(centre(transportFlags().find(f => f.modelData.id === kept.second)), Qt.RightButton)
                return 400
            },
            () => {
                check("its right-click menu on the transport is the Media Inspector's", /^\[Marker at 9\.[0-9]+ s\], Rename, Color, Add Action, Remove Action, --, Duplicate, Move to Playhead, Delete$/.test(labels()),
                      labels())
                click(menuRow("Rename"))
                return 400
            },
            () => {
                const field = named("transportMarkerName")
                check("Rename has its name typed over the media's name", field !== null && field.visible && field.activeFocus)
                testInput.type("Prop Cue")
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                const marker = row("Count.mp4").markers.find(m => m.id === kept.second)
                const live = (Show.liveMedia.markers ?? []).find(m => m.id === kept.second)
                check("and the name kept, in the file and on the live video, the field gone and the keys back with the show", marker.name === "Prop Cue"
                      && live.name === "Prop Cue" && named("transportMarkerName") === null && !win.activeFocusItem?.objectName?.startsWith("transport"),
                      marker.name + " " + (live ? live.name : ""))
                // Under the pointer, a flag says what it is
                const flag = transportFlags().find(f => f.modelData.id === kept.second)
                const at = centre(flag)
                testInput.mouse(1, at.x, at.y)
                return 300
            },
            () => {
                const flag = transportFlags().find(f => f.modelData.id === kept.second)
                check("under the pointer, a flag shows its time and name", flag.lit && texts(flag).some(t => /^0:09\.[0-9]  Prop Cue$/.test(t)), texts(flag).join())
                testInput.mouse(1, 5, 5)
                Show.clearMedia()
                Show.clearProps()
                return 400
            },
            // ---- A stage layout's text box linked to the previous marker's name
            () => {
                kept.layout = StageLayouts.layouts[0]
                Show.stageLayoutId = kept.layout.id
                startEditingStage(kept.layout.id)
                return 900
            },
            () => {
                kept.box = editScreen.canvas.elements.find(e => e.linkKind === "slideText") ?? editScreen.canvas.elements[1]
                editScreen.canvas.pick(kept.box.id)
                editScreen.inspector.tab = "text"
                return 500
            },
            () => {
                click(centre(named("linkChoice")))
                return 400
            },
            () => {
                check("the editor's Linked Text has Playback Marker", labels().includes("Playback Marker"), labels())
                hover("Playback Marker")
                return 500
            },
            () => {
                check("which shows a marker's name or the time to it", labels() === "[Playback Marker], Marker Name, Time to Marker", labels())
                click(menuRow("Marker Name"))
                return 600
            },
            () => {
                named("linkMarkerWhich").chosen(1)
                return 500
            },
            () => {
                named("linkMarkerColor").toggled(true)
                return 500
            },
            () => {
                const e = editScreen.canvas.selected
                check("set to the presentation's previous marker's name, in its colour", e !== null && e.linkKind === "marker" && e.linkMarkerDestination === 0
                      && e.linkMarkerWhich === 1 && e.linkMarkerTime === false && e.linkMarkerColor === true, e ? JSON.stringify([e.linkKind, e.linkMarkerWhich, e.linkMarkerColor]) : "none")
                stopEditing()
                return 800
            },
            () => {
                openMediaPlaylist(t.runList)
                return 400
            },
            () => {
                click(centre(cell("Count.mp4")))
                return 1200
            },
            () => {
                check("before the first marker the stage's box says nothing", stageBox() !== null && stageBox().liveLinkText === "" && stageRed() < 0.001,
                      stageBox() ? JSON.stringify(stageBox().liveLinkText) + " " + stageRed() : "no box")
                return 100
            },
            until(() => stageBox() !== null && stageBox().liveLinkText === "Lights", 6000, () => 500),
            () => {
                const shown = stageBox()
                check("after it, the stage shows its name, drawn in its red", shown.liveLinkText === "Lights" && String(shown.liveLinkColor) === "#e53935"
                      && stageRed() > 0.002, JSON.stringify(shown.liveLinkText) + " " + String(shown.liveLinkColor) + " " + stageRed())
                testInput.grabStage("2-stage")
                Show.clearMedia()
                Show.stageLayoutId = ""
                return 400
            },
            // ---- A piece of the audio bin
            () => {
                click(centre(Lib.findAll(sidePanel, i => i.chosen !== undefined && i.modelData !== undefined && i.modelData.id === "audio")[0]))
                return 500
            },
            () => {
                const rows = Lib.findAll(sidePanel, i => i.objectName === "audioRow" && i.visible)
                kept.tune = rows.find(r => r.modelData.name === "Tune")
                check("(the audio bin's tone)", kept.tune !== undefined, rows.map(r => r.modelData.name).join())
                click(centre(kept.tune), Qt.RightButton)
                return 300
            },
            () => {
                check("a piece's menu has Inspector", labels().includes("Inspector"), labels())
                click(menuRow("Inspector"))
                return 900
            },
            () => {
                check("which has Playback Markers and nothing else", panel().open && panel().tab === "markers"
                      && Lib.findAll(box(), i => String(i.objectName).startsWith("inspectorTab-")).map(i => i.objectName).join() === "inspectorTab-markers")
                panel().player.pause()
                panel().player.position = 1000
                return 500
            },
            () => {
                click(centre(named("inspectorAddMarker")))
                return 700
            },
            () => {
                click(centre(markerRow(0)), Qt.RightButton)
                return 300
            },
            () => {
                hover("Add Action")
                return 500
            },
            () => {
                hover("Clear")
                return 500
            },
            () => {
                click(menuRow("The Props"))
                return 700
            },
            () => {
                check("(a marker at a second, clearing the props)", panel().markers.length === 1 && panel().markers[0].actions.length === 1)
                click(centre(named("inspectorDone")))
                return 500
            },
            () => {
                toggleProp(kept.prop.id)
                return 500
            },
            () => {
                check("(the prop on)", liveProps.length === 1)
                playAudio(catalog.audioIn(t.tunes).find(r => r.name === "Tune"), t.tunes)
                return 100
            },
            until(() => liveProps.length === 0 && logged(/the marker at 1\.[0-9]+ s of the audio/).length === 1, 6000, () => 200),
            () => {
                check("the tone reaching its marker runs it: the props are cleared", liveProps.length === 0
                      && logged(/the marker at 1\.[0-9]+ s of the audio "Tune": 1 action/).length === 1, logged(/marker/).slice(-2).join(" | "))
                sidePanel.transportLayer = "audio"
                return 400
            },
            () => {
                check("the music's transport has its flag, which can be changed there: it is a piece of the audio bin", transport().editable
                      && transportFlags().length === 1, transportFlags().length)
                sidePanel.transportLayer = "media"
                Show.clearAudio()
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }

    function box() {
        return named("mediaInspector")
    }
}
