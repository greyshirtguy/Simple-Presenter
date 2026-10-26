import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A screen's video delay, ProPresenter's video frame delay (src/framedelay.h): set in the Screens settings, by the
// screen's frames, which they say in milliseconds too. A picture of four colours put on the media layer then reaches
// the output so many frames late, and the right way up, while the operator's preview has it at once; taken off, it
// goes late too; with the delay back at none, a change is at once again. Each look at the output is a picture of it
// taken as it would be shown at that moment. Then a screen sent out through a card (one of those the app pretends for
// the tests, SP_TEST_DECKLINK), which is drawn out of sight with its rows the other way up: given a delay, the
// picture handed to the card has the change so many of the card's frames late, and the right way up too.
QtObject {
    id: t

//COMMON
    readonly property string media: "@WORKSPACES@".replace(/WorkSpaces$/, "media")

    function quarters() {
        return { name: "Quarters.png", path: media + "/Quarters.png", source: "file://" + media + "/Quarters.png", video: false }
    }

    function settingsRows() {
        return Lib.findAll(named("screensSettings"), item => String(item.objectName) === "screenRow")
    }

    function part(name) {
        return Lib.find(settingsRows()[0], item => String(item.objectName) === name)
    }

    function near(colour, rgb) {
        const c = String(colour)
        const parts = [1, 3, 5].map(i => parseInt(c.substring(i, i + 2), 16))
        return parts.every((part, i) => Math.abs(part - rgb[i]) < 60)
    }

    // The four corners of the output's scene (the window has a bar of its own over it), as it would be shown now
    function corners() {
        const scene = delayItem().parent
        return [[0.1, 0.1], [0.9, 0.1], [0.1, 0.9], [0.9, 0.9]].map(q => {
            const p = scene.mapToItem(null, q[0] * scene.width, q[1] * scene.height)
            return testInput.pixel(false, p.x / output.width, p.y / output.height)
        })
    }

    // (Each of these is given the corners looked at, so that a check says what it judged by: each look is a new
    // picture, at a new moment)
    function quartersIn(c) {
        return near(c[0], [255, 0, 0]) && near(c[1], [0, 255, 0]) && near(c[2], [0, 0, 255]) && near(c[3], [255, 255, 255])
    }

    function blackIn(c) {
        return c.every(colour => near(colour, [0, 0, 0]))
    }

    function operatorPreviewAt(fx, fy) {
        const picture = Lib.find(win.contentItem, item => item.namePrefix === "preview" && item.screenId !== undefined)
        const aspect = output.width / output.height
        const w = Math.min(picture.width, picture.height * aspect)
        const h = w / aspect
        const p = picture.mapToItem(null, (picture.width - w) / 2 + fx * w, (picture.height - h) / 2 + fy * h)
        return testInput.windowPixel(p.x, p.y)
    }

    function delayItem() {
        return Lib.find(output.contentItem, item => String(item.objectName) === "frameDelay")
    }

    // The same of the picture last handed to the card ("#aarrggbb" there)
    function cardCorners() {
        return [[0.1, 0.1], [0.9, 0.1], [0.1, 0.9], [0.9, 0.9]].map(p => {
            const c = testInput.deckLinkPixel(senders[kept.card], p[0], p[1])
            return c.length === 9 ? "#" + c.substring(3) : c
        })
    }


    function run() {
        steps = [
            () => {
                transitionDuration = 0
                const c = corners()
                check("(the output, with no delay to begin with: drawn as it always was)", delayItem() !== null && !delayItem().active && blackIn(c), JSON.stringify(c))
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "screens"
                return 600
            },
            () => {
                check("an audience screen's settings have its Video Delay, none to begin with", part("screenVideoDelay") !== null && part("screenVideoDelay").visible
                      && part("screenVideoDelay").text === "0" && Screens.videoDelayPossible, part("screenVideoDelay") ? part("screenVideoDelay").text : "none")
                kept.frame = Screens.frameIntervalOf(Screens.audience[0])
                check("a frame of the screen is a refresh of its display", kept.frame > 5 && kept.frame < 50, kept.frame)
                click(centre(part("screenVideoDelay")))
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("12")
                testInput.key(Qt.Key_Return)
                return 400
            },
            () => {
                check("set to twelve frames, kept as this computer's, and said in milliseconds", Screens.audience[0].videoDelay === 12
                      && part("screenVideoDelayWords").text === "frames, " + Math.round(12 * kept.frame) + " ms", part("screenVideoDelayWords").text)
                check("the screen is delayed", delayItem().active, JSON.stringify([delayItem().frames, delayItem().frameInterval, output.videoDelay, Screens.audience[0].videoDelay, Screens.videoDelayPossible]))
                settingsOpen = false
                return 500
            },
            () => {
                // (As the item has it: a window's Screen in QML has no refresh rate to read)
                kept.delay = 12 * delayItem().frameInterval
                Show.showMedia(quarters())
                kept.shownAt = testInput.now()
                return 40
            },
            () => {
                const at = testInput.now() - kept.shownAt
                const c = corners()
                // (Looked at only while it is well within the delay)
                check("a picture put on the screen is not on the output yet", at > kept.delay - 40 || blackIn(c), Math.round(at) + " ms: " + JSON.stringify(c))
                check("though the operator's preview has it at once", near(operatorPreviewAt(0.1, 0.1), [255, 0, 0]), operatorPreviewAt(0.1, 0.1))
                return Math.round(kept.delay + 250)
            },
            () => {
                const c = corners()
                check("and once the delay is over, it is, the right way up", quartersIn(c), JSON.stringify(c))
                // (A window that floats has a title bar of its own over the scene: the delay's copy goes only where the
                // scene is, under it)
                const barHeight = delayItem().parent.mapToItem(null, 0, 0).y
                const inBar = testInput.pixel(false, 0.1, barHeight / 2 / output.height)
                check("in the screen's own place: a window's title bar is still over it", barHeight > 4 && !near(inBar, [255, 0, 0]) && !near(inBar, [0, 255, 0]),
                      JSON.stringify([barHeight, inBar]))
                clearMedia()
                kept.clearedAt = testInput.now()
                return 40
            },
            () => {
                const at = testInput.now() - kept.clearedAt
                const c = corners()
                check("taken off, it stays on the output for the delay", at > kept.delay - 40 || quartersIn(c), Math.round(at) + " ms: " + JSON.stringify(c))
                return Math.round(kept.delay + 250)
            },
            () => {
                const c = corners()
                check("and then goes", blackIn(c), JSON.stringify(c))
                testInput.grab("1-delayed")
                Screens.setOutput(Screens.audience[0].id, { videoDelay: 0 })
                return 400
            },
            () => {
                check("back at none, the screen is not delayed", !delayItem().active)
                Show.showMedia(quarters())
                return 120
            },
            () => {
                const c = corners()
                check("and a change is on the output at once", quartersIn(c), JSON.stringify(c))
                clearMedia()
                Screens.add("audience")
                return 400
            },
            () => {
                kept.card = Screens.audience[1].id
                Screens.setOutput(kept.card, { output: "decklink", display: "", decklinkOutput: "pretend-1", decklinkName: "DeckLink Duo (1)",
                                               decklinkFormat: "1080p29.97", decklinkAlpha: "none", videoDelay: 6 })
                return 1500
            },
            () => {
                const sender = senders[kept.card]
                const delay = Lib.find(scenes[kept.card], item => String(item.objectName) === "frameDelay")
                const c = cardCorners()
                check("a screen sent out through a card can be delayed too, by the card's frames", sender && sender.sending && delay && delay.active
                      && Math.abs(delay.frameInterval - 1001 / 30) < 0.01 && blackIn(c),
                      JSON.stringify([sender ? sender.sending : null, delay ? delay.active : null, delay ? delay.frameInterval : null, c]))
                kept.cardDelay = 6 * delay.frameInterval
                Show.showMedia(quarters())
                kept.shownAt = testInput.now()
                return 40
            },
            () => {
                const at = testInput.now() - kept.shownAt
                const c = cardCorners()
                check("a picture put on it is not handed to the card yet", at > kept.cardDelay - 60 || blackIn(c), Math.round(at) + " ms: " + JSON.stringify(c))
                return Math.round(kept.cardDelay + 400)
            },
            () => {
                const c = cardCorners()
                check("and then it is, the right way up", quartersIn(c), JSON.stringify(c))
                Screens.setOutput(kept.card, { output: "none", videoDelay: 0 })
                clearMedia()
                return 500
            },
            () => {
                check("nothing went wrong on the way", !/PROBLEM|WARNING|ERROR/.test(testInput.readText(Log.path).split("\n").filter(l => !l.includes("qt.qpa.theme")).join("\n")),
                      testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
