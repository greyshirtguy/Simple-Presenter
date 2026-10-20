import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A shape filled with a Screen Preview, a picture of what one of the screens is showing (qml/ScreenPicture.qml):
// ProPresenter's own, read from the stage layouts it wrote, each naming one of the workspace's screens; on the
// stage screen, a picture of the output, which is red while a red picture is on the media layer and black while
// nothing is, with the live slide in it, and a video in it frame for frame; the same in what the operator sees of
// the stage; a picture of the stage screen itself, which has no picture inside it; in the editor, the fill chosen in
// the Fill menu, the screen chosen, what is in the file afterwards with the rest of the element as it was, and the
// file byte for byte as it was once another fill is chosen; and such a slide on the output, whose picture of the
// output has no picture inside it, while a thumbnail of it draws none; and, where NDI's library is here, a stage
// screen sent over NDI with the picture in what goes out, before the stage is switched off and after it is switched on
// again. What is looked at is what is drawn.
QtObject {
    id: t

//COMMON
    readonly property string media: "@WORKSPACES@".replace(/WorkSpaces$/, "media")

    function sel() {
        return editScreen.canvas.selected
    }

    function isPicture(item) {
        return item.inner !== undefined && item.videoInterval !== undefined && item.screenName !== undefined
    }

    function stageContent() {
        return testInput.windowContent("stage")
    }

    // The pictures of screens on the stage screen, and in the operator window but for its preview of the output
    function stagePictures() {
        return Lib.findAll(stageContent(), isPicture)
    }

    function operatorPictures() {
        return Lib.findAll(win.contentItem, item => isPicture(item) && item.namePrefix === "")
    }

    // Where a picture is in its window, as shares of the window, a tenth of it in from each side
    function shareOf(item, root) {
        const at = item.mapToItem(root, item.width * 0.1, item.height * 0.1)
        return { x: at.x / root.width, y: at.y / root.height, width: item.width * 0.8 / root.width, height: item.height * 0.8 / root.height }
    }

    function stageRed(picture) {
        const s = shareOf(picture, stageContent())
        return testInput.red(true, s.x, s.y, s.width, s.height)
    }

    function stageLit(picture) {
        const s = shareOf(picture, stageContent())
        return testInput.lit(true, s.x, s.y, s.width, s.height)
    }

    // The same in the operator window, in its own points
    function operatorRed(picture) {
        const at = picture.mapToItem(null, picture.width * 0.1, picture.height * 0.1)
        return testInput.windowRed(at.x, at.y, picture.width * 0.8, picture.height * 0.8)
    }

    function layoutWith(screenName) {
        return StageLayouts.layouts.find(layout => Array.from(layout.slide.elements).some(e => e.fillKind === "screen" && e.fillScreenName === screenName))
    }

    function onDisk() {
        return catalog.open(kept.entry.file).slides[kept.index].elements.find(e => e.id === kept.element.id)
    }

    // An element as the file has it, but for its fill
    function restOf(e) {
        const rest = {}
        for (const key of Object.keys(e).sort()) {
            if (!key.startsWith("fill") && key !== "visible" && key !== "displayText" && key !== "text")
                rest[key] = e[key]
        }
        return JSON.stringify(rest) + testInput.plain(e.text)
    }

    function run() {
        steps = [
            () => {
                // ---- ProPresenter's own, in its stage layouts
                // (The stage in the operator's preview, which is what these look at: the first audience screen is what it shows otherwise.)
                pickPreviewScreen(Screens.stage[0].id)
                let previews = []
                for (const layout of StageLayouts.layouts)
                    previews = previews.concat(Array.from(layout.slide.elements).filter(e => e.fillKind === "screen"))
                check("the stage layouts ProPresenter wrote have Screen Previews, each of a screen of the workspace's, by id and name",
                      previews.length === 4 && previews.every(e => Screens.screens.some(s => s.id === e.fillScreenId && s.name === e.fillScreenName)),
                      JSON.stringify(previews.map(e => [e.fillScreenName, e.fillShown])))
                check("and they are fills that are on, not links of their text", previews.every(e => e.fillShown && e.linkKind === "none"))
                kept.layout = layoutWith("Main Hall Projector")
                check("(a layout with a Screen Preview of the output, and the stage screen there to show it)", kept.layout !== undefined
                      && Screens.stage.length > 0 && stageContent() !== null, kept.layout ? kept.layout.name : "none")
                Show.stageLayoutId = kept.layout.id
                return 1000
            },
            () => {
                const pictures = stagePictures()
                const previews = Array.from(kept.layout.slide.elements).filter(e => e.fillKind === "screen")
                check("the stage screen draws a picture for each of the layout's Screen Previews, and none inside them",
                      pictures.length === previews.length && pictures.some(p => p.screen !== null && p.screen.name === "Main Hall Projector"),
                      pictures.map(p => p.screen ? p.screen.name : "none").join(", "))
                kept.picture = pictures.find(p => p.screen !== null && p.screen.name === "Main Hall Projector")
                check("with nothing live, the picture is of a black screen", stageLit(kept.picture) < 0.02, stageLit(kept.picture))
                Show.showMedia({ name: "Red.png", path: media + "/Red.png", source: "file://" + media + "/Red.png", video: false })
                return 2000
            },
            () => {
                check("(the output is red)", testInput.red(false, 0.3, 0.3, 0.4, 0.4) > 0.95, testInput.red(false, 0.3, 0.3, 0.4, 0.4))
                check("the picture on the stage screen is red too", stageRed(kept.picture) > 0.9, stageRed(kept.picture))
                const seen = operatorPictures()
                check("and in what the operator sees of the stage", seen.length === 1 && operatorRed(seen[0]) > 0.6,
                      seen.length + " " + (seen.length ? operatorRed(seen[0]) : ""))
                testInput.grabStage("1-red-on-stage")
                testInput.grab("1-operator")
                // A slide over it
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = catalog.open(d.file).slides
                        const at = slides.findIndex(s => s.media === undefined && s.actions.length === 0
                                                    && s.elements.some(e => e.fillOn && e.fillKind === "color" && !e.fillLinesOnly && e.width > 300 && !e.hidden))
                        if (at >= 0 && !kept.entry) {
                            kept.entry = d
                            kept.index = at
                            kept.element = slides[at].elements.find(e => e.fillOn && e.fillKind === "color" && !e.fillLinesOnly && e.width > 300 && !e.hidden)
                        }
                    }
                    if (kept.entry)
                        break
                }
                check("(a slide with a box filled with a colour, and no media or action of its own)", kept.entry !== undefined, kept.entry ? kept.entry.name : "none")
                openEntry(kept.entry)
                goLive(kept.index)
                return 800
            },
            () => {
                // (A slide can give the stage another layout: this one is given back.)
                Show.stageLayoutId = kept.layout.id
                return 1000
            },
            () => {
                kept.picture = stagePictures().find(p => p.screen !== null && p.screen.name === "Main Hall Projector")
                const slide = kept.picture ? Lib.find(kept.picture, item => item.slide !== undefined && item.elements !== undefined && item.visible) : null
                check("the live slide is in the picture", slide !== null && slide.slide !== null && slide.slide.id === liveSlide.id,
                      slide && slide.slide ? slide.slide.id : "none")
                testInput.grabStage("2-slide-on-stage")
                // ---- A video: the slide of the workspace's with one, if it has one
                kept.video = null
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const at = catalog.open(d.file).slides.findIndex(s => s.media !== undefined && s.media.video)
                        if (at >= 0 && kept.video === null)
                            kept.video = { entry: d, index: at }
                    }
                }
                if (kept.video !== null) {
                    openEntry(kept.video.entry)
                    goLive(kept.video.index)
                }
                return 1000
            },
            () => {
                Show.stageLayoutId = kept.layout.id
                return 1500
            },
            () => {
                kept.picture = stagePictures().find(p => p.screen !== null && p.screen.name === "Main Hall Projector")
                if (kept.video === null) {
                    check("(the workspace has no video, so a video in the picture is not tried)", true)
                    return 10
                }
                kept.frames = 0
                kept.previewFrames = 0
                kept.picture.relay.relayed.connect(() => ++kept.frames)
                Lib.find(win.contentItem, item => isPicture(item) && item.namePrefix === "preview").relay.relayed.connect(() => ++kept.previewFrames)
                return 2000
            },
            () => {
                if (kept.video !== null) {
                    check("a video is in the picture frame for frame, where the operator's preview has a few a second",
                          kept.frames >= 30 && kept.frames > 2 * kept.previewFrames, kept.frames + " and " + kept.previewFrames + " in two seconds")
                    check("and it is drawn", stageLit(kept.picture) > 0.05, stageLit(kept.picture))
                    testInput.grabStage("3-video-on-stage")
                }
                clearMedia()
                clearSlide()
                // ---- A picture of the stage screen itself
                kept.own = layoutWith(Screens.stage[0].name)
                check("(a layout with a Screen Preview of the stage screen)", kept.own !== undefined, Screens.stage[0].name)
                Show.stageLayoutId = kept.own.id
                return 1200
            },
            () => {
                const pictures = stagePictures()
                const own = pictures.find(p => p.screen !== null && p.screen.id === Screens.stage[0].id)
                check("a stage screen with a picture of itself has the picture, and no picture inside that",
                      own !== undefined && pictures.length === Array.from(kept.own.slide.elements).filter(e => e.fillKind === "screen").length
                      && Lib.findAll(own, isPicture).length === 0, pictures.length + " " + (own ? Lib.findAll(own, isPicture).length : "none"))
                check("its picture is of the layout the screen has", own !== undefined
                      && Lib.find(own, item => item.slide !== undefined && item.elements !== undefined && item.visible && item.slide !== null
                                  && item.slide.elements.map(e => e.id).join() === Array.from(kept.own.slide.elements).map(e => e.id).join()) !== null)
                testInput.grabStage("4-itself")
                Show.stageLayoutId = ""
                // ---- In the editor
                kept.hash = testInput.fileHash(kept.entry.file)
                kept.rest = restOf(kept.element)
                openEntry(kept.entry)
                startEditing(kept.entry, document.slides[kept.index].id)
                return 800
            },
            () => {
                editScreen.canvas.pick(kept.element.id)
                editScreen.inspector.tab = "shape"
                return 400
            },
            () => {
                check("the Fill menu has Screen Preview", Array.from(named("fillKind").model).indexOf("Screen Preview") === 3, Array.from(named("fillKind").model).join(", "))
                named("fillKind").chosen(3)
                return 600
            },
            () => {
                const e = onDisk()
                check("choosing it makes the fill a Screen Preview of the first audience screen, in the file",
                      e !== undefined && e.fillKind === "screen" && e.fillScreenId === Screens.audience[0].id && e.fillScreenName === Screens.audience[0].name && e.fillShown,
                      e ? JSON.stringify([e.fillKind, e.fillScreenName]) : "none")
                check("with the rest of the element as it was", e !== undefined && restOf(e) === kept.rest)
                check("the inspector says which screen", named("fillScreen") !== null && named("fillScreen").visible && named("fillScreen").currentText === Screens.audience[0].name,
                      named("fillScreen") ? named("fillScreen").currentText : "none")
                const mark = Lib.findAll(editScreen, item => item.objectName === "linkMark" && item.visible && item.caption.startsWith("Screen Preview"))
                check("and the canvas marks the element with it", mark.length === 1 && mark[0].caption === "Screen Preview: " + Screens.audience[0].name,
                      mark.map(m => m.caption).join(" | "))
                check("the editor draws no picture of a screen", Lib.findAll(editScreen, isPicture).length === 0)
                testInput.grab("5-editor")
                kept.other = Screens.screens.findIndex(s => s.name === "Stream")
                named("fillScreen").chosen(kept.other)
                return 600
            },
            () => {
                const e = onDisk()
                check("another screen chosen is in the file", e !== undefined && e.fillKind === "screen" && e.fillScreenName === "Stream"
                      && e.fillScreenId === Screens.screens[kept.other].id, e ? e.fillScreenName : "none")
                named("fillKind").chosen(0)
                return 600
            },
            () => {
                const e = onDisk()
                check("Colour chosen again takes the Screen Preview away: the file is byte for byte as it was", e !== undefined && e.fillKind === "color"
                      && testInput.fileHash(kept.entry.file) === kept.hash)
                named("fillKind").chosen(3)
                return 600
            },
            () => {
                // An ellipse, filled with a Screen Preview of the stage screen: cut to its shape
                editScreen.canvas.addShape("ellipse")
                return 600
            },
            () => {
                check("(an ellipse added and picked)", sel() !== null && sel().shape === "ellipse", sel() ? sel().shape : "none")
                named("fillKind").chosen(3)
                return 600
            },
            () => {
                named("fillScreen").chosen(Screens.screens.findIndex(s => s.id === Screens.stage[0].id))
                return 600
            },
            () => {
                kept.ellipse = sel()
                check("(its fill is a Screen Preview of the stage screen)", kept.ellipse.fillKind === "screen" && kept.ellipse.fillScreenId === Screens.stage[0].id,
                      kept.ellipse.fillScreenName)
                stopEditing()
                return 800
            },
            () => {
                // ---- On the output: a slide with a picture of the output itself. (Under a look that gives the output
                // no theme: a theme's boxes are drawn in place of the slide's own.) And the output in the operator's preview.
                pickPreviewScreen(Screens.audience[0].id)
                const plain = Looks.looks.find(look => Looks.of(look.id, Screens.audience[0].id).theme === "")
                check("(a look that gives the output no theme)", plain !== undefined, Looks.looks.map(look => look.name).join(", "))
                Show.lookId = plain.id
                Show.showMedia({ name: "Red.png", path: media + "/Red.png", source: "file://" + media + "/Red.png", video: false })
                openEntry(kept.entry)
                goLive(kept.index)
                return 1500
            },
            () => {
                const onOutput = Lib.findAll(output.contentItem, isPicture)
                const drawn = Lib.findAll(output.contentItem, item => item.source !== undefined && item.pictures !== undefined && item.boxFilled !== undefined
                                          && item.source !== null && item.source.id === kept.element.id)
                check("on the output the slide has its picture of the output, and the ellipse its of the stage, with no picture inside them",
                      onOutput.length === 2 && onOutput.some(p => p.screen !== null && p.screen.name === Screens.audience[0].name)
                      && onOutput.some(p => p.screen !== null && p.screen.id === Screens.stage[0].id),
                      onOutput.length + " " + JSON.stringify(drawn.map(d => [d.source.fillKind, d.boxFilled, d.pictures !== null, d.plain, d.visible, d.source.fillShown]))
                      + " live " + (liveSlide ? liveSlide.elements.filter(e => e.id === kept.element.id).map(e => e.fillKind).join() : "none")
                      + " index " + kept.index + " held " + (output.heldSlide !== undefined) + " on output: "
                      + JSON.stringify(Lib.findAll(output.contentItem, item => item.source !== undefined && item.pictures !== undefined && item.boxFilled !== undefined && item.source !== null)
                                       .map(d => [d.source.id.substring(0, 4), d.source.fillKind, d.visible])) + " want " + kept.element.id.substring(0, 4))
                const preview = Lib.find(win.contentItem, item => isPicture(item) && item.namePrefix === "preview")
                check("the operator's preview of the output shows them as the output does", Lib.findAll(preview, isPicture).length === 2,
                      Lib.findAll(preview, isPicture).length)
                // The ellipse on the output: the stage's picture in the middle, and the red behind it at its box's corners
                const ellipse = Lib.find(output.contentItem, item => item.source !== undefined && item.boxFilled !== undefined && item.source !== null
                                         && item.source.id === kept.ellipse.id && item.visible)
                const root = output.contentItem
                const share = (fx, fy, fw, fh) => {
                    const at = ellipse.mapToItem(root, ellipse.width * fx, ellipse.height * fy)
                    return testInput.red(false, at.x / root.width, at.y / root.height, ellipse.width * fw / root.width, ellipse.height * fh / root.height)
                }
                const middle = ellipse ? share(0.35, 0.35, 0.3, 0.3) : -1
                const corners = ellipse ? Math.min(share(0.01, 0.01, 0.07, 0.07), share(0.92, 0.01, 0.07, 0.07), share(0.01, 0.92, 0.07, 0.07), share(0.92, 0.92, 0.07, 0.07)) : -1
                check("an ellipse's picture is cut to the ellipse: the stage in its middle, and at the corners of its box what is behind it",
                      middle >= 0 && middle < 0.05 && corners > 0.8, middle + " in the middle, " + corners + " at the corners")
                check("a thumbnail of the slide draws no picture", Lib.findAll(named("slideGrid"), isPicture).length === 0)
                testInput.grabOutput("6-output")
                testInput.grab("6-operator")
                clearSlide()
                return 300
            },
            () => {
                // ---- A stage screen sent over NDI, its layout's Screen Preview of the output in what goes out,
                // before the stage is switched off and after it is switched on again. (Where NDI's library is here.)
                if (!Ndi.available) {
                    check("(NDI's library is not here, so a stage screen sent over NDI is not tried)", true)
                    return 10
                }
                kept.ndiStage = Screens.stage.find(s => s.id !== Screens.stage[0].id) ?? Screens.stage[0]
                Screens.setOutput(kept.ndiStage.id, { output: "ndi", ndiName: "Test Stage", ndiWidth: 960, ndiHeight: 540, ndiRate: "30" })
                Show.setStageLayout(kept.ndiStage.id, kept.layout.id)
                Show.showMedia({ name: "Red.png", path: media + "/Red.png", source: "file://" + media + "/Red.png", video: false })
                return 3500
            },
            () => {
                if (!Ndi.available)
                    return 10
                const sender = senders[kept.ndiStage.id]
                kept.ndiRed = sender ? testInput.ndiRed(sender) : -1
                check("a stage screen sent over NDI has its Screen Preview of the output in what goes out", sender !== undefined && sender.sending && kept.ndiRed > 0.2,
                      kept.ndiRed)
                stageEnabled = false
                return 800
            },
            () => {
                if (!Ndi.available)
                    return 10
                check("the stage button takes it off the network", senders[kept.ndiStage.id].sending === false)
                stageEnabled = true
                return 2500
            },
            () => {
                if (!Ndi.available)
                    return 10
                const sender = senders[kept.ndiStage.id]
                const red = testInput.ndiRed(sender)
                check("switched on again, what goes out has it again", sender.sending && Math.abs(red - kept.ndiRed) < 0.1, red + " against " + kept.ndiRed)
                Screens.setOutput(kept.ndiStage.id, { output: "none" })
                clearMedia()
                return 300
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
