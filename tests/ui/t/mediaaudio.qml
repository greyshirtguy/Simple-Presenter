import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The Media Inspector's Audio tab, for a piece of the audio bin and for a video with sound (and none for a video
// without): its channels against the app's, as the file has them, changed by clicks, and every change taken back
// leaving ProPresenter's file exactly as it was; a video's channel turned down and sent to one of the app's channels
// only, and given a ramp in, which is then heard so (on the meter, which is what the test's sound device is sent).
// The workspace is one with four channels in ProPresenter's file, whose audio bin sends its pieces to the third and
// fourth, as the maintainer's does.
QtObject {
    id: t

    readonly property string withSound: "@MADE@/i-with-sound.mp4"
    readonly property string silent: "@MADE@/b-short-4s.mp4"

//COMMON
    function panel() {
        return mediaInspectorLoader.item
    }

    function row(name) {
        const list = named("audioList")
        let index = -1
        for (let i = 0; i < list.count && index < 0; ++i) {
            if (list.model[i].name === name)
                index = i
        }
        if (index < 0)
            return null
        list.positionViewAtIndex(index, ListView.Contain)
        return list.itemAtIndex(index)
    }

    function video(name) {
        return mediaFiles.find(m => m.name === name)
    }

    function clickNamed(name) {
        const item = named(name)
        let page = item.parent
        while (page && page.contentY === undefined)
            page = page.parent
        if (page) {
            const at = item.mapToItem(page.contentItem, 0, 0).y
            page.contentY = Math.max(0, Math.min(at - 40, page.contentHeight - page.height))
        }
        click(centre(item))
    }

    // Which cells of the Audio tab's matrix are lit, as it draws them: "r:c" each
    function lit() {
        const matrix = named("mediaMatrix")
        const found = []
        for (let r = 0; r < matrix.rows; ++r) {
            for (let c = 0; c < matrix.columns; ++c) {
                const item = named("routingCell:" + r + ":" + c)
                const p = item.mapToItem(null, item.width / 2, item.height / 2)
                const colour = String(testInput.windowPixel(p.x, p.y))
                if (parseInt(colour.substring(1, 3), 16) > 200 && parseInt(colour.substring(5, 7), 16) < 80)
                    found.push(r + ":" + c)
            }
        }
        return found.join(" ")
    }

    function cell(r, c) {
        return centre(named("routingCell:" + r + ":" + c))
    }

    function pickMapping(word) {
        const combo = named("mediaMapping")
        click(centre(combo))
        kept.mapping = word
    }

    function pickedMapping() {
        const combo = named("mediaMapping")
        click(centre(Lib.find(combo.popup.contentItem, item => item.modelData === kept.mapping && item.highlighted !== undefined)))
    }

    function typeInto(name, text) {
        clickNamed(name)
        testInput.key(Qt.Key_A, Qt.ControlModifier)
        testInput.type(text)
        testInput.key(Qt.Key_Return)
    }

    function run() {
        steps = [
            // ---- A piece of the audio bin
            () => {
                transitionDuration = 0
                sidePanel.showControlTab = "audio"
                kept.audioFile = catalog.workspacePath + "/Playlists/Audio"
                kept.audioHash = testInput.fileHash(kept.audioFile)
                return 500
            },
            () => {
                click(centre(row("Walk In")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Inspector"))
                return 800
            },
            () => {
                check("a piece's Inspector has Audio and Playback Markers, and nothing else", panel().open && named("inspectorTab-audio") !== null && named("inspectorTab-markers") !== null
                      && named("inspectorTab-information") === null, panel().tabChoices.map(c => c[1]).join())
                clickNamed("inspectorTab-audio")
                return 400
            },
            () => {
                check("its two channels against the app's four", named("mediaMatrix") !== null && named("mediaMatrix").rows === 2 && named("mediaMatrix").columns === 4)
                check("sent to the third and fourth, as its file says", lit() === "0:2 1:3", lit())
                testInput.grab("1-piece-audio")
                pickMapping("Auto")
                return 400
            },
            () => {
                pickedMapping()
                return 400
            },
            () => {
                check("Mapping's Auto sends them to the first and second", lit() === "0:0 1:1" && panel().sound.custom === false, lit())
                check("which is written to the file", catalog.audioItemSettings(panel().media.id).sound.custom === false)
                // Back to the third and fourth by clicks
                click(cell(0, 0))
                click(cell(0, 2))
                click(cell(1, 1))
                click(cell(1, 3))
                return 400
            },
            () => {
                check("a click on a cell sends a channel there, and another stops it", lit() === "0:2 1:3" && panel().sound.custom === true, lit())
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("put back as it was, the audio bin's file is as it was, to the byte", !panel().open && testInput.fileHash(kept.audioFile) === kept.audioHash)

                // ---- Videos
                const playlist = catalog.mediaPlaylists.find(n => !n.folder)
                openMediaPlaylist(playlist.path)
                check("a video with sound, and one without, added to a media playlist", report(catalog.addMedia(mediaPlaylistId, ["file://" + withSound, "file://" + silent]))
                      && video("i-with-sound.mp4") !== undefined && video("b-short-4s.mp4") !== undefined)
                setMediaItemForeground(video("i-with-sound.mp4").id, true)
                kept.mediaFile = catalog.workspacePath + "/Playlists/Media"
                return 500
            },
            () => {
                kept.mediaHash = testInput.fileHash(kept.mediaFile)
                // (Kept with the pictures, to set beside the file if the last check fails)
                testInput.snapshot(kept.mediaFile, "media-before")
                click(centre(binCell(m => m.name === "b-short-4s.mp4")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Inspector"))
                return 1200
            },
            () => {
                check("a video without sound has no Audio tab", panel().open && named("inspectorTab-audio") === null && named("inspectorTab-markers") !== null,
                      panel().tabChoices.map(c => c[1]).join())
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                click(centre(binCell(m => m.name === "i-with-sound.mp4")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Inspector"))
                return 1200
            },
            () => {
                check("a video with sound has, after Effects and before Playback Markers", panel().tabChoices.map(c => c[1]).join() === "Information,Properties,Effects,Audio,Playback Markers",
                      panel().tabChoices.map(c => c[1]).join())
                clickNamed("inspectorTab-audio")
                return 400
            },
            () => {
                check("its one channel against the app's four", named("mediaMatrix").rows === 1 && named("mediaMatrix").columns === 4)
                check("by Auto to the first two, one channel heard on both sides", lit() === "0:0 0:1", lit())
                check("no ramps", named("inspectorRampIn").text === "0 s" && named("inspectorRampOut").text === "0 s", named("inspectorRampIn").text)
                click(cell(0, 1))
                return 300
            },
            () => {
                check("sent to the first alone", lit() === "0:0", lit())
                typeInto("channelVolume:0", "-6")
                return 300
            },
            () => {
                typeInto("inspectorRampIn", "2")
                return 300
            },
            () => {
                const sound = catalog.mediaItemSettings(video("i-with-sound.mp4").id).sound
                check("turned down 6 dB, with a ramp in of two seconds, in the file", sound.custom === true && sound.rampIn === 2 && sound.channels.length === 1
                      && sound.channels[0].volume === -6 && JSON.stringify(sound.channels[0].outputs) === "[0]", JSON.stringify(sound))
                testInput.grab("2-video-audio")
                // (Done: the field typed in last takes an Esc for itself)
                click(centre(named("inspectorDone")))
                return 300
            },
            () => {
                showMedia(video("i-with-sound.mp4"), mediaPlaylistId)
                return 600
            },
            () => {
                kept.early = AudioMeter.left
                check("played, it comes in by its ramp: quiet at first", AudioMeter.sounding && AudioMeter.left < 0.6, AudioMeter.left)
                return 2400
            },
            () => {
                check("and louder when the ramp is done", AudioMeter.left > kept.early + 0.1, kept.early + " then " + AudioMeter.left)
                check("heard on the left alone: the app's first channel, which goes to the device's first", AudioMeter.right < 0.05, AudioMeter.right)
                clearMedia()
                return 400
            },
            () => {
                // Every change taken back: Auto, its volume, its ramp
                click(centre(binCell(m => m.name === "i-with-sound.mp4")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Inspector"))
                return 1200
            },
            () => {
                clickNamed("inspectorTab-audio")
                return 400
            },
            () => {
                pickMapping("Auto")
                return 400
            },
            () => {
                pickedMapping()
                return 300
            },
            () => {
                typeInto("channelVolume:0", "0")
                return 300
            },
            () => {
                typeInto("inspectorRampIn", "0")
                return 300
            },
            () => {
                check("by Auto again", lit() === "0:0 0:1", lit())
                click(centre(named("inspectorDone")))
                return 300
            },
            () => {
                check("every change taken back, the media playlist's file is as it was, to the byte", testInput.fileHash(kept.mediaFile) === kept.mediaHash)
                check("nothing went wrong on the way", !/PROBLEM|WARNING|ERROR/.test(testInput.readText(Log.path).split("\n").filter(l => !l.includes("qt.qpa.theme")).join("\n")),
                      testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
