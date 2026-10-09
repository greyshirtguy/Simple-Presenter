import QtQuick
import QtQuick.Window
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Screens: the two a workspace starts with, more of each kind added in the settings, what
// each is sent out through (a window, a display, NDI, nothing), a video played once for
// every audience screen, a layout for each stage screen, and the list as it is written
// to the workspace's file.
QtObject {
    id: t

//COMMON
    function screen(id) {
        return Screens.screens.find(s => s.id === id)
    }

    // What an audience scene's slide layer holds: the ids of the slides that are content in it
    function slidesIn(scene) {
        return Lib.findAll(scene.contentItem ?? scene, item => item.content !== undefined && item.content !== null && item.content.id !== undefined
                           && item.content.elements !== undefined).map(item => item.content.id)
    }

    function mediaItems(scene) {
        return Lib.findAll(scene.contentItem ?? scene, item => item.leads !== undefined && item.content !== undefined && item.content !== null)
    }

    function settingsRows() {
        return Lib.findAll(named("screensSettings"), item => String(item.objectName) === "screenRow")
    }

    function run() {
        steps = [
            () => {
                const file = catalog.workspacePath + "/Configuration/Workspace"
                check("a workspace whose file lists no screens has an audience screen and a stage screen", Screens.screens.length === 2
                      && Screens.audience.length === 1 && Screens.stage.length === 1 && Screens.audience[0].name === "Audience" && Screens.stage[0].name === "Stage"
                      && !testInput.exists(file), JSON.stringify(Screens.screens.map(s => s.name)))
                check("each is a window of its own, as the output and the stage display always were", Screens.screens.every(s => s.output === "window" && s.first)
                      && audienceIds.length === 1 && stageIds.length === 1 && output !== null && output.contentItem !== undefined && output.leads === true)
                check("the computer's displays are listed by name", Screens.displays.length === 2 && Screens.displays.every(d => d.name !== "" && d.width > 0),
                      JSON.stringify(Screens.displays))
                kept.first = Screens.audience[0].id
                kept.stage = Screens.stage[0].id
                // ---- the settings
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "screens"
                return 400
            },
            () => {
                check("the settings have a line for each screen", settingsRows().length === 2, settingsRows().length)
                testInput.grab("1-settings")
                click(centre(named("addAudienceScreen")))
                return 400
            },
            () => {
                const added = Screens.audience[1]
                check("an audience screen is added from the settings, named for its place", Screens.audience.length === 2 && added !== undefined && added.name === "Audience 2", JSON.stringify(Screens.audience.map(s => s.name)))
                check("it is sent out through nothing until it is said what, so nothing is drawn for it", added.output === "none" && !added.first && audienceIds.length === 1
                      && settingsRows().length === 3)
                check("and the list is in the workspace's file now, the two it had with it", testInput.exists(catalog.workspacePath + "/Configuration/Workspace"))
                kept.second = added.id
                // ---- a second window
                Screens.setOutput(kept.second, { output: "window" })
                return 800
            },
            () => {
                check("given a window, it has a scene of its own, which does not play the media itself", audienceIds.length === 2 && scenes[kept.second] !== undefined
                      && scenes[kept.second].contentItem !== undefined && scenes[kept.second].leads === false && output === scenes[kept.first] && output.leads === true)
                settingsOpen = false
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                goLive(6)
                return 3500
            },
            () => {
                const slide = document.slides[6].id
                check("a slide that goes live is on every audience screen", slidesIn(output).includes(slide) && slidesIn(scenes[kept.second]).includes(slide),
                      slidesIn(output).length + " and " + slidesIn(scenes[kept.second]).length)
                const players = Lib.findAll(output.contentItem, item => item.playbackState !== undefined && item.source !== undefined)
                    .concat(Lib.findAll(scenes[kept.second].contentItem, item => item.playbackState !== undefined && item.source !== undefined))
                const fed = mediaItems(scenes[kept.second])[0]
                check("its video is played once, by the first screen, and the other is fed its frames", liveMedia !== null && liveMedia.video
                      && output.livePlayer !== null && output.livePlayer.playbackState === MediaPlayer.PlayingState && scenes[kept.second].livePlayer === null
                      && fed !== undefined && fed.ready === true && fed.videoSink !== null && fed.videoSink.videoSize.width > 0,
                      (fed ? fed.ready + " " + (fed.videoSink ? fed.videoSink.videoSize : "no sink") : "nothing on the second screen"))
                check("with the number the playing was given", lastPlayed !== null && lastPlayed.playId === plays && plays >= 1 && MediaFeeds.sink(plays) === output.liveVideoSink)
                testInput.grabOutput("2-first-screen")
                // ---- a display
                Screens.setOutput(kept.second, { output: "display", display: Screens.displays[1].name })
                return 1500
            },
            () => {
                const window = scenes[kept.second]
                check("set to a display, the screen fills that display", screen(kept.second).output === "display" && screen(kept.second).displayThere === true
                      && window.visibility === Window.FullScreen && window.screen.name === Screens.displays[1].name && window.width === Screens.displays[1].width,
                      window.visibility + " on " + window.screen.name + " " + window.width)
                check("and still shows what is live", slidesIn(window).includes(document.slides[6].id) && output.livePlayer.playbackState === MediaPlayer.PlayingState)
                Screens.setOutput(kept.second, { output: "display", display: "NO-SUCH-1" })
                return 800
            },
            () => {
                check("a screen set to a display that is not plugged in is not shown, and says so", screen(kept.second).displayThere === false && scenes[kept.second].visible === false)
                // ---- the toggles
                outputEnabled = false
                Screens.setOutput(kept.second, { output: "window" })
                return 600
            },
            () => {
                check("the audience button switches every audience screen off", output.visible === false && scenes[kept.second].visible === false)
                outputEnabled = true
                return 600
            },
            () => {
                check("and on", output.visible === true && scenes[kept.second].visible === true)
                // ---- NDI
                Screens.setOutput(kept.second, { output: "ndi", ndiName: "Test Screen", ndiWidth: 1280, ndiHeight: 720, ndiRate: "30" })
                return 3500
            },
            () => {
                const sender = senders[kept.second]
                check("set to NDI, the screen has a sender of that name, size and rate, and a scene nobody sees", sender !== undefined && sender.name === "Test Screen"
                      && sender.width === 1280 && sender.height === 720 && sender.rateNumerator === 30 && sender.rateDenominator === 1 && scenes[kept.second] !== undefined
                      && scenes[kept.second].contentItem === undefined && scenes[kept.second].width === 1280)
                if (Ndi.available) {
                    check("with NDI's library here, it is on the network, and pictures go out", sender.sending === true && sender.problem === "" && sender.framesSent >= 2,
                          sender.sending + " " + sender.framesSent + " " + sender.problem)
                    check("the scene that is sent shows what is live", slidesIn(scenes[kept.second]).includes(document.slides[6].id) && mediaItems(scenes[kept.second]).length > 0)
                    kept.sent = sender.framesSent
                } else {
                    check("without NDI's library it is not, and says where to get it", sender.sending === false && Ndi.problem.includes("ndi.video"), Ndi.problem)
                }
                outputEnabled = false
                return 600
            },
            () => {
                check("the audience button takes an NDI screen off the network too", senders[kept.second].sending === false)
                outputEnabled = true
                Screens.setOutput(kept.second, { ndiRate: "59.94" })
                return 500
            },
            () => {
                check("a rate such as 59.94 is 60000 over 1001", senders[kept.second].rateNumerator === 60000 && senders[kept.second].rateDenominator === 1001)
                Screens.setOutput(kept.second, { output: "none" })
                // ---- stage screens
                const made = StageLayouts.add()
                kept.layout = made.id
                check("a second stage screen is added", Screens.add("stage") === "" && Screens.stage.length === 2 && Screens.stage[1].name === "Stage 2")
                kept.stage2 = Screens.stage[1].id
                Screens.setOutput(kept.stage2, { output: "window" })
                sidePanel.showControlTab = "stage"
                return 800
            },
            () => {
                check("it has a window, and a line on the stage tab", stageIds.length === 2 && named("stageScreenRow2") !== undefined && named("stageLayoutChoice2") !== undefined)
                Show.setStageLayout(kept.stage2, kept.layout)
                check("each stage screen has its own layout", Show.screenLayouts[kept.stage2] === kept.layout && (Show.screenLayouts[kept.stage] ?? "") === ""
                      && Show.stageLayoutId === "" && named("stageLayoutChoice2").currentIndex === 1 && named("stageLayoutChoice").currentIndex === 0)
                // A stage action for the second alone
                commitAction({ slide: 1 }, { kind: "stage", assignments: Show.stageAssignmentsFor(null, { [kept.stage]: StageLayouts.layouts[0], [kept.stage2]: null }) }, "")
                const action = document.slides[1].actions.find(a => a.kind === "stage")
                check("a stage action names every stage screen", action !== undefined && action.assignments.length === 2 && action.assignments[0].screenId === kept.stage
                      && action.assignments[0].layoutId === kept.layout && action.assignments[1].screenId === kept.stage2 && action.assignments[1].layoutId === "",
                      JSON.stringify(action ? action.assignments : null))
                Show.setStageLayout(kept.stage2, "")
                goLive(1)
                check("and done, gives each the layout it says and leaves the others", Show.screenLayouts[kept.stage] === kept.layout && (Show.screenLayouts[kept.stage2] ?? "") === "")
                testInput.grab("3-stage-tab")
                // ---- removing
                check("a screen is removed", Screens.remove(kept.stage2) === "" && Screens.stage.length === 1 && stageIds.length === 1)
                check("and the audience's second", Screens.remove(kept.second) === "" && Screens.audience.length === 1)
                check("but not its last", Screens.remove(kept.first) !== "" && Screens.audience.length === 1)
                check("a screen is renamed", Screens.rename(kept.first, "Sanctuary") === "" && Screens.audience[0].name === "Sanctuary" && Screens.audience[0].id === kept.first)
                for (let i = Screens.screens.length; i < Screens.limit; ++i)
                    Screens.add(i % 2 ? "stage" : "audience")
                check("there is room for sixteen, and no more", Screens.screens.length === 16 && Screens.add("audience") !== "" && Screens.screens.length === 16
                      && audienceIds.length === 1 && stageIds.length === 1)
                return 300
            },
            () => {
                check("through all of it the show went on", output === scenes[kept.first] && liveIndex === 1 && !cleared)
                return 100
            }
        ]
        next()
    }
}
