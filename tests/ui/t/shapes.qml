import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// Shapes, gradient and picture fills and feathered edges, as ProPresenter wrote them in its own example and as the
// editor makes them; deleting a slide; and the hotkeys of groups.
QtObject {
    id: t

//COMMON
    readonly property string made: "@MADE@/"
    readonly property var canvas: editScreen.canvas
    readonly property var editor: editScreen.editor

    function rgb(x, y) {
        const c = testInput.pixel(false, x / 1920, y / 1080)
        return [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)]
    }

    function lit(x, y) {
        const c = rgb(x, y)
        return c[0] + c[1] + c[2]
    }

    function picked() {
        return canvas.selected
    }

    function byShape(shape) {
        return canvas.elements.filter(e => e.shape === shape)
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("The Joy"))
                const e = document.slides[1].elements
                kept.ellipse = e.find(x => x.shape === "ellipse")
                kept.round = e.find(x => x.shape === "roundedRectangle")
                kept.media = e.find(x => x.shape === "rectangle" && x.fillKind === "media")
                check("ProPresenter's example slide is read with its shapes", e.length === 5 && kept.ellipse !== undefined && kept.round !== undefined && kept.media !== undefined,
                      e.map(x => x.shape + "/" + x.fillKind).join(", "))
                check("the rounded rectangle: how round, and its gradient's two colours and angle", Math.abs(kept.round.roundness - 0.2) < 1e-6 && kept.round.fillKind === "gradient" && kept.round.fillShown
                      && kept.round.fillGradientAngle === 315 && String(kept.round.fillGradientFrom) === "#ffe061" && String(kept.round.fillGradientTo) === "#02c4fa",
                      [kept.round.roundness, kept.round.fillGradientAngle, kept.round.fillGradientFrom, kept.round.fillGradientTo].join(" "))
                check("the ellipse: its outline, its picture, found by name in the workspace, and its feathered edge", kept.ellipse.outline.length === 4 && kept.ellipse.fillKind === "media"
                      && kept.ellipse.fillMediaName === "legobatmancover.0.jpg" && kept.ellipse.fillMediaPath.endsWith("/Media/legobatmancover.0.jpg") && kept.ellipse.fillShown
                      && kept.ellipse.featherOn && Math.abs(kept.ellipse.featherRadius - 0.0712) < 0.001 && kept.ellipse.fillMediaScale === 0, kept.ellipse.fillMediaPath)
                check("elements with no name go by their words", kept.ellipse.name === "" && kept.ellipse.words === "Elipse with media fill and feathering", kept.ellipse.words)
                output.setFullScreen(true)
                goLive(1)
                return 2500
            },
            () => {
                const r = kept.round
                check("on the output the rounded rectangle's corner is cut away", lit(r.x + 10, r.y + 10) < 30 && lit(r.x + r.width - 10, r.y + r.height - 10) < 30, lit(r.x + 10, r.y + 10))
                const from = rgb(r.x + 70, r.y + 70)
                const to = rgb(r.x + r.width - 70, r.y + r.height - 70)
                check("and its gradient runs from yellow at the top left to blue at the bottom right", from[0] > 200 && from[2] < 140 && to[0] < 90 && to[2] > 200, from + " -> " + to)
                const e = kept.ellipse
                check("the ellipse's picture is cut to the ellipse", lit(e.x + 12, e.y + 10) < 30 && lit(e.x + e.width / 2, e.y + e.height / 2) > 200, lit(e.x + 12, e.y + 10) + " and " + lit(e.x + e.width / 2, e.y + e.height / 2))
                const edge = lit(e.x + e.width / 2, e.y + 3)
                const inner = lit(e.x + e.width / 2, e.y + 40)
                check("and fades out at its edge", edge < inner * 0.6 && inner > 150, edge + " at the edge, " + inner + " further in")
                const m = kept.media
                check("the picture-filled rectangle shows its picture", lit(m.x + m.width / 2, m.y + m.height / 3) > 150)
                testInput.grabOutput("1-example")
                output.setFullScreen(false)
                // ---- the editor
                startEditing(currentEntry(), document.slides[1].id)
                return 600
            },
            () => {
                kept.count = canvas.elements.length
                const tools = ["addTextTool", "addShapeTool", "addMediaTool"].map(n => named(n))
                check("the editor's toolbar has a T, a shapes list and a picture, as glyphs", tools.every(x => x !== null) && tools.map(x => x.kind).join() === "text,shapes,media")
                const p = centre(tools[2])
                testInput.mouse(4, p.x, p.y)
                return 200
            },
            () => {
                check("and says in words what the one under the pointer does", editScreen.toolHint === "Add a picture or a video from a file", editScreen.toolHint)
                testInput.mouse(4, 700, 500)
                click(centre(named("addShapeTool")))
                return 300
            },
            () => {
                check("the shapes list", menu.opened && labels() === "[Shapes], Rectangle, Rounded Rectangle, Ellipse, Arrow", labels())
                const tool = named("addShapeTool")
                const corner = Lib.find(menu.contentItem, i => i.px !== undefined && i.rows !== undefined).mapToItem(null, 0, 0)
                const under = tool.mapToItem(null, 0, tool.height)
                check("which opens under its button", Math.abs(corner.x - under.x) < 12 && corner.y >= under.y && corner.y < under.y + 16, corner.x + "," + corner.y + " for " + under.x + "," + under.y)
                testInput.grab("5-shapes-menu")
                click(menuRow("Rounded Rectangle"))
                return 400
            },
            () => {
                const e = picked()
                kept.roundId = e ? e.id : ""
                check("a rounded rectangle is added, picked, filled with a plain colour", canvas.elements.length === kept.count + 1 && e !== null && e.shape === "roundedRectangle"
                      && e.name === "Rounded Rectangle" && e.fillKind === "color" && e.fillShown && e.roundness === 0.2 && e.outline.length === 8 && e.words === "", e ? e.shape + " " + e.name : "none")
                check("its outline round in its own proportions", Math.abs(e.outline[0][0] - 0.2 * Math.min(e.width, e.height) / e.width) < 1e-6 && Math.abs(e.outline[2][1] - 0.2 * Math.min(e.width, e.height) / e.height) < 1e-6)
                canvas.setProperties({ roundness: 0.4 }, false)
                canvas.setProperties({ width: 600, height: 300 }, false)
                const now = picked()
                check("made rounder, then resized: the outline follows both", now.roundness === 0.4 && Math.abs(now.outline[0][0] - 0.4 * 300 / 600) < 1e-6 && Math.abs(now.outline[2][1] - 0.4) < 1e-6, now.outline[0][0])
                const handle = Lib.find(canvas, i => i.objectName === "cornerHandle")
                check("it has a handle of its own on its top edge, as far in as the corners are round", handle !== null && handle.visible
                      && Math.abs(handle.x + handle.width / 2 - 0.4 * 300 * canvas.u) < 1, handle ? handle.x : "none")
                const from = handle.mapToItem(null, handle.width / 2, handle.height / 2)
                drag(from, Qt.point(from.x - 0.2 * 300 * canvas.u, from.y))
                return 300
            },
            () => {
                check("dragging that handle towards the corner makes the corners less round", Math.abs(picked().roundness - 0.2) < 0.02, picked().roundness)
                // gradient
                canvas.setProperties({ fillKind: "gradient", fillOn: true }, false)
                let e = picked()
                check("its fill made a gradient starts from the colour it had", e.fillKind === "gradient" && e.fillShown && String(e.fillGradientFrom) === "#2196f2" && e.fillGradientAngle === 270, e.fillGradientFrom + " " + e.fillGradientTo)
                canvas.setProperties({ fillGradientFrom: "#ff0000", fillGradientTo: "#0000ff", fillGradientAngle: 0 }, false)
                e = picked()
                check("and takes two colours and an angle", String(e.fillGradientFrom) === "#ff0000" && String(e.fillGradientTo) === "#0000ff" && e.fillGradientAngle === 0)
                kept.gradient = { x: e.x, y: e.y, width: e.width, height: e.height }
                return 500
            },
            () => {
                testInput.grab("3-gradient-picked")
                canvas.selectedId = ""
                return 400
            },
            () => {
                // looked at in the editor's own picture of the slide
                const g = kept.gradient
                const at = (x, y) => { const p = canvas.mapToItem(null, canvas.originX + x * canvas.u, canvas.originY + y * canvas.u); const c = String(testInput.windowPixel(p.x, p.y)); return [parseInt(c.slice(1, 3), 16), parseInt(c.slice(5, 7), 16)] }
                const left = at(g.x + 80, g.y + g.height / 2)
                const right = at(g.x + g.width - 80, g.y + g.height / 2)
                check("drawn in the editor it runs from red on the left to blue on the right", left[0] > 150 && left[1] < 100 && right[1] > 150 && right[0] < 100, left + " -> " + right)
                testInput.grab("2-editor")
                canvas.selectedId = kept.roundId
                canvas.setProperties({ fillKind: "color" }, false)
                check("made a plain colour again it keeps the gradient's first colour", picked().fillKind === "color" && String(picked().fillColor) === "#ff0000")
                canvas.setProperties({ featherOn: true }, false)
                check("feathering turned on starts at ProPresenter's own amount", picked().featherOn && Math.abs(picked().featherRadius - 0.05) < 1e-6)
                canvas.setProperties({ featherRadius: 0.2 }, false)
                check("and can be set", Math.abs(picked().featherRadius - 0.2) < 1e-6)
                // the other shapes
                canvas.addShape("rectangle")
                check("a rectangle", picked().shape === "rectangle" && picked().name === "Rectangle" && picked().fillShown && picked().outline.length === 0)
                canvas.addShape("ellipse")
                kept.ellipseId = picked().id
                check("an ellipse", picked().shape === "ellipse" && picked().outline.length === 4 && picked().fillKind === "color")
                canvas.addShape("arrow")
                check("an arrow", picked().shape === "arrow" && picked().outline.length === 7 && picked().name === "Arrow")
                canvas.addShape("arrow")
                check("a second of a kind gets a name of its own", picked().name === "Arrow 2", picked().name)
                // media
                canvas.addMedia("file://" + made + "k-bars.jpg")
                const m = picked()
                check("a picture added from a file is an element the picture's shape, no more than half the slide", m.fillKind === "media" && m.fillShown && m.fillMediaName === "k-bars.jpg" && m.fillMediaPath === made + "k-bars.jpg"
                      && m.width === 960 && m.height === 540 && m.name === "k-bars" && m.fillMediaScale === 0, m.width + "x" + m.height + " " + m.name)
                canvas.setProperties({ fillMediaScale: 1 }, false)
                check("its picture can be made to fill it", picked().fillMediaScale === 1)
                canvas.setProperties({ fillMediaScale: 2 }, false)
                check("or be stretched to it", picked().fillMediaScale === 2)
                kept.mediaId = picked().id
                canvas.addMedia("file://" + made + "notes.txt")
                check("a file that is not media is refused, in words", editScreen.notice !== "" && picked().name === "k-bars", editScreen.notice)
                canvas.selectedId = kept.ellipseId
                canvas.setProperties({ fillMediaPath: made + "j-picture.png", fillOn: true }, false)
                check("a shape can be filled with a picture instead", picked().shape === "ellipse" && picked().fillKind === "media" && picked().fillMediaName === "j-picture.png" && picked().fillShown)
                canvas.editText(kept.ellipseId)
                check("and any of them can have words typed into it", canvas.editing !== null && canvas.editing.id === kept.ellipseId)
                canvas.finishText()
                canvas.selectedId = kept.mediaId
                kept.made = canvas.elements.length
                return 500
            },
            () => {
                testInput.grab("4-media-picked")
                check("eight elements made in all", kept.made === kept.count + 6, kept.made - kept.count)
                canvas.undo()
                canvas.undo()
                check("what was done can be undone", picked() === null || canvas.elements.find(e => e.id === kept.ellipseId).fillKind === "color", canvas.elements.find(e => e.id === kept.ellipseId).fillKind)
                canvas.redo()
                canvas.redo()
                check("and done again", canvas.elements.find(e => e.id === kept.ellipseId).fillKind === "media")
                stopEditing()
                return 500
            },
            () => {
                const again = catalog.open(document.path).slides[1].elements
                check("it is all in the file", again.length === kept.made && again.filter(e => e.shape === "arrow").length === 2 && again.find(e => e.id === kept.ellipseId).fillMediaName === "j-picture.png"
                      && again.find(e => e.id === kept.roundId).featherOn, again.length)
                // ---- deleting a slide
                kept.ids = document.slides.map(s => s.id)
                kept.victim = document.slides[3].id
                goLive(3)
                showSlideMenu(3, slideCell(3), 20, 20)
                return 300
            },
            () => {
                check("a slide's menu offers to delete it", menu.opened && /\[Slide\], Copy, Paste(\(off\))?, Delete Slide…$/.test(labels()), labels())
                click(menuRow("Delete Slide…"))
                return 300
            },
            () => {
                check("and asks first", menu.opened && labels() === "(note), Delete, Cancel", labels())
                click(menuRow("Delete"))
                return 500
            },
            () => {
                check("deleted, it is gone from the presentation, and the others are as they were", !document.slides.some(s => s.id === kept.victim)
                      && document.slides.map(s => s.id).join() === kept.ids.filter(id => id !== kept.victim).join(), document.slides.length + " of " + kept.ids.length)
                check("having been the one on the output, it was taken off first", cleared === true)
                check("and it is gone from the file", !catalog.open(document.path).slides.some(s => s.id === kept.victim))
                startEditing(currentEntry(), document.slides[2].id)
                return 500
            },
            () => {
                kept.rows = editor.count
                kept.victim = canvas.slide.id
                editScreen.showRowMenu(canvas.slide, 30, 30)
                return 300
            },
            () => {
                check("in the editor a slide's menu offers the same", menu.opened && labels().endsWith("Delete Slide…"), labels())
                click(menuRow("Delete Slide…"))
                return 300
            },
            () => {
                click(menuRow("Delete"))
                return 600
            },
            () => {
                check("and the slide is gone, with the editor on the one that took its place", editor.count === kept.rows - 1 && editor.rowOf(kept.victim) < 0 && canvas.slide !== null && editing, editor.count + " of " + kept.rows)
                stopEditing()
                return 400
            },
            () => {
                check("back at the show it is gone there too", !document.slides.some(s => s.id === kept.victim))
                // ---- hotkeys of groups
                const start = document.slides.findIndex((s, i) => i > 4 && s.groupStart && configuredGroup(s) !== undefined)
                kept.group = configuredGroup(document.slides[start])
                kept.first = document.slides.findIndex(s => s.groupStart && configuredGroup(s) === kept.group)
                goLive(0)
                testInput.key(Qt.Key_G, 0, "g")
                check("a letter that is no group's hotkey does nothing", liveIndex === 0)
                settingsOpen = true
                return 400
            },
            () => {
                const boxes = Lib.findAll(win.contentItem, i => i.objectName === "groupKey")
                const index = groups.findIndex(g => g === kept.group)
                kept.index = index
                const list = Lib.find(win.contentItem, i => i.positionViewAtIndex !== undefined && i.count === groups.length && i.spacing === 6)
                check("each group in the settings has a box for its hotkey", boxes.length > 0 && list !== null, boxes.length)
                kept.list = list
                list.positionViewAtIndex(index, ListView.Contain)
                return 300
            },
            () => {
                const box = Lib.findAll(win.contentItem, i => i.objectName === "groupKey").find(b => b.key !== undefined && b.parent.index === kept.index)
                click(centre(box))
                testInput.grab("6-group-key")
                check("clicked, the box waits for a key", box.activeFocus)
                testInput.key(Qt.Key_G, 0, "g")
                check("and the key pressed is the group's hotkey", groups[kept.index].key === "G", JSON.stringify(groups[kept.index]))
                // The same key given to another group leaves this one
                keyEditedProbe()
                return 300
            },
            () => {
                settingsOpen = false
                takeFocus()
                goLive(0)
                testInput.key(Qt.Key_G, 0, "g")
                check("pressed while showing, it goes to the first slide of that group", liveIndex === kept.first && kept.first > 0, liveIndex + " for " + kept.group.name + " at " + kept.first)
                testInput.key(Qt.Key_Right)
                check("and the arrow keys go on from there", liveIndex === kept.first + 1)
                const lines = testInput.readText(Log.path).split("\n")
                check("the log says which key and which group", lines.some(l => l.includes("the hotkey of the group")))
                check("nothing went wrong on the way", lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme") && !l.includes("not an image") && !l.includes("was not found in the workspace")).length === 0,
                      lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }

    // A pixel of the operator window, as "#rrggbb"
    function testInputColour(x, y) {
        testInput.grab("probe")
        return testInput.windowPixel(x, y)
    }

    function keyEditedProbe() {
        const other = (kept.index + 1) % groups.length
        const screen = Lib.find(win.contentItem, i => i.keyEdited !== undefined)
        screen.keyEdited(other, "G")
        check("a key is one group's only: given to another, it leaves the first", groups[other].key === "G" && groups[kept.index].key === "")
        screen.keyEdited(other, "")
        screen.keyEdited(kept.index, "G")
    }
}
