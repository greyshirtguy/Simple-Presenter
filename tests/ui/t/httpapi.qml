import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The HTTP API (src/httpapiserver.h): ProPresenter's network API, served for clients on the
// network. Switched on in the settings' Network section, it listens; a client sends it
// requests as bytes over a socket (testInput.httpSend, as a client on the network would),
// and what comes back is read as bytes too: the head and body as ProPresenter writes them
// (CR LF CR LF after the JSON, counted in its length; CORS; 404 and 204 with nothing), a
// stream of several things at once (status/updates: the clock first, every second, then
// whatever changes, each chunk {url, data}), and a chunked GET. A slide taken live
// through it draws on the output what a click on it draws; next steps on as the arrow
// key does; a presentation of the library triggered by its UUID goes live without the
// window's own presentation changing; a timer, the stage message and the audience
// screens are worked. The log says what operated the show and who streamed. Switched off,
// it lets its clients go.
QtObject {
    id: t

//COMMON
    readonly property int port: 50123

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

    // A request as a client writes it
    function send(method, path, body, headers) {
        let text = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" + port + "\r\n"
        for (const name in headers ?? {})
            text += name + ": " + headers[name] + "\r\n"
        if (body !== undefined)
            text += "Content-Type: application/json\r\nContent-Length: " + utf8Length(body) + "\r\n"
        return testInput.httpSend(port, text + "\r\n" + (body ?? ""))
    }

    function utf8Length(text) {
        return unescape(encodeURIComponent(text)).length
    }

    function utf8(bytes) {
        try {
            return decodeURIComponent(escape(bytes))
        } catch (error) {
            return bytes
        }
    }

    // What came back: status, headers by lower-case name, and the body's bytes
    function response(id) {
        const raw = testInput.httpReceived(id)
        const end = raw.indexOf("\r\n\r\n")
        if (end < 0)
            return { status: 0, headers: {}, body: "", raw: raw }
        const lines = raw.slice(0, end).split("\r\n")
        const headers = {}
        for (const line of lines.slice(1)) {
            const colon = line.indexOf(":")
            headers[line.slice(0, colon).toLowerCase()] = line.slice(colon + 1).trim()
        }
        return { status: Number(lines[0].split(" ")[1]), headers: headers, body: raw.slice(end + 4), raw: raw }
    }

    // A chunked body's chunks, each as text, by their lengths in bytes; null if the
    // framing is not right
    function chunks(body) {
        const found = []
        let at = 0
        while (at < body.length) {
            const end = body.indexOf("\r\n", at)
            if (end < 0)
                return found
            const size = parseInt(body.slice(at, end), 16)
            if (isNaN(size))
                return null
            if (size === 0)
                break
            if (body.slice(end + 2 + size, end + 4 + size) !== "\r\n")
                return found.length > 0 && end + 2 + size >= body.length ? found : null
            found.push(utf8(body.slice(end + 2, end + 2 + size)))
            at = end + 4 + size
        }
        return found
    }

    // The updates of a status/updates stream, as {url, data}, every chunk ending in CR LF CR LF
    function updates(id) {
        const found = chunks(response(id).body)
        if (found === null)
            return null
        return found.map(chunk => chunk.endsWith("\r\n\r\n") ? JSON.parse(chunk) : { url: "(not separated)", data: chunk })
    }

    function lastOf(list, url) {
        const all = list.filter(update => update.url === url)
        return all.length > 0 ? all[all.length - 1].data : undefined
    }

    function json(id) {
        const body = response(id).body
        return JSON.parse(utf8(body))
    }

    function run() {
        steps = [
            () => {
                // (What the output shows with nothing live, to set a cleared slide against)
                kept.empty = onOutput()
                win.networkPort = t.port
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "network"
                return 600
            },
            () => {
                check("off until it is switched on, and its section says so", !HttpApi.listening && named("networkStatus").text === "Off.",
                      named("networkStatus").text)
                click(centre(named("networkSwitch")))
                return 800
            },
            () => {
                check("the switch in the settings starts it, on the port set, and says where it can be reached", win.networkEnabled && HttpApi.listening
                      && HttpApi.port === t.port && /^Listening (at .*:50123|on port 50123, but this computer is on no network)/.test(named("networkStatus").text),
                      named("networkStatus").text)
                settingsOpen = false
                kept.version = send("GET", "/version")
                kept.layers = send("GET", "/v1/status/layers", undefined, { Origin: "http://companion.local" })
                kept.preflight = send("OPTIONS", "/v1/stage/message", undefined, { Origin: "http://companion.local", "Access-Control-Request-Method": "PUT",
                                                                                    "Access-Control-Request-Headers": "content-type" })
                kept.unknown = send("GET", "/v1/no/such/thing")
                kept.refused = send("POST", "/v1/status/updates", "[\"status/slide\",\"/v1/status/layers\"]")
                kept.stream = send("POST", "/v1/status/updates", JSON.stringify(["status/slide", "presentation/slide_index", "status/layers", "timers/current",
                                                                                 "stage/message", "status/audience_screens", "capture/status", "look/current"]))
                kept.chunked = send("GET", "/v1/presentation/slide_index?chunked=true")
                return 900
            },
            () => {
                const version = response(kept.version)
                const about = JSON.parse(version.body)
                check("/version: this app, on Linux, by the computer's name; and no CR LF CR LF after it, as ProPresenter writes it",
                      version.status === 200 && about.host_description === "SimplePresenter " + Qt.application.version && about.platform === "linux"
                      && about.name === HttpApi.computerName() && about.api_version === "v1" && !version.body.endsWith("\r\n\r\n")
                      && Number(version.headers["content-length"]) === version.body.length, version.raw)
                const layers = response(kept.layers)
                check("a v1 answer: JSON, then CR LF CR LF, counted in its length; the origin asked from given back as allowed", layers.status === 200
                      && layers.headers["content-type"] === "application/json" && layers.body.endsWith("\r\n\r\n")
                      && Number(layers.headers["content-length"]) === layers.body.length
                      && layers.headers["access-control-allow-origin"] === "http://companion.local" && layers.headers["vary"].startsWith("Origin")
                      && JSON.parse(layers.body).slide === false && JSON.parse(layers.body).video_input === false, layers.raw)
                const preflight = response(kept.preflight)
                check("a browser's preflight is allowed", preflight.status === 200 && preflight.headers["access-control-allow-methods"].includes("PUT")
                      && preflight.headers["access-control-allow-headers"] === "content-type", preflight.raw)
                const unknown = response(kept.unknown)
                check("what it does not know is 404, with nothing", unknown.status === 404 && unknown.body === "" && unknown.headers["content-length"] === "0",
                      unknown.raw)
                check("status/updates with a path it does not stream (written with /v1/) is refused whole", response(kept.refused).status === 400,
                      response(kept.refused).raw)
                const stream = response(kept.stream)
                const first = updates(kept.stream)
                check("status/updates streams: chunked, kept open, the clock first", stream.status === 200 && stream.headers["transfer-encoding"] === "chunked"
                      && first !== null && first.length >= 9 && first[0].url === "timer/system_time"
                      && Math.abs(first[0].data - Date.now() / 1000) < 5 && testInput.httpOpen(kept.stream), JSON.stringify(first).slice(0, 300))
                check("then each path asked for, as it is now: no slide, no index, the layers off, the timer stopped, the screens on, nothing capturing",
                      lastOf(first, "status/slide").current.text === "" && lastOf(first, "presentation/slide_index").presentation_index === null
                      && lastOf(first, "status/layers").slide === false && lastOf(first, "timers/current")[0].state === "stopped"
                      && lastOf(first, "stage/message") === "" && lastOf(first, "status/audience_screens") === true
                      && lastOf(first, "capture/status").status === "inactive" && lastOf(first, "look/current").screens.length === 1,
                      JSON.stringify(first.slice(1)).slice(0, 500))
                check("a chunked GET streams the one thing", (chunks(response(kept.chunked).body) ?? []).length === 1,
                      response(kept.chunked).raw.slice(0, 300))
                // ---- What a click draws, to set the API's against
                click(centre(slideCell(2)))
                return 900
            },
            () => {
                kept.byClick = onOutput()
                const now = updates(kept.stream)
                check("a slide going live is in the stream at once: its words, its place, the slide layer on", Show.liveIndex === 2
                      && lastOf(now, "status/slide").current.text === Show.currentSlide.plainText && lastOf(now, "status/slide").current.uuid === Show.currentSlide.id
                      && lastOf(now, "status/slide").next.uuid === Show.nextSlide.id
                      && lastOf(now, "presentation/slide_index").presentation_index.index === 2
                      && lastOf(now, "presentation/slide_index").presentation_index.total_cues === document.slides.length
                      && lastOf(now, "presentation/slide_index").presentation_index.presentation_id.uuid === document.uuid
                      && lastOf(now, "status/layers").slide === true, JSON.stringify(now.slice(-4)).slice(0, 500))
                const chunked = chunks(response(kept.chunked).body)
                check("and in the chunked GET", chunked.length >= 2 && JSON.parse(chunked[chunked.length - 1]).presentation_index.index === 2,
                      JSON.stringify(chunked))
                kept.cleared = send("GET", "/v1/clear/layer/slide")
                return 700
            },
            () => {
                const cleared = response(kept.cleared)
                check("clearing the slide: 204, nothing after it, and the output is as it was with nothing live", cleared.status === 204
                      && cleared.body === "" && cleared.headers["content-length"] === undefined && Show.cleared && onOutput() !== kept.byClick
                      && onOutput() === kept.empty, cleared.raw + " | " + onOutput() + " | " + kept.empty)
                kept.triggered = send("GET", "/v1/presentation/focused/2/trigger")
                return 900
            },
            () => {
                check("the same slide taken live through the API draws on the output what the click drew", response(kept.triggered).status === 204
                      && Show.liveIndex === 2 && !Show.cleared && onOutput() === kept.byClick, onOutput() + " | " + kept.byClick)
                kept.next = send("GET", "/v1/trigger/next")
                return 700
            },
            () => {
                check("trigger/next steps on, as the arrow key does", response(kept.next).status === 204 && Show.liveIndex === 3, Show.liveIndex)
                kept.active = send("GET", "/v1/presentation/active")
                kept.libraries = send("GET", "/v1/libraries")
                kept.library = send("GET", "/v1/library/Demo")
                kept.focused = send("GET", "/v1/presentation/focused")
                return 900
            },
            () => {
                const active = json(kept.active).presentation
                check("presentation/active: the presentation the show is at, from its file, its groups in their order and its arrangements",
                      active.id.uuid === document.uuid && active.id.name === document.name && active.groups.length === document.groups.length
                      && active.groups.map(g => g.uuid).join() === document.groups.map(g => g.id).join() && active.destination === "presentation"
                      && active.arrangements.length === document.arrangementList.length && active.groups[0].slides[0].enabled === true,
                      JSON.stringify(active).slice(0, 400))
                const libraries = json(kept.libraries)
                const library = json(kept.library)
                check("the libraries, and one asked for by name, its presentations by their UUIDs", libraries.length === 1 && libraries[0].name === "Demo"
                      && /^[0-9A-F-]{36}$/.test(libraries[0].uuid) && library.update_type === "all" && library.items[0].name === documents[0].name
                      && library.items[0].uuid === document.uuid, JSON.stringify(library).slice(0, 300))
                check("presentation/focused: the one open in the window", json(kept.focused).uuid === document.uuid && json(kept.focused).index === 0,
                      JSON.stringify(json(kept.focused)))
                kept.other = library.items[1]
                kept.byUuid = send("GET", "/v1/presentation/" + kept.other.uuid + "/1/trigger")
                return 1000
            },
            () => {
                check("a presentation triggered by its UUID goes live, and the window keeps its own open", response(kept.byUuid).status === 204
                      && win.liveDocument.name === kept.other.name && Show.liveIndex === 1 && document.name === documents[0].name
                      && onOutput() !== kept.byClick, win.liveDocument.name + " / " + document.name)
                kept.timer = send("GET", "/v1/timer/0/start")
                kept.message = send("PUT", "/v1/stage/message", JSON.stringify("Two minutes"))
                kept.audienceOff = send("PUT", "/v1/status/audience_screens", "false")
                return 1600
            },
            () => {
                const now = updates(kept.stream)
                check("a timer started, the stage told, the audience screens off: each done, and each in the stream",
                      response(kept.timer).status === 204 && response(kept.message).status === 204 && response(kept.audienceOff).status === 204
                      && Show.stageMessage === "Two minutes" && !win.outputEnabled
                      && lastOf(now, "timers/current")[0].state === "running" && lastOf(now, "stage/message") === "Two minutes"
                      && lastOf(now, "status/audience_screens") === false, JSON.stringify(now.slice(-6)).slice(0, 600))
                check("the clock and the running timer every second", now.filter(update => update.url === "timer/system_time").length >= 3
                      && now.filter(update => update.url === "timers/current").length >= 3, now.map(update => update.url).join(" "))
                kept.audienceOn = send("PUT", "/v1/status/audience_screens", "true")
                kept.reset = send("GET", "/v1/timer/0/reset")
                kept.hidden = send("DELETE", "/v1/stage/message")
                return 600
            },
            () => {
                check("and back", win.outputEnabled && Show.stageMessage === "" && Timers.state(Timers.timers[0].id).running === false)
                check("the log says it listened, what operated the show, by whom, and who streamed",
                      logged(/  network +the API is listening on port 50123, at /).length === 1
                      && logged(/  network +GET \/v1\/trigger\/next from 127\.0\.0\.1: 204$/).length === 1
                      && logged(/  network +GET \/v1\/presentation\/focused\/2\/trigger from 127\.0\.0\.1: 204$/).length === 1
                      && logged(/  network +127\.0\.0\.1 streams timer\/system_time, status\/slide, presentation\/slide_index/).length === 1
                      && logged(/  network +answering .* went wrong/).length === 0, logged(/  network /).slice(0, 6).join(" | "))
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "network"
                return 600
            },
            () => {
                check("while streams are open the section says so", /1 client is following the show|2 clients are following the show/.test(named("networkStatus").text),
                      named("networkStatus").text)
                click(centre(named("networkSwitch")))
                return 800
            },
            () => {
                check("switched off, it stops listening and lets its clients go", !win.networkEnabled && !HttpApi.listening && !testInput.httpOpen(kept.stream)
                      && !testInput.httpOpen(kept.chunked) && named("networkStatus").text === "Off."
                      && logged(/  network +the API has stopped listening$/).length === 1, named("networkStatus").text)
                settingsOpen = false
                for (const id of [kept.version, kept.layers, kept.preflight, kept.unknown, kept.refused, kept.stream, kept.chunked])
                    testInput.httpClose(id)
                return 300
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
