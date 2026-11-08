import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The media bin's own transition: chosen at the bin's foot with the same controls as the slides' under the slides, kept
// in the settings apart from the slides', and what all media comes in by on the media layer, media a slide brings with
// it included, which comes in with the slide; the foot drawn.
QtObject {
    id: t

//COMMON
    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function pixel(x, y) {
        return String(testInput.windowPixel(x, y)).toLowerCase()
    }

    function topOf(item) {
        return item.mapToItem(null, 0, 0).y
    }

    function bottomOf(item) {
        return item.mapToItem(null, 0, item.height).y
    }

    // The lines of the log that say a layer began a transition: "media layer: Fade over 0.20 s, ..." (the line that
    // says it is done is not one)
    function began(layer) {
        return logged(new RegExp("transition +" + layer + ": .* over "))
    }

    // The transition the last change of a layer was made by, from those
    function lastOf(layer) {
        const lines = began(layer)
        return lines.length === 0 ? "" : lines[lines.length - 1].replace(new RegExp("^.*" + layer + ": "), "").replace(/ over .*$/, "")
    }

    function run() {
        steps = [
            () => {
                // Dissolve for the slides, Fade for the bin, both short
                selectTransition("Dissolve")
                transitionDuration = 0.3
                selectMediaTransition("Fade")
                mediaTransitionDuration = 0.2
                return 400
            },
            () => {
                check("the slides' transition and the media bin's are chosen apart", transition.name === "Dissolve" && mediaTransition.name === "Fade"
                      && transitionDuration === 0.3 && mediaTransitionDuration === 0.2, transition.name + " / " + mediaTransition.name)
                // (The slides' is Dissolve from the start, so nothing was saved for it: its key is absent or not Fade)
                check("and kept apart in the settings", String(settings.value("transition", "")) !== "Fade" && String(settings.value("mediaTransition")) === "Fade"
                      && Number(settings.value("mediaTransitionDuration")) === 0.2, settings.value("mediaTransition") + " " + settings.value("mediaTransitionDuration"))
                const foot = named("mediaFooter"), bin = foot.parent, grid = named("mediaGrid"), button = named("mediaTransitionButton")
                check("the media bin has a foot, at its bottom, which the thumbnails stop at", foot.visible && foot.height === 30 && bottomOf(foot) === bottomOf(bin)
                      && bottomOf(grid) <= topOf(foot), foot.height + " " + bottomOf(grid) + " " + topOf(foot) + " " + bottomOf(foot))
                check("with the bin's transition at its left, named", button !== null && button.visible && Lib.find(button, item => item.text === "Fade") !== null
                      && button.mapToItem(null, 0, 0).x < bin.mapToItem(null, 0, 0).x + 30 && topOf(button) > topOf(foot) && bottomOf(button) < bottomOf(foot),
                      button ? button.mapToItem(null, 0, 0).x + "," + topOf(button) : "none")
                const line = foot.mapToItem(null, foot.width / 2, 0)
                check("drawn: the line over the foot, and the button", pixel(line.x, line.y + 0.5).endsWith("3a3c42")
                      && pixel(button.mapToItem(null, 4, button.height / 2).x, button.mapToItem(null, 4, button.height / 2).y).endsWith("3a3c42"),
                      pixel(line.x, line.y + 0.5) + " " + pixel(button.mapToItem(null, 4, button.height / 2).x, button.mapToItem(null, 4, button.height / 2).y))
                check("the slides' controls are the slides' still", named("transitionButton") !== null && Lib.find(named("transitionButton"), item => item.text === "Dissolve") !== null)
                openEntry(entry("Move Of God"))
                return 600
            },
            () => {
                check("(a song whose first slide brings a background)", document !== null && document.name === "Move Of God" && document.slides[0].media !== undefined)
                kept.mediaLines = began("media layer").length
                click(centre(slideCell(0)))
                return 900
            },
            () => {
                // (The maintainer, 2026-11-08: the media transition is for a slide's media too; it had been the slide's)
                check("a slide's media comes in with the slide, by the media transition, the slide by the slides'", liveIndex === 0 && liveMedia !== null
                      && began("media layer").length === kept.mediaLines + 1 && lastOf("media layer") === "Fade" && lastOf("slide layer") === "Dissolve",
                      lastOf("slide layer") + " / " + lastOf("media layer"))
                // A picture from the bin: by the bin's
                const cell = binCell(m => !m.video)
                check("(a picture in the bin)", cell !== null)
                kept.fades = logged(/transition +media layer: Fade over 0.20 s/).length
                click(centre(cell))
                return 900
            },
            () => {
                check("media played from the bin comes in by the bin's transition", liveMedia !== null && !liveMedia.video && lastOf("media layer") === "Fade"
                      && logged(/transition +media layer: Fade over 0.20 s/).length === kept.fades + 1, lastOf("media layer"))
                check("and the slide layer was left alone", liveIndex === 0 && lastOf("slide layer") !== "Fade")
                // The bin's menu, from its foot
                click(centre(named("mediaTransitionButton")))
                return 500
            },
            () => {
                check("a click on the bin's transition opens the menu of them, the bin's ticked", menu.opened && menu.items.some(i => i.label === "Fade" && i.current)
                      && !menu.items.some(i => i.label === "Dissolve" && i.current), menu.items.filter(i => i.current).map(i => i.label).join())
                click(menuRow("Cut"))
                return 500
            },
            () => {
                check("a row of it chooses the bin's, and not the slides'", mediaTransition.name === "Cut" && transition.name === "Dissolve"
                      && Lib.find(named("mediaTransitionButton"), item => item.text === "Cut") !== null, mediaTransition.name + " / " + transition.name)
                kept.mediaLines = began("media layer").length
                kept.media = liveMedia
                const cell = binCell(m => !m.video && m.path !== kept.media.path)
                check("(another picture)", cell !== null)
                click(centre(cell))
                return 700
            },
            () => {
                check("set to Cut, the next picture cuts in: no transition is made", liveMedia !== null && liveMedia.path !== kept.media.path
                      && began("media layer").length === kept.mediaLines, began("media layer").length - kept.mediaLines)
                check("the log said what the bin's was changed to", logged(/transition +chosen for the media bin: Cut/).length === 1)
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0,
                      logged(/PROBLEM|WARNING|ERROR/).join(" | "))
            }
        ]
        next()
    }
}
