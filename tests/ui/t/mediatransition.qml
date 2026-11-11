import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The media bin's own transition: chosen at the bin's foot with the same controls as the slides' under the slides, kept
// in the settings apart from the slides', and what all media comes in by on the media layer, media a slide brings with
// it included, which comes in with the slide; the foot drawn, as tall as the slides' footer, the controls under the
// thumbnails on a faint purple, the slides' on a faint orange, and the size buttons at the foot's right; and, with the
// bin hidden, the media's beside the slides'.
QtObject {
    id: t

//COMMON
    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function pixel(x, y) {
        return String(testInput.windowPixel(x, y)).toLowerCase()
    }

    // The colour drawn just inside a tint's left edge, half way down, as { r, g, b }
    function tintHue(tint) {
        if (tint === null)
            return { r: 0, g: 0, b: 0 }
        const at = tint.mapToItem(null, 2, tint.height / 2)
        const c = pixel(at.x, at.y)
        return { r: parseInt(c.substring(1, 3), 16), g: parseInt(c.substring(3, 5), 16), b: parseInt(c.substring(5, 7), 16) }
    }

    // The one of a name that is shown (the media bin, hidden, keeps its own)
    function shown(name) {
        return Lib.findAll(win.contentItem, item => item.objectName === name && item.visible)[0] ?? null
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
                check("the media bin has a foot, at its bottom, as tall as the slides' footer, which the thumbnails stop at", foot.visible && foot.height === 34
                      && foot.height === named("gridFooter").height && bottomOf(foot) === bottomOf(bin)
                      && bottomOf(grid) <= topOf(foot), foot.height + " " + bottomOf(grid) + " " + topOf(foot) + " " + bottomOf(foot))
                // (Under the thumbnails at their left, as the slides' is under the slides: the maintainer, 2026-11-08)
                const gridLeft = grid.mapToItem(null, 0, 0).x
                check("with the media transition under the thumbnails, at their left, named", button !== null && button.visible && Lib.find(button, item => item.text === "Fade") !== null
                      && button.mapToItem(null, 0, 0).x >= gridLeft && button.mapToItem(null, 0, 0).x < gridLeft + 40 && topOf(button) > topOf(foot) && bottomOf(button) < bottomOf(foot),
                      button ? button.mapToItem(null, 0, 0).x + " from " + gridLeft + "," + topOf(button) : "none")
                // The size of the thumbnails at the foot's right, in the foot, and drawn (the maintainer, 2026-11-11: the
                // buttons had floated over the thumbnails' corner)
                const sizes = Lib.find(foot, item => item.toString().startsWith("ZoomButtons"))
                const fromRight = sizes === null ? -1 : bin.mapToItem(null, bin.width, 0).x - sizes.mapToItem(null, sizes.width, 0).x
                check("and the size buttons in the foot at its right, not over the thumbnails", sizes !== null && sizes.visible && topOf(sizes) > topOf(foot)
                      && bottomOf(sizes) < bottomOf(foot) && Math.abs(fromRight - 16) < 0.5,
                      sizes ? topOf(sizes) + "-" + bottomOf(sizes) + " in " + topOf(foot) + "-" + bottomOf(foot) + ", " + fromRight + " from the right" : "none")
                const minus = sizes === null ? null : sizes.mapToItem(null, 3, sizes.height / 2)
                check("drawn: the − button, solid", minus !== null && pixel(minus.x, minus.y).endsWith("3a3c42"), minus ? pixel(minus.x, minus.y) : "none")
                // A faint purple behind the media's, and a faint orange behind the slides', drawn: told apart side by side
                kept.mediaTint = tintHue(named("mediaTransitionTint"))
                kept.slideTint = tintHue(named("transitionTint"))
                check("drawn on a faint purple, where the slides' is on a faint orange", kept.mediaTint.b > kept.mediaTint.r + 6 && kept.mediaTint.r > kept.mediaTint.g
                      && kept.slideTint.r > kept.slideTint.b + 15 && kept.slideTint.r > kept.slideTint.g + 6,
                      JSON.stringify([kept.mediaTint, kept.slideTint]))
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
                // ---- The media bin hidden: the media's controls beside the slides'
                mediaBinVisible = false
                return 500
            },
            () => {
                const slides = named("transitionButton"), media = shown("mediaTransitionButton")
                check("with the media bin hidden, the media transition is beside the slides', in the bar under the slides, named", media !== null && media.visible
                      && Lib.find(media, item => item.text === "Cut") !== null && Math.abs(topOf(media) - topOf(slides)) < 1
                      && media.mapToItem(null, 0, 0).x > slides.mapToItem(null, slides.width, 0).x,
                      media ? media.mapToItem(null, 0, 0).x + " after " + slides.mapToItem(null, slides.width, 0).x : "none")
                const mediaTint = tintHue(shown("mediaTransitionTint")), slideTint = tintHue(named("transitionTint"))
                check("the two on their tints, side by side", mediaTint.b > mediaTint.r + 6 && slideTint.r > slideTint.b + 15, JSON.stringify([mediaTint, slideTint]))
                testInput.grab("2-side-by-side")
                mediaBinVisible = true
                return 500
            },
            () => {
                check("shown again, the bin has them back, and the bar under the slides has the slides' alone",
                      named("mediaTransitionButton") !== null && named("mediaTransitionButton").visible
                      && named("mediaTransitionButton").mapToItem(null, 0, 0).x >= named("mediaGrid").mapToItem(null, 0, 0).x
                      && Lib.findAll(win.contentItem, item => item.objectName === "mediaTransitionButton").length === 1)
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0,
                      logged(/PROBLEM|WARNING|ERROR/).join(" | "))
            }
        ]
        next()
    }
}
