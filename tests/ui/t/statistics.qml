import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Show Performance Statistics On Screen (Screens.showStatistics; OutputScene): off to begin with, and nothing of it on
// the output. Ticked in the Screens settings, the audience screen shows over everything what it is, how many times it
// was drawn in the last second, what it is drawn by, its video delay and what the app can see of the sound's delay;
// drawn on the output, where it is lit, and not in the operator's preview of it. Given a video delay, it says so, and
// is itself shown at once, over the delayed picture and not in it. Unticked, it is gone again.
QtObject {
    id: t

//COMMON
    function statistics(root) {
        return Lib.find(root, item => String(item.objectName) === "statistics")
    }

    // How much of the top left of the output's scene is lit, where the statistics are (the window has a title bar of
    // its own over the scene)
    function litTopLeft() {
        const scene = Lib.find(output.contentItem, item => String(item.objectName) === "frameDelay").parent
        const p = scene.mapToItem(null, 0, 0)
        return testInput.lit(false, p.x / output.width, p.y / output.height, scene.width / 2 / output.width, scene.height / 4 / output.height)
    }

    function previewTopLeftLit() {
        const picture = Lib.find(win.contentItem, item => item.namePrefix === "preview" && item.screenId !== undefined)
        const aspect = output.width / output.height
        const w = Math.min(picture.width, picture.height * aspect)
        const h = w / aspect
        const p = picture.mapToItem(null, (picture.width - w) / 2, (picture.height - h) / 2)
        return testInput.windowLighter(p.x, p.y, w / 2, h / 4, 120)
    }

    function run() {
        steps = [
            () => {
                check("(nothing on the output to begin with, and no statistics)", !Screens.showStatistics && statistics(output.contentItem) === null
                      && litTopLeft() === 0, litTopLeft())
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "screens"
                return 600
            },
            () => {
                const box = named("showStatistics")
                check("the Screens settings have Show Performance Statistics On Screen, off", box !== null && box.visible && !box.checked)
                click(centre(box))
                return 300
            },
            () => {
                check("ticked, it is on", Screens.showStatistics && named("showStatistics").checked)
                settingsOpen = false
                return 1600
            },
            () => {
                const shown = statistics(output.contentItem)
                const text = shown ? shown.text : ""
                check("the audience screen shows them: what it is, how often it was drawn, what by, its delay and the sound's",
                      shown !== null && shown.visible && text.startsWith(Screens.audience[0].name + " · ") && /^[^\n]* · \d+ × \d+\n/.test(text) && /\nDrawn \d+ times? in the last second, at most \d+ ms apart; its own frames come [\d.]+ times a second\n/.test(text)
                      && text.includes("\nDrawn by the refreshes of the display it is on\n") && text.includes("\nVideo delay: none\n") && text.includes("\nSound: "),
                      text.replace(/\n/g, " | "))
                check("drawn on the output, over its top left", litTopLeft() > 0.02, litTopLeft())
                check("and not in the operator's preview of it", statistics(win.contentItem) === null && previewTopLeftLit() === 0, previewTopLeftLit())
                output.width = 960
                output.height = 560
                return 1500
            },
            () => {
                testInput.grabIn("output", "stats-shown")
                output.width = 300
                output.height = 189
                return 1500
            },
            () => {
                testInput.grabIn("output", "stats-small")
                Screens.setOutput(Screens.audience[0].id, { videoDelay: 6 })
                return 1400
            },
            () => {
                const shown = statistics(output.contentItem)
                const delay = Lib.find(output.contentItem, item => String(item.objectName) === "frameDelay")
                check("given a video delay, they say so", shown !== null && delay.active
                      && shown.text.includes("\nVideo delay: 6 frames, " + Math.round(6 * delay.frameInterval) + " ms\n"), shown ? shown.text.replace(/\n/g, " | ") : "")
                check("and are shown over the delayed picture, not in it: at once", litTopLeft() > 0.02, litTopLeft())
                Screens.setOutput(Screens.audience[0].id, { videoDelay: 0 })
                Screens.showStatistics = false
                return 600
            },
            () => {
                check("unticked, they are gone", statistics(output.contentItem) === null && litTopLeft() === 0, litTopLeft())
                const bad = testInput.readText(Log.path).split("\n").filter(line => /PROBLEM|WARNING|ERROR/.test(line) && !line.includes("qt.qpa.theme"))
                check("nothing went wrong on the way", bad.length === 0, bad.slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
