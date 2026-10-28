import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A screen of two outputs drawn as one (ProPresenter's combined arrangement; src/screens.h, OutputScene), with the two
// card outputs the app pretends for the tests (SP_TEST_DECKLINK). In the settings, a second audience screen made
// Combined, two side by side: written to the workspace's file, one screen twice as wide, with a line for its second
// output under its own. Each output given a card output: a picture of four colours on the media layer (red and green
// across the top, blue and white across the bottom) is split between them, the left output showing red over blue at
// its right edge and the right output green over white at its left, each the size of its format; and a change goes
// to both in the same pass of drawing, handed to the card together. The operator's preview of the screen is of the
// whole of it, twice as wide as high again. A slide of 1920 by 1080 is fitted to its height and centred on it, as
// ProPresenter has it (two presentations of the test's own with a block at a slide's left and right edge), on the card
// outputs and in the preview. The second output given a window instead: its window shows the right half.
// Single again: one line, the file's screen of one output, the second output's settings gone.
QtObject {
    id: t

    // The test's pictures (run.py's prepare_quarters_picture)
    readonly property string media: "@WORKSPACES@".replace(/WorkSpaces$/, "media")

//COMMON
    function rows() {
        return Lib.findAll(named("screensSettings"), item => String(item.objectName) === "screenRow" && item.visible)
    }

    function part(row, name) {
        return Lib.find(rows()[row], item => String(item.objectName) === name)
    }

    function combined() {
        return Screens.audience.find(screen => screen.id === kept.id) ?? null
    }

    function sender(index) {
        const screen = combined()
        return screen && screen.parts[index] ? senders[screen.parts[index].key] ?? null : null
    }

    // A pixel of what a card output was last handed, as its colour's channels from 0 to 255
    function cardPixel(index, x, y) {
        const c = String(testInput.deckLinkPixel(sender(index), x, y))
        const channel = i => parseInt(c.substring(3 + 2 * i, 5 + 2 * i), 16)
        return { r: channel(0), g: channel(1), b: channel(2), text: c }
    }

    // A pixel of the operator's preview of the screen, at a share of its width and height
    function previewPixel(x, y) {
        const preview = named("previewSlide")
        const at = preview.mapToItem(null, preview.width * x, preview.height * y)
        const c = String(testInput.windowPixel(at.x, at.y))
        return { r: parseInt(c.substring(1, 3), 16), g: parseInt(c.substring(3, 5), 16), b: parseInt(c.substring(5, 7), 16), text: c }
    }

    function near(pixel, r, g, b) {
        return Math.abs(pixel.r - r) < 60 && Math.abs(pixel.g - g) < 60 && Math.abs(pixel.b - b) < 60
    }

    function quarters() {
        Show.showMedia({ name: "Quarters.png", path: media + "/Quarters.png", source: "file://" + media + "/Quarters.png", video: false })
    }

    function run() {
        steps = [
            () => {
                DeckLink.refresh()
                check("(the two card outputs the app pretends are found)", DeckLink.available && DeckLink.outputs.length === 2)
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "screens"
                Screens.add("audience")
                return 600
            },
            () => {
                kept.id = Screens.audience[1].id
                check("(a second audience screen, of one output: one line)", rows().length === 3 && combined().columns === 1, rows().length)
                click(centre(part(1, "screenArrangement")))
                return 400
            },
            () => {
                check("an audience screen's Arrangement is a menu: Single, or two outputs combined side by side", menu.opened
                      && labels() === "*Single, Combined: two outputs side by side", labels())
                click(menuRow("Combined: two outputs side by side"))
                return 600
            },
            () => {
                const screen = combined()
                check("made Combined, it is one screen of two outputs side by side, twice as wide", screen.columns === 2 && screen.rows === 1
                      && screen.width === 3840 && screen.height === 1080 && screen.parts.length === 2,
                      JSON.stringify([screen.columns, screen.rows, screen.width, screen.height, screen.parts.length]))
                check("its line says so, and a line for its second output is under its own", rows().length === 4
                      && part(1, "screenArrangement").text.startsWith("Combined: two outputs side by side")
                      && Lib.find(rows()[2], item => item.text === "Output 2") !== null && !part(2, "screenName").visible, rows().length)
                check("the second output goes nowhere to begin with", screen.parts[1].output === "none")
                // The first output to the first card output
                click(centre(part(1, "screenOutput")))
                return 400
            },
            () => {
                click(menuRow("DeckLink Duo (1)"))
                return 400
            },
            () => {
                click(menuRow("1080p29.97"))
                return 800
            },
            () => {
                click(centre(part(2, "screenOutput")))
                return 400
            },
            () => {
                click(menuRow("DeckLink Duo (3)"))
                return 400
            },
            () => {
                click(menuRow("1080p29.97"))
                return 1200
            },
            () => {
                const screen = combined()
                check("each output has a card output of its own, kept as this computer's", screen.parts[0].decklinkOutput === "pretend-1"
                      && screen.parts[1].output === "decklink" && screen.parts[1].decklinkName === "DeckLink Duo (3)",
                      JSON.stringify(screen.parts.map(p => [p.output, p.decklinkName, p.decklinkFormat])))
                check("each is drawn at its format's size and handed to the card", sender(0) !== null && sender(1) !== null && sender(0).sending
                      && sender(1).sending && sender(0).width === 1920 && sender(1).width === 1920,
                      sender(0) && sender(1) ? [sender(0).width, sender(1).width, sender(0).problem, sender(1).problem].join(" ") : "none")
                quarters()
                return 1500
            },
            () => {
                const leftTop = cardPixel(0, 0.9, 0.3)
                const leftBottom = cardPixel(0, 0.9, 0.7)
                const rightTop = cardPixel(1, 0.1, 0.3)
                const rightBottom = cardPixel(1, 0.1, 0.7)
                check("the picture is split between them: red over blue at the left one's right edge, green over white at the right one's left",
                      near(leftTop, 255, 0, 0) && near(leftBottom, 0, 0, 255) && near(rightTop, 0, 255, 0) && near(rightBottom, 255, 255, 255),
                      [leftTop.text, leftBottom.text, rightTop.text, rightBottom.text].join(" "))
                check("drawn in the same pass, and handed to the card together", testInput.deckLinkPass(sender(0)) > 0
                      && testInput.deckLinkPass(sender(0)) === testInput.deckLinkPass(sender(1)),
                      testInput.deckLinkPass(sender(0)) + " / " + testInput.deckLinkPass(sender(1)))
                // What the operator sees of it
                settingsOpen = false
                pickPreviewScreen(kept.id)
                return 1200
            },
            () => {
                const preview = named("previewSlide")
                check("the operator's preview of it is of the whole screen, twice as wide as high again", preview !== null && preview.height > 0
                      && Math.abs(preview.width / preview.height - 32 / 9) < 0.1, preview ? preview.width + " x " + preview.height : "none")
                clearMedia()
                openEntry(entry("Left Edge"))
                return 800
            },
            () => {
                goLive(0)
                return 1200
            },
            () => {
                // A slide of 1920 by 1080 is fitted to the screen's height and centred, as
                // ProPresenter has it: on the whole of 3840 by 1080 it is from 960 to 2880, so a
                // block at its left edge (to 660) is in the left output's right half
                const block = cardPixel(0, 0.67, 0.5)
                const outside = cardPixel(0, 0.25, 0.5)
                const other = cardPixel(1, 0.25, 0.5)
                check("a slide of another shape is centred on the whole screen: a block at its left edge is in the left output's right half",
                      near(block, 255, 0, 255) && near(outside, 0, 0, 0) && near(other, 0, 0, 0), [block.text, outside.text, other.text].join(" "))
                check("and so in the operator's preview", near(previewPixel(0.33, 0.5), 255, 0, 255) && near(previewPixel(0.1, 0.5), 0, 0, 0),
                      previewPixel(0.33, 0.5).text + " " + previewPixel(0.1, 0.5).text)
                openEntry(entry("Right Edge"))
                return 800
            },
            () => {
                goLive(0)
                return 1200
            },
            () => {
                const block = cardPixel(1, 0.33, 0.5)
                const outside = cardPixel(1, 0.75, 0.5)
                const other = cardPixel(0, 0.67, 0.5)
                check("and a block at its right edge (from 1260) is in the right output's left half",
                      near(block, 0, 255, 255) && near(outside, 0, 0, 0) && near(other, 0, 0, 0), [block.text, outside.text, other.text].join(" "))
                check("and so in the operator's preview", near(previewPixel(0.67, 0.5), 0, 255, 255) && near(previewPixel(0.9, 0.5), 0, 0, 0),
                      previewPixel(0.67, 0.5).text + " " + previewPixel(0.9, 0.5).text)
                clearSlide()
                quarters()
                settingsOpen = true
                return 600
            },
            () => {
                // The second output in a window of its own instead
                click(centre(part(2, "screenOutput")))
                return 400
            },
            () => {
                click(menuRow("A window of its own"))
                return 1500
            },
            () => {
                const screen = combined()
                const name = "output-" + screen.parts[1].key
                const window = testInput.windowContent(name)
                check("the second output in a window: a window of its own", screen.parts[1].output === "window" && window !== null && sender(1) === null,
                      screen.parts[1].output)
                if (window) {
                    const w = window.width
                    const h = window.height
                    const px = (x, y) => {
                        const c = String(testInput.pixelIn(name, x * w, y * h))
                        return { r: parseInt(c.substring(1, 3), 16), g: parseInt(c.substring(3, 5), 16), b: parseInt(c.substring(5, 7), 16), text: c }
                    }
                    const top = px(0.1, 0.3)
                    const bottom = px(0.1, 0.7)
                    check("which shows the right half: green over white at its left", near(top, 0, 255, 0) && near(bottom, 255, 255, 255), top.text + " " + bottom.text)
                }
                click(centre(part(1, "screenArrangement")))
                return 400
            },
            () => {
                click(menuRow("Single"))
                return 800
            },
            () => {
                const screen = combined()
                check("Single again: one output, one line, the second output's settings gone", screen.columns === 1 && screen.parts.length === 1
                      && screen.width === 1920 && rows().length === 3 && screen.parts[0].decklinkOutput === "pretend-1",
                      JSON.stringify([screen.columns, screen.parts.length, rows().length]))
                Screens.setArrangement(kept.id, 2, 1)
                return 600
            },
            () => {
                check("and made Combined again, its second output starts from nothing", combined().parts[1].output === "none", combined().parts[1].output)
                settingsOpen = false
                return 300
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
