import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Media behaviour: backgrounds carry on and are not restarted, foregrounds play once and
// give way to the next slide; the menus that set which; the icons.
QtObject {
    id: t

    property var steps: []
    property int at: 0
    property int failures: 0
    property int checks: 0
    property var kept: ({})
    property Timer clock: Timer {
        onTriggered: t.next()
    }

    function check(what, ok, detail) {
        ++checks
        if (!ok)
            ++failures
        testInput.say((ok ? "  ok      " : "  FAILED  ") + what + (detail !== undefined ? "   [" + detail + "]" : ""))
    }

    function next() {
        if (at >= steps.length) {
            testInput.say(failures === 0 ? "ALL " + checks + " CHECKS PASSED" : failures + " OF " + checks + " CHECKS FAILED")
            testInput.quit()
            return
        }
        const step = steps[at++]
        let wait = 300
        try {
            const asked = step()
            if (typeof asked === "number")
                wait = asked
        } catch (error) {
            ++failures
            testInput.say("  FAILED  step " + at + " threw: " + error + " " + (error.stack ?? ""))
        }
        clock.interval = wait
        clock.start()
    }

    function entry(name) {
        return documents.find(d => d.name === name && openable(d))
    }

    function centre(item) {
        return item.mapToItem(null, item.width / 2, item.height / 2)
    }

    function click(p, button) {
        testInput.mouse(0, p.x, p.y, 0, button ?? Qt.LeftButton)
        testInput.mouse(2, p.x, p.y, 0, button ?? Qt.LeftButton)
    }

    function labels() {
        return menu.items.map(i => i.header !== undefined ? "[" + i.header + "]" : (i.current ? "*" : "") + (i.label ?? "(note)") + (i.disabled ? "(off)" : "")).join(", ")
    }

    function menuRow(label) {
        const found = Lib.find(menu.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.label === label)
        return found ? centre(found) : null
    }

    function named(name) {
        return Lib.find(win.contentItem, item => item.objectName === name)
    }

    function slideCell(index) {
        const view = named("slideGrid")
        view.positionViewAtIndex(index, GridView.Contain)
        return view.itemAtIndex(index)
    }

    function binCell(test) {
        const view = named("mediaGrid")
        const index = mediaFiles.findIndex(test)
        view.positionViewAtIndex(index, GridView.Contain)
        return view.itemAtIndex(index)
    }

    function player() {
        return output.livePlayer
    }

    function run() {
        const lava = () => mediaFiles.find(m => m.name.startsWith("Lava Blast"))
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                check("the presentation is open", document !== null && document.name === "Move Of God", document ? document.slides.length + " slides" : "")
                const s = document.slides[6]
                check("its slide's media is read as a looping background", s.media !== undefined && s.media.video && !s.media.foreground && s.media.loops && !s.mediaForeground, JSON.stringify([s.mediaName, s.mediaForeground]))
                goLive(6)
                return 3000
            },
            () => {
                check("a slide's background video plays", liveMedia !== null && liveMedia.name.startsWith("Colorflow") && player() !== null
                      && player().playbackState === MediaPlayer.PlayingState && player().position > 300, player() ? player().position + " ms" : "no player")
                check("and loops", player() !== null && player().loops === MediaPlayer.Infinite)
                kept.player = player()
                kept.position = player().position
                goLive(7)
                return 1200
            },
            () => {
                check("a slide without media leaves the background playing", liveIndex === 7 && liveMedia !== null && liveMedia.name.startsWith("Colorflow")
                      && player() === kept.player && player().position > kept.position + 600 && player().playbackState === MediaPlayer.PlayingState,
                      kept.position + " -> " + (player() ? player().position : "none"))
                kept.position = player().position
                goLive(15)
                return 1200
            },
            () => {
                check("a slide with the same background does not start it again", liveIndex === 15 && player() === kept.player
                      && player().position > kept.position + 600, kept.position + " -> " + (player() ? player().position : "none"))
                kept.position = player().position
                // The media bin's copy of the same file, as a background, is the same thing playing.
                goLive(0)
                return 3500
            },
            () => {
                check("a slide with another background replaces it", liveMedia !== null && liveMedia.name.startsWith("Hopeful Horizon Bliss") && player() !== null
                      && player() !== kept.player && player().position < 3600 && player().playbackState === MediaPlayer.PlayingState,
                      liveMedia ? liveMedia.name + " at " + (player() ? player().position : "none") : "none")
                kept.player = player()
                kept.position = player().position
                const same = mediaFiles.find(m => m.name.startsWith("Hopeful Horizon Bliss"))
                check("the media bin has the same file as a background", same !== undefined && !same.foreground && same.loops)
                click(centre(binCell(m => m.id === same.id)))
                return 1200
            },
            () => {
                check("clicking it in the media bin does not start it again either", liveMediaPlaylistId === mediaPlaylistId && player() === kept.player
                      && player().position > kept.position + 600, kept.position + " -> " + (player() ? player().position : "none"))
                // ---- the media bin's menu
                click(centre(binCell(m => m.id === lava().id)), Qt.RightButton)
                return 300
            },
            () => {
                check("a media file's menu offers its behaviour", menu.opened && labels() === "[Behaviour], *Background, Foreground, [Playlist], Remove from Playlist", labels())
                testInput.grab("1-media-menu")
                click(menuRow("Foreground"))
                return 500
            },
            () => {
                check("choosing Foreground makes it one that plays once", lava().foreground === true && lava().loops === false, JSON.stringify([lava().foreground, lava().loops]))
                check("and nothing else in the bin changed", mediaFiles.filter(m => m.foreground).length === 1 && mediaFiles.length === 27)
                const badge = Lib.find(binCell(m => m.id === lava().id), item => item.foreground !== undefined && item.missing !== undefined)
                check("its icon shows a foreground", badge !== null && badge.foreground === true)
                click(centre(binCell(m => m.id === lava().id)), Qt.RightButton)
                return 300
            },
            () => {
                check("the menu ticks Foreground now", labels() === "[Behaviour], Background, *Foreground, [Playlist], Remove from Playlist", labels())
                testInput.key(Qt.Key_Escape)
                // ---- a foreground from the media bin
                goLive(7)
                click(centre(binCell(m => m.id === lava().id)))
                return 3000
            },
            () => {
                check("a foreground video plays, once", liveMedia !== null && liveMedia.foreground && player() !== null && player().loops === 1
                      && player().playbackState === MediaPlayer.PlayingState && player().position > 300, player() ? player().position + " ms, loops " + player().loops : "none")
                check("the slide that was live stays", liveIndex === 7 && !cleared)
                testInput.grab("2-foreground-playing")
                testInput.grabOutput("2-output-foreground")
                goLive(8)
                return 1500
            },
            () => {
                check("the next slide, with no media, stops the foreground", liveIndex === 8 && liveMedia === null && player() === null)
                testInput.grabOutput("3-output-after-foreground")
                // ---- a foreground gives way to a slide's background
                click(centre(binCell(m => m.id === lava().id)))
                return 2500
            },
            () => {
                check("the foreground is back on", liveMedia !== null && liveMedia.foreground && player() !== null)
                goLive(6)
                return 3000
            },
            () => {
                check("a slide with a background replaces a foreground", liveMedia !== null && !liveMedia.foreground && liveMedia.name.startsWith("Colorflow")
                      && player() !== null && player().loops === MediaPlayer.Infinite)
                // ---- playing once: to the end, and there it stays
                click(centre(binCell(m => m.id === lava().id)))
                return 2500
            },
            () => {
                kept.duration = player().duration
                player().position = player().duration - 2500
                return 1200
            },
            () => {
                check("the foreground is near its end", player() !== null && player().position > kept.duration - 2500, player() ? player().position + " of " + kept.duration : "none")
                return 3500
            },
            () => {
                check("at its end a foreground video stops", player() !== null && player().playbackState !== MediaPlayer.PlayingState,
                      player() ? "state " + player().playbackState + " at " + player().position : "none")
                check("and is still what is on the media layer", liveMedia !== null && liveMedia.foreground)
                testInput.grabOutput("4-output-foreground-ended")
                testInput.grab("4-foreground-ended")
                return 1500
            },
            () => {
                testInput.grabOutput("4-output-foreground-ended-later")
                goLive(9)
                return 1200
            },
            () => {
                check("and the next slide clears it", liveMedia === null)
                // ---- put the media bin's file back, through the menu
                click(centre(binCell(m => m.id === lava().id)), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Background"))
                return 500
            },
            () => {
                check("Background makes it a looping background again", lava().foreground === false && lava().loops === true)
                // ---- a slide's menu
                openEntry(entry("Fresh Wind"))
                const cell = slideCell(0)
                const p = centre(cell)
                click(p, Qt.RightButton)
                return 300
            },
            () => {
                check("a slide's menu offers its media's behaviour", menu.opened && /^Edit, Add Action, Remove Action(\(off\))?, \[Media\], \*Background, Foreground, Remove Media, \[Slide\], Copy, Paste\(off\), Delete Slide…$/.test(labels()), labels())
                testInput.grab("5-slide-menu")
                click(menuRow("Foreground"))
                return 600
            },
            () => {
                const s = document.slides[0]
                check("choosing Foreground changes the slide's media, and not how it plays on from its end", s.mediaForeground === true && s.media.foreground === true && s.media.loops === true && s.mediaPlayback === 1)
                check("and no other slide's", document.slides[2].mediaForeground === false && document.slides[2].media.loops === true)
                const badge = Lib.find(slideCell(0), item => item.foreground !== undefined && item.missing !== undefined)
                check("the slide's icon shows a foreground", badge !== null && badge.foreground === true)
                testInput.grab("6-slide-foreground")
                click(centre(slideCell(1)), Qt.RightButton)
                return 300
            },
            () => {
                check("a slide with no media has nothing to set", labels() === "Edit, Add Action, Remove Action(off), [Media], Background(off), Foreground(off), Remove Media(off), [Slide], Copy, Paste(off), Delete Slide…", labels())
                testInput.key(Qt.Key_Escape)
                // ---- the slide's foreground, set to play once, live
                report(catalog.setSlideMediaPlayback(document.path, document.slides[0].id, 0, 0, 0))
                reloadDocument()
                goLive(0)
                return 3000
            },
            () => {
                check("the slide's foreground plays once", liveMedia !== null && liveMedia.foreground && player() !== null && player().loops === 1 && player().position > 300)
                goLive(1)
                return 1500
            },
            () => {
                check("and stops with the next slide", liveIndex === 1 && liveMedia === null)
                // ---- media put on a slide that has none is a background there, whatever it was in the bin
                setMediaItemForeground(lava().id, true)
                assignMedia(1, lava().path)
                return 600
            },
            () => {
                const s = document.slides[1]
                check("media put on a slide with none is a background there, whatever it was in the bin", s.mediaName.startsWith("Lava Blast") && s.mediaForeground === false && s.media.loops === true
                      && lava().foreground === true, JSON.stringify([s.mediaName, s.mediaForeground]))
                setSlideMediaForeground(1, true)
                return 400
            },
            () => {
                check("the slide's can be made a foreground by itself", document.slides[1].mediaForeground === true && document.slides[1].media.loops === true)
                setMediaItemForeground(lava().id, false)
                return 400
            },
            () => {
                check("changing the bin's afterwards leaves the slide's alone", document.slides[1].mediaForeground === true && lava().foreground === false)
                setSlideMediaForeground(1, false)
                return 400
            },
            () => {
                check("and the slide's can be set back by itself", document.slides[1].mediaForeground === false && document.slides[1].media.loops === true && lava().foreground === false)
                return 100
            }
        ]
        next()
    }
}
