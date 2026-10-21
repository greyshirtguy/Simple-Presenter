import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The actions that play a playlist of the media bin or of the audio bin, and the action that shows a message
// (src/playlistactions.h, the message's in src/actions.h and src/messagefile.h, the show's rules in show::State), on
// the test's own workspace (prepare_playacts in run.py): a media bin with a folder "Services" of a playlist "Walk In"
// (a red picture and a green one) and a playlist "Backgrounds" (green); an audio bin whose "Before the Service" is two
// tones, each followed by the next; two messages, "Parent Wanted" with the tokens Name and Room (last given Sam and
// Foyer) and "Car Lights" with none; and a presentation "Playlists" of five blocks in the top left of the slide, so
// that the media under a slide shows round it, whose fifth slide has ProPresenter's own Audio Bin Playlist (by a name
// and an id the playlist no longer has) and Message actions.
//
// Made: Walk In from Add Action → Media Playlist → Services, on the first slide, and given Always Retrigger from its
// icon's menu; Before the Service dragged out of the audio bin's list onto the second (a folder dragged is not
// taken); Parent Wanted dragged out of the messages' list onto the third, which brings up the panel for its tokens,
// where Name is given Alex; Car Lights from Add Action → Message onto the fourth, with no panel; Backgrounds dragged
// out of the media bin's list onto the fourth and onto a macro. Then the show: what each does on the output, the
// operator's preview and the audio, with and without Always Retrigger; a message shown with the action's words and
// then from its tab with its own; the panel opened again from the action's menu; and ProPresenter's own two.
// after_playacts in run.py reads the files.
QtObject {
    id: t

//COMMON
    readonly property string walkIn: "0C0C0C0C-0000-0000-0000-000000000001"
    readonly property string backgrounds: "0C0C0C0C-0000-0000-0000-000000000002"
    readonly property string beforeService: "0A0A0A0A-0000-0000-0000-000000000001"
    readonly property string parentWanted: "7A7A7A7A-0000-0000-0000-000000000001"
    readonly property string carLights: "7A7A7A7A-0000-0000-0000-000000000002"

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function hover(label) {
        const row = menuRow(label)
        testInput.mouse(4, row.x, row.y)
    }

    function tabs() {
        return Lib.findAll(sidePanel, item => item.chosen !== undefined && item.modelData !== undefined && item.modelData.id !== undefined)
    }

    function tab(id) {
        return tabs().find(item => item.modelData.id === id)
    }

    // The row of one of the lists (the bins' playlists, the messages) that has this name
    function listRow(list, name) {
        const view = named(list)
        for (let i = 0; i < view.count; ++i) {
            if (view.model[i].name === name) {
                view.positionViewAtIndex(i, ListView.Contain)
                return view.itemAtIndex(i)
            }
        }
        return null
    }

    function dragTo(from, to) {
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 12; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
        kept.to = to
    }

    function badge(slide, kind) {
        return Lib.findAll(slideCell(slide), item => item.objectName === "actionBadge" && item.modelData.kind === kind)[0] ?? null
    }

    function actionsOf(slide, kind) {
        return document.slides[slide].actions.filter(action => action.kind === kind)
    }

    // The colour at a point of the output, and of the operator's preview of it, as fractions of the screen
    function outputAt(fx, fy) {
        return String(testInput.pixel(false, fx, fy))
    }

    function previewAt(fx, fy) {
        const picture = Lib.find(win.contentItem, item => item.namePrefix === "preview" && item.screenId !== undefined)
        const aspect = output.width / output.height
        const w = Math.min(picture.width, picture.height * aspect)
        const h = w / aspect
        const p = picture.mapToItem(null, (picture.width - w) / 2 + fx * w, (picture.height - h) / 2 + fy * h)
        return String(testInput.windowPixel(p.x, p.y))
    }

    // Whether a colour is near pure red or pure green
    function reddish(c) {
        return parseInt(c.substring(1, 3), 16) > 200 && parseInt(c.substring(3, 5), 16) < 60 && parseInt(c.substring(5, 7), 16) < 60
    }

    function greenish(c) {
        return parseInt(c.substring(1, 3), 16) < 60 && parseInt(c.substring(3, 5), 16) > 200 && parseInt(c.substring(5, 7), 16) < 60
    }

    // What a message action gives its tokens, by the token's name
    function wordsOf(values) {
        const words = {}
        for (const value of values)
            words[value.name] = value.value
        return words
    }

    function macroOf(id) {
        for (const collection of Macros.collections) {
            for (const macro of collection.macros) {
                if (macro.id === id)
                    return macro
            }
        }
        return undefined
    }

    function binRow(name) {
        return binCell(m => m.name === name + ".png")
    }

    function run() {
        steps = [
            () => {
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    if (entry("Playlists"))
                        break
                }
                openEntry(entry("Playlists"))
                return 800
            },
            // ---- Add Action → Media Playlist
            () => {
                check("the presentation is open, with ProPresenter's two actions on its fifth slide read", document.slides.length === 5
                      && actionsOf(4, "audioPlaylist").length === 1 && actionsOf(4, "message").length === 1, JSON.stringify(document.slides[4].actions.map(a => a.kind)))
                const pro = actionsOf(4, "audioPlaylist")[0]
                check("ProPresenter's Audio Bin Playlist: the playlist by id and name, no item, Always Retrigger off", pro.playlistName === "Before the Service"
                      && pro.playlistId === "9E98B135-0000-4A67-9450-8F86707697DC" && pro.itemId === "" && !pro.retrigger && pro.done, JSON.stringify(pro))
                showAddActionMenu({ slide: 0 }, slideCell(0), 10, 10)
                return 300
            },
            () => {
                check("Add Action has Media Playlist, Audio Playlist and Message, after Macro (and Video Input after Media Playlist)", /Macro, Media Playlist, Video Input, Audio Playlist, Message, Communication\(off\)$/.test(labels()), labels())
                hover("Media Playlist")
                return 500
            },
            () => {
                check("Media Playlist has the media bin's playlists, in their folders", labels() === "[Media Playlist], Services, Backgrounds", labels())
                hover("Services")
                return 500
            },
            () => {
                check("a folder leads to its playlists", labels() === "[Services], Walk In", labels())
                click(menuRow("Walk In"))
                return 600
            },
            () => {
                const made = actionsOf(0, "mediaPlaylist")[0]
                check("the slide plays Walk In, by its id and its name, Always Retrigger off", made !== undefined && made.playlistId === t.walkIn
                      && made.playlistName === "Walk In" && !made.retrigger && made.title === "Play the media playlist “Walk In”", JSON.stringify(made))
                const icon = badge(0, "mediaPlaylist")
                check("its icon is on the slide, wide for a picture of a list", icon !== null && icon.visible && icon.width === 22)
                click(centre(icon), Qt.RightButton)
                return 300
            },
            () => {
                check("the icon's menu says what it does and has Always Retrigger, off", labels() === "(note), Always Retrigger, Remove", labels())
                click(menuRow("Always Retrigger"))
                return 600
            },
            () => {
                const made = actionsOf(0, "mediaPlaylist")[0]
                check("Always Retrigger on", made.retrigger === true && made.title === "Play the media playlist “Walk In”, again if it is playing", JSON.stringify(made))
                click(centre(tab("audio")))
                return 500
            },
            // ---- dragged out of the audio bin's list
            () => {
                const row = listRow("audioPlaylistList", "Before the Service")
                check("the audio bin lists its playlists", row !== null, named("audioPlaylistList").count)
                dragTo(centre(listRow("audioPlaylistList", "Services")), centre(slideCell(1)))
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 400
            },
            () => {
                check("a folder of playlists dragged onto a slide gives it nothing", document.slides[1].actions.length === 0, document.slides[1].actions.length)
                dragTo(centre(listRow("audioPlaylistList", "Before the Service")), centre(slideCell(1)))
                return 300
            },
            () => {
testInput.mouse(2, kept.to.x, kept.to.y)
                return 600
            },
            () => {
                const made = actionsOf(1, "audioPlaylist")[0]
                check("a playlist of the audio bin dragged onto a slide gives it the action that plays it", made !== undefined && made.playlistId === t.beforeService
                      && made.playlistName === "Before the Service" && !made.retrigger, JSON.stringify(document.slides[1].actions))
                check("its icon is the audio bin's notes beside a list", badge(1, "audioPlaylist") !== null && badge(1, "audioPlaylist").visible)
                click(centre(tab("messages")))
                return 500
            },
            // ---- dragged out of the messages' list: the panel for its tokens
            () => {
                dragTo(centre(listRow("messageList", "Parent Wanted")), centre(slideCell(2)))
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 600
            },
            () => {
                const name = named("actionToken:Name")
                const room = named("actionToken:Room")
                check("a message with tokens dropped on a slide brings up the panel, a box for each token, with what each was last given",
                      actionDialog.mode === "message" && name !== null && name.text === "Sam" && room !== null && room.text === "Foyer",
                      actionDialog.mode + " " + (name ? name.text : "-") + " " + (room ? room.text : "-"))
                name.text = "Alex"
                name.textEdited()
                testInput.grab("message-panel")
                click(centre(named("actionAccept")))
                return 600
            },
            () => {
                const made = actionsOf(2, "message")[0]
                const words = made ? wordsOf(made.values) : ({})
                check("the slide shows Parent Wanted, giving Name the words typed, and Room what it had", made !== undefined && made.messageId === t.parentWanted
                      && words.Name === "Alex" && words.Room === "Foyer", JSON.stringify(made))
                check("its icon is ProPresenter's message", badge(2, "message") !== null)
                showAddActionMenu({ slide: 3 }, slideCell(3), 10, 10)
                return 300
            },
            // ---- Add Action → Message, of a message with no tokens
            () => {
                hover("Message")
                return 500
            },
            () => {
                check("Message has the workspace's messages, one whose tokens are given words with a panel to come", labels() === "[Message], Parent Wanted…, Car Lights", labels())
                click(menuRow("Car Lights"))
                return 600
            },
            () => {
                const made = actionsOf(3, "message")[0]
                check("one with no tokens needs nothing more: no panel", actionDialog.mode === "" && made !== undefined && made.messageName === "Car Lights"
                      && made.values.length === 0, JSON.stringify(made))
                dragTo(centre(listRow("mediaPlaylistList", "Backgrounds")), centre(slideCell(3)))
                return 300
            },
            // ---- dragged out of the media bin's list, onto a slide and onto a macro
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 600
            },
            () => {
                const made = actionsOf(3, "mediaPlaylist")[0]
                check("a playlist of the media bin dragged onto a slide", made !== undefined && made.playlistId === t.backgrounds, JSON.stringify(document.slides[3].actions))
                click(centre(tab("macros")))
                return 500
            },
            () => {
                const row = Lib.findAll(named("macroList"), i => i.objectName === "macroRow" && !i.heading)[0]
                kept.macro = row.modelData.id
                kept.macroActions = row.modelData.actions.length
                dragTo(centre(listRow("mediaPlaylistList", "Backgrounds")), centre(row))
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 600
            },
            () => {
                const macro = macroOf(kept.macro)
                const made = macro ? macro.actions.filter(a => a.kind === "mediaPlaylist") : []
                check("and onto a macro", macro !== undefined && macro.actions.length === kept.macroActions + 1 && made.length === 1 && made[0].playlistName === "Backgrounds",
                      macro ? JSON.stringify(macro.actions.map(a => a.kind)) : "no macro")
                testInput.grab("slides-with-actions")
                click(centre(slideCell(0)))
                return 1500
            },
            // ---- the show: Walk In, from its first row
            () => {
                check("the first slide plays Walk In from its first row: the red picture, from that playlist", liveMedia !== null && liveMedia.name === "Red.png"
                      && Show.liveMediaPlaylistId === t.walkIn, (liveMedia ? liveMedia.name : "-") + " " + Show.liveMediaPlaylistId)
                check("the output shows it round the slide's block", reddish(outputAt(0.8, 0.8)), outputAt(0.8, 0.8))
                check("and so does the operator's preview", reddish(previewAt(0.8, 0.8)), previewAt(0.8, 0.8))
                testInput.grabOutput("walk-in-red")
                openMediaPlaylist(t.walkIn)
                return 500
            },
            () => {
                click(centre(binRow("Green")))
                return 1200
            },
            () => {
                check("Green, clicked in Walk In: the playlist is still the one playing", liveMedia.name === "Green.png" && Show.liveMediaPlaylistId === t.walkIn
                      && greenish(outputAt(0.8, 0.8)), liveMedia.name + " " + outputAt(0.8, 0.8))
                click(centre(slideCell(0)))
                return 1500
            },
            () => {
                check("Always Retrigger: the slide starts Walk In again, from its first row", liveMedia.name === "Red.png" && reddish(outputAt(0.8, 0.8))
                      && logged(/action .*“Walk In”, again if it is playing, which is playing already: from its start again/).length === 1,
                      liveMedia.name + " " + outputAt(0.8, 0.8))
                click(centre(badge(0, "mediaPlaylist")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Always Retrigger"))
                return 500
            },
            () => {
                check("Always Retrigger off again", actionsOf(0, "mediaPlaylist")[0].retrigger === false)
                click(centre(binRow("Green")))
                return 1200
            },
            () => {
                click(centre(slideCell(0)))
                return 1500
            },
            () => {
                check("off, a playlist that is playing is left to play on", liveMedia.name === "Green.png" && greenish(outputAt(0.8, 0.8)) && greenish(previewAt(0.8, 0.8))
                      && logged(/action .*“Walk In”: it is playing already, and is left to play on/).length === 1, liveMedia.name + " " + outputAt(0.8, 0.8))
                // (Back on, for what the file is looked at for after)
                click(centre(badge(0, "mediaPlaylist")), Qt.RightButton)
                return 300
            },
            () => {
                click(menuRow("Always Retrigger"))
                return 500
            },
            // ---- Before the Service
            () => {
                check("on again", actionsOf(0, "mediaPlaylist")[0].retrigger === true)
                click(centre(slideCell(1)))
                return 1000
            },
            () => {
                check("the second slide plays Before the Service from its first piece, as music of that playlist", liveMusic !== null && liveMusic.name === "Walk In"
                      && Show.liveMusicPlaylistId === t.beforeService, (liveMusic ? liveMusic.name : "-") + " " + Show.liveMusicPlaylistId)
                kept.play = liveMusic.play
                click(centre(slideCell(1)))
                return 800
            },
            () => {
                check("again: it is playing, and is left to play on", liveMusic.name === "Walk In" && liveMusic.play === kept.play, liveMusic.name + " " + liveMusic.play)
                return 3000
            },
            () => {
                check("at its end the next of the playlist follows", liveMusic !== null && liveMusic.name === "Second Tune", liveMusic ? liveMusic.name : "-")
                kept.before = outputAt(0.95, 0.95)
                click(centre(slideCell(2)))
                return 1200
            },
            // ---- Parent Wanted, with the action's words
            () => {
                check("the third slide shows Parent Wanted, with the words its action gives Name", Show.liveMessages.includes(t.parentWanted)
                      && Messages.words(t.parentWanted) === "Parent of Alex to the Foyer", Messages.words(t.parentWanted))
                const after = outputAt(0.95, 0.95)
                check("the output has it drawn over the foot of the screen", after !== kept.before, kept.before + " then " + after)
                testInput.grabOutput("parent-wanted-alex")
                testInput.grab("parent-wanted-operator")
                const own = Messages.messages.find(m => m.id === t.parentWanted).tokens.find(token => token.name === "Name")
                check("the message's own Name is as it was: Sam", own.value === "Sam", own.value)
                showMessage(t.parentWanted)
                return 800
            },
            () => {
                check("shown from its tab, it has its own words", Messages.words(t.parentWanted) === "Parent of Sam to the Foyer", Messages.words(t.parentWanted))
                click(centre(badge(2, "message")), Qt.RightButton)
                return 300
            },
            () => {
                check("the action's menu has Change… for a message with tokens that are given words", labels() === "(note), Change…, Remove", labels())
                click(menuRow("Change…"))
                return 500
            },
            () => {
                const name = named("actionToken:Name")
                const room = named("actionToken:Room")
                check("the panel has the words the action gives", actionDialog.mode === "message" && actionDialog.existing !== null && name.text === "Alex"
                      && room.text === "Foyer", name.text + " " + room.text)
                room.text = "Hall"
                room.textEdited()
                click(centre(named("actionAccept")))
                return 600
            },
            () => {
                const made = actionsOf(2, "message")[0]
                const words = wordsOf(made.values)
                check("changed: one action still, Room now Hall", actionsOf(2, "message").length === 1 && words.Name === "Alex" && words.Room === "Hall", JSON.stringify(made.values))
                click(centre(slideCell(2)))
                return 1000
            },
            () => {
                check("shown again by the slide, with its words", Messages.words(t.parentWanted) === "Parent of Alex to the Hall", Messages.words(t.parentWanted))
                clearAudio()
                return 400
            },
            // ---- ProPresenter's own
            () => {
                click(centre(slideCell(4)))
                return 1200
            },
            () => {
                check("ProPresenter's Audio Bin Playlist finds the playlist by its name, its id being no playlist's", liveMusic !== null && liveMusic.name === "Walk In"
                      && Show.liveMusicPlaylistId === t.beforeService, (liveMusic ? liveMusic.name : "-") + " " + Show.liveMusicPlaylistId)
                check("and its Message shows Car Lights", Show.liveMessages.includes(t.carLights) && Messages.words(t.carLights) === "A car has its lights on",
                      Messages.words(t.carLights))
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
