import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The app's channels and where they go: the channel count and the Channel Routing window in the settings, as
// ProPresenter's, worked by clicks; with what is heard checked by run.py, which records the test's sound device of
// eight channels all through, and after it says on which of them sound was (after_routing). The workspace is one with
// four channels in ProPresenter's file, whose audio bin sends its pieces to the third and fourth, as the
// maintainer's does.
QtObject {
    id: t

//COMMON
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
                check("a piece played is heard, on the channels its file sends it to", liveMusic !== null && AudioMeter.sounding && AudioMeter.right > 0.5 && AudioMeter.left < 0.05,
                      AudioMeter.left + " and " + AudioMeter.right)
                click(centre(named("clearAudio")))
                return 500
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
