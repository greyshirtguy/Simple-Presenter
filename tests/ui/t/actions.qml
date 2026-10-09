import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// What a slide's cue does to a timer when the slide goes live, for cues as ProPresenter
// writes them: which timer it means (by its id, or failing that by its name, or none),
// and each thing it can do.
QtObject {
    id: t

//COMMON
    function timer(name) {
        return Timers.timers.find(x => x.name === name)
    }

    function state(name) {
        return Timers.state(timer(name).id)
    }

    function all() {
        return ["One", "Two", "Elapsed"].map(name => name + (state(name).running ? " running " : " stopped ") + Math.round(state(name).seconds)).join(", ")
    }

    function run() {
        const near = (a, b) => Math.abs(a - b) < 1.6
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Timer Actions"))
                check("the presentation made for this opens, eleven slides", document !== null && document.name === "Timer Actions" && document.slides.length === 11, document ? document.slides.length : "none")
                check("its timers are the three made for it", Timers.timers.map(x => x.name).join() === "One,Two,Elapsed" && timer("One").duration === 100 && timer("Two").duration === 200,
                      JSON.stringify(Timers.timers.map(x => [x.name, x.duration])))
                check("each slide has the actions its cue has", document.slides.map(s => s.actions.length).join("") === "11111111210", document.slides.map(s => s.actions.length).join(""))
                check("an action says which timer by id and by name", document.slides[0].actions[0].timerId === timer("One").id && document.slides[0].actions[0].timerName === "Two"
                      && document.slides[0].actions[0].action === 3, JSON.stringify(document.slides[0].actions[0]))
                check("nothing runs before a slide is live", !state("One").running && !state("Two").running && !state("Elapsed").running, all())
                kept.file = testInput.fileHash(catalog.workspacePath + "/Configuration/Timers")
                notice = ""
                goLive(0)
                return 1300
            },
            () => {
                check("id and name disagree: the timer with the id is the one, and it starts", state("One").running && near(state("One").seconds, 99), all())
                check("the timer the name names is left alone", !state("Two").running && state("Two").seconds === 200, all())
                goLive(1)
                return 1300
            },
            () => {
                check("no timer has the id: the timer of that name starts", state("Two").running && near(state("Two").seconds, 199), all())
                check("and the one already running runs on", state("One").running && near(state("One").seconds, 97.7), all())
                kept.before = all()
                goLive(2)
                return 800
            },
            () => {
                check("no timer has the id or the name: nothing happens", state("One").running && state("Two").running && !state("Elapsed").running && Timers.timers.length === 3, all())
                check("and nothing is said about it", notice === "", notice)
                check("the slide still goes live", liveIndex === 2 && !cleared)
                goLive(3)
                return 500
            },
            () => {
                kept.one = state("One").seconds
                check("stop stops the timer where it is", !state("One").running && kept.one < 97 && kept.one > 90, all())
                check("and no other", state("Two").running)
                return 1200
            },
            () => {
                check("(it stays there)", state("One").seconds === kept.one)
                goLive(4)
                return 500
            },
            () => {
                check("reset puts it back at its start, stopped", !state("One").running && state("One").seconds === 100, all())
                goLive(5)
                return 1300
            },
            () => {
                check("reset and start, with how the timer is to be set: Two is a minute's countdown now, and running", state("Two").running && near(state("Two").seconds, 59)
                      && timer("Two").duration === 60, all() + " duration " + timer("Two").duration)
                check("found by name again, its id being no timer's", timer("Two").id === "22222222-BBBB-4BBB-8BBB-222222222222")
                goLive(6)
                return 500
            },
            () => {
                check("stop and reset: stopped, at its start as last set", !state("Two").running && state("Two").seconds === 60, all())
                goLive(7)
                return 500
            },
            () => {
                check("increment adds its seconds to the timer", !state("One").running && state("One").seconds === 130, all())
                goLive(8)
                return 1300
            },
            () => {
                check("a cue with two actions does both: One starts", state("One").running && near(state("One").seconds, 129), all())
                check("and Elapsed starts, found by name with no id given", state("Elapsed").running && near(state("Elapsed").seconds, 1), all())
                goLive(9)
                return 1300
            },
            () => {
                check("id and name disagree the other way: the id's timer, Two, is put back and started", state("Two").running && near(state("Two").seconds, 59), all())
                check("and One, which the name names, is not put back", state("One").running && near(state("One").seconds, 127.7), all())
                kept.before = all()
                goLive(10)
                return 400
            },
            () => {
                check("a slide with no timer action leaves them all as they are", state("One").running && state("Two").running && state("Elapsed").running)
                goLive(9)
                return 1300
            },
            () => {
                check("going back to a slide does what its cue does again", near(state("Two").seconds, 59), all())
                check("none of this was written to the workspace's timers file", testInput.fileHash(catalog.workspacePath + "/Configuration/Timers") === kept.file && kept.file !== "")
                check("nothing was reported along the way", notice === "", notice)
                Timers.timers.forEach(x => { Timers.stop(x.id); Timers.reset(x.id) })
                return 100
            }
        ]
        next()
    }
}
