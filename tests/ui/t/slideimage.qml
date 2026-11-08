import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Pictures of the slides (qml/SlidePicture.qml): ProPresenter's Current Slide and Next Slide, a shape whose fill is a
// link to the live or the next slide, a still drawn as the slide's thumbnail draws it. ProPresenter's own, read from
// the stage layout it wrote ("Live Slides", which has a Next Slide); on the stage screen, black with nothing live and
// not the colour that only stands in for it; the next slide's words once a slide is live, changing as the live slide
// does; the same in the operator window's preview of the stage, which is drawn as the stage draws it, effects and all
// (the maintainer, 2026-11-08: "for stage display previews - we need to see all live"); in the editor, Slide Image
// ticked in the Fill menu, the slide said under it and on the canvas, and no picture drawn there; Current Slide chosen
// there, in the file, and the stage then showing the live slide, and black once the slide is cleared; the file byte
// for byte as it was once Next Slide is chosen again, and after a Colour and back; and a picture of a stage screen whose
// layout has a picture of the output and of the next slide (the maintainer's Triple Preview, whose picture of the Rear
// Projector had them missing) drawing both inside it. What is looked at is what is drawn.
QtObject {
    id: t

//COMMON
    function isSlidePicture(item) {
        return item.next !== undefined && item.media !== undefined && item.win !== undefined
    }

    function isPicture(item) {
        return item.inner !== undefined && item.videoInterval !== undefined && item.screenName !== undefined
    }

    function stageContent() {
        return testInput.windowContent("stage")
    }

    function stagePictures() {
        return Lib.findAll(stageContent(), isSlidePicture)
    }

    function previewPictures() {
        return Lib.findAll(win.contentItem, isSlidePicture)
    }

    // Where an item is in its window, as shares of the window, a tenth of it in from each side
    function shareOf(item, root) {
        const at = item.mapToItem(root, item.width * 0.1, item.height * 0.1)
        return { x: at.x / root.width, y: at.y / root.height, width: item.width * 0.8 / root.width, height: item.height * 0.8 / root.height }
    }

    // How much of a picture on the stage is lit (lighter than a dark grey): words are, black and the stand-in's blue
    // are not
    function stageLit(picture) {
        const s = shareOf(picture, stageContent())
        return testInput.lit(true, s.x, s.y, s.width, s.height)
    }

    // The stage's pixels at places over the whole of a picture on it: twenty across, at eight heights (a slide's words
    // can be anywhere in it: Act's are at its top)
    function stageLine(picture) {
        const root = stageContent()
        const found = []
        for (let row = 0; row < 8; ++row) {
            for (let i = 0; i < 20; ++i) {
                const at = picture.mapToItem(root, picture.width * (i + 0.5) / 20, picture.height * (row + 0.5) / 8)
                found.push(testInput.pixel(true, at.x / root.width, at.y / root.height))
            }
        }
        return found
    }

    // Whether any of those is the stand-in's pure blue
    function blue(line) {
        return line.some(c => { const v = String(c).toLowerCase(); return parseInt(v.substring(5, 7), 16) > 200 && parseInt(v.substring(1, 3), 16) < 60 })
    }

    // The same lit share in the operator window
    function windowLit(item) {
        const at = item.mapToItem(null, item.width * 0.1, item.height * 0.1)
        return testInput.windowLighter(at.x, at.y, item.width * 0.8, item.height * 0.8, 40)
    }

    function sel() {
        return editScreen.canvas.selected
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                // (The stage in the operator's preview)
                pickPreviewScreen(Screens.stage[0].id)
                kept.layout = StageLayouts.layouts.find(layout => Array.from(layout.slide.elements).some(e => e.fillKind === "slideImage"))
                kept.element = kept.layout ? Array.from(kept.layout.slide.elements).find(e => e.fillKind === "slideImage") : undefined
                check("a stage layout ProPresenter wrote has a picture of the next slide, read as a fill that is on, not a link of its text",
                      kept.layout !== undefined && kept.layout.name === "Live Slides" && kept.element.fillSlideNext === true && kept.element.fillShown
                      && kept.element.linkKind === "none", kept.layout ? kept.layout.name : "none")
                Show.stageLayoutId = kept.layout.id
                // A presentation whose first slides are words alone
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = catalog.open(d.file).slides
                        if (slides.length >= 4 && slides.slice(0, 4).every(s => s.media === undefined && s.actions.length === 0 && s.plainText.trim() !== "")
                            && new Set(slides.slice(1, 3).map(s => s.plainText)).size === 2) {
                            kept.entry = d
                            break
                        }
                    }
                    if (kept.entry)
                        break
                }
                check("(a presentation of slides of words to show)", kept.entry !== undefined)
                openEntry(kept.entry)
                return 1200
            },
            () => {
                const pictures = stagePictures()
                kept.picture = pictures[0]
                check("the stage screen draws a picture for it", pictures.length === 1 && kept.picture.next === true, pictures.length)
                const line = stageLine(kept.picture)
                check("with nothing live, black, and not the blue that stands in for it", kept.picture.slide === null && stageLit(kept.picture) < 0.005
                      && !blue(line), stageLit(kept.picture) + " " + line.slice(0, 3).join())
                check("and so does what the operator sees of the stage", previewPictures().length === 1, previewPictures().length)
                goLive(0)
                return 1500
            },
            () => {
                check("a slide live, the picture is of the next one", kept.picture.slide !== null && kept.picture.slide.id === document.slides[1].id
                      && nextSlide !== null && nextSlide.id === document.slides[1].id, kept.picture.slide ? kept.picture.slide.plainText : "none")
                kept.line = stageLine(kept.picture)
                check("its words drawn, on black, and not the stand-in", stageLit(kept.picture) > 0.005 && !blue(kept.line), stageLit(kept.picture))
                const seen = previewPictures()[0]
                const scene = Lib.find(win.contentItem, item => item.currentText !== undefined && item.nextText !== undefined && item.effects !== undefined && item.layout !== undefined && item.layout !== null)
                check("in the operator's preview of the stage too, the same slide, drawn", seen !== undefined && seen.slide !== null && seen.slide.id === document.slides[1].id
                      && windowLit(seen) > 0.005, seen ? windowLit(seen) : "none")
                check("and the preview draws the stage as the stage does, effects and all", scene !== null && scene.effects === true && seen.effects === true)
                testInput.grabStage("1-next-slide")
                testInput.grab("1-operator")
                goLive(1)
                return 1500
            },
            () => {
                const line = stageLine(kept.picture)
                check("the next slide going live, the picture is of the one after", kept.picture.slide !== null && kept.picture.slide.id === document.slides[2].id
                      && line.join() !== kept.line.join() && stageLit(kept.picture) > 0.005, kept.picture.slide ? kept.picture.slide.plainText : "none")
                // ---- In the editor
                kept.hash = testInput.fileHash(StageLayouts.path)
                startEditingStage(kept.layout.id)
                return 1200
            },
            () => {
                editScreen.canvas.pick(kept.element.id)
                editScreen.inspector.tab = "shape"
                return 500
            },
            () => {
                const menu = named("fillKind")
                check("the Fill menu has Slide Image, ticked", menu !== null && Array.from(menu.model)[menu.choice] === "Slide Image", menu ? Array.from(menu.model).join(", ") : "none")
                check("with Next Slide under it", named("fillSlide") !== null && named("fillSlide").visible && named("fillSlide").currentText === "Next Slide")
                const mark = Lib.findAll(editScreen, item => item.objectName === "linkMark" && item.visible && item.caption.startsWith("Slide Image"))
                check("and the canvas marks the element with it", mark.length === 1 && mark[0].caption === "Slide Image: Next Slide", mark.map(m => m.caption).join(" | "))
                check("the editor draws no picture of a slide", Lib.findAll(editScreen, isSlidePicture).length === 0)
                testInput.grab("2-editor")
                named("fillSlide").chosen(0)
                return 800
            },
            () => {
                check("Current Slide chosen is in the file", sel() !== null && sel().fillKind === "slideImage" && sel().fillSlideNext === false
                      && testInput.fileHash(StageLayouts.path) !== kept.hash, sel() ? JSON.stringify([sel().fillKind, sel().fillSlideNext]) : "none")
                stopEditing()
                return 1500
            },
            () => {
                kept.picture = stagePictures()[0]
                check("on the stage, the picture is of the live slide now", kept.picture !== undefined && kept.picture.next === false && kept.picture.slide !== null
                      && kept.picture.slide.id === document.slides[1].id && stageLit(kept.picture) > 0.005, kept.picture ? kept.picture.next : "none")
                clearSlide()
                return 1000
            },
            () => {
                check("cleared, the live slide's picture is black", kept.picture.slide === null && stageLit(kept.picture) < 0.005, stageLit(kept.picture))
                startEditingStage(kept.layout.id)
                return 1200
            },
            () => {
                editScreen.canvas.pick(kept.element.id)
                editScreen.inspector.tab = "shape"
                return 500
            },
            () => {
                named("fillSlide").chosen(1)
                return 800
            },
            () => {
                check("Next Slide chosen again: the file is byte for byte as ProPresenter wrote it", testInput.fileHash(StageLayouts.path) === kept.hash)
                named("fillKind").chosen(0)
                return 800
            },
            () => {
                check("Colour chosen takes the picture away: a plain fill, with no link", sel() !== null && sel().fillKind === "color" && sel().linkKind === "none",
                      sel() ? sel().fillKind : "none")
                named("fillKind").chosen(Array.from(named("fillKind").model).indexOf("Slide Image"))
                return 800
            },
            () => {
                check("Slide Image chosen starts as the current slide", sel() !== null && sel().fillKind === "slideImage" && sel().fillSlideNext === false)
                named("fillSlide").chosen(1)
                return 800
            },
            () => {
                check("and with Next Slide the file is byte for byte as it was", testInput.fileHash(StageLayouts.path) === kept.hash)
                stopEditing()
                return 800
            },
            () => {
                // ---- Inside a picture of a stage screen (the maintainer's Triple Preview, 2026-11-08): the Triple
                // Preview layout's picture of the Rear Projector, whose own layout is Live Slides
                const triple = Screens.stage.find(screen => screen.name === "Triple Preview")
                const rear = Screens.stage.find(screen => screen.name === "Rear Projector")
                kept.triple = StageLayouts.layouts.find(layout => layout.name === "Triple Preview")
                check("(the two stage screens, and the Triple Preview layout)", triple !== undefined && rear !== undefined && kept.triple !== undefined)
                Show.setStageLayout(rear.id, kept.layout.id)
                Show.setStageLayout(triple.id, kept.triple.id)
                pickPreviewScreen(triple.id)
                goLive(0)
                return 1500
            },
            () => {
                const rear = Lib.findAll(win.contentItem, item => isPicture(item) && item.namePrefix === "" && item.screen !== null && item.screen.name === "Rear Projector")[0]
                const inside = rear !== undefined ? Lib.findAll(rear, isPicture) : []
                const slides = rear !== undefined ? Lib.findAll(rear, isSlidePicture) : []
                check("in what the operator sees of the Triple Preview, its picture of the Rear Projector has that screen's picture of the output in it",
                      rear !== undefined && inside.length === 1 && inside[0].screen !== null && inside[0].screen.name === "Main Hall Projector",
                      inside.map(p => p.screen ? p.screen.name : "none").join(", "))
                check("and its picture of the next slide, drawn", slides.length === 1 && slides[0].slide !== null && slides[0].slide.id === document.slides[1].id
                      && windowLit(slides[0]) > 0.003, slides.length ? windowLit(slides[0]) : "none")
                testInput.grab("3-triple-preview")
                clearAll()
                return 600
            },
            () => {
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0,
                      logged(/PROBLEM|WARNING|ERROR/).join(" | "))
            }
        ]
        next()
    }
}
