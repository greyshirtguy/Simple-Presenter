import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// ProPresenter Remote (src/proremote.h, src/httpapiserver.h): switched on in the settings'
// Network section under the API, it is advertised on the network (through Avahi, as
// ProPresenter's is, `_proapiv1ws._tcp`) and a WebSocket at /v1/ws is opened to it. Three
// Remotes are played here over sockets (testInput.httpSend, as one on the network would):
// a controller with the controller's password, an observer with the observer's, and one
// with a password that is neither. Each is sent ProPresenter's /version at once under
// each password; every message is sealed (AES-256 CTR) under its password, in base64 in a
// text frame or bare in a binary one, and answered in the same kind, with the url it was
// asked with. The Remote's own opening requests are answered; its streams send again
// when the show changes, with their own url; a slide taken live by the controller as the
// Remote takes one draws on the output; the observer is refused what operates the show;
// the wrong password is answered nothing. A changed password lets go of the Remotes it
// let in, and the Remote switched off lets go of the rest and leaves the network.
QtObject {
    id: t

//COMMON
    readonly property int port: 50124

    function onOutput() {
        const parts = []
        for (let row = 0; row < 3; ++row) {
            for (let column = 0; column < 4; ++column)
                parts.push(testInput.lit(false, column / 4, row / 3, 1 / 4, 1 / 3).toFixed(3))
        }
        return parts.join(" ")
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // A Remote connecting, with RFC 6455's own example key
    function connect() {
        return testInput.httpSend(port, "GET /v1/ws HTTP/1.1\r\nHost: 127.0.0.1:" + port + "\r\nSec-WebSocket-Version: 13\r\n"
                                        + "Upgrade: websocket\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nConnection: Upgrade\r\n"
                                        + "Origin: http://127.0.0.1:" + port + "\r\n\r\n")
    }

    // A message from a Remote, sealed under its password
    function ask(id, password, message, binary) {
        testInput.httpWrite(id, testInput.remoteFrame(password, JSON.stringify(message), binary === true))
    }

    // What has come to a Remote: each frame, opened under its password ({opcode, text,
    // json: the message read, or null})
    function heard(id, password) {
        return testInput.remoteMessages(password, testInput.httpReceived(id)).map(frame => {
            let json = null
            try {
                json = JSON.parse(frame.text)
            } catch (error) {
            }
            return { opcode: frame.opcode, text: frame.text, json: json }
        })
    }

    // The answers to one url (as it was asked), in order
    function answers(id, password, url) {
        return heard(id, password).filter(frame => frame.json !== null && frame.json.url === url)
    }

    function head(id) {
        const raw = testInput.httpReceived(id)
        return raw.slice(0, Math.max(0, raw.indexOf("\r\n\r\n")))
    }

    function run() {
        steps = [
            () => {
                kept.empty = onOutput()
                win.networkPort = t.port
                win.networkName = "Simple Presenter test " + Math.floor(Math.random() * 100000)
                win.networkEnabled = true
                win.remoteControllerPassword = "control"
                win.remoteObserver = true
                win.remoteObserverPassword = "watch"
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "network"
                return 800
            },
            () => {
                // (A Remote's request while it is off: the answer is looked at once it is on)
                kept.off = connect()
                return 500
            },
            () => {
                check("the Remote is off until it is switched on, and its part of the Network section says so",
                      HttpApi.listening && !win.remoteEnabled && named("remoteStatus").text === "Off." && named("remote:controller").checked
                      && named("remote:observer").checked && named("remotePassword:controller").text === "control", named("remoteStatus").text)
                check("with the Remote off there is no WebSocket: 404", head(kept.off).startsWith("HTTP/1.1 404"), head(kept.off))
                click(centre(named("remoteSwitch")))
                return 2500
            },
            () => {
                check("its switch turns it on, and it is advertised on the network under the Network Name, as ProPresenter is",
                      win.remoteEnabled && HttpApi.remoteEnabled && HttpApi.remoteAdvertised && HttpApi.remoteName === win.networkName
                      && named("remoteStatus").text.startsWith("On the network as “" + win.networkName + "”"),
                      named("remoteStatus").text + " / " + HttpApi.remoteProblem)
                check("the log says so", logged(/advertised on the network as ".*" \(_proapiv1ws\._tcp, port 50124\)/).length === 1)
                testInput.grab("1-remote-settings")
                settingsOpen = false
                kept.controller = connect()
                kept.observer = connect()
                kept.stranger = connect()
                return 700
            },
            () => {
                const upgraded = head(kept.controller)
                check("a Remote's request is upgraded to a WebSocket, with RFC 6455's accept and the API's own headers",
                      upgraded.startsWith("HTTP/1.1 101 Switching Protocols") && upgraded.includes("sec-websocket-accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")
                      && upgraded.includes("upgrade: websocket") && upgraded.includes("connection: upgrade")
                      && upgraded.includes("access-control-allow-origin: http://127.0.0.1:" + port) && upgraded.includes("vary: Origin"), upgraded)
                const welcome = heard(kept.controller, "control")
                const watching = heard(kept.observer, "watch")
                check("ProPresenter's /version is sent at once, under the controller's password and then the observer's",
                      welcome.length === 2 && welcome[0].opcode === 1 && welcome[0].json !== null && welcome[0].json.url === "version"
                      && welcome[0].json.data.host_description === "SimplePresenter " + Qt.application.version && welcome[0].json.data.name === win.networkName
                      && welcome[1].json === null && watching[1].json !== null && watching[1].json.url === "version", JSON.stringify(welcome.map(f => f.text.slice(0, 60))))
                check("three Remotes are connected, and the settings count them", HttpApi.remotes === 3)
                // The controller: as the Remote opens (streams, tagged with an id and a
                // queryKey), and the other kinds of answer
                ask(kept.controller, "control", { url: "v1/presentation/slide_index?id=a1&chunked=true&queryKey=activeSlide", method: "GET" })
                ask(kept.controller, "control", { url: "v1/status/layers?id=a2&chunked=true&queryKey=clear%2Cstatus", method: "GET" })
                ask(kept.controller, "control", { url: "v1/libraries?id=a3", method: "GET" })
                ask(kept.controller, "control", { url: "v1/libraries?id=a4&chunked=true&queryKey=libraries", method: "GET" })
                ask(kept.controller, "control", { url: "v1/macro/collections?id=a5&chunked=true&queryKey=macro%2Ccollections", method: "GET" })
                ask(kept.controller, "control", { url: "v1/clear/groups?id=a6", method: "GET" }, true)
                ask(kept.controller, "control", { url: "version" })
                // The observer reads, and may not operate; the stranger is answered nothing
                ask(kept.observer, "watch", { url: "v1/status/layers?id=b1", method: "GET" })
                ask(kept.observer, "watch", { url: "v1/presentation/" + document.uuid + "/1/trigger?id=b2", method: "GET" })
                ask(kept.stranger, "guess", { url: "v1/status/layers?id=c1", method: "GET" })
                return 800
            },
            () => {
                const slideIndex = answers(kept.controller, "control", "v1/presentation/slide_index?id=a1&chunked=true&queryKey=activeSlide")
                const layers = answers(kept.controller, "control", "v1/status/layers?id=a2&chunked=true&queryKey=clear%2Cstatus")
                check("a stream is answered at once, with the url it was asked with, query and all", slideIndex.length === 1 && layers.length === 1
                      && slideIndex[0].json.data.presentation_index === null && layers[0].json.data.slide === false,
                      JSON.stringify(slideIndex.concat(layers).map(f => f.text)))
                const libraries = answers(kept.controller, "control", "v1/libraries?id=a3")
                check("a plain request is answered once, as the HTTP API answers it", libraries.length === 1 && Array.isArray(libraries[0].json.data)
                      && libraries[0].json.data.length === catalog.libraries.length, JSON.stringify(libraries.map(f => f.text)))
                check("a stream asked of what does not stream is refused, 400, as ProPresenter refuses libraries",
                      answers(kept.controller, "control", "v1/libraries?id=a4&chunked=true&queryKey=libraries")[0]?.json.error === "400 Bad Request")
                check("what is not there is 404, as ProPresenter answers the Remote's own macro/collections",
                      answers(kept.controller, "control", "v1/macro/collections?id=a5&chunked=true&queryKey=macro%2Ccollections")[0]?.json.error === "404 Not Found")
                const binary = answers(kept.controller, "control", "v1/clear/groups?id=a6")
                check("a binary frame is answered in one", binary.length === 1 && binary[0].opcode === 2 && Array.isArray(binary[0].json.data),
                      JSON.stringify(binary))
                check("/version, asked bare, is answered as it was sent at first", answers(kept.controller, "control", "version").length === 2)
                const observed = answers(kept.observer, "watch", "v1/status/layers?id=b1")
                check("the observer is answered what reports the show", observed.length === 1 && observed[0].json.data.slide === false)
                check("but is refused what operates it, 403, and nothing goes live",
                      answers(kept.observer, "watch", "v1/presentation/" + document.uuid + "/1/trigger?id=b2")[0]?.json.error === "403 Forbidden"
                      && Show.liveIndex < 0 && onOutput() === kept.empty, String(Show.liveIndex))
                check("a password that is neither is answered nothing at all, but the welcome", heard(kept.stranger, "guess").every(f => f.json === null)
                      && testInput.httpOpen(kept.stranger))
                check("the log says who came, as what, and of the one whose message did not open",
                      logged(/ProPresenter Remote at 127\.0\.0\.1 is in, as a controller/).length === 1
                      && logged(/ProPresenter Remote at 127\.0\.0\.1 is in, as an observer/).length === 1
                      && logged(/does not open with the Remote's passwords/).length === 1
                      && logged(/an observer, may not GET \/v1\/presentation\/.*\/1\/trigger/).length === 1)
                // A slide taken live as the Remote takes one: by the presentation's UUID and
                // the slide's index, from the controller
                ask(kept.controller, "control", { url: "v1/presentation/" + document.uuid + "/2/trigger?id=a7", method: "GET" })
                return 900
            },
            () => {
                check("the controller takes a slide live, and nothing is answered for it (a 204, as ProPresenter sends nothing)",
                      Show.liveIndex === 2 && answers(kept.controller, "control", "v1/presentation/" + document.uuid + "/2/trigger?id=a7").length === 0,
                      String(Show.liveIndex))
                check("drawn: the slide on the output", onOutput() !== kept.empty, onOutput())
                const slideIndex = answers(kept.controller, "control", "v1/presentation/slide_index?id=a1&chunked=true&queryKey=activeSlide")
                const layers = answers(kept.controller, "control", "v1/status/layers?id=a2&chunked=true&queryKey=clear%2Cstatus")
                check("its streams send again, with their own urls: the slide's place, the slide layer on",
                      slideIndex.length === 2 && slideIndex[1].json.data.presentation_index.index === 2
                      && slideIndex[1].json.data.presentation_index.presentation_id.uuid === document.uuid
                      && layers.length === 2 && layers[1].json.data.slide === true, JSON.stringify(slideIndex.concat(layers).map(f => f.text.slice(0, 120))))
                check("the log has what it operated, and who", logged(/GET \/v1\/presentation\/.*\/2\/trigger from ProPresenter Remote at 127\.0\.0\.1: 204/).length === 1)
                // A changed password lets go of those it let in
                win.remoteControllerPassword = "changed"
                return 600
            },
            () => {
                const closes = heard(kept.controller, "control").filter(f => f.opcode === 8)
                check("the controller's password changed: the controller is let go of, with a close saying why (1008)",
                      !testInput.httpOpen(kept.controller) && closes.length === 1 && closes[0].text === "03f0", JSON.stringify(closes))
                check("the observer stays", testInput.httpOpen(kept.observer) && HttpApi.remotes === 2, String(HttpApi.remotes))
                win.remoteEnabled = false
                return 1500
            },
            () => {
                check("the Remote switched off lets go of the rest, and leaves the network", !testInput.httpOpen(kept.observer)
                      && !testInput.httpOpen(kept.stranger) && HttpApi.remotes === 0 && !HttpApi.remoteAdvertised && HttpApi.listening)
                check("the log says they have gone", logged(/ProPresenter Remote at 127\.0\.0\.1 has gone/).length === 3)
                win.networkEnabled = false
                return 300
            },
        ]
        next()
    }
}
