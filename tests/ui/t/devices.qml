import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Communication devices (src/devices.h, src/communications.h, qml/DevicesSettings.qml, qml/MidiControl.qml), on the
// test's own workspace (prepare_devices in run.py): ProPresenter's devices file has a MIDI device on Midi Through, a
// RossTalk switcher and a kind this app has not; a presentation "Devices" has, as ProPresenter writes them, a MIDI
// Note On (note 12, which ProPresenter's MIDI Map has for Next Slide) on its first slide and a Custom Control on its
// second. Settings → Devices offers ProPresenter's two, which are added, connected (the switcher at the stand-in run.py
// keeps), and an HTTP Requests device is added beside them, all kept in this app's file. An HTTP request is added to the
// third slide through Add Action → Communication. Then the show: the first slide going live sends its note to Midi
// Through, which is heard back from it and takes the next slide live by the MIDI Map, whose Custom Control reaches the
// switcher; the next sends its request to the stand-in web server, with its headers and its body. The MIDI Map's Auto
// Fill moves Next Slide away from note 12, and the same note then does nothing. after_devices in run.py looks at what
// ALSA's own tool heard and at what the files hold.
QtObject {
    id: t

//COMMON
    readonly property string folder: "@WORKSPACES@".replace(/WorkSpaces$/, "")
    readonly property var ports: JSON.parse(testInput.readText(folder + "devices-ports.json"))

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function device(name) {
        return Devices.connections.find(c => c.name === name)
    }

    function requests() {
        const text = testInput.readText(folder + "http-requests.json")
        return text === "" ? [] : JSON.parse(text)
    }

    function lines() {
        return testInput.readText(folder + "rosstalk.txt").split("\n").filter(line => line !== "")
    }

    function hover(label) {
        const row = menuRow(label)
        testInput.mouse(4, row.x, row.y)
    }

    function run() {
        steps = [
            () => {
                settingsOpen = true
                settingsScreen.section = "devices"
                return 600
            },
            // ---- Settings → Devices
            () => {
                check("ProPresenter's MIDI and RossTalk devices for the workspace are offered, and not its kind this app has not",
                      Devices.offered.map(d => d.name).join() === "Show MIDI,Carbonite" && Devices.connections.length === 0,
                      Devices.offered.map(d => d.name).join())
                check("the log says what ProPresenter's file has", logged(/devices .*ProPresenter's file has 2 more .* and 1 of kinds this app does not have/).length === 1)
                click(centre(named("deviceOffer:Show MIDI")))
                return 400
            },
            () => {
                const midi = device("Show MIDI")
                check("added, it keeps ProPresenter's id and name, and the ports ProPresenter's file named", midi !== undefined
                      && midi.id === "0C9FC689-0000-4B33-A8F2-FAC5CF89A30A" && midi.sources.join() === "Midi Through Port-0", JSON.stringify(midi))
                check("its page ticks Midi Through, which is here", named("devicePort:sources:Midi Through Port-0") !== null
                      && named("devicePort:sources:Midi Through Port-0").checked)
                click(centre(named("deviceConnect")))
                return 500
            },
            () => {
                check("connected", device("Show MIDI").state === "connected" && device("Show MIDI").problem === "", JSON.stringify(device("Show MIDI")))
                click(centre(named("deviceBack")))
                return 300
            },
            () => {
                click(centre(named("deviceOffer:Carbonite")))
                return 400
            },
            () => {
                const address = named("deviceAddress")
                address.text = "127.0.0.1"
                address.editingFinished()
                named("devicePort").edited(t.ports.rosstalk)
                return 200
            },
            () => {
                check("the switcher's address and port set", device("Carbonite").address === "127.0.0.1" && device("Carbonite").port === t.ports.rosstalk,
                      JSON.stringify(device("Carbonite")))
                click(centre(named("deviceConnect")))
                return 800
            },
            () => {
                check("connected to the switcher", device("Carbonite").state === "connected", JSON.stringify(device("Carbonite")))
                click(centre(named("deviceBack")))
                return 300
            },
            () => {
                click(centre(named("deviceAdd")))
                return 300
            },
            () => {
                check("+ offers the three kinds there are here", labels() === "[Add a Device], MIDI, RossTalk, HTTP Requests", labels())
                click(menuRow("HTTP Requests"))
                return 400
            },
            () => {
                const name = named("deviceName")
                name.text = "Webhooks"
                name.editingFinished()
                const base = named("deviceBaseUrl")
                base.text = "http://127.0.0.1:" + t.ports.http + "/api"
                base.editingFinished()
                const headers = named("deviceHeaders")
                headers.text = "X-Test: yes"
                headers.editingFinished()
                return 300
            },
            () => {
                const hooks = device("Webhooks")
                check("an HTTP Requests device: a name, a base address and a header for every request", hooks !== undefined && hooks.kind === "http"
                      && hooks.baseUrl === "http://127.0.0.1:" + t.ports.http + "/api" && hooks.headers.join() === "X-Test: yes" && hooks.state === "ready",
                      JSON.stringify(hooks))
                check("it has no Connect: there is nothing to keep connected", named("deviceConnect") === null || !named("deviceConnect").visible)
                click(centre(named("deviceBack")))
                return 400
            },
            () => {
                testInput.grab("devices")
                const kept = testInput.readText(folder + "config/SimplePresenter/Devices.json")
                check("the three are kept in this app's own file, for this workspace", kept.includes("Webhooks") && kept.includes("Carbonite")
                      && kept.includes("0C9FC689-0000-4B33-A8F2-FAC5CF89A30A"), kept.slice(0, 200))
                check("the rows say what each is", named("deviceRow:Carbonite") !== null && named("deviceRow:Webhooks") !== null)
                settingsOpen = false
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    if (entry("Devices"))
                        break
                }
                openEntry(entry("Devices"))
                return 800
            },
            // ---- The actions
            () => {
                const first = document.slides[0].actions[0]
                const second = document.slides[1].actions[0]
                check("ProPresenter's MIDI Note On is read: note 12 on channel 1, velocity 100, to Show MIDI", first !== undefined && first.kind === "communication"
                      && first.command === "midi" && first.note === 12 && first.intensity === 100 && first.deviceName === "Show MIDI" && first.done,
                      JSON.stringify(first))
                check("and its Custom Control, to the switcher", second !== undefined && second.command === "words" && second.customControl
                      && second.value === "1:3" && second.words === "CC 1:3", JSON.stringify(second))
                showAddActionMenu({ slide: 2 }, slideCell(2), 10, 10)
                return 300
            },
            () => {
                check("Add Action has Communication", /Message(\(off\))?, Communication$/.test(labels()), labels())
                hover("Communication")
                return 500
            },
            () => {
                check("Communication has each kind of device there is", labels() === "[Communication], MIDI, RossTalk, HTTP Requests", labels())
                hover("HTTP Requests")
                return 500
            },
            () => {
                check("one device of the kind: its commands straight away", labels() === "[HTTP Requests], HTTP Request…", labels())
                click(menuRow("HTTP Request…"))
                return 400
            },
            () => {
                check("the panel to set the request up", actionDialog.mode === "communication" && actionDialog.command === "http")
                named("actionMethod").activated(1)
                const path = named("actionPath")
                path.text = "/v1/go"
                path.textEdited()
                named("actionBody").text = "{\"slide\": 3}"
                named("actionHeaders").text = "X-Slide: 3"
                testInput.grab("http-action")
                click(centre(named("actionAccept")))
                return 600
            },
            () => {
                const made = document.slides[2].actions[0]
                check("the slide has the request: a POST to /v1/go, with its body and its header", made !== undefined && made.command === "http"
                      && made.method === "POST" && made.path === "/v1/go" && made.body === "{\"slide\": 3}" && made.headers.join() === "X-Slide: 3",
                      JSON.stringify(made))
                click(centre(slideCell(0)))
                return 1500
            },
            // ---- The show
            () => {
                check("the first slide sent its note to Midi Through", logged(/devices .*“Show MIDI”: note on 12 \(C-1\), velocity 100, channel 1 sent to Midi Through Port-0/).length === 1,
                      logged(/“Show MIDI”/).join(" | "))
                check("heard back from Midi Through, the note is Next Slide by the MIDI Map", Devices.lastNote.includes("C-1 (12), velocity 100")
                      && Devices.lastNote.includes("Next Slide"), Devices.lastNote)
                check("and the next slide went live", Show.liveIndex === 1, Show.liveIndex)
                check("whose Custom Control reached the switcher", lines().join() === "CC 1:3", lines().join(" | "))
                testInput.key(Qt.Key_Right)
                return 1200
            },
            () => {
                const sent = requests()
                const one = sent[0] ?? {}
                const header = name => Object.keys(one.headers ?? {}).filter(k => k.toLowerCase() === name.toLowerCase()).map(k => one.headers[k])[0]
                check("the third slide's request reached the web server: a POST under the base address", sent.length === 1 && one.method === "POST"
                      && one.path === "/api/v1/go", JSON.stringify(sent))
                check("with the device's header and its own, and its body, said to be JSON", header("X-Test") === "yes" && header("X-Slide") === "3"
                      && header("Content-Type") === "application/json" && one.body === "{\"slide\": 3}", JSON.stringify(one.headers))
                check("the log says how it went", logged(/devices .*“Webhooks”: POST http:\/\/127\.0\.0\.1:\d+\/api\/v1\/go: 200 in \d+ ms/).length === 1,
                      logged(/“Webhooks”/).join(" | "))
                settingsOpen = true
                settingsScreen.section = "devices"
                return 400
            },
            // ---- The MIDI Map
            () => {
                click(centre(named("midiMapButton")))
                return 400
            },
            () => {
                check("the map has ProPresenter's notes: Next Slide is 12, Clear All 0, Clear Announcements 30", named("midiNote:nextSlide") !== null
                      && named("midiNote:nextSlide").value === 12 && named("midiNote:clearAll").value === 0 && named("midiNote:clearAnnouncements").value === 30)
                check("it shows the last note heard", named("midiLastNote").text.includes("C-1 (12)"), named("midiLastNote").text)
                testInput.grab("midi-map")
                named("midiMapFirst").edited(20)
                return 200
            },
            () => {
                click(centre(named("midiMapAutoFill")))
                return 300
            },
            () => {
                check("Auto Fill from 20: Next Slide is 32", named("midiNote:nextSlide").value === 32 && named("midiNote:clearAll").value === 20,
                      named("midiNote:nextSlide").value)
                settingsOpen = false
                click(centre(slideCell(0)))
                return 1500
            },
            () => {
                check("note 12 is no command now: the first slide stays", Show.liveIndex === 0 && Devices.lastNote.includes("no command"), Devices.lastNote)
                Devices.autoFillMidiMap(0)
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
