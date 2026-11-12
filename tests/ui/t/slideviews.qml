import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// ProPresenter's three ways of showing the slides, by three buttons at the right of the bar under them, before the size
// buttons (the maintainer, 2026-11-12): the grid; Easy View, each slide's words alone in a format of the operator's over a
// plain colour, the slides themselves not drawn; and Table View, a row for each slide, its thumbnail at the left and its
// words (and notes) beside it. The view lit, kept in the settings; the … of a view's format, its panel, and what it
// changes drawn; Table View's rows grown by the size buttons, its Truncate, and a click on a row taking it live with the
// ring round the row. Then Clear to Logo (F12, and in the clears' menu): everything cleared and the logo set in the
// settings put up on the output, until a slide, media or a prop comes on; and with no logo set, everything cleared and
// the operator told where one is set.
QtObject {
    id: t

//COMMON
    readonly property string media: "@WORKSPACES@".replace(/WorkSpaces$/, "media")

    function viewButton(view) {
        return named("slideView:" + view)
    }

    // The colour drawn at a point of the operator window, as { r, g, b }
    function rgbAt(p) {
        const c = String(testInput.windowPixel(p.x, p.y)).toLowerCase()
        return { r: parseInt(c.substring(1, 3), 16), g: parseInt(c.substring(3, 5), 16), b: parseInt(c.substring(5, 7), 16), text: c }
    }

    // The middle of the output window, as { r, g, b }
    function outputMiddle() {
        const window = testInput.windowContent("output")
        if (!window)
            return { r: -1, g: -1, b: -1, text: "no output window" }
        const c = String(testInput.pixelIn("output", window.width / 2, window.height / 2))
        return { r: parseInt(c.substring(1, 3), 16), g: parseInt(c.substring(3, 5), 16), b: parseInt(c.substring(5, 7), 16), text: c }
    }

    function red(c) {
        return c.r > 200 && c.g < 60 && c.b < 60
    }

    // How much of an item the operator window draws lighter than a grey, as a share of it
    function lighter(item, grey) {
        const at = item.mapToItem(null, 0, 0)
        return testInput.windowLighter(at.x, at.y, item.width, item.height, grey)
    }

    // The same, of the first `width` of an item: where words set at its left are
    function lighterLeft(item, width, grey) {
        const at = item.mapToItem(null, 0, 0)
        return testInput.windowLighter(at.x, at.y, Math.min(width, item.width), item.height, grey)
    }

    function thumbnailOf(cell) {
        return Lib.find(cell, item => item.objectName.startsWith("slideFrame:")).children[0]
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                mediaTransitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("The Joy"))
                return 800
            },
            () => {
                kept.words = document.slides.findIndex(s => (s.plainText ?? "").length > 20)
                check("a presentation with words", kept.words >= 0, document.name)
                const grid = viewButton("grid"), easy = viewButton("easy"), table = viewButton("table")
                const sizes = Lib.find(named("gridFooter"), item => item.toString().startsWith("ZoomButtons"))
                check("the three view buttons are in the bar under the slides, at the left of the size buttons", grid !== null && easy !== null && table !== null
                      && grid.visible && table.mapToItem(null, table.width, 0).x < sizes.mapToItem(null, 0, 0).x
                      && grid.mapToItem(null, 0, 0).x < easy.mapToItem(null, 0, 0).x && easy.mapToItem(null, 0, 0).x < table.mapToItem(null, 0, 0).x,
                      grid ? grid.mapToItem(null, 0, 0).x + " " + table.mapToItem(null, table.width, 0).x + " " + sizes.mapToItem(null, 0, 0).x : "none")
                check("the grid is the view, and its button is lit", slideView === "grid" && grid.on && !easy.on && !table.on)
                const lit = rgbAt(grid.mapToItem(null, 2, grid.height / 2))
                check("drawn: the lit button blue", lit.b > 200 && lit.r < 120, lit.text)
                // The … is always there, so that nothing comes and goes with the view: dimmed
                // in the grid, and a click on it there does nothing
                const dots = named("viewFormatButton")
                check("the … is there in the grid, but dimmed", dots.visible && !dots.available && dots.opacity < 0.5, dots.opacity)
                click(centre(dots))
                check("and a click on it opens nothing", !viewFormatOpen && (named("viewFormat") === null || named("viewFormat").item === null))
                kept.dotsX = dots.mapToItem(null, 0, 0).x
                kept.columns = named("slideGrid").columns
                click(centre(easy))
                return 500
            },
            () => {
                check("Easy View, lit, and kept", slideView === "easy" && viewButton("easy").on && !viewButton("grid").on
                      && String(settings.value("slideView")) === "easy", slideView + " " + settings.value("slideView"))
                const cell = slideCell(kept.words)
                const words = Lib.find(cell, item => item.objectName === "easyWords")
                const slide = Lib.find(cell, item => item.slideWidth !== undefined && item.elements !== undefined)
                check("a slide's thumbnail is its words, and the slide is not drawn: nothing of it made", words !== null && words.words === document.slides[kept.words].plainText
                      && slide !== null && slide.slide === null && slide.elements.length === 0, words ? words.words.substring(0, 30) : "none")
                // White and bold on black, as it starts: much of it black, some of it white
                const thumbnail = thumbnailOf(cell)
                kept.light = lighter(thumbnail, 200)
                testInput.grab("1-easy-view")
                check("drawn: light words on black", kept.light > 0.02 && kept.light < 0.6 && rgbAt(thumbnail.mapToItem(null, 3, 3)).text === "#000000",
                      kept.light + " " + rgbAt(thumbnail.mapToItem(null, 3, 3)).text)
                check("the same columns as the grid", named("slideGrid").columns === kept.columns)
                check("and the … of its format, lit up where it was", named("viewFormatButton").available && named("viewFormatButton").opacity === 1
                      && named("viewFormatButton").mapToItem(null, 0, 0).x === kept.dotsX, named("viewFormatButton").mapToItem(null, 0, 0).x + " " + kept.dotsX)
                click(centre(named("viewFormatButton")))
                return 400
            },
            () => {
                const panel = named("viewFormat")
                check("the … opens the panel of Easy View's format", viewFormatOpen && panel !== null && panel.item !== null
                      && Lib.find(panel.item, item => item.text === "Easy View") !== null)
                check("over the bar, at the …", panel.item !== null && panel.mapToItem(null, 0, panel.height).y <= named("viewFormatButton").mapToItem(null, 0, 0).y)
                click(centre(named("viewFormat:alignLeft")))
                click(centre(named("viewFormat:bold")))
                setViewFormat("easy", { color: "#ff0000", background: "#0000ff" })
                return 400
            },
            () => {
                check("the panel's alignment and bold, and a colour and background given", easyFormat.align === "left" && easyFormat.bold === false
                      && easyFormat.color === "#ff0000" && easyFormat.background === "#0000ff", JSON.stringify(easyFormat))
                const saved = JSON.parse(String(settings.value("easyFormat")))
                check("kept in the settings", saved.color === "#ff0000" && saved.align === "left", String(settings.value("easyFormat")))
                const words = Lib.find(slideCell(kept.words), item => item.objectName === "easyWords")
                const text = Lib.find(words, item => item.horizontalAlignment !== undefined && item.font !== undefined)
                check("the words at the left now", text.horizontalAlignment === Text.AlignLeft && !text.font.bold)
                const corner = rgbAt(words.mapToItem(null, 2, 2))
                const at = words.mapToItem(null, 0, 0)
                const reddish = testInput.windowRed(at.x, at.y, words.width, words.height)
                check("drawn: red words on blue", corner.b > 200 && corner.r < 40 && reddish > 0.01, corner.text + " red " + reddish)
                testInput.key(Qt.Key_Escape, 0, "")
                return 300
            },
            () => {
                check("Esc puts the panel away", !viewFormatOpen)
                resetViewFormat("easy")
                click(centre(viewButton("table")))
                return 600
            },
            () => {
                check("Table View, lit", slideView === "table" && viewButton("table").on && !viewButton("easy").on)
                const view = named("slideGrid")
                check("one slide to a row", view.columns === 1, view.columns)
                const cell = slideCell(kept.words)
                const frame = Lib.find(cell, item => item.objectName.startsWith("slideFrame:"))
                const words = Lib.find(cell, item => item.objectName === "tableWords")
                check("a row: the slide's cell at the left, as wide as Table View's thumbnails, and its words beside it",
                      frame !== null && Math.abs(frame.width - tableThumbnailWidth) < 0.5 && words !== null && words.visible
                      && words.mapToItem(null, 0, 0).x > frame.mapToItem(null, frame.width, 0).x
                      && words.text.indexOf(document.slides[kept.words].plainText.split("\n")[0]) === 0,
                      frame ? frame.width + " " + (words ? words.text.substring(0, 30) : "no words") : "none")
                const slide = Lib.find(cell, item => item.slideWidth !== undefined && item.elements !== undefined)
                check("the thumbnail drawn as the grid draws it", slide !== null && slide.visible && slide.elements.length > 0)
                check("drawn: the words, light on dark", lighterLeft(words, 120, 160) > 0.008, lighterLeft(words, 120, 160))
                testInput.grab("2-table-view")
                kept.rowHeight = view.cellHeight
                kept.tableWidth = tableThumbnailWidth
                click(centre(Lib.find(named("gridFooter"), item => item.toString().startsWith("ZoomButtons")).children[1]))
                return 400
            },
            () => {
                check("the + makes the thumbnails, and so the rows, larger", tableThumbnailWidth === kept.tableWidth + 24 && named("slideGrid").cellHeight > kept.rowHeight,
                      kept.tableWidth + " -> " + tableThumbnailWidth)
                check("and is kept", Number(settings.value("tableThumbnailWidth")) === tableThumbnailWidth)
                // Truncate: at the beginning, in the middle, at the end
                const grid = named("slideGrid").parent
                check("Truncate at the beginning keeps the last lines", grid.truncated("a\nb\nc\nd\ne", "start", 3) === "…\nd\ne")
                check("in the middle, the first and the last", grid.truncated("a\nb\nc\nd\ne", "middle", 3) === "a\n…\ne"
                      && grid.truncated("a\nb\nc\nd\ne\nf", "middle", 4) === "a\nb\n…\nf")
                check("at the end, left to the eliding; and words that fit are not cut", grid.truncated("a\nb\nc\nd", "end", 2) === "a\nb\nc\nd"
                      && grid.truncated("a\nb", "start", 3) === "a\nb")
                // A click on a row's words takes it live, with the ring round the whole row
                click(centre(Lib.find(slideCell(kept.words), item => item.objectName === "tableWords")))
                return 500
            },
            () => {
                check("a click on a row's words takes the slide live", liveIndex === kept.words && !cleared, liveIndex)
                const cell = slideCell(kept.words)
                const ring = Lib.find(cell, item => item.objectName === "liveRing")
                check("the live ring round the whole row", ring !== null && ring.width > cell.width - 4, ring ? ring.width + " of " + cell.width : "none")
                // Notes, at the right of the words, for a slide that has them (the first such in the library)
                kept.notes = null
                for (const row of documents.filter(openable)) {
                    const opened = catalog.open(row.path)
                    const at = (opened.slides ?? []).findIndex(s => (s.notesText ?? "").trim() !== "")
                    if (at >= 0) {
                        kept.notes = { entry: row, index: at, text: opened.slides[at].notesText }
                        break
                    }
                }
                if (kept.notes)
                    openEntry(kept.notes.entry)
                return 800
            },
            () => {
                if (kept.notes) {
                    const notes = Lib.find(slideCell(kept.notes.index), item => item.objectName === "tableNotes")
                    check("a slide's notes at the right of its words", notes !== null && notes.visible && notes.text === kept.notes.text
                          && lighter(notes, 110) > 0.01, notes ? notes.text.substring(0, 30) : "none")
                } else {
                    check("(no slide of the library has notes to show)", true)
                }
                openEntry(entry("The Joy"))
                click(centre(viewButton("grid")))
                return 600
            },
            () => {
                check("back to the grid: its columns, and the … dimmed again", slideView === "grid" && named("slideGrid").columns === kept.columns
                      && named("viewFormatButton").visible && !named("viewFormatButton").available, named("slideGrid").columns)
                // ---- Clear to Logo
                goLive(kept.words)
                logoFile = media + "/Red.png"
                return 300
            },
            () => {
                check("the logo is handed to the show", Show.logoFile === media + "/Red.png", Show.logoFile)
                check("and named on the settings screen, Screens", named("logoFileName").text === "Red.png", named("logoFileName").text)
                check("and kept in the settings", String(settings.value("logoFile")) === media + "/Red.png")
                testInput.key(Qt.Key_F12, 0, "")
                return 500
            },
            () => {
                check("F12 clears to the logo: the slide cleared, the logo on the media layer", Show.logoLive && cleared && liveMedia !== null
                      && liveMedia.logo === true && liveMedia.name === "Red.png", liveMedia ? liveMedia.name : "no media")
                const middle = outputMiddle()
                check("drawn: the logo on the output", red(middle), middle.text)
                check("the media's clear button lit", named("clearMedia").live && !named("clearSlide").live)
                clickSlide(kept.words + 1)
                return 500
            },
            () => {
                check("a slide coming on takes the logo off", !Show.logoLive && liveMedia === null && !cleared && liveIndex === kept.words + 1)
                const middle = outputMiddle()
                check("drawn: the logo gone from the output", !red(middle), middle.text)
                // From the clears' menu, this time
                showClearMenu(named("clearSlide"), "")
                return 300
            },
            () => {
                check("the clears' menu has Clear to Logo", labels().startsWith("Clear to Logo  (F12), --, Configure Clear Groups…"), labels())
                click(menuRow("Clear to Logo  (F12)"))
                return 400
            },
            () => {
                check("and it clears to the logo", Show.logoLive && cleared)
                testInput.grab("3-logo")
                // Clearing the media (F3) takes it off
                testInput.key(Qt.Key_F3, 0, "")
                return 400
            },
            () => {
                check("clearing the media takes the logo off", !Show.logoLive && liveMedia === null)
                check("drawn: the logo gone from the output", !red(outputMiddle()), outputMiddle().text)
                // With no logo set: everything cleared, and the operator told where one is set
                logoFile = ""
                clickSlide(kept.words)
                return 300
            },
            () => {
                testInput.key(Qt.Key_F12, 0, "")
                return 400
            },
            () => {
                check("with no logo set, F12 clears everything and puts nothing up", !Show.logoLive && cleared && liveMedia === null)
                check("and says where a logo is set", notice.indexOf("No logo is set") === 0 && notice.indexOf("Settings → Screens") > 0, notice)
                check("the log says it", testInput.readText(Log.path).indexOf("no logo is set, so nothing is put up") >= 0)
                return 100
            },
        ]
        next()
    }
}
