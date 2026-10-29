import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Export to PowerPoint (PptxExport, ExportPowerPoint.qml), on a presentation of the test's
// own (run.py's prepare_pptx: Verse, two slides of made-up words, the first with notes and
// a picture behind it; Chorus, one slide, which puts up a picture that is not there; an
// arrangement, Chorus First). A right click on its row has Export to PowerPoint…, which
// puts up the panel, drawn over the window, with ProToPPTX's options, an option changed
// being kept; exported (into the test's own folder, the file dialog passed over), it says
// how many slides went where and lists what could not be carried over (the missing
// picture); and the file is a PowerPoint file: its slides, their words, the picture behind
// the first slide as its background, kept on the next, the notes, the groups as sections.
// The log says so. Exported again in the order of the arrangement, the chorus comes first.
QtObject {
    id: t

    readonly property string exported: "@WORKSPACES@".replace(/WorkSpaces$/, "Export Test.pptx")
    readonly property string withVideo: "@WORKSPACES@".replace(/WorkSpaces$/, "Yeshua.pptx")

//COMMON
    function row(name) {
        const list = named("presentationList")
        for (let i = 0; i < list.count; ++i) {
            if (list.model[i].name === name) {
                list.positionViewAtIndex(i, ListView.Contain)
                return list.itemAtIndex(i)
            }
        }
        return null
    }

    function panel() {
        return Lib.find(contentItem, item => item.stage !== undefined && item.exportTo !== undefined)
    }

    function part(name) {
        return named(name)
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function slideText(n) {
        return testInput.zipText(exported, "ppt/slides/slide" + n + ".xml")
    }

    function run() {
        steps = [
            () => {
                check("(the test's presentation is in the library)", row("Export Test") !== null)
                click(centre(row("Export Test")), Qt.RightButton)
                return 500
            },
            () => {
                check("a presentation's right-click menu has Export to PowerPoint…", menu.opened && labels().includes("Export to PowerPoint…"), labels())
                // (Looked for once to scroll it into sight, and again where it then is: in a
                // menu this long the row moves, by 300 pixels, once the menu has scrolled)
                menuRow("Export to PowerPoint…")
                return 400
            },
            () => {
                click(menuRow("Export to PowerPoint…"))
                return 800
            },
            () => {
                const loader = part("exportPowerPointLoader")
                check("(its panel is made the first time it is asked for)", loader !== null && loader.status === Loader.Ready,
                      loader ? loader.status + " " + loader.item : "no loader")
                const shown = part("exportPowerPointPanel")
                check("which puts up its panel", panel() !== null && panel().open && shown !== null && shown.visible)
                if (shown) {
                    // Drawn: the panel's colour at its middle, in the window
                    const at = shown.mapToItem(null, shown.width / 2, shown.height - 8)
                    check("drawn over the window", String(testInput.windowPixel(at.x, at.y)) === "#2b2d31", String(testInput.windowPixel(at.x, at.y)))
                }
                check("with ProToPPTX's options, as they start", part("pptxCarryBackground").checked && part("pptxSkipDisabled").checked
                      && part("pptxNotes").checked && part("pptxSections").checked && part("pptxArrangement").checked
                      && part("pptxBackgroundPlacement").text.startsWith("As the slide's background") && part("pptxVideos").text.startsWith("Put the video in"))
                testInput.grab("pptx-1-options")
                part("pptxSections").toggled(false)
                return 300
            },
            () => {
                check("an option changed is kept for the next time", PptxExport.options().groupsAsSections === false && !part("pptxSections").checked)
                part("pptxSections").toggled(true)
                // Into the test's own folder: the file dialog passed over
                panel().exportTo(exported)
                return 300
            },
            () => {
                check("while it exports, it says how far it has got, with a Stop", panel().stage === "exporting" || panel().stage === "done",
                      panel().stage)
                return 3000
            },
            () => {
                const result = panel().result
                check("done, it says how many slides went where", panel().stage === "done" && result && result.ok && result.slides === 3
                      && part("pptxResult").text === "3 slides written to " + exported, part("pptxResult") ? part("pptxResult").text : "none")
                const notes = Lib.findAll(part("exportPowerPointPanel"), item => String(item.objectName) === "pptxNote").map(item => item.text)
                check("and what could not be carried over: the picture that is not there, on its slide", notes.some(n => n.includes("Not There.png") && n.includes("slide 3")),
                      JSON.stringify(notes))
                const entries = testInput.zipEntries(exported)
                check("the file is a PowerPoint file of three slides, with the notes of the first", ["[Content_Types].xml", "ppt/presentation.xml",
                      "ppt/slides/slide1.xml", "ppt/slides/slide2.xml", "ppt/slides/slide3.xml", "ppt/notesSlides/notesSlide1.xml"].every(e => entries.includes(e))
                      && !entries.includes("ppt/slides/slide4.xml"), JSON.stringify(entries))
                check("its slides' words, in Master's order", slideText(1).includes("<a:t>Made up words one</a:t>") && slideText(2).includes("<a:t>Made up words two</a:t>")
                      && slideText(3).includes("<a:t>The chorus words</a:t>"))
                check("the picture behind the first slide as its background, and kept on the next", slideText(1).includes("<p:bg><p:bgPr><a:blipFill")
                      && slideText(2).includes("<p:bg><p:bgPr><a:blipFill") && entries.includes("ppt/media/image1.png"))
                const presentation = testInput.zipText(exported, "ppt/presentation.xml")
                check("the groups as sections", presentation.includes("<p14:section name=\"Verse\"") && presentation.includes("<p14:section name=\"Chorus\""))
                check("and the notes", testInput.zipText(exported, "ppt/notesSlides/notesSlide1.xml").includes("A note of the first"))
                testInput.grab("pptx-2-done")
                check("the log says so", logged(/exported to PowerPoint as .*Export Test\.pptx: 3 slides/).length === 1, logged(/PowerPoint/).join(" | "))
                click(centre(part("pptxDone")))
                return 400
            },
            () => {
                check("Done closes the panel", !panel().open && !part("exportPowerPointPanel").visible)
                // In the order of an arrangement chosen for the presentation
                setArrangement(entry("Export Test"), "Chorus First")
                return 800
            },
            () => {
                click(centre(row("Export Test")), Qt.RightButton)
                return 400
            },
            () => {
                menuRow("Export to PowerPoint…")
                return 400
            },
            () => {
                click(menuRow("Export to PowerPoint…"))
                return 400
            },
            () => {
                check("the panel says the arrangement it follows", Lib.find(part("exportPowerPointPanel"), item => String(item.text ?? "").includes("“Chorus First”")) !== null)
                panel().exportTo(exported)
                return 3000
            },
            () => {
                check("exported again, over the file there, in its order: the chorus first", panel().result && panel().result.ok
                      && slideText(1).includes("<a:t>The chorus words</a:t>") && slideText(2).includes("<a:t>Made up words one</a:t>"),
                      panel().result ? JSON.stringify(panel().result) : "none")
                check("nothing left beside it", testInput.readText(exported + ".partial") === "")
                click(centre(part("pptxDone")))
                setArrangement(entry("Export Test"), "")
                return 300
            },
            () => {
                // A presentation of the Demo library's with a video behind its slides (as the
                // menu does: see above)
                exportPowerPoint(entry("Yeshua"))
                return 400
            },
            () => {
                panel().exportTo(withVideo)
                kept.tries = 0
                return 500
            },
            () => {
                // (Waited for, as long as copying the video into the file takes)
                if (panel().stage !== "done" && kept.tries++ < 80) {
                    --at
                    return 500
                }
                const result = panel().result
                const entries = testInput.zipEntries(withVideo)
                const video = entries.find(e => /^ppt\/media\/media\d+\.(mp4|mov|m4v)$/.test(e))
                const slides = entries.filter(e => /^ppt\/slides\/slide\d+\.xml$/.test(e)).map(e => testInput.zipText(withVideo, e))
                check("its video goes into the file, with a still, and starts by itself when its slide comes up", result && result.ok && video !== undefined
                      && slides.some(xml => xml.includes("<a:videoFile") && xml.includes("playFrom(0.0)")),
                      JSON.stringify([result ? result.ok : null, video, entries.length]))
                check("no video of it is said to be unreadable", result && !result.notes.some(n => n.message.includes("could not be read")),
                      result ? JSON.stringify(result.notes) : "none")
                click(centre(part("pptxDone")))
                return 300
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
