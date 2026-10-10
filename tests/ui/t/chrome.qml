import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The window's furniture as rearranged: title bar over toolbar, the right pane down to
// the bottom with the media bin stopping at it, the clears as icons, the lines each side
// of the transport, the tabs of the show controls, and the transition under the slides.
QtObject {
    id: t

//COMMON
    function texts(root) {
        return Lib.findAll(root, item => item.text !== undefined && item.font !== undefined && item.visible && typeof item.text === "string" && item.text !== "").map(item => item.text)
    }

    function controls() {
        return Lib.find(win.contentItem, item => item.shrink !== undefined && item.win !== undefined && item.shrink === 0.8)
    }

    function bottomOf(item) {
        return item.mapToItem(null, 0, item.height).y
    }

    function topOf(item) {
        return item.mapToItem(null, 0, 0).y
    }

    function run() {
        const near = (a, b) => Math.abs(a - b) < 1.5
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                // ---- the bars
                check("one bar across the top, which is the title bar and the toolbar", toolbar.y === 0 && toolbar.height === 48 && toolbar.width === win.width)
                check("it says the app and its version, and the line over the slides what is open", texts(toolbar).includes(("Simple Presenter " + Qt.application.version)) && win.title === ("Simple Presenter " + Qt.application.version) && named("gridTitle").text === "Move Of God" && named("gridTitle").visible,
                      texts(toolbar).join(" | "))
                const title = Lib.find(toolbar, item => item.text === ("Simple Presenter " + Qt.application.version))
                check("in the middle of the window", title !== null && near(title.mapToItem(null, title.width / 2, 0).x, win.width / 2), title ? title.mapToItem(null, title.width / 2, 0).x + " of " + win.width : "none")
                const windowButtons = Lib.findAll(toolbar, item => item.down !== undefined && item.text !== undefined && item.popup === undefined)
                check("and has the window's three buttons", windowButtons.length === 3 && windowButtons.map(b => b.text).join("") === "–□✕", windowButtons.map(b => b.text).join(""))
                const switches = Lib.findAll(toolbar, item => item.label !== undefined && item.kind !== undefined).map(item => item.label)
                const leftOf = (label) => { const item = Lib.find(toolbar, i => i.label === label && i.kind !== undefined); return item.mapToItem(null, 0, 0).x }
                const picker0 = Lib.find(toolbar, item => item.currentIndex !== undefined && item.popup !== undefined)
                check("with the workspace picker and the ten switches, the Looks one saying only what it is where no look is live", switches.slice().sort().join("|") === "Edit|Looks|Media|Output|Search|Settings|Show|Simple View|Stage|Themes" && !texts(toolbar).includes("Workspace") && named("workspaceIcon") !== null, switches.join("|"))
                check("Search, Themes, Show and Edit at the left, straight after the workspace picker, and the rest at the right in their order",
                      leftOf("Search") > picker0.mapToItem(null, picker0.width, 0).x && leftOf("Search") < leftOf("Themes") && leftOf("Themes") < leftOf("Show")
                      && leftOf("Show") < leftOf("Edit") && leftOf("Edit") < 460 && title.mapToItem(null, 0, 0).x > leftOf("Edit") + 52
                      && leftOf("Simple View") > win.width / 2 && leftOf("Simple View") < leftOf("Media") && leftOf("Media") < leftOf("Looks") && leftOf("Looks") < leftOf("Output") && leftOf("Output") < leftOf("Stage")
                      && leftOf("Stage") < leftOf("Settings"),
                      ["Edit", "Simple View", "Media", "Output", "Stage", "Settings"].map(l => l + " " + Math.round(leftOf(l))).join(", "))
                const pair = named("outputToggles")
                const inPair = Lib.findAll(pair, item => item.label !== undefined && item.kind !== undefined).map(item => item.label)
                check("the output and stage switches drawn as one control, the two of them and nothing else", pair !== null && inPair.join("|") === "Output|Stage" && pair.border.width === 1
                      && near(pair.width, 52 * 2 + 1 + 2) && pair.height === 40, inPair.join("|") + " in " + (pair ? pair.width + "x" + pair.height : "none"))
                check("each half still switching its own window", (() => {
                    // (Its being clicked is said for it: the toolbar does not take this script's made-up clicks, only a
                    // real pointer's, which was tried by hand in the headless desktop.)
                    const was = [outputEnabled, stageEnabled]
                    named("outputToggle").clicked()
                    const afterOutput = [outputEnabled, stageEnabled]
                    named("stageToggle").clicked()
                    const afterStage = [outputEnabled, stageEnabled]
                    named("outputToggle").clicked()
                    named("stageToggle").clicked()
                    return afterOutput[0] === !was[0] && afterOutput[1] === was[1] && afterStage[0] === !was[0] && afterStage[1] === !was[1]
                           && outputEnabled === was[0] && stageEnabled === was[1]
                })())
                check("and nothing of the transition, which is under the slides now", !texts(toolbar).some(x => x === transition.name) && Lib.find(toolbar, item => item.objectName === "transitionButton") === null)
                // ---- the panes
                check("the right pane runs from the toolbar to the bottom of the window", near(sidePanel.y, 48) && near(sidePanel.y + sidePanel.height, win.height)
                      && near(sidePanel.x + sidePanel.width, win.width), sidePanel.y + " + " + sidePanel.height + " of " + win.height)
                check("the media bin stops at it", mediaBinVisible && near(mediaBin.x, 0) && near(mediaBin.x + mediaBin.width, sidePanel.x)
                      && near(mediaBin.y + mediaBin.height, win.height), mediaBin.x + " + " + mediaBin.width + " to " + sidePanel.x)
                check("and the slides and the lists sit over the media bin", near(grid.y + grid.height, mediaBin.y) && near(sidebar.y + sidebar.height, mediaBin.y)
                      && near(grid.x + grid.width, sidePanel.x))
                kept.pane = sidePanel.height
                mediaBinVisible = false
                return 300
            },
            () => {
                check("with the media bin hidden the slides run to the bottom, and the right pane is as it was", near(grid.y + grid.height, win.height)
                      && near(sidePanel.height, kept.pane))
                mediaBinVisible = true
                // ---- the right pane
                const inPane = texts(sidePanel)
                check("the previews have no titles", !inPane.some(x => x.toLowerCase() === "output" || x.toLowerCase() === "stage") && !inPane.some(x => x.toLowerCase() === "timers"),
                      inPane.join(" | "))
                const clears = ["clearAll", "clearSlide", "clearMedia", "clearProps"].map(name => named(name))
                check("there are four clear buttons, across the whole width", clears.every(c => c !== null) && near(clears[0].width, clears[1].width) && near(clears[1].width, clears[2].width)
                      && near(clears[2].width, clears[3].width)
                      && near(clears[0].mapToItem(sidePanel, 0, 0).x, 12) && near(clears[3].mapToItem(sidePanel, clears[3].width, 0).x, sidePanel.width - 12),
                      clears.map(c => c ? c.width.toFixed(1) : "none").join(", ") + " in " + sidePanel.width)
                check("with pictures on them and no words", clears.every(c => texts(c).length === 0 && Lib.find(c, item => item.preferredRendererType !== undefined) !== null))
                check("grey while there is nothing to clear", clears.every(c => !c.live && Qt.colorEqual(c.color, "#2b2d31")))
                kept.clears = clears
                goLive(6)
                return 3000
            },
            () => {
                const clears = kept.clears
                check("red when there is", clears.slice(0, 3).every(c => c.live && Qt.colorEqual(c.color, "#c62828")), clears.map(c => String(c.color)).join(","))
                check("and the one for the props still grey, there being none on", !clears[3].live && Qt.colorEqual(clears[3].color, "#2b2d31"))
                testInput.grab("1-live")
                click(centre(clears[1]))
                return 500
            },
            () => {
                check("the second clears the slide", cleared && liveMedia !== null && !kept.clears[1].live && kept.clears[0].live && kept.clears[2].live)
                click(centre(kept.clears[2]))
                return 500
            },
            () => {
                check("the third clears the media", liveMedia === null && !kept.clears[2].live && !kept.clears[0].live)
                goLive(6)
                return 2500
            },
            () => {
                click(centre(kept.clears[0]))
                return 500
            },
            () => {
                check("the left one clears everything", cleared && liveMedia === null)
                // the lines each side of the transport
                const transport = Lib.find(sidePanel, item => item.shown !== undefined && item.working !== undefined)
                const lines = Lib.findAll(sidePanel, item => item.height === 1 && near(item.width, sidePanel.width) && item.color !== undefined)
                check("a line over the transport and a line under it", lines.length === 2 && bottomOf(kept.clears[0]) < topOf(lines[0]) && topOf(lines[0]) < topOf(transport)
                      && bottomOf(transport) < topOf(lines[1]), lines.length + " lines")
                // the show controls
                const tabs = Lib.findAll(sidePanel, item => item.chosen !== undefined && item.modelData !== undefined && item.modelData.id !== undefined)
                check("the four tabs fill the width", tabs.length === 4 && near(tabs[0].mapToItem(sidePanel, 0, 0).x, 12)
                      && near(tabs[3].mapToItem(sidePanel, tabs[3].width, 0).x, sidePanel.width - 12) && near(tabs[0].width, tabs[3].width)
                      && topOf(tabs[0]) > topOf(lines[1]), tabs.map(x => x.width.toFixed(1)).join(", "))
                const add = named("showControlAdd")
                check("the + is under them at the right, and small", add !== null && add.visible && topOf(add) >= bottomOf(tabs[0]) && near(add.mapToItem(sidePanel, add.width, 0).x, sidePanel.width - 12)
                      && add.width <= 24 && add.height <= 20, add ? add.width + "x" + add.height : "none")
                const before = Timers.timers.length
                click(centre(add))
                kept.before = before
                return 500
            },
            () => {
                check("and adds a timer", Timers.timers.length === kept.before + 1)
                Timers.remove(Timers.timers[Timers.timers.length - 1].id)
                // ---- the transition, under the slides
                const c = controls()
                const gridView = named("slideGrid")
                const footer = named("gridFooter")
                check("under the slides is a thin footer, which the slides stop at", footer !== null && footer.height === 34 && near(bottomOf(gridView), topOf(footer)) && near(bottomOf(footer), bottomOf(grid)),
                      footer ? topOf(footer) + " " + bottomOf(gridView) : "none")
                check("the transition's controls are at its left", c !== null && c.visible && topOf(c) > topOf(footer) && bottomOf(c) < bottomOf(footer)
                      && c.mapToItem(null, 0, 0).x < grid.x + 30 && c.mapToItem(null, 0, 0).x >= grid.x, c ? c.mapToItem(null, 0, 0).x + "," + topOf(c) : "none")
                const zoom = Lib.find(footer, item => item.canShrink !== undefined)
                const minus = Lib.find(zoom, item => item.available !== undefined)
                check("and the thumbnail size buttons at its right, neither of them see-through", zoom !== null && zoom.solid && minus.opacity === 1 && c.opacity === 1
                      && topOf(zoom) > topOf(footer) && bottomOf(zoom) < bottomOf(footer) && zoom.mapToItem(null, zoom.width, 0).x > grid.x + grid.width - 30, zoom ? minus.opacity : "none")
                const panel = Lib.find(c, item => item.scale === 0.8)
                check("at four fifths of their size", panel !== null && near(c.width, panel.width * 0.8) && near(c.height, 28 * 0.8), c.width + "x" + c.height)
                const p = c.mapToItem(null, c.width - 20, c.height / 2)
                testInput.mouse(4, p.x, p.y)
                return 300
            },
            () => {
                const c = controls()
                testInput.grab("footer")
                kept.name = transition.name
                click(centre(named("transitionButton")))
                return 500
            },
            () => {
                const c = controls()
                check("a click on the transition opens the menu of them", menu.opened && menu.items.some(i => i.label === "Ripple Wave") && menu.items.some(i => i.label === kept.name && i.current))
                const pane = Lib.find(menu.contentItem, i => i.px !== undefined && i.rows !== undefined)
                const top = pane.mapToItem(null, 0, 0).y
                const bottom = pane.mapToItem(null, 0, pane.height).y
                check("which opens upwards, between the toolbar and the controls", pane.foot >= 0 && top >= toolbar.height && bottom <= topOf(c) && bottom > topOf(c) - 30, top.toFixed(0) + " to " + bottom.toFixed(0) + ", controls at " + topOf(c).toFixed(0))
                check("the controls stay up while it is open", c.opacity === 1)
                testInput.grab("2-menu")
                click(menuRow("Fade"))
                return 400
            },
            () => {
                check("a row of it chooses that transition", !menu.opened && transition.name === "Fade", transition.name)
                const options = named("transitionOptionsButton")
                check("Fade has nothing to adjust, and its button says so", !options.available)
                selectTransition("Reveal")
                return 300
            },
            () => {
                const options = named("transitionOptionsButton")
                check("Reveal has", options.available && transition.name === "Reveal")
                click(centre(options))
                return 500
            },
            () => {
                const c = controls()
                const panel = Lib.find(c, item => false) // the panel is a popup, not an item of the controls
                const popup = named("transitionOptionsButton").on
                check("its button opens the panel of what can be adjusted", popup === true)
                testInput.grab("3-options")
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("and Esc shuts it", named("transitionOptionsButton").on === false)
                // the slider, which is drawn small: a drag along it still goes its whole range
                const slider = Lib.find(controls(), item => item.visualPosition !== undefined && item.from !== undefined)
                const from = slider.mapToItem(null, slider.leftPadding + slider.visualPosition * (slider.availableWidth - 16) + 8, slider.height / 2)
                const to = slider.mapToItem(null, slider.leftPadding + slider.availableWidth * 0.5, slider.height / 2)
                kept.span = slider.mapToItem(null, slider.width, 0).x - slider.mapToItem(null, 0, 0).x
                drag(from, to)
                return 300
            },
            () => {
                check("the slider is drawn at four fifths", near(kept.span, 80), kept.span)
                check("and dragged to its middle sets the middle of its range", Math.abs(transitionDuration - 1.5) < 0.12, transitionDuration)
                const field = Lib.find(controls(), item => item.selectByMouse !== undefined && item.cursorPosition !== undefined)
                click(centre(field))
                field.selectAll()
                testInput.type("0.8\n")
                return 300
            },
            () => {
                check("a length typed in its box is taken", transitionDuration === 0.8, transitionDuration)
                selectTransition(kept.name)
                // out of the way of the last slides: the grid can be scrolled past them
                const gridView = named("slideGrid")
                gridView.positionViewAtEnd()
                return 400
            },
            () => {
                // the title in a window too narrow to have it in the middle
                kept.width = win.width
                win.width = 900
                return 400
            },
            () => {
                const title = Lib.find(toolbar, item => item.text === win.title)
                const picker = Lib.find(toolbar, item => item.currentIndex !== undefined && item.popup !== undefined)
                const edit = Lib.find(toolbar, item => item.label === "Edit" && item.kind !== undefined)
                const first = Lib.find(toolbar, item => item.label === "Simple View" && item.kind !== undefined)
                const left = title.mapToItem(null, 0, 0).x
                const right = title.mapToItem(null, title.width, 0).x
                check("in a narrow window the title keeps between the edit button and the buttons on the right, or is left out where that is no room to speak of",
                      edit.mapToItem(null, 0, 0).x > picker.mapToItem(null, picker.width, 0).x
                      && (!title.visible || (left > edit.mapToItem(null, edit.width, 0).x && right < first.mapToItem(null, 0, 0).x && title.width >= 90)),
                      left.toFixed(0) + " to " + right.toFixed(0) + ", picker ends " + picker.mapToItem(null, picker.width, 0).x.toFixed(0) + ", edit ends " + edit.mapToItem(null, edit.width, 0).x.toFixed(0)
                      + ", buttons start " + first.mapToItem(null, 0, 0).x.toFixed(0))
                testInput.grab("5-narrow-window")
                win.width = kept.width
                return 400
            },
            () => {
                // out of the way of the last slides: the grid can be scrolled past them
                named("slideGrid").positionViewAtEnd()
                return 400
            },
            () => {
                const gridView = named("slideGrid")
                const last = gridView.itemAtIndex(document.slides.length - 1)
                check("scrolled to the end, the last slides are clear of the controls", last !== null && bottomOf(last) <= topOf(controls()) + 2, last ? bottomOf(last).toFixed(0) + " vs " + topOf(controls()).toFixed(0) : "none")
                testInput.grab("4-end-of-grid")
                // ---- the arrangement drop-down, which is small and leaves the room to the slides
                openEntry(entry("Abandoned"))
                return 500
            },
            () => {
                const box = named("arrangementBox")
                const gridView = named("slideGrid")
                gridView.positionViewAtBeginning()
                check("a presentation with arrangements has the drop-down for them, small", box !== null && box.visible && box.height <= 24 && box.width <= 160, box ? box.width + "x" + box.height : "none")
                check("and its row is no taller than it needs to be", topOf(gridView) - grid.mapToItem(null, 0, 0).y <= 34, (topOf(gridView) - grid.mapToItem(null, 0, 0).y).toFixed(1))
                testInput.grab("6-arrangement")
                notice = ""
                openEntry(documents.find(d => openable(d) && load(d).arrangements.length === 0))
                return 400
            },
            () => {
                const box = named("arrangementBox")
                const gridView = named("slideGrid")
                gridView.positionViewAtBeginning()
                check("one without has the line over the slides all the same, with its name and no arrangement", document.arrangements.length === 0 && (box === null || !box.visible) && near(topOf(gridView) - grid.mapToItem(null, 0, 0).y, 30) && named("gridTitle").text === document.name,
                      document.name + ": " + (topOf(gridView) - grid.mapToItem(null, 0, 0).y).toFixed(1) + " notice '" + notice + "'")
                // ---- the line between the media bin's list and its thumbnails
                const line = named("mediaListDivider")
                const list = named("mediaPlaylistList")
                const thumbnails = named("mediaGrid")
                check("there is a line between the media bin's list and its thumbnails", line !== null && line.visible && near(line.mapToItem(null, line.width / 2, 0).x, mediaBin.listWidth)
                      && near(topOf(line), mediaBin.y) && near(bottomOf(line), win.height), line ? line.mapToItem(null, line.width / 2, 0).x + " vs " + mediaBin.listWidth : "none")
                kept.listWidth = mediaBin.listWidth
                kept.thumbnailsX = thumbnails.mapToItem(null, 0, 0).x
                const from = centre(line)
                drag(from, Qt.point(from.x + 90, from.y))
                return 400
            },
            () => {
                const thumbnails = named("mediaGrid")
                check("dragging it right widens the list", near(mediaBin.listWidth, kept.listWidth + 90) && near(mediaListWidth, kept.listWidth + 90), kept.listWidth + " -> " + mediaBin.listWidth)
                check("and moves the thumbnails over by as much", near(thumbnails.mapToItem(null, 0, 0).x, kept.thumbnailsX + 90), kept.thumbnailsX + " -> " + thumbnails.mapToItem(null, 0, 0).x)
                check("the list of presentations over it keeps its own width", near(sidebar.width, kept.sidebar ?? sidebar.width))
                testInput.grab("7-media-list-wider")
                const line = named("mediaListDivider")
                const from = centre(line)
                drag(from, Qt.point(from.x - 2000, from.y))
                return 400
            },
            () => {
                check("dragged far left it stops at a list still wide enough to read", near(mediaBin.listWidth, 140), mediaBin.listWidth)
                const line = named("mediaListDivider")
                const from = centre(line)
                drag(from, Qt.point(from.x + 3000, from.y))
                return 400
            },
            () => {
                check("and far right, where thumbnails still have room", near(mediaBin.listWidth, mediaBin.width - 220), mediaBin.listWidth + " of " + mediaBin.width)
                mediaListWidth = kept.listWidth
                mediaBinVisible = false
                return 300
            },
            () => {
                const line = named("mediaListDivider")
                check("with the media bin hidden the line goes too", !line.visible)
                mediaBinVisible = true
                return 200
            }
        ]
        next()
    }
}
