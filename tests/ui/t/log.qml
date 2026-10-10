import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The session's log: where it is, what it says the app is running on, a line for each
// thing done, what Qt says (and not too much of it), an error shown to the user, the app
// not answering for a while, a log grown long, and what a line costs.
QtObject {
    id: t

//COMMON
    function text() {
        return testInput.readText(Log.path)
    }

    // The lines of the log of one kind, without their time and kind
    function of(kind) {
        return text().split("\n").filter(line => line.length > 26 && line.substring(14, 24).trim() === kind).map(line => line.substring(26))
    }

    function said(kind, part) {
        return of(kind).some(line => line.includes(part))
    }

    function last(kind) {
        const lines = of(kind)
        return lines.length > 0 ? lines[lines.length - 1] : ""
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                check("the window says the app and its version, and the line over the slides what is open", win.title === ("Simple Presenter " + Qt.application.version) && /^\d+\.\d+$/.test(Qt.application.version) && named("gridTitle").text === "Move Of God", win.title)
                check("the log is in the app's folder in Documents, named for when the app started",
                      /\/log\/docs\/SimplePresenter\/Logs\/SimplePresenter \d{4}-\d\d-\d\d \d\d\.\d\d\.\d\d( \(\d+\))?\.log$/.test(Log.path), Log.path)
                const all = text()
                check("it starts by saying what it is", all.startsWith(("Simple Presenter " + Qt.application.version + " session log, started ")) && all.includes("for working out what happened")
                      && all.includes("something that went wrong"))
                check("then the app, its version and Qt's, and its process", /^Simple Presenter \d+\.\d+, with Qt 6\.\d+\.\d+ \(built with 6\.\d+\.\d+\); process \d+$/.test(of("start")[0]), of("start")[0])
                check("how it was started", of("start")[1].startsWith("started as: ") && of("start")[1].includes("--workspace"))
                check("the system, the processor and the memory", of("system").length === 3 && /Linux \d/.test(of("system")[0]) && / processors; [\d.]+ GB of memory, [\d.]+ GB of it free$/.test(of("system")[1])
                      && of("system")[2].startsWith("environment: "), of("system").join(" | "))
                check("what Qt draws through, and the screens", /^Qt is drawing through "wayland"; \d+ screens?$/.test(of("display")[0]) && /^screen 0 ".*": \d+x\d+ at /.test(of("display")[1]),
                      of("display").join(" | "))
                check("what draws the windows: the graphics chip and its driver", /^operator window: .+; OpenGL.* \d\.\d.*; shading language .+; driver by .+; drawn on /.test(of("graphics")[0]), of("graphics").join(" | "))
                check("once, the other windows being drawn by the same", of("graphics").filter(line => line.endsWith(": the same")).length === 2, of("graphics").join(" | "))
                check("the workspace and what it holds", said("workspace", "opening ") && said("workspace", "\"Demo\" at ") && / 1 library, \d+ playlists?, \d+ media playlists, 1 timer, 0 props, 0 stage layouts$/.test(last("workspace")),
                      last("workspace"))
                check("how the app is set", /^transition ".+" over [\d.]+ s; output window on; stage window on, with the plain view; media bin shown; this window \d+x\d+/.test(last("settings")), last("settings"))
                check("the three windows", said("window", "operator window: a window of") && said("window", "output window: a window of 300x189") && said("window", "stage window: a window of 300x189"),
                      of("window").join(" | "))
                check("the presentation opened", last("open") === "\"Move Of God\" [Default], 43 slides, from the library \"Demo\"", last("open"))
                check("what Qt itself said in starting", said("qt", "qt.multimedia.ffmpeg: Using Qt multimedia with FFmpeg"), of("qt").join(" | "))
                // (Where this is run there is no session bus, which Qt's theme warns of; that is the test's doing.)
                const wrong = all.split("\n").filter(line => /^\d\d:\d\d:\d\d\.\d\d\d  [A-Z]{4,}/.test(line) && !line.includes("qt.qpa.theme"))
                check("nothing in capitals so far", wrong.length === 0, wrong.join(" | ").replace(/dbus/gi, "d-bus"))
                check("every line has its time and its kind in their columns", all.split("\n").slice(7).every(line => line === "" || /^\d\d:\d\d:\d\d\.\d\d\d  [A-Za-z ]{10}  \S/.test(line) || line.startsWith("                          ")),
                      all.split("\n").slice(7).find(line => line !== "" && !/^\d\d:\d\d:\d\d\.\d\d\d  [A-Za-z ]{10}  \S/.test(line)))
                // ---- a slide goes live, with a video and a transition
                selectTransition("Dissolve")
                transitionDuration = 0.6
                goLive(0)
                return 2500
            },
            () => {
                check("a slide going live is a line, with its media", /^slide 1 of 43 of "Move Of God" \(.*\); with its background video "Hopeful Horizon Bliss - 4K.mp4", looping(; and has \d+ actions?)?$/.test(last("live")), last("live"))
                check("the transition is in the log before its shader is used, by name and by file", said("transition", "media layer: Dissolve over 0.60 s, shaders/dissolve.frag.qsb"), of("transition").join(" | "))
                const done = of("transition").find(line => line.startsWith("media layer: Dissolve done: "))
                const frames = done ? Number(/done: (\d+) frames/.exec(done)[1]) : 0
                check("and again when it is over, with the frames it was drawn in", done !== undefined && /done: \d+ frames in 0\.\d\d s, \d+ a second$/.test(done) && frames >= 10 && frames <= 40, done)
                check("what the video turned out to be", /^video "Hopeful Horizon Bliss - 4K.mp4": first picture after \d+ ms; 3840x2160, \w+, frames arriving (as textures|in memory) \(.*\); .*, [\d.]+ frames a second, [\d.]+ s long; no sound in the file$/.test(last("media")), last("media"))
                testInput.say("  " + last("media"))
                testInput.say("  " + done)
                selectTransition("Ripple Wave")
                goLive(1)
                return 1500
            },
            () => {
                check("choosing a transition is a line", said("transition", "chosen: Ripple Wave"))
                check("a transition says what it is set to", said("transition", "slide layer: Ripple Wave over 0.60 s, shaders/gl-transitions/ripple.frag.qsb; options 100, 50, 0, 0"), of("transition").slice(-3).join(" | "))
                check("the next slide, whose media is the one playing", last("live").startsWith("slide 2 of 43 of \"Move Of God\""), last("live"))
                goLive(2)
                goLive(3)
                return 1500
            },
            () => {
                check("a transition the next change cuts short says so", of("transition").some(line => /^slide layer: Ripple Wave cut short at \d+% by the next change: \d+ frames in/.test(line)),
                      of("transition").slice(-4).join(" | "))
                clearSlide()
                clearMedia()
                return 900
            },
            () => {
                check("the clears", of("clear").join("|") === "the slide|the media, \"Hopeful Horizon Bliss - 4K.mp4\"", of("clear").join("|"))
                showMedia(mediaFiles.find(m => !m.video), mediaPlaylistId)
                return 900
            },
            () => {
                testInput.key(Qt.Key_F1)
                return 900
            },
            () => {
                check("media from the bin", of("media").some(line => /^(background|foreground) picture ".+"$/.test(line)), of("media").slice(-2).join(" | "))
                check("clearing everything, and what there was to clear", of("clear").slice(-2)[0] === "everything asked for" && last("clear").startsWith("the media, "), of("clear").slice(-2).join("|"))
                // ---- timers, props, the stage, the editor
                const id = Timers.timers[0].id
                Timers.start(id)
                Timers.stop(id)
                Timers.reset(id)
                Timers.act({ action: 0, timerId: "none", timerName: "Nobody" })
                const added = Props.add("")
                toggleProp(added.id)
                toggleProp(added.id)
                kept.prop = added.id
                const layout = StageLayouts.add()
                Show.stageLayoutId = layout.id
                startEditingStage(layout.id)
                return 800
            },
            () => {
                stopEditing()
                Show.stageLayoutId = ""
                return 400
            },
            () => {
                check("a timer started, stopped and put back", of("timer").slice(0, 3).join("|") === "\"Countdown\" started, at 0:05:00|\"Countdown\" stopped, at 0:05:00|\"Countdown\" put back to its start", of("timer").join("|"))
                check("an action for a timer that is not there, which the user is told nothing of", last("timer") === "a slide's action is for a timer that is not here, \"Nobody\": nothing done", last("timer"))
                check("a prop on and off", of("prop").join("|") === "\"Prop\" on; 1 on|\"Prop\" off; 0 on", of("prop").join("|"))
                check("the files that were written", of("saved").some(line => line.endsWith("/Configuration/Props")) && of("saved").some(line => line.endsWith("/Configuration/Stage")), of("saved").join(" | "))
                check("the stage's layout", of("stage").join("|") === "the stage has the layout \"Layout\"|the stage has the plain view", of("stage").join("|"))
                check("the editor opened and closed", of("edit").join("|") === "the editor opened on the stage layouts, layout 1 of 1|the editor closed; nothing was changed", of("edit").join("|"))
                // ---- something going wrong
                report("The file could not be written")
                Log.problem("A made-up problem")
                console.warn("a made-up warning")
                return 300
            },
            () => {
                check("an error shown to the user is in the log, in capitals", last("PROBLEM") === "A made-up problem" && of("PROBLEM")[0] === "Shown to the user: The file could not be written", of("PROBLEM").join("|"))
                check("and so is a warning from Qt or from QML", last("WARNING").includes("a made-up warning"), last("WARNING"))
                notice = ""
                // the same thing said over and over is counted, not written
                for (let i = 0; i < 500; ++i)
                    console.warn("the same thing again")
                Log.note("test", "after the repeats")
                return 300
            },
            () => {
                check("a message that repeats is written once", of("WARNING").filter(line => line.includes("the same thing again")).length === 1)
                check("with how often it came again", said("qt", "The message above came 499 more times, the last at "), of("qt").slice(-1)[0])
                // a flood of different ones is let through at a trickle
                kept.before = of("WARNING").length
                for (let i = 0; i < 400; ++i)
                    console.warn("flood " + i)
                return 2300
            },
            () => {
                console.warn("after the flood")
                return 300
            },
            () => {
                const through = of("WARNING").length - kept.before
                check("of a flood only so much gets through", through >= 50 && through <= 66, through + " of 401")
                check("and the log says how much it left out", of("qt").some(line => /^\d+ messages from Qt are left out here, from \d\d:\d\d:\d\d\.\d\d\d on/.test(line)), of("qt").slice(-1)[0])
                // ---- what a line costs
                const lines = 20000
                const started = testInput.now()
                const cpu = testInput.cpu()
                for (let i = 0; i < lines; ++i)
                    Log.note("test", "A line of about the length a line usually is, to see what one costs: number " + i)
                const each = (testInput.now() - started) * 1000 / lines
                testInput.say("  " + lines + " lines took " + (testInput.now() - started) + " ms: " + each.toFixed(1) + " millionths of a second each (" + (testInput.cpu() - cpu).toFixed(0) + " ms of processor time)")
                check("a line costs a few millionths of a second", each < 25, each.toFixed(1))
                // which is also more than one file holds
                check("a log grown long goes on in a new file, the one before kept beside it", testInput.exists(Log.path.replace(/\.log$/, " (earlier).log")))
                const all = text()
                check("the new one starts with what the app is running on", all.startsWith("Simple Presenter session log, continued.") && all.includes("  start       Simple Presenter " + Qt.application.version)
                      && all.includes("  graphics    operator window: ") && all.includes("  log         Continued from \"SimplePresenter "), all.substring(0, 200))
                check("and is a good deal smaller than the limit", all.length < 1500000, all.length)
                // ---- the app not answering
                Log.note("test", "about to be busy for four seconds")
                const until = Date.now() + 4000
                while (Date.now() < until) {
                }
                return 500
            },
            () => {
                check("the app not answering for a while is in the log", /^The app has not answered for \d seconds: it is busy with what the lines above say, or stuck in it\.$/.test(last("STALLED")), last("STALLED"))
                check("and so is its coming back, with how long it was", /^The app is answering again\. It did not for at least \d\.\d seconds\.$/.test(last("recovered")), last("recovered"))
                const lines = text().split("\n").filter(line => line.includes("  STALLED  ") || line.includes("  recovered  ") || line.includes("about to be busy"))
                testInput.say("  " + lines.join("\n  "))
                // ---- after a minute, what the app is using
                kept.wait = Math.max(0, 63000 - (testInput.now() - kept.startedAt))
                return 100
            },
            () => kept.wait,
            () => {
                check("after a minute, a line says what the app has been using", /^Over the last \d+ (s|min \d+ s): [\d.]+% of one processor; \d+ MB of memory in use now, \d+ MB at most; frames shown: (operator|output|stage) window \d+, (operator|output|stage) window \d+, (operator|output|stage) window \d+$/.test(last("health")),
                      last("health"))
                testInput.say("  " + last("health"))
                Props.remove(kept.prop)
                // ---- where the settings say the log is
                settingsOpen = true
                return 400
            },
            () => {
                const settingsScreen = Lib.find(win.contentItem, item => item.sections !== undefined && item.section !== undefined && item.useX11 !== undefined)
                check("the settings have an About section", settingsScreen !== null && settingsScreen.sections.map(x => x.name).join() === "Groups,Slides,Screens,Windows,About",
                      settingsScreen ? settingsScreen.sections.map(x => x.name).join() : "none")
                const row = Lib.find(settingsScreen, item => item.text === "About" && item.font !== undefined)
                click(centre(row))
                kept.settings = settingsScreen
                return 400
            },
            () => {
                const texts = Lib.findAll(kept.settings, item => item.text !== undefined && item.font !== undefined && item.visible && typeof item.text === "string").map(item => item.text)
                check("which says the version", kept.settings.section === "about" && texts.includes(("Simple Presenter " + Qt.application.version)), kept.settings.section)
                const what = named("whatThisIs")
                check("and, in a box that cannot be missed, that this is an experiment and not a product", what !== null && what.visible && what.height > 60
                      && texts.some(x => x.startsWith("<b>This is a personal experiment, not a product.</b>") && x.includes("vibe coded") && x.includes("Nobody supports it")
                                         && x.includes("no warranty of any kind") && x.includes("never your only one")), what ? what.height : "none")
                check("that it is free software, and nothing to do with ProPresenter's makers", texts.some(x => x.includes("under the MIT licence")
                      && x.includes("nothing to do with Renewed Vision")))
                check("and what this run's log is called and where it is", texts.some(x => x.includes("This run's log is “" + Log.path.substring(Log.path.lastIndexOf("/") + 1) + "”, in " + Log.folder + ".")
                      && x.includes("nothing of what is in them")) && Log.folder.endsWith("/SimplePresenter/Logs"), Log.folder)
                const button = named("showLogsFolder")
                check("with a button to show the folder", button !== null && button.visible && button.enabled && button.text === "Show the Logs Folder")
                testInput.grab("1-about")
                settingsOpen = false
                return 300
            }
        ]
        kept.startedAt = testInput.now()
        next()
    }
}
