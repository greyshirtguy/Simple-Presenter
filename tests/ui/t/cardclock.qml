import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The Clock Source (src/decklinkformats.h; DeckLink.clockSource): what the screens going out through a card are drawn
// by. With the outputs the app pretends for the tests (SP_TEST_DECKLINK), whose own frames are two percent longer than
// their format's, as a card's clock is not the computer's (by far less than that, in a real one): once the outputs
// have been found, the Screens settings have a Clock Source, Automatic to begin with. Through a dissolve of three
// seconds, the frames handed to the card come at every point of its frames, the app's clock drifting through them, and
// now and then two in one of them. With one of the outputs chosen from its menu, one comes in each of the card's frames,
// never two (where in them moves only as the lead before them does: by the longest of the last passes); the screen's
// settings say what it is drawn by. Set to Automatic again, it is not.
QtObject {
    id: t

//COMMON
    readonly property string media: "@WORKSPACES@".replace(/WorkSpaces$/, "media")

    function red() {
        Show.showMedia({ name: "Red.png", path: media + "/Red.png", source: "file://" + media + "/Red.png", video: false })
    }

    function sender() {
        return senders[kept.id] ?? null
    }

    function settingsRows() {
        return Lib.findAll(named("screensSettings"), item => String(item.objectName) === "screenRow")
    }

    function part(row, name) {
        return Lib.find(settingsRows()[row], item => String(item.objectName) === name)
    }

    function inSettings(name) {
        return Lib.find(named("screensSettings"), item => String(item.objectName) === name)
    }

    // What became of the frames handed to the card through a dissolve: how many; in how many of the card's frames two
    // came, and how many of its frames had none, between the first and the last; and over how much of one of its
    // frames the points they came at spread (the frame less the widest gap between two of them, going round)
    function handed(period) {
        const at = testInput.deckLinkHandedAt(sender())
        let twice = 0
        let none = 0
        for (let i = 1; i < at.length; ++i) {
            const step = at[i][0] - at[i - 1][0]
            if (step === 0)
                ++twice
            else if (step > 1)
                none += step - 1
        }
        const points = at.map(a => a[1]).sort((a, b) => a - b)
        let widest = points.length > 0 ? points[0] + period - points[points.length - 1] : period
        for (let i = 1; i < points.length; ++i)
            widest = Math.max(widest, points[i] - points[i - 1])
        return { frames: at.length, twice: twice, none: none, spread: Math.round((period - widest) * 10) / 10 }
    }

    function run() {
        steps = [
            () => {
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "screens"
                Screens.add("audience")
                DeckLink.refresh()
                return 600
            },
            () => {
                const button = inSettings("clockSource")
                check("once the card's outputs are found, the Screens settings have a Clock Source, Automatic", button !== null && button.visible
                      && button.text.startsWith("Automatic") && DeckLink.clockSource === "", button ? button.text : "none")
                kept.id = Screens.audience[1].id
                Screens.setOutput(kept.id, { output: "decklink", display: "", decklinkOutput: "pretend-1", decklinkName: "DeckLink Duo (1)",
                                             decklinkFormat: "1080p29.97", decklinkAlpha: "none" })
                return 1500
            },
            () => {
                check("(a screen going out through one of them)", sender() !== null && sender().sending && sender().clock === "",
                      JSON.stringify([sender() ? sender().sending : null, sender() ? sender().clock : null]))
                check("which by Automatic is drawn by the app's own clock", inSettings("clockSourceStatus").text.includes("app's own clock"),
                      inSettings("clockSourceStatus").text)
                selectTransition("Dissolve")
                transitionDuration = 3
                testInput.deckLinkHandedAt(sender())
                red()
                return 3400
            },
            () => {
                kept.period = 1001 / 30 * 1.02
                const got = handed(kept.period)
                check("by the app's clock, through a dissolve, the frames come at every point of the card's own frames, and now and then "
                      + "two in one of them", got.frames >= 80 && got.spread > 25 && got.twice >= 1, JSON.stringify(got))
                clearMedia()
                return 3600
            },
            () => {
                click(centre(inSettings("clockSource")))
                return 400
            },
            () => {
                check("the Clock Source's menu: Automatic, and each of the card's outputs", menu.opened
                      && labels() === "*Automatic, DeckLink Duo (1), DeckLink Duo (3)", labels())
                click(menuRow("DeckLink Duo (1)"))
                return 600
            },
            () => {
                check("one chosen is the clock", DeckLink.clockSource === "pretend-1" && DeckLink.clockSourceName === "DeckLink Duo (1)"
                      && inSettings("clockSource").text.startsWith("DeckLink Duo (1)"), JSON.stringify([DeckLink.clockSource, DeckLink.clockSourceName]))
                check("and the screen going out through it is drawn by its frames, as its settings say", sender().clock === "DeckLink Duo (1)"
                      && inSettings("clockSourceStatus").text.includes("in time with its frames")
                      && part(1, "screenStatus").text.includes("Drawn in time with the frames of DeckLink Duo (1)"),
                      sender().clock + " | " + inSettings("clockSourceStatus").text + " | " + part(1, "screenStatus").text)
                testInput.deckLinkHandedAt(sender())
                red()
                return 3400
            },
            () => {
                const got = handed(kept.period)
                // (A card's frame with none is a pass woken too late for its moment: on a busy computer, now and then)
                check("by the card's frames, one comes in each of them, never two", got.frames >= 80 && got.twice === 0 && got.none <= 3,
                      JSON.stringify(got))
                clearMedia()
                return 3600
            },
            () => {
                DeckLink.setClockSource("", "")
                return 300
            },
            () => {
                check("set to Automatic again, it is drawn by the app's clock again", DeckLink.clockSource === "" && sender().clock === ""
                      && !part(1, "screenStatus").text.includes("Drawn in time"), sender().clock + " | " + part(1, "screenStatus").text)
                Screens.setOutput(kept.id, { output: "none" })
                return 600
            },
            () => {
                const bad = testInput.readText(Log.path).split("\n").filter(line => /PROBLEM|WARNING|ERROR/.test(line) && !line.includes("qt.qpa.theme"))
                check("nothing went wrong on the way", bad.length === 0, bad.slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
