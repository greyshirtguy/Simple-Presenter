import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The audio bin, on a bin as ProPresenter might have left one (run.py makes it: prepare_audio). Its tab in the show
// controls, with the playlists on the lighter ground and their pieces on the darker; a piece played by a click, and
// what then says so: its row, the button that clears the audio, the meter in the preview. Music one at a time and
// sound effects over it and over each other; a piece followed by the next of its playlist, past one whose file is not
// there; one that goes round; crossing one piece of music into the next; clearing, by the button, by F5 and by the
// first clear group. The box that narrows the list, the fade's length dragged and typed, the line between the two
// lists dragged. A row's menu: how it plays on, music or a sound effect. Sound files added, and that a long list
// stays where it was scrolled to when a piece far down it is changed. A playlist made. A piece
// dragged onto a slide, onto a macro by way of the macros' tab, and onto the bare list of the macros. The device
// sound is played through, in the settings. What the files then hold is looked at when the test is over, by
// after_audio in run.py.
QtObject {
    id: t

//COMMON
    readonly property string sounds: "@WORKSPACES@".replace(/WorkSpaces$/, "sounds")

    function piece(name) {
        return audioFiles.find(a => a.name === name)
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

    function playlist(name) {
        return catalog.audioPlaylists.find(p => p.name === name)
    }

    // The row of a playlist or folder in the list of them, brought into view
    function listRow(name) {
        const list = named("audioPlaylistList")
        let index = -1
        for (let i = 0; i < list.count && index < 0; ++i) {
            if (list.model[i].name === name)
                index = i
        }
        list.positionViewAtIndex(index, ListView.Contain)
        return list.itemAtIndex(index)
    }

    function allMacros() {
        let all = []
        for (const collection of Macros.collections)
            all = all.concat(Array.from(collection.macros))
        return all
    }

    function macrosTab() {
        return Lib.findAll(sidePanel, item => item.modelData !== undefined && item.modelData !== null && item.modelData.id === "macros" && item.chosen !== undefined)[0]
    }

    // How light a point of the operator window is, from 0 to 255
    function grey(x, y) {
        const c = String(testInput.windowPixel(x, y))
        return (parseInt(c.substring(1, 3), 16) * 299 + parseInt(c.substring(3, 5), 16) * 587 + parseInt(c.substring(5, 7), 16) * 114) / 1000
    }

    function red(item) {
        const p = item.mapToItem(null, 4, item.height / 2)
        const c = String(testInput.windowPixel(p.x, p.y))
        return parseInt(c.substring(1, 3), 16) > 150 && parseInt(c.substring(3, 5), 16) < 110
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function dragTo(from, to) {
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 12; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
        kept.to = to
    }

    function playing() {
        return (liveMusic ? liveMusic.name : "-") + " + " + liveSounds.map(s => s.name).join(",")
    }

    function run() {
        steps = [
            // ---- What is read, and what is drawn
            () => {
                transitionDuration = 0
                check("the audio bin's playlists and folders are read from ProPresenter's file", catalog.audioPlaylists.map(p => "  ".repeat(p.depth) + p.name).join("|") === "Services|  Before the Service|  Effects|Nothing Yet",
                      catalog.audioPlaylists.map(p => "  ".repeat(p.depth) + p.name).join("|"))
                check("and the first playlist is the one being browsed, with its pieces in the file's order", audioPlaylistId === playlist("Before the Service").path
                      && audioFiles.map(a => a.name).join() === "Walk In,Second Tune,Lost Tune,Last Tune,Round", audioFiles.map(a => a.name).join())
                check("how each plays on from its end is read: the next follows, or it goes round", audioFiles.map(a => a.playback).join() === "next,next,next,next,loop", audioFiles.map(a => a.playback).join())
                check("and how long each is, and that all are music", audioFiles.map(a => a.duration).join() === "3,3,5,2,1.2" && audioFiles.every(a => a.effect === false), audioFiles.map(a => a.duration).join())
                check("a piece whose file is not on this computer is missing, and the others are found", audioFiles.map(a => a.missing).join() === "false,false,true,false,false"
                      && piece("Walk In").path === sounds + "/Walk In.wav", piece("Walk In").path)
                check("nothing of the audio bin is made until its tab is come to", named("audioList") === null && named("audioPlaylistList") === null)
                const controlTabs = Lib.findAll(sidePanel, item => item.modelData !== undefined && item.modelData !== null && item.modelData.picture !== undefined && item.chosen !== undefined)
                check("the show controls have a tab for it, the fifth", controlTabs.length === 6 && controlTabs[4].modelData.id === "audio", controlTabs.map(x => x.modelData.id).join())
                sidePanel.showControlTab = "audio"
                return 500
            },
            () => {
                const lists = named("audioPlaylistList"), pieces = named("audioList")
                check("its tab shows the playlists over their pieces", lists !== null && pieces !== null && lists.mapToItem(null, 0, 0).y < pieces.mapToItem(null, 0, 0).y)
                check("how many pieces there are is said over them", named("audioCount").text === "5 items", named("audioCount").text)
                // (A bare corner of each: the foot of the playlists' ground at its left, and the pieces' beside how many there are.)
                const lightArea = named("audioPlaylistArea"), darkArea = named("audioPieceArea")
                const light = lightArea.mapToItem(null, 5, lightArea.height - 2), dark = darkArea.mapToItem(null, 4, 13)
                kept.light = grey(light.x, light.y)
                kept.dark = grey(dark.x, dark.y)
                check("the playlists are on a lighter ground than the pieces", kept.light > kept.dark + 8, kept.light + " and " + kept.dark)
                const walk = row("Walk In")
                check("a piece's row has its name drawn", walk !== null && strokes(Lib.find(walk, item => item.text === "Walk In")).light > 3)
                check("how long it is", Lib.find(walk, item => item.objectName === "audioRowLength").text === "0:03", Lib.find(walk, item => item.objectName === "audioRowLength").text)
                check("that it is music, and that the next follows it", Lib.find(walk, item => item.objectName === "audioRowType").kind === "music"
                      && Lib.find(walk, item => item.objectName === "audioRowPlayback").kind === "next")
                check("the piece that is not here says so", Lib.find(row("Lost Tune"), item => item.objectName === "audioRowLength").text === "missing")
                check("the one that goes round says that", Lib.find(row("Round"), item => item.objectName === "audioRowPlayback").kind === "loop")
                check("there is a button that clears the audio, grey with nothing to clear", named("clearAudio") !== null && !red(named("clearAudio")) && !audioLive)
                check("and no meter in the preview", named("audioMeter").item === null && !AudioMeter.sounding)
                testInput.grab("1-audio-bin")
                kept.clickedAt = Date.now()
                click(centre(row("Walk In")))
                return 900
            },

            // ---- A piece played
            () => {
                check("a click on a piece plays it", liveMusic !== null && liveMusic.name === "Walk In" && liveMusicPlaylistId === audioPlaylistId && AudioLayer.voices === 1, playing())
                check("it is heard: the player is playing and has got somewhere", AudioLayer.musicPosition > 0.3 && AudioLayer.musicDuration > 2.9 && AudioLayer.musicDuration < 3.1, AudioLayer.musicPosition + " of " + AudioLayer.musicDuration)
                const walk = row("Walk In")
                check("its row is marked as playing", walk.sounding === true && walk.music === true && grey(walk.mapToItem(null, walk.width / 2, 3).x, walk.mapToItem(null, walk.width / 2, 3).y) > kept.dark + 25)
                check("and counts down what it has left", /^-0:0[0-3]$/.test(Lib.find(walk, item => item.objectName === "audioRowLength").text), Lib.find(walk, item => item.objectName === "audioRowLength").text)
                check("the button that clears the audio is red", red(named("clearAudio")) && audioLive)
                check("its playlist is marked in the list of them", named("audioPlaylistList").livePath === audioPlaylistId)
                // The file is a steady note at half of as loud as can be, on both sides: 6 dB down, nine tenths of the way up.
                check("the meter is in the preview, both bars at the height of a note half as loud as can be", named("audioMeter").item !== null && AudioMeter.sounding
                      && Math.abs(AudioMeter.left - 0.9) < 0.03 && Math.abs(AudioMeter.right - 0.9) < 0.03, AudioMeter.left + " and " + AudioMeter.right)
                const bar = Lib.find(named("audioMeter"), item => item.objectName === "audioMeterLeft")
                const low = bar.mapToItem(null, bar.width / 2, bar.height * 0.6), top = bar.mapToItem(null, bar.width / 2, bar.height * 0.03)
                const drawn = String(testInput.windowPixel(low.x, low.y))
                check("and drawn: green part of the way up, and nothing at the very top", parseInt(drawn.substring(3, 5), 16) > 150 && parseInt(drawn.substring(1, 3), 16) < 120 && grey(top.x, top.y) < 90,
                      drawn + " and " + testInput.windowPixel(top.x, top.y))
                check("the log says what was played", logged(/audio\s+music "Walk In", then the next/).length === 1 && logged(/"Walk In" started/).length === 1)
                testInput.grab("2-playing")
                // Other music takes its place, at once: nothing is crossed unless that is switched on.
                click(centre(row("Second Tune")))
                return 500
            },
            () => {
                check("other music takes its place", liveMusic.name === "Second Tune" && liveSounds.length === 0, playing())
                check("at once: one piece is sounding, not two", AudioLayer.voices === 1, AudioLayer.voices)
                check("this one is a quarter as loud as can be, and the meter is lower for it", Math.abs(AudioMeter.left - 0.8) < 0.04 && Math.abs(AudioMeter.right - 0.8) < 0.04, AudioMeter.left + " and " + AudioMeter.right)
                check("the rows say which is playing", row("Second Tune").sounding === true && row("Walk In").sounding === false)
                // Sound effects, from the other playlist
                click(centre(listRow("Effects")))
                return 400
            },

            // ---- Sound effects
            () => {
                check("a click on another playlist shows its pieces", audioPlaylistId === playlist("Effects").path && audioFiles.map(a => a.name).join() === "Bell,Thunder" && named("audioCount").text === "2 items",
                      audioFiles.map(a => a.name).join() + " " + named("audioCount").text)
                check("which are sound effects, and say so with their picture", audioFiles.every(a => a.effect === true && a.playback === "stop")
                      && Lib.find(row("Bell"), item => item.objectName === "audioRowType").kind === "sound" && Lib.find(row("Bell"), item => item.objectName === "audioRowPlayback").kind === "stop")
                check("the music plays on, from the playlist it was played from, which is still the one marked", liveMusic.name === "Second Tune" && liveMusicPlaylistId === playlist("Before the Service").path
                      && named("audioPlaylistList").livePath === playlist("Before the Service").path)
                click(centre(row("Thunder")))
                return 250
            },
            () => {
                click(centre(row("Bell")))
                return 250
            },
            () => {
                click(centre(row("Bell")))
                return 500
            },
            () => {
                check("sound effects play over the music and over each other: the same one twice is two", liveMusic !== null && liveMusic.name === "Second Tune"
                      && liveSounds.map(s => s.name).join() === "Thunder,Bell,Bell" && AudioLayer.voices === 4, playing() + ", " + AudioLayer.voices + " sounding")
                check("their rows are marked", row("Bell").sounding === true && row("Thunder").sounding === true && row("Bell").music === false)
                // The bell is on the left only: with the others the left is the louder.
                check("the meter has them all, and the left louder for the bell that is only on the left", AudioMeter.left > AudioMeter.right + 0.02 && AudioMeter.right > 0.75, AudioMeter.left + " and " + AudioMeter.right)
                testInput.grab("3-effects-over-music")
                return 1900
            },
            () => {
                check("an effect that has played to its end is no longer playing, and nothing follows it: the longer one plays on", liveSounds.map(s => s.name).join() === "Thunder", playing())
                check("(the music, which the next follows, has ended too, and the piece after it is missing: the one after that is played, in its own playlist)",
                      logged(/music "Second Tune", then the next has played to its end/).length === 1, logged(/has played to its end/).join(" | "))
                return 400
            },
            () => {
                check("the next that has a file is what follows", liveMusic !== null && liveMusic.name === "Last Tune" && liveMusicPlaylistId === playlist("Before the Service").path, playing())
                // ---- Clearing
                audioFade = 0.4
                kept.meter = AudioMeter.left
                click(centre(named("clearAudio")))
                check("a click on the button that clears the audio clears it: nothing is said to be playing", liveMusic === null && liveSounds.length === 0 && !audioLive)
                check("and the button is grey again", !red(named("clearAudio")))
                check("it goes out over the fade's length: it is still sounding for the moment", AudioLayer.voices >= 1, AudioLayer.voices)
                return 200
            },
            () => {
                check("part of the way through the fade the meter is part of the way down", AudioMeter.sounding && AudioMeter.left < kept.meter - 0.01 && AudioMeter.left > 0.05, AudioMeter.left + " from " + kept.meter)
                return 700
            },
            () => {
                check("and then nothing sounds", AudioLayer.voices === 0, AudioLayer.voices)
                return 700
            },
            () => {
                check("and the meter has gone from the preview", !AudioMeter.sounding && named("audioMeter").item === null && AudioMeter.left === 0)
                // The last of a playlist, which the next would follow: there is none.
                audioFade = 0
                click(centre(listRow("Before the Service")))
                return 300
            },

            // ---- The next, and the last
            () => {
                click(centre(row("Last Tune")))
                return 2600
            },
            () => {
                // (Round is after it, and goes round: it is what follows, the last being followed by the next.)
                check("the piece after the last tune follows it", liveMusic !== null && liveMusic.name === "Round", playing())
                return 700
            },
            () => {
                kept.frames = Log.frames(win)
                return 1500
            },
            () => {
                // (The meter moves ten times a second at most, and a steady note does not move it at all; the row counts down in seconds.)
                const drawn = Log.frames(win) - kept.frames
                check("while a piece plays the window is drawn no more often than the meter moves: far from every frame", drawn <= 18, drawn + " frames in a second and a half")
                check("and goes round: it is still playing well after its length", liveMusic !== null && liveMusic.name === "Round" && AudioLayer.voices === 1 && AudioLayer.musicDuration < 1.4, playing() + " " + AudioLayer.musicDuration)
                check("F5 clears the audio", (testInput.key(Qt.Key_F5), true) && liveMusic === null)
                return 300
            },
            () => {
                check("with a fade of no length, at once", AudioLayer.voices === 0, AudioLayer.voices)
                // A piece that stops: the menu of a row
                click(centre(row("Last Tune")), Qt.RightButton)
                return 300
            },

            // ---- A row's menu
            () => {
                check("a right click on a piece has how it plays on, what it is, and taking it out", labels() === "[Playback], Stop, Loop, *Next, [Type], *Music, Sound Effect, [Playlist], Remove from Playlist", labels())
                click(menuRow("Stop"))
                return 400
            },
            () => {
                check("set to stop, it stops", piece("Last Tune").playback === "stop" && Lib.find(row("Last Tune"), item => item.objectName === "audioRowPlayback").kind === "stop", piece("Last Tune").playback)
                click(centre(row("Last Tune")))
                return 2600
            },
            () => {
                check("played, it plays to its end and nothing follows it", liveMusic === null && AudioLayer.voices === 0, playing())
                click(centre(row("Walk In")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Sound Effect"))
                return 400
            },
            () => {
                check("made a sound effect, it is one", piece("Walk In").effect === true && piece("Walk In").playback === "next" && Lib.find(row("Walk In"), item => item.objectName === "audioRowType").kind === "sound")
                click(centre(row("Walk In")), Qt.RightButton)
                return 300
            },
            () => {
                check("and its menu says so", labels().includes("Music, *Sound Effect"), labels())
                click(menuRow("Music"))
                return 400
            },
            () => {
                check("and music again", piece("Walk In").effect === false)
                click(centre(row("Round")), Qt.RightButton)
                return 300
            },
            () => {
                check("the one that goes round has Loop ticked", labels().startsWith("[Playback], Stop, *Loop, Next"), labels())
                testInput.key(Qt.Key_Escape)
                return 300
            },

            // ---- Crossing one into the next
            () => {
                const toggle = named("audioCrossfade")
                check("crossing is off to begin with", audioCrossfade === false && AudioLayer.crossfade === false)
                click(centre(toggle))
                check("a click on its switch turns it on", audioCrossfade === true && AudioLayer.crossfade === true)
                const field = named("audioFadeField")
                click(centre(field))
                return 200
            },
            () => {
                const field = named("audioFadeField")
                check("the fade's length can be typed", field.activeFocus === true)
                field.selectAll()
                testInput.type("1.2")
                testInput.key(Qt.Key_Return)
                return 200
            },
            () => {
                check("and is the fade's length", Math.abs(audioFade - 1.2) < 0.001 && Math.abs(AudioLayer.fade - 1.2) < 0.001 && named("audioFadeField").text === "1.2", audioFade + " " + named("audioFadeField").text)
                check("the slider is at it", Math.abs(named("audioFadeSlider").value - 1.2) < 0.001, named("audioFadeSlider").value)
                check("and the keyboard is the show's again", named("audioFadeField").activeFocus === false)
                click(centre(row("Walk In")))
                return 1000
            },
            () => {
                check("music started with nothing playing starts as loud as it is", liveMusic.name === "Walk In" && AudioLayer.voices === 1 && Math.abs(AudioMeter.left - 0.9) < 0.03, AudioMeter.left)
                click(centre(row("Second Tune")))
                return 450
            },
            () => {
                check("with crossing on, other music comes in as this one goes out: both are sounding", liveMusic.name === "Second Tune" && AudioLayer.voices === 2, playing() + ", " + AudioLayer.voices + " sounding")
                check("the log says they crossed", logged(/"Second Tune" started, crossing with what was playing/).length === 1)
                return 1300
            },
            () => {
                check("and when the fade is done only the new one is", liveMusic.name === "Second Tune" && AudioLayer.voices === 1 && Math.abs(AudioMeter.left - 0.8) < 0.04, AudioLayer.voices + " sounding, the meter at " + AudioMeter.left)
                return 800
            },
            () => {
                // Second Tune is three seconds long, and the next follows it: a fade's length before its end.
                check("music the next follows hands over one fade's length before its end, so that the two cross", liveMusic !== null && liveMusic.name === "Last Tune" && AudioLayer.voices === 2,
                      playing() + ", " + AudioLayer.voices + " sounding")
                return 900
            },
            () => {
                check("and has gone when it has ended", liveMusic !== null && liveMusic.name === "Last Tune" && AudioLayer.voices === 1, playing() + ", " + AudioLayer.voices)
                // The first clear group, which a workspace starts with as one that clears everything: F1
                testInput.key(Qt.Key_F1)
                check("F1, the first clear group, clears the audio with the rest", liveMusic === null && !audioLive)
                return 1500
            },
            () => {
                check("over the fade's length", AudioLayer.voices === 0, AudioLayer.voices)
                click(centre(named("audioCrossfade")))
                audioFade = 0
                check("crossing is switched off again", audioCrossfade === false)

                // ---- The box that narrows the list
                click(centre(named("audioFilter")))
                return 200
            },
            () => {
                testInput.type("tune")
                return 300
            },
            () => {
                const list = named("audioList")
                const names = []
                for (let i = 0; i < list.count; ++i)
                    names.push(list.model[i].name)
                check("what is typed in the box narrows the pieces to the names that have it", names.join() === "Second Tune,Lost Tune,Last Tune", names.join())
                check("and how many are left is said", named("audioCount").text === "5 items, 3 shown", named("audioCount").text)
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc empties the box and gives the keys back", audioFilter === "" && named("audioList").count === 5 && named("audioFilter").activeFocus === false)

                // ---- The line between the two lists
                const line = named("audioSplit")
                kept.listHeight = named("audioPlaylistList").height
                drag(centre(line), Qt.point(centre(line).x, centre(line).y - 20))
                return 300
            },
            () => {
                check("the line between the lists can be dragged: up, the playlists have less and the pieces more", named("audioPlaylistList").height < kept.listHeight - 10, named("audioPlaylistList").height + " from " + kept.listHeight)
                kept.split = audioSplit

                // ---- Sound files added
                kept.before = audioFiles.length
                check("sound files are added to the playlist being browsed", report(catalog.addAudio(audioPlaylistId, ["file://" + sounds + "/Extra Tune.wav", "file://" + sounds + "/notes.txt"]))
                      && audioFiles.length === kept.before + 1 && piece("Extra Tune") !== undefined, audioFiles.map(a => a.name).join())
                check("a new piece is music that the next follows, as loud as its file, where the file is", piece("Extra Tune").effect === false && piece("Extra Tune").playback === "next" && piece("Extra Tune").volume === 1
                      && piece("Extra Tune").path === sounds + "/Extra Tune.wav" && !piece("Extra Tune").missing, JSON.stringify(piece("Extra Tune")))
                check("and how long it is was read from it", Math.abs(piece("Extra Tune").duration - 2.5) < 0.05, piece("Extra Tune").duration)
                check("files that are no sound files are turned away", catalog.addAudio(audioPlaylistId, ["file://" + sounds + "/notes.txt"]) !== "" && audioFiles.length === kept.before + 1)
                check("and so the count says", named("audioCount").text === "6 items", named("audioCount").text)

                // ---- The list stays where it is. (The playlists given most of the height, as the line between the two
                // drags it, so that the pieces are more than their list has room for: the show controls have the room of
                // the window under the preview, which is more than six pieces need.)
                kept.splitBefore = audioSplit
                audioSplit = named("audioPlaylistArea").parent.height - 118
                return 300
            },
            () => {
                const list = named("audioList")
                kept.furthest = list.count * 29 - 2 - list.height
                list.contentY = kept.furthest
                return 300
            },
            () => {
                const list = named("audioList")
                const last = row("Extra Tune")
                kept.scrolled = list.contentY
                kept.lastAt = last.mapToItem(null, 0, 0).y
                check("the pieces are more than the list has room for, and it is scrolled to its end", kept.furthest > 40 && Math.abs(kept.scrolled - kept.furthest) < 1
                      && row("Walk In") !== null && list.contentY < kept.scrolled, kept.scrolled + " of " + kept.furthest)
                list.contentY = kept.scrolled
                return 300
            },
            () => {
                click(centre(row("Extra Tune")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Sound Effect"))
                return 500
            },
            () => {
                const list = named("audioList")
                const last = list.itemAtIndex(list.count - 1)
                check("a piece at the end of a long list, made a sound effect, is one", piece("Extra Tune").effect === true)
                check("and the list has stayed where it was scrolled to, with that piece where it was", Math.abs(list.contentY - kept.scrolled) < 1 && last !== null && last.modelData.name === "Extra Tune"
                      && Math.abs(last.mapToItem(null, 0, 0).y - kept.lastAt) < 1 && Lib.find(last, item => item.objectName === "audioRowType").kind === "sound",
                      list.contentY + " from " + kept.scrolled + ", the row at " + (last ? last.mapToItem(null, 0, 0).y : "nowhere") + " from " + kept.lastAt)
                testInput.grab("9-list-stays")
                click(centre(last), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Loop"))
                return 500
            },
            () => {
                const list = named("audioList")
                check("set to go round, the same", piece("Extra Tune").playback === "loop" && Math.abs(list.contentY - kept.scrolled) < 1, list.contentY + " from " + kept.scrolled)
                click(centre(list.itemAtIndex(list.count - 1)), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Music"))
                return 500
            },
            () => {
                const list = named("audioList")
                click(centre(list.itemAtIndex(list.count - 1)), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Next"))
                return 500
            },
            () => {
                const list = named("audioList")
                check("and made music that the next follows again, the same", piece("Extra Tune").effect === false && piece("Extra Tune").playback === "next"
                      && Math.abs(list.contentY - kept.scrolled) < 1, list.contentY + " from " + kept.scrolled)
                audioSplit = kept.splitBefore
                // Words in the box are another list, which starts at its top
                audioFilter = "tune"
                return 300
            },
            () => {
                const list = named("audioList")
                check("narrowed by the box, the list starts at its top", list.contentY === 0 && list.count < 6, list.contentY + " with " + list.count)
                audioFilter = ""
                return 300
            },
            () => {
                // ---- A playlist made, with the + under the tabs
                click(centre(named("showControlAdd")))
                return 300
            },
            () => {
                check("the + under the tabs adds a folder, a playlist, or audio", labels() === "Add Folder, Add Playlist, Add Audio…", labels())
                click(menuRow("Add Playlist"))
                return 400
            },
            () => {
                const made = playlist("New Playlist")
                check("a playlist is made beside the one that was picked, and is the one being browsed", made !== undefined && made.parent === playlist("Before the Service").parent && audioPlaylistId === made.path && audioFiles.length === 0)
                check("its name is there to be typed over", named("audioPlaylistList").editingPath === made.path && sidePanel.renaming === true)
                testInput.type("Sunday")
                testInput.key(Qt.Key_Return)
                return 400
            },
            () => {
                check("and it is called what was typed", playlist("Sunday") !== undefined && playlist("New Playlist") === undefined && sidePanel.renaming === false)
                check("an empty playlist says how to fill it", named("audioCount").text === "0 items")
                click(centre(listRow("Effects")))
                return 300
            },

            // ---- Dragged onto a slide
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Awake My Soul"))
                return 500
            },
            () => {
                testInput.snapshot(document.path, "song-before")
                kept.song = document.path
                kept.actions = document.slides[1].actions.length
                dragTo(centre(row("Bell")), centre(slideCell(1)))
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 500
            },
            () => {
                const actions = document.slides[1].actions
                const made = actions[actions.length - 1]
                check("a piece dragged onto a slide gives the slide an action that plays it", actions.length === kept.actions + 1 && made.kind === "audio" && made.done === true && made.audio.name === "Bell", JSON.stringify(actions.map(a => a.kind)))
                check("as it is in the bin: a sound effect that stops, the same file", made.effect === true && made.playback === "stop" && made.audio.path === sounds + "/Bell.wav" && made.title === "Play the sound effect “Bell”", made.title)
                check("with an id of its own", made.id !== piece("Bell").id && made.audio.id === made.id)
                goLive(1)
                return 500
            },
            () => {
                check("the slide going live plays it", liveSounds.map(s => s.name).join() === "Bell" && AudioLayer.voices === 1, playing())
                clearAudio()
                const cell = slideCell(1)
                const icon = Lib.findAll(cell, item => item.kind === "audio" && item.audio !== undefined)
                check("the slide shows the action with the sound effect's picture", icon.length === 1 && icon[0].audio === "sound", icon.length)
                return 300
            },

            // ---- Dragged to the macros, by way of their tab
            () => {
                kept.macros = allMacros().length
                dragTo(centre(row("Thunder")), centre(macrosTab()))
                check("held over the macros' tab, nothing yet", sidePanel.showControlTab === "audio")
                return 900
            },
            () => {
                check("after a moment the tab comes forward", sidePanel.showControlTab === "macros")
                check("and the piece is still being carried", audioDragged !== null && audioDragged.name === "Thunder")
                // (The workspace has more macros than the list has room for: there is no bare part of it to drop on.)
                const strip = named("macroNewDrop")
                check("with a strip along the bottom of the macros to drop it on for a macro of its own", strip !== null && strip.visible === true && strokes(Lib.find(strip, item => item.text !== undefined)).light > 3)
                const bare = centre(strip)
                for (let i = 1; i <= 6; ++i)
                    testInput.mouse(1, kept.to.x + (bare.x - kept.to.x) * i / 6, kept.to.y + (bare.y - kept.to.y) * i / 6)
                kept.to = bare
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 500
            },
            () => {
                const all = allMacros()
                const made = all.find(m => m.name === "Thunder")
                check("dropped there, the piece is given a macro of its own, called what it is called", all.length === kept.macros + 1 && made !== undefined, all.map(m => m.name).join())
                check("and the strip has gone with the drag", named("macroNewDrop").visible === false && audioDragged === null)
                check("whose one action plays it", made !== undefined && made.actions.length === 1 && made.actions[0].kind === "audio" && made.actions[0].audio.name === "Thunder" && made.actions[0].effect === true)
                kept.macro = made.id
                check("as loud as its row says", made.actions[0].audio.volume === 0.5, made.actions[0].audio.volume)
                runMacro(made.id)
                return 500
            },
            () => {
                check("run, the macro plays it", liveSounds.map(s => s.name).join() === "Thunder", playing())
                // The meter has its row's volume in it: half of a half
                check("at half its file's loudness: the meter is 6 dB lower than for a file played at full", Math.abs(AudioMeter.left - 0.8) < 0.04, AudioMeter.left)
                clearAudio()
                // And onto a macro that is there
                sidePanel.showControlTab = "audio"
                return 500
            },
            () => {
                dragTo(centre(row("Bell")), centre(macrosTab()))
                return 900
            },
            () => {
                const target = Lib.findAll(named("macroList"), item => item.objectName === "macroRow" && item.modelData.id === kept.macro)[0]
                named("macroList").positionViewAtEnd()
                const at = centre(target)
                for (let i = 1; i <= 6; ++i)
                    testInput.mouse(1, kept.to.x + (at.x - kept.to.x) * i / 6, kept.to.y + (at.y - kept.to.y) * i / 6)
                kept.to = at
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 500
            },
            () => {
                const made = allMacros().find(m => m.id === kept.macro)
                check("dropped on a macro, the piece is another of that macro's actions", made.actions.length === 2 && made.actions[1].kind === "audio" && made.actions[1].audio.name === "Bell"
                      && allMacros().length === kept.macros + 1, Array.from(made.actions).map(a => a.title).join(" | "))
                // The menu of an action that plays audio
                showActionMenu({ macro: kept.macro }, made.actions[1], named("macroList"), 20, 20)
                return 300
            },
            () => {
                check("such an action can be made to stop or go round, and to be music or a sound effect", labels().includes("[Playback], *Stop, Loop, [Type], Music, *Sound Effect") && labels().endsWith("Remove"), labels())
                click(menuRow("Loop"))
                return 400
            },
            () => {
                const made = allMacros().find(m => m.id === kept.macro)
                check("set to go round, it goes round, and is otherwise what it was", made.actions[1].playback === "loop" && made.actions[1].effect === true && made.actions[1].audio.name === "Bell"
                      && made.actions[1].title === "Play the sound effect “Bell”, looping", made.actions[1].title)

                // ---- The device, in the settings
                sidePanel.showControlTab = "audio"
                settingsOpen = true
                return 400
            },
            () => {
                settingsScreen.section = "audio"
                return 500
            },
            () => {
                const part = Lib.find(settingsScreen, item => item.objectName === "audioSettings")
                check("the settings have a section for audio", part !== null)
                const choices = Lib.findAll(settingsScreen, item => String(item.objectName).startsWith("audioDevice:"))
                check("with the system's own choice, which is the one chosen, and the devices there are", choices.length >= 2 && choices[0].device === "" && choices[0].chosen === true && AudioLayer.devices.length === choices.length - 1,
                      choices.map(c => c.title).join(" | "))
                kept.device = choices[1].device
                kept.deviceName = choices[1].title
                testInput.grab("4-settings-audio")
                click(Qt.point(choices[1].mapToItem(null, 9, 11).x, choices[1].mapToItem(null, 9, 11).y))
                return 300
            },
            () => {
                check("a click on a device chooses it", audioDevice === kept.device && audioDevice !== "" && AudioLayer.device === kept.device && AudioLayer.deviceFound === true)
                settingsOpen = false
                return 300
            },
            () => {
                click(centre(row("Thunder")))
                return 600
            },
            () => {
                check("what is played is then played through it, and the log says so", liveSounds.length === 1 && logged(new RegExp('"Thunder" started, through "' + kept.deviceName.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + '"')).length === 1,
                      logged(/"Thunder" started/).pop())
                audioDevice = "bm8gc3VjaCBkZXZpY2U="
                check("a device that is not there is said not to be", AudioLayer.deviceFound === false)
                audioDevice = ""
                clearAudio()
                return 400
            },

            // ---- A workspace with nothing in its audio bin
            () => {
                check("the settings that are kept are the fade, the crossing, the device and the line between the lists", audioFade === 0 && audioCrossfade === false && audioDevice === "" && audioSplit === kept.split)
                check("nothing went wrong on the way", !/PROBLEM|WARNING|ERROR|qrc:/.test(testInput.readText(Log.path).split("\n").filter(l => !l.includes("qt.qpa.theme")).join("\n")),
                      testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR|qrc:/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
