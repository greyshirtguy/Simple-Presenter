import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A change of slide on an audience screen, whose words are painted before they are shown (src/strokedtext.h,
// src/paintahead.h, TransitionLayer's `paced`), on the Demo workspace's "All Hail King Jesus", on slides with nothing
// on them that moves or starts something:
// - with nothing playing, a slide's words drawn on the output, and in the operator's preview;
// - the slides likely next after a change (the next, the one before, the first of a group) painted before they are
//   asked for, so that each, asked for, is brought in within the click (with no video playing, nothing waits for
//   one): which also says that what is painted ahead is what the slide's words are drawn from, StrokedText::lookOf
//   kept in step with SlideElement, or nothing painted ahead would ever be taken;
// - with a video from the media bin playing under them, a change waiting for the video's next frame (or first for its
//   words, where they were not painted ahead), and then on the output; and two clicks at once bringing in the second,
//   the first put aside before it is given. (Not reached here:
//   a first slide given, its words still being painted, as the second click comes, which TransitionLayer.begin is not
//   to bring in when they are. That is a moment of a few thousandths of a second after a frame of the video, which a
//   script cannot be sure of meeting.)
// - nothing going wrong in the windows' scripts.
QtObject {
    id: t

//COMMON
    function slideLayer() {
        return Lib.find(testInput.windowContent("output"), item => item.name === "slide layer")
    }

    // The id of the slide on show on the output, "" for none
    function shown() {
        const layer = slideLayer()
        return layer && layer.currentItem && layer.currentItem.slide ? layer.currentItem.slide.id : ""
    }

    // A part of the output, as shares of the slide layer, and how much of it is near a colour (as textfills.qml)
    function share(x, y, width, height, colour, tolerance) {
        const content = testInput.windowContent("output")
        const layer = slideLayer()
        const at = layer.mapToItem(null, x * layer.width, y * layer.height)
        return testInput.outputShare(at.x / content.width, at.y / content.height, width * layer.width / content.width,
                                     height * layer.height / content.height, colour, tolerance ?? 60)
    }

    function shareOf(item, x, y, width, height, colour, tolerance) {
        const at = item.mapToItem(null, x * item.width, y * item.height)
        return testInput.windowShare(at.x, at.y, width * item.width, height * item.height, colour, tolerance ?? 60)
    }

    // Whether a slide of the open presentation has nothing that moves or starts something, and words
    function still(index) {
        const slide = document.slides[index]
        return slide && !slide.media && slide.actions.length === 0 && slide.elements.some(e => e.hasText)
               && !slide.elements.some(e => e.scroll !== undefined)
    }

    function fixed(n) {
        return Number(n).toFixed(4)
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries.find(l => l.name === "Demo").path)
                openEntry(entry("All Hail King Jesus"))
                return 700
            },
            () => {
                // Three still slides together, and a group's first slide away from them
                kept.at = -1
                for (let i = 1; i + 2 < document.slides.length && kept.at < 0; ++i) {
                    if (still(i) && still(i + 1) && still(i + 2))
                        kept.at = i
                }
                kept.group = document.slides.findIndex((s, i) => s.groupStart === true && still(i) && (i < kept.at - 1 || i > kept.at + 3))
                check("the song has three still slides together, and a group's first slide apart from them", kept.at >= 0 && kept.group >= 0,
                      kept.at + ", " + kept.group)
                selectTransition("Cut")
                clearMedia()
                goLive(kept.at)
                return 900
            },
            () => {
                check("(nothing playing on the output)", output.liveVideoSink === null)
                check("the slide is on the output", shown() === document.slides[kept.at].id, shown())
                // (Its white letters over black, a line or two: about a thousandth of the screen, where
                // there would be none at all without them)
                const words = share(0, 0, 1, 1, "#ffffff")
                check("its words are drawn on the output", words > 0.0005, fixed(words))
                const preview = named("previewSlide")
                const there = preview ? shareOf(preview, 0, 0, 1, 1, "#ffffff") : -1
                check("and in the operator's preview", there > 0.0005, fixed(there))
                // The next, painted ahead since: brought in within the click
                goLive(kept.at + 1)
                check("the next slide, its words painted ahead, is brought in within the click that asks for it",
                      shown() === document.slides[kept.at + 1].id, shown())
                return 900
            },
            () => {
                goLive(kept.at)
                check("the one before, likewise", shown() === document.slides[kept.at].id, shown())
                return 900
            },
            () => {
                goLive(kept.group)
                check("a group's first slide, likewise", shown() === document.slides[kept.group].id, shown())
                return 900
            },
            () => {
                // A video from the media bin, under the slides
                for (const list of catalog.mediaPlaylists.filter(p => !p.folder)) {
                    openMediaPlaylist(list.path)
                    if (mediaFiles.some(m => m.video))
                        break
                }
                kept.video = mediaFiles.find(m => m.video)
                if (kept.video)
                    showMedia(kept.video, mediaPlaylistId)
                return 2500
            },
            () => {
                check("(a video from the media bin is playing on the output)", output.liveVideoSink !== null, kept.video ? kept.video.name : "none")
                goLive(kept.at + 2)
                // (Waiting in the slide layer for the frame, or, its words not painted ahead, for
                // them first: see OutputScene.showSlide)
                const scene = Lib.find(testInput.windowContent("output"), item => item.paintingFirst !== undefined)
                check("over a playing video, a change waits for the video's next frame",
                      (slideLayer().pacing !== "" || (scene && scene.paintingFirst !== null)) && shown() !== document.slides[kept.at + 2].id,
                      slideLayer().pacing + ", " + (scene && scene.paintingFirst !== null) + ", " + shown())
                return 400
            },
            () => {
                check("and then it is on the output", shown() === document.slides[kept.at + 2].id, shown())
                // Two clicks at once: the first put aside for the second before it is given
                goLive(kept.at)
                goLive(kept.at + 1)
                return 700
            },
            () => {
                check("two clicks at once over a playing video bring in the second", shown() === document.slides[kept.at + 1].id, shown())
                check("and nothing is left waiting", !slideLayer().waiting && slideLayer().pacing === "", slideLayer().pacing)
                clearMedia()
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 300
            },
        ]
        next()
    }
}
