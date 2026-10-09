import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Simple View: the slides and nothing else. Its button in the toolbar, the button that floats over the slides while it
// is on, and the ~ key, which has to be held.
QtObject {
    id: t

//COMMON
    readonly property int tilde: Qt.Key_QuoteLeft
    property var frames: ({})
    // Watches the panes go: how many frames the window drew while they slid, and how long the slides then took to fill it
    property Timer watch: Timer {
        interval: 2
        repeat: true
        onTriggered: {
            const now = testInput.now()
            if (t.frames.gone === undefined && win.chrome === 0) {
                t.frames.gone = now
                t.frames.atGone = Log.frames(win)
            } else if (t.frames.gone !== undefined && t.frames.filled === undefined && Log.frames(win) > t.frames.atGone + 1) {
                t.frames.filled = now
                stop()
            }
        }
    }

    function geometry() {
        return [grid.x, grid.y, grid.width, grid.height, grid.columns].map(n => Math.round(n)).join(",")
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function hold(ms, more) {
        // The key goes down, and the pairs a held key sends follow it for as long as it is held
        testInput.keyDown(more?.key ?? tilde, false, more?.text ?? "`", more?.modifiers ?? 0, more?.scanCode ?? 0)
        kept.repeats = 0
        kept.holding = more ?? {}
        repeater.start()
    }

    function letGo() {
        repeater.stop()
        const more = kept.holding
        testInput.keyUp(more.key ?? tilde, false, more.text ?? "`", more.modifiers ?? 0, more.scanCode ?? 0)
    }

    property Timer repeater: Timer {
        interval: 60
        repeat: true
        onTriggered: {
            const more = t.kept.holding
            testInput.keyUp(more.key ?? t.tilde, true, more.text ?? "`", more.modifiers ?? 0, more.scanCode ?? 0)
            testInput.keyDown(more.key ?? t.tilde, true, more.text ?? "`", more.modifiers ?? 0, more.scanCode ?? 0)
            ++t.kept.repeats
        }
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                goLive(2)
                return 500
            },
            () => {
                kept.normal = geometry()
                kept.columns = grid.columns
                kept.panes = [sidebar.width, sidePanel.width, mediaBin.height, toolbar.height].join(",")
                check("the window as it usually is: the panes round the slides, and no floating button", !simpleView && !simple && chrome === 1 && chromeShown && sidebar.visible && toolbar.visible
                      && sidePanel.visible && mediaBin.visible && !simpleToggle.visible && grid.x > 100 && grid.y === 48, kept.normal)
                const button = named("simpleViewButton")
                check("the toolbar has a button for Simple View, before the one for the media bin", button !== null && button.label === "Simple View" && button.kind === "expand" && !button.on
                      && button.mapToItem(null, 0, 0).x < Lib.find(toolbar, i => i.label === "Media" && i.kind !== undefined).mapToItem(null, 0, 0).x)
                // In, by the toolbar's button
                kept.before = Log.frames(win)
                kept.started = testInput.now()
                frames = {}
                watch.start()
                button.clicked()
                check("clicked, the view is on at once", simpleView && simple && simpleToggle.visible && simpleToggle.announcing)
                check("and the slides stay as they are while the panes go", geometry() === kept.normal && chromeShown && sidebar.visible, geometry())
                return 90
            },
            () => {
                check("the panes are on their way off the edges of the window", chrome > 0 && chrome < 1 && chromeShown && geometry() === kept.normal && sidebar.visible && toolbar.visible, chrome.toFixed(2))
                return 600
            },
            () => {
                check("and then gone: the toolbar, the lists, the previews and the media bin", chrome === 0 && !chromeShown && !sidebar.visible && !toolbar.visible && !sidePanel.visible && !mediaBin.visible)
                check("the slides have the whole window", grid.x === 0 && grid.y === 0 && grid.width === win.width && grid.height === win.height, geometry() + " in " + win.width + "x" + win.height)
                check("and so there are more of them to a row", grid.columns > kept.columns, kept.columns + " -> " + grid.columns)
                check("the panes keep their sizes for when they come back", [sidebar.width, sidePanel.width, toolbar.height].join(",") === kept.panes.split(",").filter((x, i) => i !== 2).join(","))
                const drawn = frames.atGone - kept.before
                testInput.say("    (the panes took " + (frames.gone - kept.started) + " ms to go, in " + drawn + " frames; the slides had filled the window " + (frames.filled - frames.gone) + " ms after that)")
                check("the going was drawn, not jumped", drawn >= 6 && frames.gone - kept.started < 400, drawn + " frames in " + (frames.gone - kept.started) + " ms")
                check("and the slides filled the window soon after", frames.filled - frames.gone < 400, (frames.filled - frames.gone) + " ms")
                const said = named("gridTitle")
                check("the line over the slides says which presentation this is", said !== null && said.text === "Move Of God" && said.visible, said ? said.text : "none")
                check("the slide that is live is still the live one", liveIndex === 2 && viewingLive)
                // The button over the slides
                const place = simpleToggle.mapToItem(null, simpleToggle.width - 15, 13)
                check("a button floats over the slides, where the toolbar's button was", Math.abs(place.x - toolbar.simpleViewCentre) < 1 && place.y < 40, place.x + " against " + toolbar.simpleViewCentre)
                const words = Lib.find(simpleToggle, i => i.text !== undefined && i.text.includes("Simple View"))
                check("making itself known: large, bright, and saying how to go back", simpleToggle.announcing && simpleToggle.width > 300 && simpleToggle.height >= 30 && words !== null && words.opacity === 1
                      && /click here, or hold the ~ key, to go back/.test(words.text) && Lib.find(simpleToggle, i => i.radius !== undefined && i.clip === true).opacity === 1, simpleToggle.width + "x" + simpleToggle.height)
                testInput.grab("1-announced")
                // The show goes on as ever
                testInput.key(Qt.Key_Right)
                check("the arrow keys still run the show", liveIndex === 3)
                click(centre(slideCell(7)))
                check("and so does a click on a slide", liveIndex === 7)
                testInput.key(Qt.Key_F2)
                check("and the clears", cleared === true || liveSlide === null)
                return 4500
            },
            () => {
                const pill = Lib.find(simpleToggle, i => i.radius !== undefined && i.clip === true)
                check("after five seconds it has shrunk to a small, faint button", !simpleToggle.announcing && simpleToggle.width === 30 && simpleToggle.height === 26 && Math.abs(pill.opacity - 0.5) < 0.01,
                      simpleToggle.width + "x" + simpleToggle.height + " at " + pill.opacity)
                const place = simpleToggle.mapToItem(null, simpleToggle.width - 15, 13)
                check("in the same place", Math.abs(place.x - toolbar.simpleViewCentre) < 1)
                testInput.grab("2-at-rest")
                testInput.mouse(4, place.x, place.y)
                return 500
            },
            () => {
                const pill = Lib.find(simpleToggle, i => i.radius !== undefined && i.clip === true)
                const tip = Lib.find(simpleToggle, i => i.text !== undefined && i.text.startsWith("Back to the normal view"))
                check("under the pointer it comes up in full and says what it does", simpleToggle.hovered && pill.opacity === 1 && simpleToggle.width > 150 && tip !== null && tip.opacity === 1, simpleToggle.width + " at " + pill.opacity)
                testInput.mouse(4, 500, 500)
                return 500
            },
            () => {
                const pill = Lib.find(simpleToggle, i => i.radius !== undefined && i.clip === true)
                check("and goes faint and small again when the pointer leaves", !simpleToggle.hovered && simpleToggle.width === 30 && Math.abs(pill.opacity - 0.5) < 0.01)
                // Out, by the floating button
                const place = simpleToggle.mapToItem(null, simpleToggle.width - 15, 13)
                click(place)
                check("clicked, the view is off, and the slides are back between where the panes will be at once", !simpleView && !simple && chromeShown && geometry() === kept.normal && !simpleToggle.visible, geometry())
                check("with the panes still on their way in", chrome < 1 && sidebar.visible)
                return 500
            },
            () => {
                check("and then everything is as it was", chrome === 1 && sidebar.visible && toolbar.visible && sidePanel.visible && mediaBin.visible && geometry() === kept.normal
                      && [sidebar.width, sidePanel.width, mediaBin.height, toolbar.height].join(",") === kept.panes, geometry())
                goLive(2)
                // The key: a press is not enough
                hold()
                return 300
            },
            () => {
                check("the ~ key pressed for a third of a second has switched nothing", !simpleView && kept.repeats >= 3, kept.repeats + " repeats")
                check("though the toolbar's button shows it being held", holdProgress > 0.2 && holdProgress < 0.7 && named("simpleViewButton").progress === holdProgress, holdProgress.toFixed(2))
                testInput.grab("3-key-held")
                letGo()
                check("let go, the showing stops", holdProgress === 0)
                return 700
            },
            () => {
                check("and nothing happens afterwards", !simpleView && holdProgress === 0)
                hold()
                return 120
            },
            () => {
                check("a key only brushed shows nothing at all", holdProgress === 0 && !simpleView)
                letGo()
                hold()
                return 800
            },
            () => {
                check("held for more than seven tenths of a second, it switches the view, without being let go", simpleView && simple && simpleToggle.announcing && holdProgress === 0, kept.repeats + " repeats")
                return 1500
            },
            () => {
                check("and held on for as long again and more, it does not switch it back", simpleView && kept.repeats > 25, kept.repeats + " repeats")
                letGo()
                return 300
            },
            () => {
                check("nor does letting go", simpleView && chrome === 0)
                // Out by the key, with Shift held (which is what makes it a ~)
                hold(0, { key: Qt.Key_AsciiTilde, text: "~", modifiers: Qt.ShiftModifier })
                return 400
            },
            () => {
                check("held again, the button over the slides shows the hold", holdProgress > 0.3 && holdProgress < 0.8 && simpleToggle.progress === holdProgress && simpleView, holdProgress.toFixed(2))
                testInput.grab("4-key-held-in-simple-view")
                return 450
            },
            () => {
                check("and the view goes off", !simpleView)
                letGo()
                return 400
            },
            () => {
                // The key by its place on the keyboard, whatever it types there
                hold(0, { key: Qt.Key_Dead_Circumflex, text: "", scanCode: 49 })
                return 800
            },
            () => {
                check("the key is known by its place on the keyboard too, whatever it types", simpleView)
                letGo()
                setSimpleView(false, "the test")
                return 400
            },
            () => {
                hold(0, { modifiers: Qt.ControlModifier })
                return 800
            },
            () => {
                check("with Ctrl held it is some other key combination, and switches nothing", !simpleView && holdProgress === 0)
                letGo()
                testInput.key(Qt.Key_1, 0, "1")
                hold(0, { key: Qt.Key_Q, text: "q" })
                return 800
            },
            () => {
                check("and no other key does it", !simpleView)
                letGo()
                // The keyboard taken away during a hold ends the hold
                hold()
                return 300
            },
            () => {
                kept.progress = holdProgress
                sidebar.renamePlaylist(catalog.playlists.find(p => !p.folder).path)
                return 150
            },
            () => {
                check("the keyboard going elsewhere during a hold ends it", kept.progress > 0 && holdProgress === 0 && !keys.activeFocus, kept.progress.toFixed(2))
                return 700
            },
            () => {
                check("and the view is not switched", !simpleView)
                letGo()
                // Typed into a box, the key is a character and nothing more
                hold()
                return 800
            },
            () => {
                const field = Lib.find(sidebar, i => i.selectAll !== undefined && i.activeFocus)
                check("held while a name is being typed, it is typed and switches nothing", !simpleView && field !== null, field ? field.text : "no field")
                letGo()
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                takeFocus()
                // The editor and the settings need the toolbar
                setSimpleView(true, "the test")
                return 500
            },
            () => {
                check("in Simple View again", simple && chrome === 0)
                startEditing(currentEntry())
                check("the editor coming up brings the toolbar back with it", editing && simpleView && !simple && chromeShown && toolbar.visible && !simpleToggle.visible)
                return 500
            },
            () => {
                const button = named("simpleViewButton")
                check("where the button for the view is lit, and dim: it waits for the editor", chrome === 1 && button.on && button.opacity < 0.5)
                button.clicked()
                check("and does nothing while the editor is up", simpleView)
                stopEditing()
                check("done with, the view is back", !editing && simple && simpleToggle.visible && simpleToggle.announcing)
                return 500
            },
            () => {
                check("with the slides filling the window again", chrome === 0 && grid.width === win.width && grid.y === 0)
                settingsOpen = true
                check("the settings screen brings the toolbar back in the same way", !simple && chromeShown && toolbar.visible)
                hold()
                return 800
            },
            () => {
                check("and the key does nothing while it is up", simpleView && settingsOpen && holdProgress === 0)
                letGo()
                settingsOpen = false
                takeFocus()
                return 500
            },
            () => {
                check("closed, the view is back", simple && chrome === 0)
                // A presentation picked with the keys is named
                testInput.key(Qt.Key_Down)
                return 500
            },
            () => {
                const said = named("gridTitle")
                check("going to the next presentation with the arrow key, the line over the slides names it", said.text === document.name && document.name !== "Move Of God", said.text)
                testInput.grab("5-next-presentation")
                // Dropping files still works there
                const cell = slideCell(0)
                const p = cell.mapToItem(null, cell.width / 2, cell.height / 2)
                check("files can still be dragged onto a slide", testInput.fileDrag(0, p.x, p.y, ["@MADE@/j-picture.png"]) === "copy")
                testInput.fileDrag(2, 0, 0)
                setSimpleView(false, "the test")
                return 500
            },
            () => {
                check("the log has a line for each switch, saying what made it", logged(/  view  /).length === 8 && logged(/Simple View on, by its button in the toolbar/).length === 1
                      && logged(/Simple View off, by the button over the slides/).length === 1 && logged(/Simple View on, by the ~ key, held/).length === 2
                      && logged(/Simple View off, by the ~ key, held/).length === 1, logged(/  view  /).length + ": " + logged(/  view  /).map(l => l.slice(26, 90)).join(" | "))
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0, logged(/PROBLEM|WARNING|ERROR/).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
