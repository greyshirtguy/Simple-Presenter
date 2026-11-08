import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The maintainer's asks of 2026-11-08, after running services: the media icon drawn as ProPresenter draws it (the
// foreground's layer at the top left, the background's at the bottom right); a slide's background video muted, with a
// speaker struck through to say so, whose click has it heard (at once, while it plays) and mutes it again, kept in the
// workspace's own file and not in ProPresenter's; media dropped on a slide labelling the slide with the file's name, and
// in place of media there taking on how that played; and a slide going live in the bottom row in view bringing the row
// under it into view.
QtObject {
    id: t

//COMMON
    readonly property string made: "@MADE@/"
    readonly property string video: made + "i-with-sound.mp4"
    readonly property string other: made + "b-short-4s.mp4"
    readonly property string bars: made + "k-bars.jpg"

    function spot(item, fx, fy) {
        return item.mapToItem(null, item.width * fx, item.height * fy)
    }

    function dropAt(point, files) {
        testInput.fileDrag(0, point.x, point.y, files, 0)
        return testInput.fileDrag(1, point.x, point.y, files, 0)
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // An icon of a slide's thumbnail, by its name ("mediaBadge", "playbackBadge", "soundBadge"); null if it has none
    function badge(index, name) {
        const cell = named("slideGrid").itemAtIndex(index)
        return cell ? Lib.find(cell, item => item.objectName === name) : null
    }

    // The red of the lightest of the nine pixels round a point of an item, as the operator window draws it: what is
    // drawn in the icons' ink is light (some 230), the icons' grey is dark (56)
    function lightest(item, x, y) {
        const at = item.mapToItem(null, x, y)
        let most = 0
        for (let dx = -1; dx <= 1; ++dx) {
            for (let dy = -1; dy <= 1; ++dy)
                most = Math.max(most, parseInt(String(testInput.windowPixel(at.x + dx, at.y + dy)).substring(1, 3), 16))
        }
        return most
    }

    // What hands the sound of the video `p` plays to the mixer, if anything does
    function feedOf(p) {
        const shown = Lib.findAll(output.contentItem, item => item.feed !== undefined && item.player === p)
        return shown.length > 0 ? shown[0].feed : null
    }

    function soundFile() {
        const path = catalog.workspacePath + "/SimplePresenter/Background Sound.json"
        return testInput.exists(path) ? testInput.readText(path) : ""
    }

    // Whether a cell of the slides' grid is wholly in view, by the grid's own place (the test's slideCell would
    // scroll the grid to it)
    function inView(index) {
        const view = named("slideGrid")
        const cell = view.itemAtIndex(index)
        return cell !== null && cell.y >= view.contentY - 0.5 && cell.y + cell.height <= view.contentY + view.height + 0.5
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                mediaTransitionDuration = 0
                // The icons as solid as they go, for their colours to be read
                actionIconOpacity = 1
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                kept.plain = document.slides.findIndex(s => s.mediaName === "")
                kept.backed = document.slides.findIndex(s => s.mediaName !== "" && !s.mediaForeground)
                check("a presentation with a slide that triggers no media, and one that has a background", kept.plain >= 0 && kept.backed >= 0,
                      kept.plain + ", " + kept.backed)
                slideCell(kept.backed)
                return 400
            },
            () => {
                // ---- The media icon, as ProPresenter draws it
                const icon = badge(kept.backed, "mediaBadge")
                const layers = icon ? Lib.find(icon, item => item.foreground !== undefined && item.missing !== undefined) : null
                kept.backBehind = layers ? lightest(layers, 16, 13) : -1
                kept.backFront = layers ? lightest(layers, 5, 5) : -1
                check("a background's icon has its layer solid at the bottom right, behind the one at the top left", layers !== null && !layers.foreground
                      && kept.backBehind > 200 && kept.backFront < 120, kept.backBehind + " " + kept.backFront)
                report(catalog.setSlideMediaForeground(document.path, document.slides[kept.backed].id, true))
                reloadDocument()
                slideCell(kept.backed)
                return 400
            },
            () => {
                const icon = badge(kept.backed, "mediaBadge")
                const layers = icon ? Lib.find(icon, item => item.foreground !== undefined && item.missing !== undefined) : null
                const front = layers ? lightest(layers, 5, 5) : -1
                const behind = layers ? lightest(layers, 16, 13) : -1
                check("a foreground's has its layer solid at the top left, in front, and the one behind in outline", layers !== null && layers.foreground
                      && front > 200 && behind < 160, front + " " + behind)
                check("and a foreground video has no speaker: its sound is always played", badge(kept.backed, "soundBadge") === null
                      || !badge(kept.backed, "soundBadge").visible)
                report(catalog.setSlideMediaForeground(document.path, document.slides[kept.backed].id, false))
                // ---- A video dropped onto a slide that has no media
                kept.labelWas = document.slides[kept.plain].label
                const cell = slideCell(kept.plain)
                check("a video with sound dropped onto a slide with no media", dropAt(spot(cell, 0.5, 0.5), [video]) === "copy")
                return 500
            },
            () => {
                const slide = document.slides[kept.plain]
                check("is its background, silent", slide.mediaName === "i-with-sound.mp4" && !slide.mediaForeground && slide.media.volume === 0
                      && slide.mediaSound === undefined, JSON.stringify([slide.mediaName, slide.mediaForeground, slide.media.volume]))
                check("and the slide is labelled with the file's name", slide.label === "i-with-sound.mp4", "was " + JSON.stringify(kept.labelWas)
                      + ", now " + JSON.stringify(slide.label))
                const sound = badge(kept.plain, "soundBadge"), playback = badge(kept.plain, "playbackBadge")
                check("its thumbnail has a speaker, under the icon of how it plays on from its end", sound !== null && sound.visible && playback !== null
                      && Math.abs(sound.mapToItem(null, 0, 0).x - playback.mapToItem(null, 0, 0).x) < 1
                      && Math.abs(sound.mapToItem(null, 0, 0).y - (playback.mapToItem(null, 0, 0).y + 21)) < 1)
                const glyph = sound ? Lib.find(sound, item => item.muted !== undefined) : null
                kept.strike = glyph ? lightest(glyph, 1.5, 10.7) : -1
                check("struck through, since a background is muted: the line is drawn at the speaker's bottom left", glyph !== null && glyph.muted
                      && kept.strike > 150, kept.strike)
                kept.hash = testInput.fileHash(document.path)
                goLive(kept.plain)
                return 1500
            },
            () => {
                kept.player = player()
                check("live, the background plays and is not heard", kept.player !== null && kept.player.playbackState === MediaPlayer.PlayingState
                      && feedOf(kept.player) === null && kept.player.audioTracks.length === 1 && !AudioMeter.sounding)
                check("the log says it is muted, as a background is", logged(/i-with-sound.mp4.*its sound muted, as a background's is/).length === 1)
                kept.position = kept.player.position
                // ---- A plain click on the speaker: heard, at once
                click(centre(badge(kept.plain, "soundBadge")))
                return 1200
            },
            () => {
                const slide = document.slides[kept.plain]
                check("a click on the speaker has the background heard", slide.mediaSound === true && slide.media.volume === 1 && !slide.mediaForeground,
                      JSON.stringify([slide.mediaSound, slide.media.volume]))
                check("at once, as it plays: the same playing, gone on from where it was, now handed to the mixer at full",
                      player() === kept.player && kept.player.position > kept.position && feedOf(kept.player) !== null
                      && Math.abs(feedOf(kept.player).gain - 1) < 0.001 && Show.liveMedia.volume === 1,
                      player() === kept.player ? kept.player.position + " " + (feedOf(kept.player) ? feedOf(kept.player).gain : "no feed") : "another player")
                check("and it is heard: the meters beside the preview show it", AudioMeter.sounding && Math.max(AudioMeter.left, AudioMeter.right) > 0.05,
                      AudioMeter.left + " " + AudioMeter.right)
                check("the click took nothing live and cleared nothing: the slide is the live one still", liveIndex === kept.plain && !cleared)
                const glyph = Lib.find(badge(kept.plain, "soundBadge"), item => item.muted !== undefined)
                const strike = lightest(glyph, 1.5, 10.7)
                check("the speaker is drawn without its line", !glyph.muted && strike < 110, strike)
                check("kept in the workspace's own file, by the presentation and the slide", soundFile().includes(slide.id)
                      && soundFile().includes(document.uuid) && soundFile().includes("Libraries/"), soundFile().slice(0, 200))
                check("and not in ProPresenter's: the presentation's file is as it was", testInput.fileHash(document.path) === kept.hash)
                check("the log says so", logged(/edit +the background video of slide \d+ of "Move Of God" played with its sound/).length === 1)
                click(centre(badge(kept.plain, "soundBadge")), Qt.RightButton)
                return 300
            },
            () => {
                check("a right click on the speaker has its menu, the way it is ticked", labels() === "[Background Sound], Muted, *Played", labels())
                click(menuRow("Muted"))
                return 800
            },
            () => {
                const slide = document.slides[kept.plain]
                // (Its sound, once made, is kept for the rest of the playing, at no gain: see MediaContent)
                check("muted from the menu, it is not heard, at once", slide.mediaSound === undefined && slide.media.volume === 0
                      && player() === kept.player && feedOf(kept.player) !== null && feedOf(kept.player).gain === 0 && Show.liveMedia.volume === 0,
                      feedOf(kept.player) ? feedOf(kept.player).gain : "no feed")
                check("and the file has it no more", !soundFile().includes(slide.id) && soundFile().includes("\"played\""), soundFile().slice(0, 200))
                // ---- The speaker of a slide that is not live
                kept.other = document.slides.findIndex((s, i) => i !== kept.plain && s.mediaName === "")
                goLive(kept.other)
                return 600
            },
            () => {
                check("(another slide, without media, is live over the background)", liveIndex === kept.other && player() === kept.player)
                click(centre(badge(kept.plain, "soundBadge")))
                return 900
            },
            () => {
                check("a click on the speaker of a slide that is not live takes it no more live than that", liveIndex === kept.other
                      && document.slides[kept.plain].mediaSound === true)
                check("and the background it brought, playing on, is heard", player() === kept.player && feedOf(kept.player) !== null
                      && Math.abs(feedOf(kept.player).gain - 1) < 0.001, feedOf(kept.player) ? feedOf(kept.player).gain : "no feed")
                // ---- New media on old takes on how the old played
                report(catalog.setSlideMediaPlayback(document.path, document.slides[kept.plain].id, 2, 3, 0))
                reloadDocument()
                const cell = slideCell(kept.plain)
                check("another video dropped onto that slide, set to play three times and heard", dropAt(spot(cell, 0.5, 0.5), [other]) === "copy")
                return 500
            },
            () => {
                const slide = document.slides[kept.plain]
                check("takes the old one's place, labelled with its own name", slide.mediaName === "b-short-4s.mp4" && slide.label === "b-short-4s.mp4", slide.label)
                check("and plays as the old did: a background, three times, heard", !slide.mediaForeground && slide.mediaPlayback === 2 && slide.mediaLoopCount === 3
                      && slide.mediaSound === true && slide.media.volume === 1, JSON.stringify([slide.mediaForeground, slide.mediaPlayback, slide.mediaLoopCount, slide.mediaSound]))
                report(catalog.setSlideMediaForeground(document.path, slide.id, true))
                reloadDocument()
                const cell = slideCell(kept.plain)
                check("a picture dropped onto it once it is a foreground", dropAt(spot(cell, 0.5, 0.5), [bars]) === "copy")
                return 500
            },
            () => {
                const slide = document.slides[kept.plain]
                check("is a foreground as that was", slide.mediaName === "k-bars.jpg" && slide.mediaForeground === true && slide.label === "k-bars.jpg"
                      && badge(kept.plain, "soundBadge") === null, JSON.stringify([slide.mediaName, slide.mediaForeground, slide.label]))
                const cell = slideCell(kept.plain)
                check("and a video dropped onto the picture", dropAt(spot(cell, 0.5, 0.5), [video]) === "copy")
                return 500
            },
            () => {
                const slide = document.slides[kept.plain]
                check("is a foreground too, stopping at its end as a foreground does, a picture having no way of playing on to give it",
                      slide.mediaName === "i-with-sound.mp4" && slide.mediaForeground === true && slide.mediaPlayback === 0 && slide.media.volume === 1,
                      JSON.stringify([slide.mediaForeground, slide.mediaPlayback]))
                // Its media taken off: what was kept for its background's sound goes with it
                assignMedia(kept.plain, "")
                return 300
            },
            () => {
                check("media taken off a slide takes its background's sound out of the file", document.slides[kept.plain].mediaName === ""
                      && !soundFile().includes(document.slides[kept.plain].id), soundFile().slice(0, 200))
                clearAll()
                // ---- The grid, brought on a row at a slide in its bottom row
                const view = named("slideGrid")
                view.positionViewAtBeginning()
                kept.columns = view.columns
                kept.count = document.slides.length
                return 300
            },
            () => {
                const view = named("slideGrid")
                // The last slide wholly in view that has a row under it
                kept.bottom = -1
                for (let i = 0; i < kept.count - kept.columns; ++i) {
                    if (inView(i))
                        kept.bottom = i
                }
                check("(a presentation of more rows than the grid shows)", kept.bottom >= kept.columns && !inView(kept.bottom + kept.columns),
                      kept.bottom + " of " + kept.count + ", " + kept.columns + " a row")
                kept.top = view.contentY
                goLive(0)
                return 300
            },
            () => {
                check("a slide going live in the top row leaves the grid where it was", named("slideGrid").contentY === kept.top)
                goLive(kept.bottom)
                return 400
            },
            () => {
                check("a slide going live in the bottom row in view brings the row under it into view, and stays in view itself",
                      inView(kept.bottom) && inView(kept.bottom + kept.columns) && named("slideGrid").contentY > kept.top,
                      named("slideGrid").contentY + " from " + kept.top)
                testInput.grab("1-bottom-row")
                clearAll()
                return 300
            },
            () => {
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0,
                      logged(/PROBLEM|WARNING|ERROR/).join(" | "))
            }
        ]
        next()
    }
}
