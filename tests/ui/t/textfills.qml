import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// ProPresenter's text fills, drawn (src/textfills.h, qml/TextFills.qml), on the presentations
// prepare_textfills writes: each slide of "Text Fills" shown on the output, over a picture of
// four colours (red, green, blue and white from the top left, its quarter lines at the middle
// of the slide), and what is drawn looked at, as shares of a colour in parts of the screen,
// letters being too fine, and their shapes too much the font's, to find a point at a time.
// Then the same in the operator's preview, in a thumbnail and in the editor; letters filled
// with a blur of nothing in the editor; and the words of the gradient's slide edited, the
// file looked at after (after_textfills).
//
// Where things are, as shares of the screen: the element is 160 to 1760 across and 290 to 790
// down a slide of 1920 by 1080; its seven M's, at 200 points, are about 380 to 1540 across and
// 470 to 610 down.
QtObject {
    id: t

    // The test's own folder, where prepare_textfills put its pictures
    readonly property string folder: "@WORKSPACES@".replace(/\/WorkSpaces$/, "")

//COMMON
    // A part of the screen on the output, as shares of it, and how much of it is near a colour.
    // (The screen is drawn where the slide layer is, below the window's title strip: the
    // part is made shares of the window.)
    function share(x, y, width, height, colour, tolerance) {
        const content = testInput.windowContent("output")
        const layer = Lib.find(content, item => item.name === "slide layer")
        const at = layer.mapToItem(null, x * layer.width, y * layer.height)
        return testInput.outputShare(at.x / content.width, at.y / content.height, width * layer.width / content.width,
                                     height * layer.height / content.height, colour, tolerance ?? 40)
    }

    // The same of a part of an item of the operator window, given as shares of the item
    function shareOf(item, x, y, width, height, colour, tolerance) {
        const at = item.mapToItem(null, x * item.width, y * item.height)
        return testInput.windowShare(at.x, at.y, width * item.width, height * item.height, colour, tolerance ?? 40)
    }

    // The editor's page, the slide being edited, in the window's points
    function page() {
        const canvas = editScreen.canvas
        const at = canvas.mapToItem(null, canvas.originX, canvas.originY)
        return { x: at.x, y: at.y, width: canvas.slideWidth * canvas.u, height: canvas.slideHeight * canvas.u }
    }

    function pageShare(x, y, width, height, colour, tolerance) {
        const p = page()
        return testInput.windowShare(p.x + x * p.width, p.y + y * p.height, width * p.width, height * p.height, colour, tolerance ?? 40)
    }

    // Clicks an item of the Media Inspector by its name, scrolled into view first (as
    // mediainspector.qml does: the tabs' pages are taller than the panel)
    function clickNamed(name) {
        const item = named(name)
        let page = item.parent
        while (page && page.contentY === undefined)
            page = page.parent
        if (page) {
            const at = item.mapToItem(page.contentItem, 0, 0).y
            page.contentY = Math.max(0, Math.min(at - 40, page.contentHeight - page.height))
        }
        click(centre(item))
    }

    // Whether the inspector shows these words
    function shown(words) {
        return Lib.find(editScreen.inspector, item => item.text === words && item.visible) !== null
    }

    function fixed(value) {
        return value.toFixed(3)
    }

    function run() {
        steps = [
            () => {
                openEntry(entry("Text Fills"))
                return 1200
            },
            () => {
                check("the presentation of fills is open, its seven slides", document !== null && document.slides.length === 7,
                      document ? document.slides.length : "none")
                goLive(0)
                return 1800
            },
            // 1. Invert: the letters are what is behind them inverted, red become cyan, and
            // not their own black
            () => {
                const element = liveSlide.elements[0]
                check("the invert's element says its letters' fill, for the screens to draw",
                      element.fills !== undefined && element.fills.text.length === 1 && element.fills.text[0].kind === "invert",
                      JSON.stringify(element.fills))
                const cyan = share(0.22, 0.45, 0.26, 0.05, "#00ffff")
                const red = share(0.22, 0.45, 0.26, 0.05, "#ff0000")
                const black = share(0.22, 0.45, 0.26, 0.05, "#000000", 30)
                check("on the output, letters filled with an invert are the red behind them inverted, cyan, and not their own black",
                      cyan > 0.1 && red > 0.1 && black < 0.01, "cyan " + fixed(cyan) + " red " + fixed(red) + " black " + fixed(black))
                // Over the green, magenta
                const magenta = share(0.52, 0.45, 0.26, 0.05, "#ff00ff")
                check("and over the green, magenta", magenta > 0.1, fixed(magenta))
                // In the operator's preview of the output
                const preview = named("previewSlide")
                const there = preview ? shareOf(preview, 0.22, 0.45, 0.26, 0.05, "#00ffff") : -1
                check("in the operator's preview, the same", there > 0.05, fixed(there))
                // In the slide's thumbnail
                const cell = slideCell(0)
                const small = cell ? shareOf(cell, 0, 0, 1, 1, "#00ffff", 50) : -1
                check("in the slide's thumbnail, the same", small > 0.003, fixed(small))
                testInput.grabOutput("invert")
                goLive(1)
                return 1500
            },
            // 2. Blur: where the red meets the blue, which every M's stems cross, the letters
            // are a blur of the two, which the picture beside them is not
            () => {
                const mixed = share(0.22, 0.475, 0.26, 0.05, "#800080", 70)
                const beside = share(0.03, 0.475, 0.12, 0.05, "#800080", 70)
                check("on the output, letters filled with a blur mix the red and the blue where they meet, and the picture beside them does not",
                      mixed > 0.25 && beside < 0.15, "in the letters " + fixed(mixed) + ", beside them " + fixed(beside))
                const black = share(0.22, 0.45, 0.26, 0.05, "#000000", 30)
                check("and they are not their own black", black < 0.01, fixed(black))
                testInput.grabOutput("blur")
                goLive(2)
                return 1500
            },
            // 3. Cut Out: the box is its own navy, and the letters holes in it
            () => {
                const element = liveSlide.elements[0]
                check("the cut-out's element says its box's fill", element.boxFill !== undefined && element.boxFill.kind === "cutOut"
                      && element.fills === undefined, JSON.stringify(element.boxFill))
                const navy = share(0.1, 0.3, 0.8, 0.4, "#000080", 30)
                const red = share(0.22, 0.45, 0.26, 0.05, "#ff0000")
                check("on the output, letters cut out of their box's navy show the red behind it",
                      navy > 0.4 && red > 0.1, "navy " + fixed(navy) + " red " + fixed(red))
                testInput.grabOutput("cutout")
                goLive(3)
                return 1500
            },
            // 4. Media: the letters are the picture of the fill, its yellow at the top left
            () => {
                const yellow = share(0.22, 0.44, 0.26, 0.05, "#ffff00")
                const magenta = share(0.52, 0.44, 0.26, 0.05, "#ff00ff")
                check("on the output, letters filled with a picture show it, fitted to the box: yellow at its top left, magenta at its top right",
                      yellow > 0.1 && magenta > 0.1, "yellow " + fixed(yellow) + " magenta " + fixed(magenta))
                testInput.grabOutput("media")
                goLive(4)
                return 1500
            },
            // 5. A gradient over the first three letters, and the rest their own black
            () => {
                const start = share(0.19, 0.45, 0.06, 0.1, "#ffd42a", 45)
                const black = share(0.6, 0.45, 0.2, 0.1, "#000000", 30)
                const blackLeft = share(0.19, 0.45, 0.06, 0.1, "#000000", 30)
                check("on the output, a gradient over the first three letters begins yellow, and the letters after them are their own black",
                      start > 0.05 && black > 0.1 && blackLeft < 0.02, "yellow " + fixed(start) + " black after " + fixed(black) + " black in it " + fixed(blackLeft))
                testInput.grabOutput("gradient")
                goLive(5)
                return 1500
            },
            // 6. A box whose fill is a blur of what is behind it
            () => {
                const element = liveSlide.elements[0]
                check("the blurred box's element says its fill", element.boxFill !== undefined && element.boxFill.kind === "blur",
                      JSON.stringify(element.boxFill))
                const mixed = share(0.485, 0.3, 0.03, 0.15, "#808000", 70)
                const above = share(0.485, 0.05, 0.03, 0.15, "#808000", 70)
                check("on the output, a box filled with a blur mixes the red and the green inside it, and not above it",
                      mixed > 0.2 && above < 0.03, "inside " + fixed(mixed) + ", above " + fixed(above))
                testInput.grabOutput("blurbox")
                goLive(6)
                return 1500
            },
            // 7. An oval whose fill is an invert: inverted inside it, as it was at the corner
            // of its box, outside it
            () => {
                const inside = share(0.27, 0.42, 0.06, 0.05, "#00ffff")
                const corner = share(0.084, 0.27, 0.025, 0.03, "#ff0000")
                check("on the output, an oval filled with an invert is the red inverted inside it, and the red as it was at the corner of its box",
                      inside > 0.8 && corner > 0.8, "inside " + fixed(inside) + " corner " + fixed(corner))
                testInput.grabOutput("invertoval")
                return 300
            },
            // In the editor: the invert, over the slide's picture
            () => {
                startEditing(currentEntry(), document.slides[0].id)
                return 1800
            },
            () => {
                const cyan = pageShare(0.22, 0.45, 0.26, 0.05, "#00ffff", 50)
                check("in the editor, letters filled with an invert are the slide's picture inverted", cyan > 0.1, fixed(cyan))
                testInput.grab("editor-invert")
                stopEditing()
                return 600
            },
            // The words of the gradient's slide: two letters typed before its three, which take
            // its fill, as typed letters take the format of where they go
            () => {
                startEditing(currentEntry(), document.slides[4].id)
                return 1800
            },
            () => {
                const canvas = editScreen.canvas
                canvas.editText(canvas.elements[0].id)
                return 400
            },
            () => {
                testInput.key(Qt.Key_Home, Qt.ControlModifier)
                testInput.type("XY")
                return 400
            },
            () => {
                editScreen.canvas.finishText()
                return 600
            },
            () => {
                const yellow = pageShare(0.12, 0.45, 0.06, 0.1, "#ffd42a", 45)
                check("in the editor, the letters typed before the gradient's are in it", yellow > 0.05, fixed(yellow))
                testInput.grab("editor-gradient")
                stopEditing()
                return 800
            },
            // The Text tab's fill, as ProPresenter's: the first slide's words, an invert, given
            // a blur, a gradient, a picture and Text Color in turn
            () => {
                startEditing(currentEntry(), document.slides[0].id)
                return 1800
            },
            () => {
                editScreen.canvas.pick(editScreen.canvas.elements[0].id)
                editScreen.inspector.tab = "text"
                return 600
            },
            () => {
                const menu = named("textFillKind")
                check("the Text tab's fill menu has ProPresenter's six, and says the words' fill, Background Invert",
                      menu !== null && Array.from(menu.model).join(", ") === "Text Color, Text Gradient, Text Cut Out, Media Fill, Background Invert, Background Blur"
                      && menu.choice === 4, menu ? Array.from(menu.model).join(", ") + " / " + menu.choice : "none")
                testInput.grab("editor-text-invert")
                menu.chosen(5)
                return 1200
            },
            () => {
                const fills = editScreen.canvas.elements[0].fills
                check("Background Blur chosen: the words are filled with a blur, of 0.5, there being no other on the slide",
                      fills !== undefined && fills.text[0].kind === "blur" && fills.text[0].amount === 0.5, JSON.stringify(fills))
                check("and under it, its saturation and its blur, and ProPresenter's note",
                      named("textBlurSaturation").visible && named("textBlurAmount").visible && shown("Blur percentage is shared among all objects on a slide."))
                testInput.grab("editor-text-blur")
                const slider = named("textBlurAmount")
                slider.value = 0.8
                slider.moved()
                return 400
            },
            () => {
                editScreen.canvas.settle()
                return 800
            },
            () => {
                const fills = editScreen.canvas.elements[0].fills
                check("the Blur slider sets its amount", fills !== undefined && Math.abs(fills.text[0].amount - 0.8) < 0.001, JSON.stringify(fills))
                named("textFillKind").chosen(1)
                return 1000
            },
            () => {
                check("Text Gradient chosen: the menu says it, with its colours, angle and the object bounds' box",
                      named("textFillKind").choice === 1 && named("textGradientFrom").visible && named("textGradientAngle").visible
                      && named("textGradientBounds").visible)
                named("textGradientAngle").edited(0)
                return 800
            },
            () => {
                named("textGradientFrom").picked("#ffff00")
                return 800
            },
            () => {
                const left = pageShare(0.2, 0.45, 0.06, 0.1, "#ffff00", 60)
                const right = pageShare(0.74, 0.45, 0.06, 0.1, "#ffff00", 60)
                check("the gradient's first colour, yellow, is at the left of the words, along an angle of 0", left > 0.05 && right < 0.02,
                      "left " + fixed(left) + " right " + fixed(right))
                named("textGradientSwap").clicked()
                return 800
            },
            () => {
                const left = pageShare(0.2, 0.45, 0.06, 0.1, "#ffff00", 60)
                const right = pageShare(0.74, 0.45, 0.06, 0.1, "#ffff00", 60)
                check("swapped, the yellow is at the right", right > 0.05 && left < 0.02, "left " + fixed(left) + " right " + fixed(right))
                named("textGradientBounds").toggled(true)
                return 800
            },
            () => {
                const fill = editScreen.canvas.format ? editScreen.canvas.format.fill : null
                check("Use object bounds for gradient ticked: the words' gradient says it runs over the box", fill !== null && fill.stretch === true
                      && named("textGradientBounds").checked, JSON.stringify(fill))
                testInput.grab("editor-text-gradient")
                editScreen.fillWordsWith(t.folder + "/Fills Picture.png")
                return 1500
            },
            () => {
                const fills = editScreen.canvas.elements[0].fills
                const yellow = pageShare(0.22, 0.44, 0.26, 0.05, "#ffff00", 50)
                check("a media file given to the words: they are filled with it, Scale to Fill, its yellow at the top left",
                      fills !== undefined && fills.text[0].kind === "media" && fills.text[0].scale === 1 && yellow > 0.05,
                      JSON.stringify(fills) + " yellow " + fixed(yellow))
                check("and the menu says Media Fill, with the file's name and the Media Inspector", named("textFillKind").choice === 3
                      && shown("Fills Picture.png") && named("textFillInspector").enabled)
                check("and ProPresenter's gear, for another file, and its magnifier, to show this one in the file manager",
                      named("textFillChoose") !== null && named("textFillChoose").visible && named("textFillShow") !== null
                      && named("textFillShow").visible && named("textFillShow").available)
                testInput.grab("editor-text-media")
                named("textFillScale").chosen(2)
                return 800
            },
            () => {
                const fills = editScreen.canvas.elements[0].fills
                check("Stretch to Fill chosen: the words' picture is stretched", fills !== undefined && fills.text[0].scale === 2, JSON.stringify(fills))
                named("textFillInspector").clicked()
                return 900
            },
            () => {
                const panel = mediaInspectorLoader.item
                check("the Media Inspector comes up on the words' picture", panel !== null && panel.open && panel.about.startsWith("The fill of the words of"),
                      panel ? panel.about : "none")
                clickNamed("inspectorTab-properties")
                return 300
            },
            () => {
                clickNamed("inspectorRotate180")
                return 900
            },
            () => {
                const fills = editScreen.canvas.elements[0].fills
                check("turned half round there, the words' picture is", fills !== undefined && fills.text[0].settings !== undefined
                      && fills.text[0].settings.rotation === 180, JSON.stringify(fills))
                clickNamed("inspectorDone")
                return 900
            },
            () => {
                const orange = pageShare(0.22, 0.44, 0.26, 0.05, "#ff8000", 50)
                check("and drawn so: its orange, from the bottom right, at the top left", orange > 0.05, "orange " + fixed(orange))
                testInput.grab("editor-text-media-turned")
                return 300
            },
            () => {
                named("textFillKind").chosen(0)
                return 1000
            },
            () => {
                const black = pageShare(0.22, 0.45, 0.26, 0.05, "#000000", 40)
                check("Text Color chosen: the words are their own black again, with no fills", editScreen.canvas.elements[0].fills === undefined
                      && black > 0.05 && named("textColor").visible, "black " + fixed(black))
                stopEditing()
                return 800
            },
            // A blur's amount, set for the words, is set for every blur on their slide
            () => {
                openEntry(entry("Fills Shared"))
                return 1200
            },
            () => {
                startEditing(currentEntry(), document.slides[0].id)
                return 1800
            },
            () => {
                const words = editScreen.canvas.elements.find(e => e.name === "Shared")
                editScreen.canvas.pick(words.id)
                editScreen.inspector.tab = "text"
                return 600
            },
            () => {
                const slider = named("textBlurAmount")
                slider.value = 0.8
                slider.moved()
                return 400
            },
            () => {
                editScreen.canvas.settle()
                return 800
            },
            () => {
                const box = editScreen.canvas.elements.find(e => e.name === "Blurred Box")
                check("the words' blur set to 0.8, the box's on the same slide is 0.8 too", box !== undefined && box.boxFill !== undefined
                      && Math.abs(box.boxFill.amount - 0.8) < 0.001, box ? JSON.stringify(box.boxFill) : "no box")
                stopEditing()
                return 800
            },
            // A box's own blur and invert, in the Shape tab, with the Text tab's controls
            () => {
                openEntry(entry("Text Fills"))
                return 1200
            },
            () => {
                startEditing(currentEntry(), document.slides[5].id)
                return 1800
            },
            () => {
                editScreen.canvas.pick(editScreen.canvas.elements[0].id)
                editScreen.inspector.tab = "shape"
                return 600
            },
            () => {
                const menu = named("fillKind")
                check("the Shape tab's fill menu says the box's fill, Background Blur, with its saturation, its blur and ProPresenter's note",
                      menu.choice === 5 && Array.from(menu.model).includes("Background Invert") && named("boxBlurSaturation") !== null
                      && named("boxBlurAmount").visible && shown("Blur percentage is shared among all objects on a slide."),
                      Array.from(menu.model).join(", ") + " / " + menu.choice)
                const slider = named("boxBlurAmount")
                slider.value = 0.8
                slider.moved()
                return 400
            },
            () => {
                editScreen.canvas.settle()
                return 800
            },
            () => {
                const box = editScreen.canvas.elements[0].boxFill
                check("the Blur slider sets the box's blur", box !== undefined && box.kind === "blur" && Math.abs(box.amount - 0.8) < 0.001,
                      JSON.stringify(box))
                testInput.grab("editor-shape-blur")
                named("fillKind").chosen(6)
                return 1000
            },
            () => {
                const element = editScreen.canvas.elements[0]
                const cyan = pageShare(0.2, 0.3, 0.2, 0.1, "#00ffff", 60)
                check("Background Invert chosen: the box is what is behind it inverted, the red cyan", element.fillKind === "invert"
                      && element.boxFill !== undefined && element.boxFill.kind === "invert" && cyan > 0.5,
                      element.fillKind + " " + JSON.stringify(element.boxFill) + " cyan " + fixed(cyan))
                testInput.grab("editor-shape-invert")
                named("fillKind").chosen(0)
                return 1000
            },
            () => {
                const element = editScreen.canvas.elements[0]
                check("and Colour: a colour again", element.fillKind === "color" && element.boxFill === undefined, element.fillKind)
                stopEditing()
                return 800
            },
            // Letters filled with a blur, with nothing behind the slide: in the editor, half
            // see-through white, to be seen and placed
            () => {
                openEntry(entry("Fills Unseen"))
                return 1200
            },
            () => {
                startEditing(currentEntry(), document.slides[0].id)
                return 1800
            },
            () => {
                const p = page()
                const grey = testInput.windowLighter(p.x + 0.22 * p.width, p.y + 0.45 * p.height, 0.56 * p.width, 0.1 * p.height, 100)
                check("in the editor, letters filled with a blur of nothing are drawn half see-through white", grey > 0.1, fixed(grey))
                testInput.grab("editor-unseen")
                stopEditing()
                return 600
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
