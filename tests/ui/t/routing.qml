import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The app's channels and where they go: the channel count and the Channel Routing window in the settings, as
// ProPresenter's, worked by clicks; with what is heard checked by run.py, which records the test's sound device of
// eight channels all through, and after it says on which of them sound was (after_routing). The workspace is one with
// four channels in ProPresenter's file, whose audio bin sends its pieces to the third and fourth, as the
// maintainer's does. And the meters beside the preview, a bar a channel: how many (the setting, up to sixteen), laid
// out as ProPresenter's are (up both edges, two to a column above eight), and how solid.
QtObject {
    id: t

//COMMON
    // The channels whose bars a block of meters has, by their bars' names: "0|1|2|3"
    function bars(block) {
        return Lib.findAll(block, item => /^audioMeterBar:/.test(item.objectName)).map(item => item.objectName.substring(14)).join("|")
    }

    function barOf(channel) {
        return Lib.find(sidePanel, item => item.objectName === "audioMeterBar:" + channel)
    }

    // What is drawn part of the way up a bar (at six tenths), and whether that is green: what is drawn, not the level
    // the bar is given. (With the settings screen closed: open, it is over the preview.)
    function colourAt(bar) {
        const p = bar.mapToItem(null, bar.width / 2, bar.height * 0.6)
        return String(testInput.windowPixel(p.x, p.y))
    }

    function green(colour) {
        return parseInt(colour.substring(3, 5), 16) > parseInt(colour.substring(1, 3), 16) + 30
    }

    function meterField(count) {
        const field = named("audioMeters")
        click(centre(field))
        testInput.key(Qt.Key_A, Qt.ControlModifier)
        testInput.type(String(count))
        testInput.key(Qt.Key_Return)
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

    function cell(row, column) {
        return named("routingCell:" + row + ":" + column)
    }

    // Which cells are lit, as the matrix draws them: "r:c" each, by the colour at their middles
    function lit() {
        const found = []
        for (let r = 0; r < AudioMixer.channels.length; ++r) {
            for (let c = 0; c < AudioMixer.deviceChannels; ++c) {
                const item = cell(r, c)
                const p = item.mapToItem(null, item.width / 2, item.height / 2)
                const colour = String(testInput.windowPixel(p.x, p.y))
                if (parseInt(colour.substring(1, 3), 16) > 200 && parseInt(colour.substring(5, 7), 16) < 80)
                    found.push(r + ":" + c)
            }
        }
        return found.join(" ")
    }

    function names() {
        return AudioMixer.channels.map(c => c.name).join("|")
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                settingsOpen = true
                return 400
            },
            () => {
                settingsScreen.section = "audio"
                return 500
            },
            () => {
                const eight = named("audioDevice:Test sink, eight channels")
                check("the sound devices include the test's one of eight channels", eight !== null)
                check("the app's channels are the workspace's: four, as ProPresenter's file has them", named("audioChannelCount").text === "4"
                      && names() === "Pro Channel 1|Pro Channel 2|Pro Channel 3|Pro Channel 4", named("audioChannelCount").text + ": " + names())
                click(Qt.point(eight.mapToItem(null, 9, 11).x, eight.mapToItem(null, 9, 11).y))
                return 400
            },
            () => {
                check("chosen, it is the device", AudioLayer.device !== "" && AudioLayer.deviceFound)
                click(centre(named("channelRouting")))
                return 600
            },
            () => {
                check("Channel Routing opens its window, for the device chosen", named("audioRouting") !== null && named("audioRouting").visible
                      && AudioMixer.deviceName === "Test sink, eight channels", AudioMixer.deviceName)
                check("of the app's four channels and the device's eight", AudioMixer.deviceChannels === 8 && cell(3, 7) !== null && cell(4, 0) === null && cell(0, 8) === null,
                      AudioMixer.deviceChannels)
                check("each by Auto to the device's of its number, as the matrix draws it", lit() === "0:0 1:1 2:2 3:3" && !AudioMixer.deviceRoutesOwn, lit())
                testInput.grab("1-routing")
                click(centre(named("deviceMapping")))
                return 400
            },
            () => {
                const combo = named("deviceMapping")
                const option = (word) => Lib.find(combo.popup.contentItem, item => item.modelData === word && item.highlighted !== undefined)
                const clear = option("Clear")
                check("Mapping has Auto and Clear", combo.popup.visible && clear !== null && option("Auto") !== null)
                click(centre(clear))
                return 400
            },
            () => {
                check("Clear sends none anywhere", lit() === "" && JSON.stringify(AudioMixer.deviceRoutes) === "[[],[],[],[]]", JSON.stringify(AudioMixer.deviceRoutes))
                // The first to the device's fifth, the third to its eighth and its second (and the second off again)
                click(centre(cell(0, 4)))
                click(centre(cell(2, 7)))
                click(centre(cell(2, 1)))
                click(centre(cell(2, 1)))
                return 300
            },
            () => {
                check("a click on a cell sends the one there, and another click stops it", lit() === "0:4 2:7" && JSON.stringify(AudioMixer.deviceRoutes) === "[[4],[],[7],[]]"
                      && AudioMixer.deviceRoutesOwn, lit() + "; " + JSON.stringify(AudioMixer.deviceRoutes))
                // A name, clicked, can be changed
                click(centre(named("channelName:1")))
                return 300
            },
            () => {
                check("a click on a channel's name lets it be changed", named("channelNameEditor:1").visible && named("channelNameEditor:1").activeFocus)
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Band")
                testInput.key(Qt.Key_Return)
                return 300
            },
            () => {
                check("and is changed", names() === "Pro Channel 1|Band|Pro Channel 3|Pro Channel 4" && named("channelName:1").text === "Band", names())
                click(centre(named("channelMute:3")))
                click(centre(named("channelSolo:1")))
                return 300
            },
            () => {
                check("M mutes a channel, S solos one", AudioMixer.channels[3].muted && AudioMixer.channels[1].solo && named("channelMute:3").on && named("channelSolo:1").on
                      && !AudioMixer.channels[0].muted, JSON.stringify(AudioMixer.channels))
                check("the S over them all is lit while any is soloed", named("clearSolos").on)
                click(centre(named("clearSolos")))
                return 300
            },
            () => {
                check("and takes every solo off", AudioMixer.channels.every(c => !c.solo) && !named("clearSolos").on)
                click(centre(named("muteAll")))
                return 300
            },
            () => {
                check("the M over them all mutes every channel", AudioMixer.channels.every(c => c.muted) && named("muteAll").on)
                click(centre(named("muteAll")))
                return 300
            },
            () => {
                check("and again, unmutes them all", AudioMixer.channels.every(c => !c.muted))
                click(centre(named("channelMute:3")))
                // A tone on the first channel, which goes to the device's fifth: heard there (after_routing)
                click(centre(named("channelTone:0")))
                return 1500
            },
            () => {
                check("T puts a tone on a channel", AudioMixer.channels[0].tone && named("channelTone:0").on && AudioMeter.sounding && AudioMeter.left > 0.5,
                      AudioMeter.left + " and " + AudioMeter.right)
                click(centre(named("channelTone:0")))
                return 300
            },
            () => {
                check("and takes it off", !AudioMixer.channels[0].tone)
                click(centre(named("audioRoutingDone")))
                return 300
            },
            () => {
                check("Done closes the window", named("audioRouting") === null || !named("audioRouting").visible)
                settingsOpen = false
                sidePanel.showControlTab = "audio"
                return 600
            },
            () => {
                // A piece of the audio bin, which goes to the third and fourth channels, as its file says: the third
                // to the device's eighth, the fourth (muted) nowhere
                click(centre(row("Walk In")))
                return 1500
            },
            () => {
                check("a piece played is on the meter of the channel its file sends it to, the third, and not on the first two's; the fourth's, muted, is at nothing",
                      liveMusic !== null && AudioMeter.sounding && AudioMeter.levels[2] > 0.5 && AudioMeter.left < 0.05 && AudioMeter.right < 0.05 && AudioMeter.levels[3] === 0,
                      AudioMeter.levels.slice(0, 4).join(" "))
                check("the preview has two bars, up its left edge, for the first two channels, and none up its right; half solid",
                      named("audioMeter").item !== null && bars(named("audioMeter")) === "0|1" && named("audioMeterRight").item === null && Math.abs(named("audioMeter").opacity - 0.5) < 1e-6,
                      bars(named("audioMeter")) + " at " + named("audioMeter").opacity)
                click(centre(named("clearAudio")))
                return 500
            },
            () => {
                // ---- the meters beside the preview: how many, and how solid, in the settings. Tones on the first and
                // fourth channels, so that something sounds all through: the first goes to the device's fifth, which has
                // had that tone already (after_routing), and the fourth, muted, goes nowhere on it, and is on its bar all
                // the same.
                AudioMixer.setTone(0, true)
                AudioMixer.setTone(3, true)
                settingsOpen = true
                return 500
            },
            () => {
                settingsScreen.section = "audio"
                return 500
            },
            () => {
                const field = named("audioMeters")
                check("the settings say two meters, from two to sixteen, and half solid", field !== null && field.text === "2" && field.from === 2 && field.to === 16
                      && Math.abs(named("audioMeterOpacity").value - 0.5) < 1e-6 && named("audioMeterOpacity").from === 0.05,
                      (field ? field.text : "none") + " and " + named("audioMeterOpacity").value)
                meterField(4)
                return 400
            },
            () => {
                const left = named("audioMeter"), right = named("audioMeterRight")
                check("four: two bars up the left edge and two up the right, each the height of the preview", bars(left) === "0|1" && bars(right) === "2|3"
                      && barOf(0).height > left.height - 10 && barOf(3).height > right.height - 10 && barOf(3).x > barOf(2).x && left.width === right.width,
                      bars(left) + " and " + bars(right) + "; " + barOf(0).height + " of " + left.height)
                check("the tones on the first and fourth channels are on their bars and not on the others', the fourth's though it goes nowhere on the device",
                      AudioMeter.levels[0] > 0.5 && AudioMeter.levels[3] > 0.5 && AudioMeter.levels[1] === 0 && AudioMeter.levels[2] === 0, AudioMeter.levels.slice(0, 4).join(" "))
                check("which is the app's own setting", Number(settings.value("audioMeters")) === 4 && audioMeters === 4, settings.value("audioMeters"))
                meterField(3)
                return 400
            },
            () => {
                check("three: two up the left edge and one up the right", bars(named("audioMeter")) === "0|1" && bars(named("audioMeterRight")) === "2"
                      && named("audioMeterRight").width < named("audioMeter").width, bars(named("audioMeter")) + " and " + bars(named("audioMeterRight")))
                meterField(6)
                return 400
            },
            () => {
                check("six: four up the left edge and two up the right, for the fifth and sixth channels", bars(named("audioMeter")) === "0|1|2|3" && bars(named("audioMeterRight")) === "4|5"
                      && barOf(4).height > named("audioMeterRight").height - 10, bars(named("audioMeter")) + " and " + bars(named("audioMeterRight")))
                meterField(12)
                return 400
            },
            () => {
                const left = named("audioMeter"), right = named("audioMeterRight")
                check("twelve: eight up the left edge and four up the right, two to a column at half the height, the fifth under the first",
                      bars(left) === "0|1|2|3|4|5|6|7" && bars(right) === "8|9|10|11" && barOf(0).height < left.height / 2 && barOf(4).y > barOf(0).y + barOf(0).height
                      && barOf(4).x === barOf(0).x && barOf(11).height === barOf(0).height, bars(left) + " and " + bars(right) + "; " + barOf(0).height + " of " + left.height)
                settingsScreen.audioMeterOpacityEdited(1)
                return 300
            },
            () => {
                check("the opacity slider makes them solid, which is the app's own setting too", Math.abs(named("audioMeter").opacity - 1) < 1e-6 && Math.abs(named("audioMeterRight").opacity - 1) < 1e-6
                      && Number(settings.value("audioMeterOpacity")) === 1, named("audioMeter").opacity)
                settingsScreen.audioMeterOpacityEdited(0.5)
                meterField(20)
                return 400
            },
            () => {
                check("no more than sixteen, and no fewer than two", audioMeters === 16 && bars(named("audioMeterRight")) === "8|9|10|11|12|13|14|15", audioMeters)
                meterField(1)
                return 400
            },
            () => {
                check("and two again: two bars up the left edge, none up the right", audioMeters === 2 && bars(named("audioMeter")) === "0|1" && named("audioMeterRight").item === null, audioMeters)
                // Four, and the settings closed, to see what is drawn
                settingsScreen.audioMetersEdited(4)
                settingsOpen = false
                return 600
            },
            () => {
                const fourth = colourAt(barOf(3)), third = colourAt(barOf(2))
                check("drawn, half solid, up the right edge: the fourth channel's bar green part of the way up, for its tone, and the third's not", green(fourth) && !green(third), fourth + " and " + third)
                settingsScreen.audioMetersEdited(2)
                AudioMixer.setTone(0, false)
                AudioMixer.setTone(3, false)
                return 800
            },
            () => {
                check("with the tones off, the meters go", !AudioMeter.sounding && named("audioMeter").item === null)
                return 200
            },
            () => {
                // Six channels: two more, which the file is given with their names
                settingsOpen = true
                return 400
            },
            () => {
                settingsScreen.section = "audio"
                return 400
            },
            () => {
                const count = named("audioChannelCount")
                click(centre(count))
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("6")
                testInput.key(Qt.Key_Return)
                return 400
            },
            () => {
                check("the channel count made six: two more, named as the app names them", AudioMixer.channels.length === 6
                      && names() === "Pro Channel 1|Band|Pro Channel 3|Pro Channel 4|Channel 5|Channel 6", names())
                check("where they go kept as it was, the new ones nowhere", JSON.stringify(AudioMixer.deviceRoutes) === "[[4],[],[7],[],[],[]]", JSON.stringify(AudioMixer.deviceRoutes))
                check("nothing went wrong on the way", !/PROBLEM|WARNING|ERROR/.test(testInput.readText(Log.path).split("\n").filter(l => !l.includes("qt.qpa.theme")).join("\n")),
                      testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR/.test(l)).slice(0, 3).join(" | "))
                settingsOpen = false
                return 300
            }
        ]
        next()
    }
}
