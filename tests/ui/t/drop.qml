import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Dragging media in: files from another application onto slides, between them and into the media bin, and the same
// out of the media bin; where it lands settles how it plays.
QtObject {
    id: t

//COMMON
    readonly property string made: "@MADE@/"
    readonly property string video: made + "i-with-sound.mp4"
    readonly property string other: made + "b-short-4s.mp4"
    readonly property string picture: made + "j-picture.png"
    readonly property string bars: made + "k-bars.jpg"
    readonly property string notes: made + "notes.txt"

    function spot(item, fx, fy) {
        return item.mapToItem(null, item.width * fx, item.height * fy)
    }

    function dropAreaOf(cell) {
        return Lib.find(cell, item => item.zone !== undefined && item.edge !== undefined)
    }

    function over(point, files, modifiers) {
        return testInput.fileDrag(0, point.x, point.y, files, modifiers ?? 0)
    }

    function dropAt(point, files, modifiers) {
        testInput.fileDrag(0, point.x, point.y, files, modifiers ?? 0)
        return testInput.fileDrag(1, point.x, point.y, files, modifiers ?? 0)
    }

    function ids() {
        return document.slides.map(s => s.id)
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                kept.before = ids()
                kept.plain = document.slides.findIndex(s => s.mediaName === "")
                kept.backed = document.slides.findIndex(s => s.mediaName !== "" && !s.mediaForeground)
                check("a presentation with a slide that triggers no media, and one that has a background", kept.plain >= 0 && kept.backed >= 0, kept.plain + ", " + kept.backed + " of " + kept.before.length)
                kept.cell = slideCell(kept.plain)
                return 300
            },
            () => {
                // Files from another application, over a slide
                const cell = slideCell(kept.plain)
                const area = dropAreaOf(cell)
                check("a video dragged over the middle of a slide can be dropped, and is asked for as a copy", over(spot(cell, 0.5, 0.5), [video]) === "copy" && area.containsDrag && area.zone === "onto", area.zone)
                testInput.grab("1-over-slide")
                check("over its left edge it would go before the slide", over(spot(cell, 0.03, 0.5), [video]) === "copy" && area.zone === "before", area.zone)
                testInput.grab("2-before-slide")
                check("and over its right edge after it", over(spot(cell, 0.97, 0.5), [video]) === "copy" && area.zone === "after", area.zone)
                testInput.grab("3-after-slide")
                check("with Shift held, which asks for a move, it is still only a copy that is taken", over(spot(cell, 0.5, 0.5), [video], Qt.ShiftModifier) === "copy")
                testInput.fileDrag(2, 0, 0)
                check("taken away again, nothing is marked", !area.containsDrag)
                check("a file that is not media cannot be dropped on a slide", over(spot(cell, 0.5, 0.5), [notes]) === "refused" && !area.containsDrag)
                testInput.fileDrag(2, 0, 0)
                check("nor can a folder", over(spot(cell, 0.5, 0.5), [made]) === "refused")
                testInput.fileDrag(2, 0, 0)
                check("nothing has changed yet", ids().join() === kept.before.join() && document.slides[kept.plain].mediaName === "")
                // Dropped onto the slide: its background
                check("dropped onto the slide (Shift held), it is taken as a copy", dropAt(spot(cell, 0.5, 0.5), [video], Qt.ShiftModifier) === "copy")
                return 400
            },
            () => {
                const slide = document.slides[kept.plain]
                check("and the slide triggers it as a background, silent and looping", slide.mediaName === "i-with-sound.mp4" && slide.mediaForeground === false && slide.media !== undefined
                      && slide.media.volume === 0 && slide.media.loops === true && slide.media.path === video, JSON.stringify([slide.mediaName, slide.mediaForeground]))
                check("the slides are otherwise as they were", ids().join() === kept.before.join())
                check("the file is where it was", testInput.exists(video))
                // A second file onto the same slide takes the first one's place and plays as it did
                const cell = slideCell(kept.plain)
                check("several files onto one slide: the first is used", dropAt(spot(cell, 0.5, 0.5), [bars, picture, notes]) === "copy")
                return 400
            },
            () => {
                const slide = document.slides[kept.plain]
                check("which takes the place of what was there, still a background", slide.mediaName === "k-bars.jpg" && slide.mediaForeground === false, slide.mediaName)
                check("and the user is told the others were left", /only the first of those 2 files/.test(notice) && !noticeIsError, notice)
                // Between slides: slides of their own, as foregrounds
                kept.second = document.slides[1].id
                kept.group = document.slides[1].group
                goLive(4)
                kept.live = document.slides[4].id
                const cell = slideCell(1)
                check("two media files and a text file dropped after the second slide", dropAt(spot(cell, 0.97, 0.5), [picture, notes, video]) === "copy")
                return 500
            },
            () => {
                check("make two new slides", document.slides.length === kept.before.length + 2, document.slides.length)
                const first = document.slides[2]
                const second = document.slides[3]
                check("right after it, in the order given", document.slides[1].id === kept.second && first.label === "j-picture.png" && second.label === "i-with-sound.mp4", first.label + ", " + second.label)
                check("each with nothing on it", first.elements.length === 0 && second.elements.length === 0 && first.width === document.slides[1].width)
                check("triggering its file as a foreground", first.mediaForeground === true && first.media.path === picture && second.mediaForeground === true && second.media.path === video)
                check("the video one to be played once, with its sound", second.media.loops === false && second.media.volume === 1 && first.media.volume === 0)
                check("in the group of the slide they follow", first.group === kept.group && second.group === kept.group && !first.groupStart, first.group + " / " + kept.group)
                check("the slides that were there are all still there, in their order", ids().filter(id => kept.before.includes(id)).join() === kept.before.join())
                check("and the slide that is live is still the one marked", liveIndex === 6 && document.slides[liveIndex].id === kept.live && liveDocument.slides.length === document.slides.length, liveIndex)
                check("the user is not told anything is wrong", notice === "" || !noticeIsError, notice)
                kept.afterInsert = ids()
                testInput.grab("4-new-slides")
                // Before the first slide
                const cell = slideCell(0)
                check("a file dropped before the first slide", dropAt(spot(cell, 0.03, 0.5), [bars]) === "copy")
                return 500
            },
            () => {
                check("becomes the first slide", document.slides.length === kept.afterInsert.length + 1 && document.slides[0].label === "k-bars.jpg" && document.slides[0].mediaForeground === true
                      && document.slides[0].groupStart === true && document.slides[1].groupStart === false && document.slides[1].id === kept.afterInsert[0], document.slides[0].label)
                check("with the live slide one further on", liveIndex === 7 && document.slides[liveIndex].id === kept.live, liveIndex)
                // Onto a slide that has a foreground: the new file plays as the old one did
                const cell = slideCell(4)
                check("another video dropped onto the slide that has the foreground video", document.slides[4].label === "i-with-sound.mp4" && dropAt(spot(cell, 0.5, 0.5), [other]) === "copy", document.slides[4].label)
                return 400
            },
            () => {
                const slide = document.slides[4]
                check("takes its place, and is a foreground as that was", slide.mediaName === "b-short-4s.mp4" && slide.mediaForeground === true && slide.media.loops === false && slide.media.volume === 1,
                      JSON.stringify([slide.mediaName, slide.mediaForeground]))
                // Past the last slide
                const view = named("slideGrid")
                view.positionViewAtEnd()
                kept.count = document.slides.length
                kept.lastGroup = document.slides[kept.count - 1].group
                kept.lastId = document.slides[kept.count - 1].id
                testInput.say("    (groups as shown: " + document.slides.filter(s => s.groupStart).map(s => s.group).join(", ") + "; arrangement \"" + document.arrangement + "\")")
                return 300
            },
            () => {
                const view = named("slideGrid")
                const point = view.mapToItem(null, view.width / 2, view.height - 12)
                const last = view.itemAtIndex(kept.count - 1)
                check("a drag over the grid past its last slide can be dropped", over(point, [picture]) === "copy")
                const line = Lib.find(last, item => item.atEnd !== undefined)
                check("and is shown as going after the last slide", line !== null && line.atEnd && line.visible)
                testInput.grab("5-past-the-end")
                check("dropped there", testInput.fileDrag(1, point.x, point.y) === "copy")
                return 500
            },
            () => {
                const last = document.slides[document.slides.length - 1]
                // (This arrangement shows the last group several times over, and a slide added to a group is in each showing of it.)
                const showings = document.slides.filter(s => s.groupStart && s.group === kept.lastGroup).length
                check("it is the last slide, after what was the last, in its group", document.slides.length === kept.count + showings && last.label === "j-picture.png" && last.mediaForeground
                      && last.group === kept.lastGroup && document.slides[document.slides.length - 2].id === kept.lastId,
                      last.label + " in " + last.group + ", after one in " + kept.lastGroup + "; " + (document.slides.length - kept.count) + " more slides for " + showings + " showings")
                // The file as written is one that can be read back, and says the same
                const again = catalog.open(document.path)
                check("read back from its file the presentation says the same", again.error === "" && again.slides.map(s => s.id).join() === ids().join()
                      && again.slides[0].mediaForeground === true && again.slides[0].label === "k-bars.jpg")
                // While the settings are up nothing can be dropped
                settingsOpen = true
                return 300
            },
            () => {
                const cell = slideCell(8)
                check("with the settings screen up, nothing can be dropped on what is behind it", over(spot(cell, 0.5, 0.5), [video]) === "refused")
                testInput.fileDrag(2, 0, 0)
                settingsOpen = false
                return 300
            },
            () => {
                // Out of the media bin with the mouse: onto a slide, and between two
                mediaBinVisible = true
                openMediaPlaylist(catalog.mediaPlaylists.find(n => !n.folder).path)
                return 500
            },
            () => {
                const clip = mediaFiles.find(m => m.video && !m.missing)
                kept.clip = clip
                setMediaItemForeground(clip.id, true)
                return 300
            },
            () => {
                const clip = mediaFiles.find(m => m.id === kept.clip.id)
                check("a file of the media bin set to be a foreground", clip.foreground === true)
                kept.target = document.slides.findIndex(s => s.mediaName === "" && s.elements.length > 0)
                kept.targetId = document.slides[kept.target].id
                const from = centre(binCell(m => m.id === clip.id))
                const to = spot(slideCell(kept.target), 0.5, 0.5)
                drag(from, to)
                return 500
            },
            () => {
                const slide = document.slides.find(s => s.id === kept.targetId)
                check("dragged onto a slide with no media, it is that slide's background all the same", slide.mediaName === kept.clip.name && slide.mediaForeground === false, JSON.stringify([slide.mediaName, slide.mediaForeground]))
                setMediaItemForeground(kept.clip.id, false)
                return 300
            },
            () => {
                kept.count = document.slides.length
                kept.target = document.slides.findIndex(s => s.id === kept.targetId)
                const from = centre(binCell(m => m.id === kept.clip.id))
                const cell = slideCell(kept.target)
                const to = spot(cell, 0.03, 0.5)
                // Stop on the way, to look at what is shown
                testInput.mouse(0, from.x, from.y)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10)
                kept.to = to
                return 300
            },
            () => {
                const area = dropAreaOf(slideCell(kept.target))
                check("the same file, now a background, dragged to the gap before that slide", area.containsDrag && area.zone === "before", area.zone)
                testInput.grab("6-bin-file-between")
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 500
            },
            () => {
                const made = document.slides[kept.target]
                check("gets a slide of its own there, as a foreground", document.slides.length === kept.count + 1 && made.label === kept.clip.name && made.mediaForeground === true
                      && made.elements.length === 0 && document.slides[kept.target + 1].id === kept.targetId, made.label + " " + made.mediaForeground)
                // Files into the media bin
                kept.rows = mediaFiles.map(m => m.id)
                const cell = binCell(m => m.id === kept.rows[1])
                const area = Lib.find(cell, item => item.moving !== undefined)
                check("files dragged over the left half of a thumbnail of the media bin can be dropped", over(spot(cell, 0.2, 0.5), [video, picture]) === "copy" && area.moving && !area.after && area.files)
                testInput.grab("7-into-the-bin")
                check("dropped", testInput.fileDrag(1, spot(cell, 0.2, 0.5).x, spot(cell, 0.2, 0.5).y) === "copy")
                return 600
            },
            () => {
                const names = mediaFiles.map(m => m.name)
                check("they are added to the playlist at that place, in order", mediaFiles.length === kept.rows.length + 2 && mediaFiles[0].id === kept.rows[0] && names[1] === "i-with-sound.mp4"
                      && names[2] === "j-picture.png" && mediaFiles[3].id === kept.rows[1], names.slice(0, 4).join(", "))
                check("as backgrounds, referred to where they are", mediaFiles[1].foreground === false && mediaFiles[1].path === video && mediaFiles[2].path === picture)
                check("text files cannot be dropped there", over(spot(binCell(m => m.id === kept.rows[1]), 0.5, 0.5), [notes]) === "refused")
                testInput.fileDrag(2, 0, 0)
                // On the grid past the thumbnails: the end
                const view = named("mediaGrid")
                view.positionViewAtEnd()
                kept.count = mediaFiles.length
                return 300
            },
            () => {
                const view = named("mediaGrid")
                const point = view.mapToItem(null, view.width - 40, view.height - 8)
                const result = dropAt(point, [bars])
                check("a file dropped past the last thumbnail is taken", result === "copy", result)
                return 600
            },
            () => {
                check("and goes at the end of the playlist", mediaFiles.length === kept.count + 1 && mediaFiles[mediaFiles.length - 1].name === "k-bars.jpg", mediaFiles[mediaFiles.length - 1].name)
                // Onto another playlist in the list
                const others = catalog.mediaPlaylists.filter(n => !n.folder && n.path !== mediaPlaylistId)
                kept.otherList = others[0]
                kept.otherCount = catalog.mediaIn(kept.otherList.path).length
                kept.count = mediaFiles.length
                const list = named("mediaPlaylistList")
                list.positionViewAtIndex(catalog.mediaPlaylists.findIndex(n => n.path === kept.otherList.path), ListView.Contain)
                return 300
            },
            () => {
                const list = named("mediaPlaylistList")
                const row = Lib.find(list.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.path === kept.otherList.path)
                const folder = Lib.find(list.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.folder === true)
                check("files cannot be dropped on a folder of the media bin's list", folder === null || over(spot(folder, 0.5, 0.5), [video]) === "refused")
                testInput.fileDrag(2, 0, 0)
                check("nor text files on a playlist", over(spot(row, 0.5, 0.5), [notes]) === "refused")
                testInput.fileDrag(2, 0, 0)
                check("media files can, onto a playlist that is not the one open", over(spot(row, 0.5, 0.5), [video, notes, picture]) === "copy")
                testInput.grab("8-onto-a-playlist")
                check("dropped", testInput.fileDrag(1, spot(row, 0.5, 0.5).x, spot(row, 0.5, 0.5).y) === "copy")
                return 600
            },
            () => {
                const there = catalog.mediaIn(kept.otherList.path)
                check("they are at the end of that playlist", there.length === kept.otherCount + 2 && there[there.length - 2].name === "i-with-sound.mp4" && there[there.length - 1].name === "j-picture.png", there.length)
                check("the playlist that is open is as it was", mediaFiles.length === kept.count)
                check("and the user is told, in words, what went where and what was left out", /2 files added to .*; 1 left out/.test(notice) && !noticeIsError, notice)
                check("the log has a line for each drop", logged(/  drop  /).length === 11, logged(/  drop  /).length + ": " + logged(/  drop  /).map(l => l.slice(26, 110)).join(" | "))
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0, logged(/PROBLEM|WARNING|ERROR/).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
