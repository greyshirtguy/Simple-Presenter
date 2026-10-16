import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A screen sent out through a Blackmagic DeckLink card's output (src/decklink.h), with the two outputs the app pretends
// for the tests (SP_TEST_DECKLINK): one that can key, as a pair of connectors linked for fill and key is, and one that
// cannot. In the settings, where a screen goes is a menu, and each output a menu of its formats; chosen, the screen is
// drawn and handed to the card, and what was handed over is looked at: red while a red picture is on the media layer,
// and not handed over again while it stays; black and opaque while nothing is on; see-through once it is keyed
// (premultiplied, straight; internal; a blend); switched off and on, it goes out again; another format chosen while
// it goes out starts it again at that size; an output that cannot key has its alpha set to none and its settings shut;
// an output that is not there says so. And the log says what was found and sent.
QtObject {
    id: t

//COMMON
    readonly property string media: "@WORKSPACES@".replace(/WorkSpaces$/, "media")

    function settingsRows() {
        return Lib.findAll(named("screensSettings"), item => String(item.objectName) === "screenRow")
    }

    function part(name) {
        return Lib.find(settingsRows()[1], item => String(item.objectName) === name)
    }

    function sender() {
        return senders[kept.id] ?? null
    }

    function shares() {
        return testInput.deckLinkShares(sender())
    }

    function red() {
        Show.showMedia({ name: "Red.png", path: media + "/Red.png", source: "file://" + media + "/Red.png", video: false })
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                DeckLink.refresh()
                check("the outputs are found, as Desktop Video Setup has them: one that can key, and one that cannot", DeckLink.available
                      && DeckLink.outputs.length === 2 && DeckLink.outputs[0].name === "DeckLink Duo (1)" && DeckLink.outputs[0].external
                      && DeckLink.outputs[0].internal && !DeckLink.outputs[1].external && !DeckLink.outputs[1].internal,
                      JSON.stringify(DeckLink.outputs.map(o => [o.name, o.internal, o.external, o.formats.length])))
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "screens"
                Screens.add("audience")
                return 600
            },
            () => {
                kept.id = Screens.audience[1].id
                click(centre(part("screenOutput")))
                return 400
            },
            () => {
                check("where a screen goes is a menu, with the card's outputs under Blackmagic", menu.opened
                      && labels().includes("NDI, [Blackmagic], DeckLink Duo (1), DeckLink Duo (3), *Nothing"), labels())
                click(menuRow("DeckLink Duo (1)"))
                return 400
            },
            () => {
                check("and each output a menu of the formats it can send", labels() === "[DeckLink Duo (1)], 1080p24, 1080p25, 1080p29.97, 1080p30, "
                      + "1080p50, 1080p59.94, 1080p60, 720p50, 720p59.94, 720p60", labels())
                click(menuRow("1080p29.97"))
                return 1200
            },
            () => {
                const screen = Screens.audience[1]
                check("a format chosen is the output chosen, kept as this computer's", screen.output === "decklink" && screen.decklinkOutput === "pretend-1"
                      && screen.decklinkName === "DeckLink Duo (1)" && screen.decklinkFormat === "1080p29.97" && screen.decklinkAlpha === "none",
                      JSON.stringify([screen.output, screen.decklinkOutput, screen.decklinkFormat, screen.decklinkAlpha]))
                check("the screen is drawn at the format's size and handed to the card, and has no window", sender() !== null && sender().sending
                      && sender().width === 1920 && sender().height === 1080 && sender().framesSent >= 1 && scenes[kept.id] !== undefined
                      && scenes[kept.id].contentItem === undefined, sender() ? sender().framesSent + " " + sender().problem : "none")
                check("its line says so, and the menu's button which output and format", part("screenStatus").text === "Going out through DeckLink Duo (1) as 1080p29.97."
                      && part("screenOutput").text.startsWith("DeckLink Duo (1) · 1080p29.97"), part("screenStatus").text + " / " + part("screenOutput").text)
                check("an output that can key has the alpha settings open", part("screenDeckLinkAlpha").visible && part("screenDeckLinkAlpha").enabled)
                red()
                return 1500
            },
            () => {
                const [clear, reds] = shares()
                check("what the card is given is what is live: red, and opaque", reds > 0.9 && clear === 0, reds + " red, " + clear + " clear")
                kept.frames = sender().framesSent
                return 1500
            },
            () => {
                check("while nothing changes, nothing more is drawn or handed to the card", sender().framesSent === kept.frames,
                      kept.frames + " then " + sender().framesSent)
                clearMedia()
                clearSlide()
                return 1200
            },
            () => {
                const [clear] = shares()
                check("with nothing live, black, and still opaque, not keyed", clear === 0 && testInput.deckLinkPixel(sender(), 0.02, 0.02) === "#ff000000",
                      testInput.deckLinkPixel(sender(), 0.02, 0.02))
                part("screenDeckLinkAlpha").activated(1)
                return 1200
            },
            () => {
                const [clear] = shares()
                check("keyed premultiplied, what is not drawn on is see-through", Screens.audience[1].decklinkAlpha === "premultiplied"
                      && sender().alpha === "premultiplied" && sender().sending && clear > 0.99, clear + " " + sender().problem)
                check("and the key and blend can be set", part("screenDeckLinkKey").enabled && part("screenDeckLinkBlend").enabled)
                part("screenDeckLinkKey").activated(0)
                part("screenDeckLinkBlend").edited(60)
                return 1200
            },
            () => {
                check("internal, at 60%, as the card is asked", Screens.audience[1].decklinkKey === "internal" && Screens.audience[1].decklinkBlend === 60
                      && sender().key === "internal" && sender().blend === 60 && sender().sending, JSON.stringify([sender().key, sender().blend, sender().problem]))
                check("and its line says so", part("screenStatus").text === "Going out through DeckLink Duo (1) as 1080p29.97, keyed internally, premultiplied, at 60%.",
                      part("screenStatus").text)
                part("screenDeckLinkAlpha").activated(2)
                red()
                return 1500
            },
            () => {
                const [clear, reds] = shares()
                check("straight, a picture that fills the screen is red and opaque", sender().alpha === "straight" && reds > 0.9 && clear === 0, reds + " " + clear)
                testInput.grab("1-settings")
                outputEnabled = false
                return 800
            },
            () => {
                check("the audience button takes it off the card", sender() !== null && !sender().sending)
                outputEnabled = true
                return 1500
            },
            () => {
                const [clear, reds] = shares()
                check("and switched on again, it goes out again, as it was", sender().sending && sender().framesSent >= 1 && reds > 0.9 && clear === 0,
                      reds + " " + clear + " " + sender().framesSent)
                click(centre(part("screenOutput")))
                return 400
            },
            () => {
                click(menuRow("DeckLink Duo (1)"))
                return 400
            },
            () => {
                click(menuRow("720p50"))
                return 1500
            },
            () => {
                const [clear, reds] = shares()
                check("another format while it goes out: started again at that size, and drawn whole", sender().format === "720p50" && sender().width === 1280
                      && sender().height === 720 && sender().sending && sender().framesSent >= 1 && reds > 0.9 && clear === 0
                      && logged(/  decklink +a screen goes out through "DeckLink Duo \(1\)" as 720p50, keyed internally, straight, at 60%$/).length === 1,
                      JSON.stringify([sender().format, sender().width, sender().framesSent, reds, clear, sender().problem]))
                click(centre(part("screenOutput")))
                return 400
            },
            () => {
                click(menuRow("DeckLink Duo (3)"))
                return 400
            },
            () => {
                check("an output offers only the formats it can send", labels() === "[DeckLink Duo (3)], 1080p24, 1080p25, 1080p29.97, 1080p30, 1080p50, "
                      + "1080p59.94, 1080p60", labels())
                click(menuRow("1080p25"))
                return 1200
            },
            () => {
                const screen = Screens.audience[1]
                check("an output that cannot key sends the screen as it is, and has its alpha settings shut", screen.decklinkOutput === "pretend-3"
                      && screen.decklinkFormat === "1080p25" && screen.decklinkAlpha === "none" && sender().alpha === "none" && sender().sending
                      && !part("screenDeckLinkAlpha").enabled, JSON.stringify([screen.decklinkOutput, screen.decklinkAlpha, sender().problem]))
                Screens.setOutput(kept.id, { decklinkOutput: "gone", decklinkName: "Gone Card" })
                return 1000
            },
            () => {
                check("an output that is not there now: not sent, and its line says why", !sender().sending && sender().problem.includes("no Blackmagic output")
                      && part("screenStatus").text.includes("Gone Card"), part("screenStatus").text)
                check("the log says what was found, what was sent and what stopped", logged(/  decklink +a screen goes out through "DeckLink Duo \(1\)" as 1080p29\.97$/).length >= 1
                      && logged(/  decklink +a screen goes out through "DeckLink Duo \(1\)" as 1080p29\.97, keyed internally, premultiplied, at 60%$/).length === 1
                      && logged(/  decklink +the screen sent through "DeckLink Duo \(1\)" has stopped, after \d+ frames$/).length >= 1
                      && logged(/  PROBLEM +A screen is not being sent through the Blackmagic output "Gone Card": there is no Blackmagic output/).length === 1,
                      logged(/decklink|Blackmagic/).join(" | "))
                Screens.setOutput(kept.id, { output: "none" })
                clearMedia()
                return 500
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
