import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The extended API and MCP (src/extendedapi.h, src/extendedapirun.h, src/mcp.h): this
// app's own addition to the HTTP API, which makes and changes what the workspace holds.
// Off until its own switch in the settings' Network section is on, and then answered
// with why; a web page from elsewhere and a name that is not this computer's turned
// away. A library and a presentation made through it are in the window's lists and
// open there; a slide changed through it while its presentation is being viewed is the
// slide the window then shows, and what the output draws when it next goes live, the
// slide that is live being followed when slides are added before it; a presentation
// open in the editor is left alone; a theme made and put on; a playlist laid out, with
// a heading; a presentation of ProPresenter's own changed (its file is looked at after,
// by run.py); one deleted, which goes to the Trash; MCP, both ways it is spoken; and the
// log, which says what changed the workspace and not the working copies.
QtObject {
    id: t

//COMMON
    readonly property int port: 50124

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // A request as a client writes it; `headers` may say another Host than this computer's
    function send(method, path, body, headers) {
        // (Each on a connection of its own, which the app is asked to close when it has
        // answered: it keeps only so many open at once)
        const all = Object.assign({ Host: "127.0.0.1:" + port, Connection: "close" }, headers ?? {})
        let text = method + " " + path + " HTTP/1.1\r\n"
        for (const name in all)
            text += name + ": " + all[name] + "\r\n"
        if (body !== undefined)
            text += "Content-Type: application/json\r\nContent-Length: " + body.length + "\r\n"
        return testInput.httpSend(port, text + "\r\n" + (body ?? ""))
    }

    // One of the extended API's, its body an object
    function api(method, path, body) {
        return send(method, "/sp/v1/" + path, body === undefined ? undefined : JSON.stringify(body))
    }

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

    function json(id) {
        try {
            return JSON.parse(decodeURIComponent(escape(response(id).body)))
        } catch (error) {
            return {}
        }
    }

    // An MCP message as the current revision has one, with the headers it must come with
    function mcp(id, method, params) {
        const all = Object.assign({ _meta: { "io.modelcontextprotocol/protocolVersion": "2026-07-28", "io.modelcontextprotocol/clientCapabilities": {} } },
                                  params ?? {})
        const headers = { "MCP-Protocol-Version": "2026-07-28", "Mcp-Method": method, Accept: "application/json, text/event-stream" }
        if (method === "tools/call")
            headers["Mcp-Name"] = all.name
        return send("POST", "/mcp", JSON.stringify({ jsonrpc: "2.0", id: id, method: method, params: all }), headers)
    }

    function onOutput() {
        const parts = []
        for (let row = 0; row < 3; ++row) {
            for (let column = 0; column < 4; ++column)
                parts.push(testInput.lit(false, column / 4, row / 3, 1 / 4, 1 / 3).toFixed(3))
        }
        return parts.join(" ")
    }

    // The colour of a point of the screen on the output, given as shares of the slide. (The
    // screen is drawn where the slide layer is, below the window's title strip: the
    // point is made shares of the window.)
    function outPixel(x, y) {
        const content = testInput.windowContent("output")
        const layer = Lib.find(content, item => item.name === "slide layer")
        const at = layer.mapToItem(null, x * layer.width, y * layer.height)
        return String(testInput.pixel(false, at.x / content.width, at.y / content.height))
    }

    // How red a slide's thumbnail in the grid is drawn: the share of it that is
    function cellRed(index) {
        const cell = slideCell(index)
        const at = cell.mapToItem(null, 0, 0)
        return testInput.windowRed(at.x + 6, at.y + 6, cell.width - 12, cell.height * 0.5)
    }

    function library(name) {
        return catalog.libraries.find(l => l.name === name)
    }

    // One request as two steps: sent (its path and body worked out then, from what
    // earlier answers gave), and its answer looked at. `ok` is given the answer's status
    // and its JSON, and what it returns is the check.
    function asked(what, method, path, body, ok) {
        return [
            () => {
                kept.last = api(method, typeof path === "function" ? path() : path, typeof body === "function" ? body() : body)
                return 450
            },
            () => {
                const answer = json(kept.last)
                check(what, ok(response(kept.last).status, answer) === true, response(kept.last).status + " " + response(kept.last).body.slice(0, 260))
                return 50
            }
        ]
    }

    // Every other operation, each once, on a presentation, a theme and a playlist of
    // their own: what the steps before these did not ask for
    function theRest() {
        const second = () => "presentation/Second%20One"
        return [].concat(
            asked("a presentation made of slides in no group, of a size asked for", "POST", "library/Made%20Here/presentations",
                  { name: "Second One", slides: ["one", "two", { text: "three", label: "Third" }], width: 1280, height: 720 },
                  (status, made) => status === 201 && made.slide_count === 3 && made.groups.length === 1 && made.groups[0].name === ""),
            asked("its slides are of that size, in the order given", "GET", second,
                  undefined, (status, read) => { kept.second = read; return status === 200 && read.slides.map(slide => slide.text).join(",") === "one,two,three"
                                                                                  && read.slides[0].width === 1280 && read.slides[2].label === "Third" }),
            asked("a slide moved to just after another", "PATCH", () => second() + "/slide/2", { after: "0" },
                  (status, slide) => status === 200 && slide.index === 1 && slide.text === "three"),
            asked("and one moved to just before another", "PATCH", () => second() + "/slide/" + kept.second.slides[1].id, () => ({ before: kept.second.slides[0].id }),
                  (status, slide) => status === 200 && slide.index === 0 && slide.text === "two"),
            asked("a group renamed and given a colour", "PATCH", () => second() + "/group/0", { name: "Opening", color: "#112233" },
                  (status, now) => status === 200 && now.groups[0].name === "Opening" && now.groups[0].color === "#112233" && now.groups[0].slides.length === 3),
            // (Changed at the window between two changes by the API: the copy kept before the API's first
            // change no longer has what would be lost, so another is kept. run.py counts them.)
            [() => {
                kept.outside = catalog.setDestination(library("Made Here").path + "/Second One.pro", "announcements")
                return 1200
            }],
            asked("changed at the window meanwhile, it is changed through the API again", "PATCH", second, { destination: "presentation" },
                  (status, now) => status === 200 && kept.outside === "" && now.destination === "presentation"),
            asked("slides added at the end of a group the presentation has not make the group", "POST", () => second() + "/slides",
                  { group: "Closing", slides: ["goodbye", "and again"] }, (status, added) => status === 201 && added.slides.length === 2 && added.slides[0].group === "Closing"),
            asked("a group moved before another", "PATCH", () => second() + "/group/Closing", { before: "Opening" },
                  (status, now) => status === 200 && now.groups.map(group => group.name).join(",") === "Closing,Opening"),
            asked("an arrangement made of groups named, one twice", "PUT", () => second() + "/arrangement/Twice", { groups: ["Opening", "Closing", "Opening"] },
                  (status, now) => status === 200 && now.arrangements.length === 1 && now.arrangements[0].group_names.join(",") === "Opening,Closing,Opening"),
            asked("and renamed, and given other groups", "PUT", () => second() + "/arrangement/Twice", { name: "Once", groups: ["Closing"] },
                  (status, now) => status === 200 && now.arrangements[0].name === "Once" && now.arrangements[0].groups.length === 1),
            asked("an arrangement of a group the presentation has not is refused, and nothing is changed", "PUT", () => second() + "/arrangement/Once", { groups: ["Nowhere"] },
                  (status, answer) => status === 404 && answer.error.includes("Nowhere")),
            asked("an arrangement deleted", "DELETE", () => second() + "/arrangement/Once", undefined,
                  (status, now) => status === 200 && now.arrangements.length === 0),
            asked("a group deleted, with its slides", "DELETE", () => second() + "/group/Closing", undefined,
                  (status, now) => status === 200 && now.groups.length === 1 && now.slide_count === 3),
            asked("a slide given an action, by a layer's word", "POST", () => second() + "/slide/0/actions", { kind: "clear", layer: "props" },
                  (status, now) => { kept.action = (now.actions ?? [])[0]; return status === 201 && now.actions.length === 1 && now.actions[0].kind === "clear"
                                                                                  && now.actions[0].layer === 4 }),
            asked("the action changed", "PATCH", () => second() + "/slide/0/action/" + kept.action.id, { kind: "clear", layer: "media" },
                  (status, now) => status === 200 && now.actions.length === 1 && now.actions[0].layer === 2 && now.actions[0].id === kept.action.id),
            asked("an action of a kind there is not is refused", "POST", () => second() + "/slide/0/actions", { kind: "fireworks" },
                  (status, answer) => status === 400 && answer.error.includes("kind")),
            asked("the action taken away", "DELETE", () => second() + "/slide/0/action/" + kept.action.id, undefined,
                  (status, now) => status === 200 && now.actions.length === 0),
            asked("a slide given a Go To Next Timer, and notes", "PATCH", () => second() + "/slide/0", { go_to_next: { seconds: 4, loop: true }, notes: "Then on" },
                  (status, slide) => status === 200 && slide.go_to_next.seconds === 4 && slide.go_to_next.loop === true && slide.notes === "Then on"),
            asked("and the timer taken off again", "PATCH", () => second() + "/slide/0", { go_to_next: null },
                  (status, slide) => status === 200 && slide.go_to_next === undefined && slide.notes === "Then on"),
            asked("media that is not on the computer is refused, and the slide left as it was", "PATCH", () => second() + "/slide/0",
                  { label: "Not kept", media: "no/such/picture.png" }, (status, answer) => status === 400 && answer.error.includes("picture.png")),
            asked("its timeline given a length and a cue", "PATCH", () => second() + "/timeline", { duration: 60, loop: true, add: [{ time: 1.5, slide: "0" }] },
                  (status, timeline) => status === 200 && timeline.duration === 60 && timeline.loop === true && timeline.cues.length === 1
                                        && timeline.cues[0].time === 1.5 && timeline.cues[0].track === "slide"),
            asked("the cue moved", "PATCH", () => second() + "/timeline", { move: [{ place: 0, time: 3 }] },
                  (status, timeline) => status === 200 && timeline.cues[0].time === 3),
            asked("a place that is no number is refused, and is not taken for the first cue", "PATCH", () => second() + "/timeline", { remove: ["x"] },
                  (status, answer) => status === 400 && answer.error.includes("place")),
            asked("and so is a place the timeline has not", "PATCH", () => second() + "/timeline", { duration: 30, remove: [7] },
                  (status, answer) => status === 404 && answer.error.includes("no cue 7")),
            asked("the cue is still there, and the timeline as long as it was", "GET", second, undefined,
                  (status, read) => status === 200 && read.timeline.cues.length === 1 && read.timeline.duration === 60),
            asked("and taken off", "PATCH", () => second() + "/timeline", { remove: [0] }, (status, timeline) => status === 200 && timeline.cues.length === 0),
            asked("a theme asked for no slides at all is refused, and not put on every slide", "POST", () => second() + "/theme", { theme: "API Theme", slides: [] },
                  (status, answer) => status === 400 && answer.error.includes("names no slides")),
            asked("slides to go after a slide that is not there are not added", "POST", () => second() + "/slides", { after: "99", slides: ["nowhere"] },
                  (status, answer) => status === 404 && answer.error.includes("99")),
            asked("the text of all its slides formatted at once", "POST", () => second() + "/format", { format: { size: 50, capitalization: "all_caps", alignment: "left" } },
                  (status, done) => status === 200 && done.text_boxes_changed === 3),
            asked("which its elements then say", "GET", () => second() + "/slide/0", undefined,
                  (status, slide) => { kept.box = slide.elements[0]; return status === 200 && slide.label === "" && slide.elements[0].format.size === 50
                                                                           && slide.elements[0].format.capitalization === "all_caps" && slide.elements[0].format.alignment === "left" }),
            asked("an element sent back as it was shown changes nothing, and is answered the same", "PATCH", () => second() + "/slide/0/element/" + kept.box.id, () => kept.box,
                  (status, element) => status === 200 && JSON.stringify(element) === JSON.stringify(kept.box)),
            asked("its text set anew, run by run", "PATCH", () => second() + "/slide/0/element/" + kept.box.id,
                  { paragraphs: [{ runs: [{ text: "Plain and " }, { text: "bold", bold: true, color: "#ff0000" }] }] },
                  (status, element) => status === 200 && element.text === "Plain and bold" && element.paragraphs[0].runs.length === 2
                                       && element.paragraphs[0].runs[1].bold === true && element.paragraphs[0].runs[1].color === "#ff0000"
                                       && element.paragraphs[0].runs[0].size === 50),
            asked("a colour asked for all its words is put on all of them, though the first have it already", "PATCH", () => second() + "/slide/0/element/" + kept.box.id,
                  () => ({ format: { color: kept.box.format.color } }),
                  (status, element) => status === 200 && element.text === "Plain and bold" && element.paragraphs[0].runs.length === 2
                                       && element.paragraphs[0].runs[1].color === kept.box.format.color && element.paragraphs[0].runs[1].bold === true),
            asked("a slide copied into another presentation, at its end", "POST", () => second() + "/slide/0/duplicate", () => ({ to: kept.uuid }),
                  (status, slide) => status === 201 && slide.text === "Plain and bold" && slide.index === 5),
            asked("and one copied straight after itself", "POST", () => second() + "/slide/0/duplicate", {},
                  (status, slide) => status === 201 && slide.index === 1 && slide.text === "Plain and bold"),
            asked("an element taken off", "DELETE", () => second() + "/slide/1/element/0", undefined, (status, gone) => status === 200 && gone.deleted !== undefined),
            asked("a slide deleted", "DELETE", () => second() + "/slide/1", undefined, (status, gone) => status === 200 && gone.deleted !== undefined),
            asked("which leaves the presentation with the slides it had", "GET", second, undefined,
                  (status, read) => status === 200 && read.slide_count === 3 && read.slides[0].text === "Plain and bold"),
            // A theme's own slides
            asked("a slide added to a theme, a copy of another under a name of its own", "POST", "theme/API%20Theme/slides", { copy_of: "Lower", name: "Lower Copy" },
                  (status, slide) => { kept.themeSlide = slide; return status === 201 && slide.name === "Lower Copy" && slide.index === 1 && slide.elements.length === 1 }),
            asked("the copy has ids of its own", "GET", "theme/API%20Theme", undefined,
                  (status, theme) => status === 200 && theme.slides.length === 2 && theme.slides[0].id !== theme.slides[1].id
                                     && theme.slides[0].elements[0].id !== theme.slides[1].elements[0].id),
            asked("a theme's slide renamed and given another background", "PATCH", "theme/API%20Theme/slide/Lower%20Copy", { name: "Upper", background: "#00ff00" },
                  (status, slide) => status === 200 && slide.name === "Upper" && slide.background === "#00ff00"),
            asked("an element of a theme's slide changed, by the theme's own path", "PATCH", "theme/API%20Theme/slide/Upper/element/Lyrics", { y: 40, format: { bold: true } },
                  (status, element) => status === 200 && element.y === 40 && element.format.bold === true),
            asked("a shape put on a theme's slide, and found on it", "POST", "theme/API%20Theme/slide/Upper/elements", { kind: "ellipse", name: "Dot" },
                  (status, element) => status === 201 && element.shape === "ellipse" && element.order === 1),
            asked("and taken off", "DELETE", "theme/API%20Theme/slide/Upper/element/Dot", undefined, (status, gone) => status === 200),
            asked("a theme's slide read by itself", "GET", "theme/API%20Theme/slide/Upper", undefined,
                  (status, slide) => status === 200 && slide.name === "Upper" && slide.elements.length === 1),
            asked("a theme's slide deleted", "DELETE", "theme/API%20Theme/slide/Upper", undefined, (status, gone) => status === 200),
            asked("but not its last", "DELETE", "theme/API%20Theme/slide/0", undefined, (status, answer) => status === 400 && answer.error.includes("last")),
            asked("a second theme made with nothing said has a slide with a text box", "POST", "themes", { name: "Plain Theme" },
                  (status, theme) => status === 201 && theme.slides.length === 1 && theme.slides[0].elements.length === 1 && theme.slides[0].elements[0].text === "Text"),
            asked("a theme deleted goes to the Trash", "DELETE", "theme/Plain%20Theme", undefined,
                  (status, gone) => status === 200 && testInput.exists(gone.trash) && !Themes.themes.some(theme => theme.place === "Plain Theme")),
            // A playlist's own
            asked("a folder of playlists made", "POST", "playlists", { name: "Folder By API", folder: true },
                  (status, made) => { kept.folder = made.id; return status === 201 && made.folder === true }),
            asked("a playlist made in it", "POST", "playlists", { name: "Inside", parent: "Folder By API", items: [{ presentation: "Second One" }, { header: "End" }] },
                  (status, made) => { kept.inside = made; return status === 201 && made.items.length === 2 }),
            asked("the playlists listed, the new one inside its folder", "GET", "playlists", undefined,
                  (status, list) => status === 200 && list.some(node => node.name === "Inside" && node.parent === kept.folder && node.depth === 1 && node.items === 2)),
            asked("a playlist renamed and moved out of its folder, before another", "PATCH", () => "playlist/" + kept.inside.id, { name: "Outside", before: "API Service" },
                  (status, now) => status === 200 && now.name === "Outside" && catalog.playlists.find(node => node.path === kept.inside.id).depth === 0),
            asked("a row added after another", "POST", () => "playlist/" + kept.inside.id + "/items", { after: "0", items: [{ presentation: "It Is Well" }] },
                  (status, now) => status === 201 && now.items.map(item => item.name).join(",") === "Second One,It Is Well,End"),
            asked("a heading renamed, recoloured and moved up", "PATCH", () => "playlist/" + kept.inside.id + "/item/2", { name: "Start", color: "#0000ff", before: "0" },
                  (status, now) => status === 200 && now.items[0].name === "Start" && now.items[0].color === "#0000ff" && now.items[0].kind === "header"),
            asked("a row's arrangement that its presentation has not is refused", "PATCH", () => "playlist/" + kept.inside.id + "/item/1", { arrangement: "No Such" },
                  (status, answer) => status === 404 && answer.error.includes("No Such")),
            asked("a row changed in two ways, one of which cannot be, is not changed at all", "PATCH", () => "playlist/" + kept.inside.id + "/item/0",
                  { name: "Not Kept", before: "No Such Row" }, (status, answer) => status === 404 && answer.error.includes("No Such Row")),
            asked("the heading is as it was", "GET", () => "playlist/" + kept.inside.id, undefined,
                  (status, now) => status === 200 && now.items[0].name === "Start"),
            asked("a playlist whose rows cannot be made is not made", "POST", "playlists", { name: "Never Made", items: [{ presentation: "No Such Presentation" }] },
                  (status, answer) => status === 404 && !catalog.playlists.some(node => node.name === "Never Made")),
            asked("a row removed", "DELETE", () => "playlist/" + kept.inside.id + "/item/1", undefined,
                  (status, now) => status === 200 && now.items.map(item => item.name).join(",") === "Start,It Is Well"),
            asked("a playlist removed, and its presentations left in their libraries", "DELETE", () => "playlist/" + kept.inside.id, undefined,
                  (status, gone) => status === 200 && gone.deleted === "Outside" && !catalog.playlists.some(node => node.path === kept.inside.id)),
            asked("a folder removed", "DELETE", "playlist/Folder%20By%20API", undefined, (status, gone) => status === 200),
            // Finding things
            asked("a library's presentations listed, each with its uuid", "GET", "library/Made%20Here", undefined,
                  (status, list) => status === 200 && list.presentations.length === 2 && list.presentations.every(each => each.uuid.length === 36)
                                    && list.presentations.some(each => each.name === "Second One")),
            asked("presentations found by the words on their slides", "GET", "search?q=plain+and+bold", undefined,
                  (status, found) => status === 200 && found.some(hit => hit.name === "Second One" && hit.by_name === false && hit.library === "Made Here")),
            asked("the themes and the libraries listed", "GET", "themes", undefined,
                  (status, themes) => status === 200 && themes.length === 1 && themes[0].name === "API Theme" && themes[0].slides.length === 1),
            asked("the workspace's media folder listed, without leaving it", "GET", "media", undefined,
                  (status, media) => status === 200 && media.folder === "" && Array.isArray(media.folders) && Array.isArray(media.files)),
            asked("a folder outside Media is not looked in", "GET", "media?folder=../Libraries", undefined, (status, answer) => status === 400),
            asked("what a slide's action can name: the workspace's timers, its clear groups, the bins", "GET", "action_targets", undefined,
                  (status, targets) => status === 200 && targets.timers.length === Timers.timers.length && targets.timers[0].id === Timers.timers[0].id
                                       && targets.clear_groups.length === ClearGroups.groups.length && targets.media_playlists.length > 0
                                       && Array.isArray(targets.macros) && Array.isArray(targets.stage_layouts)),
            asked("the presentation of its own deleted at the end", "DELETE", second, undefined, (status, gone) => status === 200 && testInput.exists(gone.trash))
        )
    }

    function run() {
        steps = [
            () => {
                win.networkPort = t.port
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "network"
                return 600
            },
            () => {
                check("the extended API is off to begin with, and its part of the settings says so", !win.extendedEnabled && !HttpApi.extendedEnabled
                      && named("extendedStatus").text === "Off.", named("extendedStatus").text)
                click(centre(named("networkSwitch")))
                return 800
            },
            () => {
                check("with the network on and its own switch off, it waits", HttpApi.listening && !HttpApi.extendedEnabled)
                kept.off = api("GET", "workspace")
                kept.offMcp = send("POST", "/mcp", "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}")
                // (ProPresenter's own paths answer all the while)
                kept.version = send("GET", "/version")
                return 700
            },
            () => {
                const off = response(kept.off)
                check("off, its paths are not there, and say that it is switched off and where it is switched on", off.status === 404
                      && /switched off/.test(json(kept.off).error) && /Settings/.test(json(kept.off).error) && response(kept.offMcp).status === 404
                      && response(kept.version).status === 200, off.raw)
                click(centre(named("extendedSwitch")))
                return 800
            },
            () => {
                check("its switch in the settings turns it on, and the settings say where it is and that only this computer is let in",
                      win.extendedEnabled && HttpApi.extendedEnabled && !HttpApi.extendedNetwork
                      && named("extendedStatus").text.includes("http://127.0.0.1:" + t.port + "/sp/v1/")
                      && named("extendedStatus").text.includes("http://127.0.0.1:" + t.port + "/mcp")
                      && named("extendedStatus").text.includes("this computer only") && named("extendedStatus").text.includes("SimplePresenter --mcp"),
                      named("extendedStatus").text)
                // (A picture of the section, for whoever wants to see how it is laid out)
                testInput.grab("settings-network")
                click(centre(named("extendedNetwork")))
                return 600
            },
            () => {
                check("From Other Computers lets the network in, and the settings say so", win.extendedNetwork && HttpApi.extendedNetwork
                      && named("extendedStatus").text.includes("other addresses"), named("extendedStatus").text)
                click(centre(named("extendedNetwork")))
                return 600
            },
            () => {
                check("and is switched back", !win.extendedNetwork && !HttpApi.extendedNetwork)
                settingsOpen = false
                kept.workspace = api("GET", "workspace")
                kept.elsewhere = send("GET", "/sp/v1/workspace", undefined, { Origin: "https://example.com" })
                kept.ownPage = send("GET", "/sp/v1/libraries", undefined, { Origin: "http://localhost:3000" })
                kept.unsaid = send("GET", "/sp/v1/libraries", undefined, { Origin: "null" })
                kept.otherName = send("GET", "/sp/v1/workspace", undefined, { Host: "elsewhere.example.com:" + t.port })
                kept.preflight = send("OPTIONS", "/sp/v1/libraries", undefined, { Origin: "https://example.com", "Access-Control-Request-Method": "POST" })
                kept.wrongMethod = send("PUT", "/sp/v1/workspace", "{}")
                kept.nothing = api("GET", "no/such/thing")
                kept.notJson = send("POST", "/sp/v1/libraries", "this is not JSON")
                kept.unknownKey = api("POST", "libraries", { nmae: "x" })
                kept.openapi = api("GET", "openapi.json")
                kept.reference = api("GET", "reference?topic=elements")
                return 1500
            },
            () => {
                const about = json(kept.workspace)
                check("the workspace in outline: its libraries with how many presentations, its playlists, the groups a slide can be put in",
                      response(kept.workspace).status === 200 && about.name === catalog.workspaceName
                      && about.libraries.length === catalog.libraries.length && about.libraries[0].presentations > 20
                      && about.playlists.length === catalog.playlists.length && about.playlists.some(p => p.items > 0)
                      && about.groups.length > 3 && response(kept.workspace).headers["content-type"] === "application/json",
                      response(kept.workspace).body.slice(0, 300))
                const elsewhere = response(kept.elsewhere)
                check("a web page from elsewhere is turned away, and not told it may ask", elsewhere.status === 403
                      && elsewhere.headers["access-control-allow-origin"] === undefined && /web page/.test(json(kept.elsewhere).error), elsewhere.raw)
                check("a page of this computer's own is answered", response(kept.ownPage).status === 200
                      && response(kept.ownPage).headers["access-control-allow-origin"] === "http://localhost:3000", response(kept.ownPage).raw.slice(0, 300))
                check("a page that does not say where it is from is turned away", response(kept.unsaid).status === 403
                      && response(kept.unsaid).headers["access-control-allow-origin"] === undefined, response(kept.unsaid).raw)
                check("asked for by a name that is not this computer's, it does not answer", response(kept.otherName).status === 403,
                      response(kept.otherName).raw)
                check("a browser's question for a page from elsewhere is refused too", response(kept.preflight).status === 403, response(kept.preflight).raw)
                check("a path asked with a method it has not says which it has", response(kept.wrongMethod).status === 405
                      && response(kept.wrongMethod).headers["allow"] === "GET", response(kept.wrongMethod).raw)
                check("what is not there is 404 with the reason, and what is not JSON or names what is not known is 400 with it",
                      response(kept.nothing).status === 404 && json(kept.nothing).error.includes("openapi.json")
                      && response(kept.notJson).status === 400 && response(kept.unknownKey).status === 400 && json(kept.unknownKey).error.includes("nmae"),
                      response(kept.unknownKey).body)
                const described = json(kept.openapi)
                check("it describes itself in OpenAPI's form, and hands out its reference a part at a time", described.openapi === "3.1.0"
                      && described.info.version === Qt.application.version && described.paths["/sp/v1/presentation/{presentation}/slide/{slide}"].patch !== undefined
                      && json(kept.reference).reference.startsWith("## Elements") && json(kept.reference).reference.includes("fill_color"),
                      response(kept.reference).body.slice(0, 120))
                // ---- A library and a presentation made
                kept.library = api("POST", "libraries", { name: "Made Here" })
                return 700
            },
            () => {
                check("a library made through it is one of the workspace's, in the window's list", response(kept.library).status === 201
                      && library("Made Here") !== undefined, response(kept.library).raw)
                kept.made = api("POST", "library/Made%20Here/presentations", {
                    name: "Made By API", key: "G",
                    groups: [{ name: "Verse", slides: ["First line\nSecond line", { text: "Third line\nFourth line", label: "Second", notes: "Look up" }] },
                             { name: "Chorus", slides: ["The chorus\nof the song"] }],
                    arrangements: [{ name: "Sunday", groups: ["Verse", "Chorus", "Verse"] }], arrangement: "Sunday"
                })
                return 900
            },
            () => {
                const made = json(kept.made)
                kept.uuid = made.uuid
                check("a presentation made: its groups in the workspace's colours, its arrangement selected, its key", response(kept.made).status === 201
                      && made.name === "Made By API" && made.library === "Made Here" && made.slide_count === 3 && made.groups.length === 2
                      && made.groups[0].name === "Verse" && made.groups[0].color === groupChoices().find(g => g.name === "Verse").color
                      && made.groups[0].slides.length === 2 && made.arrangement === "Sunday" && made.arrangements[0].group_names.join(",") === "Verse,Chorus,Verse"
                      && made.key === "G", response(kept.made).body.slice(0, 400))
                // In the window: its library opened, and it in it
                openLibrary(library("Made Here").path)
                return 700
            },
            () => {
                const shown = listed("Made By API")
                check("it is in its library's list at the window, its name drawn, and opens there with its slides in its arrangement",
                      entry("Made By API") !== undefined && shown.light > 0 && document !== null && document.name === "Made By API"
                      && document.arrangement === "Sunday" && document.slides.length === 5 && document.slides[0].plainText === "First line\nSecond line"
                      && document.slides[1].label === "Second" && document.slides[1].notesText === "Look up" && document.uuid === kept.uuid,
                      JSON.stringify(shown) + " " + (document ? document.slides.length : "no document"))
                const words = slideCell(0)
                const at = words.mapToItem(null, 0, 0)
                // (Small words on black: any of the slide lighter than a dark grey is words)
                kept.wordsDrawn = testInput.windowLighter(at.x + words.width * 0.15, at.y + words.height * 0.15, words.width * 0.7, words.height * 0.5, 100)
                testInput.grab("grid")
                check("and its slides are drawn in the grid, with words on them", words !== null && kept.wordsDrawn > 0, kept.wordsDrawn)
                click(centre(slideCell(0)))
                return 900
            },
            () => {
                kept.live = onOutput()
                kept.redBefore = cellRed(0)
                check("a slide of it taken live draws its words on the output", Show.liveIndex === 0 && testInput.lit(false, 0, 0, 1, 1) > 0.003
                      && outPixel(0.02, 0.03) === "#000000", kept.live)
                // ---- Changed while it is being viewed, and live
                kept.changed = api("PATCH", "presentation/" + kept.uuid + "/slide/0", { text: "Changed by the API", background: "#ff0000", label: "Changed" })
                return 1000
            },
            () => {
                const slide = json(kept.changed)
                check("a slide changed through it: its words, its label and its background, answered as it now is", response(kept.changed).status === 200
                      && slide.text === "Changed by the API" && slide.label === "Changed" && slide.background === "#ff0000" && slide.index === 0,
                      response(kept.changed).body.slice(0, 300))
                check("the window has read the presentation again: the slide it shows is the slide as changed", document.slides[0].plainText === "Changed by the API"
                      && document.slides[0].label === "Changed" && Show.liveIndex === 0 && viewingLive)
                check("and draws it so in the grid, its background red where it was not", kept.redBefore < 0.1 && cellRed(0) > 0.5,
                      kept.redBefore + " then " + cellRed(0))
                check("what is on the output is left as it was until a slide is next shown", onOutput() === kept.live
                      && outPixel(0.02, 0.03) === "#000000", onOutput())
                click(centre(slideCell(0)))
                return 900
            },
            () => {
                check("shown again, the output draws it as changed: on red, in other words", outPixel(0.02, 0.03) === "#ff0000"
                      && onOutput() !== kept.live, outPixel(0.02, 0.03) + " " + onOutput())
                kept.added = api("POST", "presentation/" + kept.uuid + "/slides", { before: "0", slides: ["Put in before", { text: "And another", enabled: false }] })
                return 1000
            },
            () => {
                const added = json(kept.added)
                check("slides added before the live one: they look as their neighbour does, and the second is disabled", response(kept.added).status === 201
                      && added.slides.length === 2 && added.slides[0].index === 0 && added.slides[0].text === "Put in before" && added.slides[1].enabled === false,
                      response(kept.added).body.slice(0, 300))
                check("the window shows them, and the show has followed its slide to where it now is", document.slides.length === 9
                      && document.slides[0].plainText === "Put in before" && document.slides[1].disabled === true && Show.liveIndex === 2
                      && document.slides[2].plainText === "Changed by the API" && Show.currentSlide.plainText === "Changed by the API",
                      document.slides.length + " slides, live at " + Show.liveIndex)
                const first = document.slides[0].elements[0]
                const like = document.slides[2].elements[0]
                check("a slide made of words alone has its neighbour's text box over again", first.name === like.name && first.x === like.x
                      && first.width === like.width && first.id !== like.id, first.name + " " + first.x)
                // ---- Open in the editor, it is the editor's
                startEditing(currentEntry())
                return 900
            },
            () => {
                check("the editor is up on it", editing && editingPath.endsWith("/Made By API.pro"), editingPath)
                kept.refused = api("PATCH", "presentation/" + kept.uuid + "/slide/0", { label: "Not while it is being edited" })
                kept.readAll = api("GET", "presentation/" + kept.uuid + "?detail=full")
                return 800
            },
            () => {
                check("while it is open in the editor a change to it is refused, with why", response(kept.refused).status === 409
                      && /editor/.test(json(kept.refused).error), response(kept.refused).body)
                const read = json(kept.readAll)
                check("but it can still be read, every element of it", response(kept.readAll).status === 200 && read.slides.length === 5
                      && read.slides[2].elements.length === 1 && read.slides[2].elements[0].name === "Lyrics"
                      && read.slides[2].elements[0].format.size > 20 && read.slides[2].elements[0].fill_on === false,
                      response(kept.readAll).body.slice(0, 200))
                stopEditing()
                return 800
            },
            () => {
                kept.element = api("PATCH", "presentation/" + kept.uuid + "/slide/2/element/Lyrics",
                                   { vertical_alignment: "bottom", format: { size: 60, color: "#ffff00" }, fill_on: true, fill_color: "#00000080" })
                kept.bar = api("POST", "presentation/" + kept.uuid + "/slide/2/elements", { kind: "rectangle", name: "Bar", x: 0, y: 0, width: 1920, height: 200,
                                                                                         fill_color: "#00ff00", order: 0 })
                return 1000
            },
            () => {
                const element = json(kept.element)
                check("with the editor closed, an element is changed: where its words sit, their size and colour, its fill", response(kept.element).status === 200
                      && element.vertical_alignment === "bottom" && element.format.size === 60 && element.format.color === "#ffff00"
                      && element.fill_on === true && element.fill_color === "#00000080" && element.text === "Changed by the API",
                      response(kept.element).body.slice(0, 300))
                const bar = json(kept.bar)
                check("and a shape is put on the slide, behind its words", response(kept.bar).status === 201 && bar.name === "Bar" && bar.order === 0
                      && bar.fill_color === "#00ff00" && document.slides[2].elements.length === 2 && document.slides[2].elements[0].name === "Bar",
                      response(kept.bar).body.slice(0, 200))
                click(centre(slideCell(2)))
                return 900
            },
            () => {
                check("the output draws the slide with the shape across its top", outPixel(0.5, 0.05) === "#00ff00"
                      && outPixel(0.02, 0.5) === "#ff0000", outPixel(0.5, 0.05) + " " + outPixel(0.02, 0.5))
                // ---- A theme made, and put on
                kept.theme = api("POST", "themes", { name: "API Theme", slides: [{ name: "Lower", background: "#0000ff", elements: [
                    { name: "Lyrics", text: "Lyrics", x: 100, y: 800, width: 1720, height: 240, format: { size: 50, color: "#ffffff" } }] }] })
                return 800
            },
            () => {
                const theme = json(kept.theme)
                check("a theme made through it is among the workspace's themes at the window", response(kept.theme).status === 201
                      && theme.slides.length === 1 && theme.slides[0].elements[0].name === "Lyrics"
                      && Themes.themes.some(each => each.place === "API Theme" && each.slides.length === 1 && each.slides[0].name === "Lower"),
                      response(kept.theme).body.slice(0, 200))
                kept.dressed = api("POST", "presentation/" + kept.uuid + "/theme", { theme: "API Theme", slides: ["0", "1"] })
                return 1000
            },
            () => {
                const first = document.slides[0].elements.find(e => e.name === "Lyrics")
                check("slides dressed in it take the theme's place and size for their words, and its background; the others are left",
                      response(kept.dressed).status === 200 && json(kept.dressed).dressed === 2 && first !== undefined && first.y === 800 && first.height === 240
                      && testInput.plain(first.text) === "Put in before" && document.slides[2].elements.find(e => e.name === "Lyrics").y !== 800,
                      response(kept.dressed).body + " " + (first ? first.y : "no box"))
                // ---- A playlist laid out
                kept.playlist = api("POST", "playlists", { name: "API Service", items: [{ header: "Worship", color: "#ff9300" }, { presentation: kept.uuid },
                                                                                        { header: "Message" }, { presentation: "It Is Well" }] })
                return 900
            },
            () => {
                const playlist = json(kept.playlist)
                kept.playlistId = playlist.id
                check("a playlist made with its rows: headings, and presentations by uuid and by name, in the arrangement each has selected",
                      response(kept.playlist).status === 201 && playlist.items.length === 4 && playlist.items[0].kind === "header"
                      && playlist.items[0].color === "#ff9300" && playlist.items[1].presentation === kept.uuid && playlist.items[1].arrangement === "Sunday"
                      && playlist.items[2].color === "" && playlist.items[3].name === "It Is Well" && playlist.items[3].missing === false,
                      response(kept.playlist).body.slice(0, 400))
                check("it is among the playlists at the window", catalog.playlists.some(p => p.path === kept.playlistId && p.name === "API Service"))
                openPlaylist(kept.playlistId)
                return 800
            },
            () => {
                const heading = listed("Worship")
                check("opened there, its rows are the heading and the presentations, the heading drawn", documents.length === 4 && documents[0].kind === "header"
                      && documents[0].color === "#ff9300" && documents[1].kind === "presentation" && documents[1].arrangement === "Sunday"
                      && documents[3].name === "It Is Well" && heading.light > 0, JSON.stringify(documents.map(d => d.kind + ":" + d.name)))
                kept.laidOut = api("PUT", "playlist/" + kept.playlistId + "/items", { items: [{ header: "Welcome" }, { presentation: "It Is Well" },
                                                                                               { header: "Worship" }, { presentation: kept.uuid, arrangement: "" }] })
                kept.rowIds = documents.map(d => d.path)
                return 900
            },
            () => {
                const rows = json(kept.laidOut).items
                check("given a whole list of rows, the playlist holds those and no others, and the rows that stay keep their ids",
                      response(kept.laidOut).status === 200 && rows.length === 4 && rows[0].name === "Welcome" && rows[1].id === kept.rowIds[3]
                      && rows[2].id === kept.rowIds[0] && rows[2].color === "#ff9300" && rows[3].id === kept.rowIds[1] && rows[3].arrangement === ""
                      && documents.length === 4 && documents[0].name === "Welcome", response(kept.laidOut).body.slice(0, 400))
                // ---- A presentation of ProPresenter's own (run.py looks at its file after)
                kept.theirs = api("PATCH", "presentation/It%20Is%20Well/slide/1", { label: "From the API", notes: "Kept with the slide" })
                kept.theirsGroup = api("PATCH", "presentation/It%20Is%20Well/slide/0", { enabled: false })
                return 900
            },
            () => {
                check("a slide of a presentation ProPresenter wrote is given a label and notes, and another disabled", response(kept.theirs).status === 200
                      && json(kept.theirs).label === "From the API" && json(kept.theirs).notes === "Kept with the slide"
                      && response(kept.theirsGroup).status === 200 && json(kept.theirsGroup).enabled === false, response(kept.theirs).body.slice(0, 300))
                // ---- Renamed, while it is the one on the output
                kept.renamed = api("PATCH", "presentation/" + kept.uuid, { name: "Renamed By API", slide_show: 7 })
                return 1000
            },
            () => {
                kept.byName = api("GET", "presentation/Renamed%20By%20API")
                return 700
            },
            () => {
                check("renamed, it is the same presentation under its new name, with what else was asked of it", response(kept.renamed).status === 200
                      && json(kept.renamed).name === "Renamed By API" && json(kept.renamed).uuid === kept.uuid && json(kept.renamed).slide_show === 7
                      && json(kept.byName).uuid === kept.uuid && json(kept.byName).file.endsWith("/Made Here/Renamed By API.pro"),
                      response(kept.renamed).body.slice(0, 200))
                check("the show goes on with it as it was, and the window's lists have it by its new name", liveDocument.path.endsWith("/Renamed By API.pro")
                      && liveDocument.slideShow === 7 && Show.currentSlide.plainText === "Changed by the API" && outPixel(0.5, 0.05) === "#00ff00"
                      && documents.some(d => d.kind === "presentation" && d.name === "Renamed By API" && !d.missing)
                      && !documents.some(d => d.name === "Made By API"), JSON.stringify(documents.map(d => d.name)) + " " + liveDocument.path)
                // ---- One copied, and the copy deleted
                kept.copy = api("POST", "presentation/" + kept.uuid + "/duplicate", { name: "Copy To Delete" })
                return 800
            },
            () => {
                kept.copyUuid = json(kept.copy).uuid
                check("a copy is a presentation of its own, with the slides of the first", response(kept.copy).status === 201 && kept.copyUuid !== kept.uuid
                      && json(kept.copy).slide_count === 5 && json(kept.copy).name === "Copy To Delete", response(kept.copy).body.slice(0, 200))
                kept.deleted = api("DELETE", "presentation/" + kept.copyUuid)
                return 800
            },
            () => {
                const gone = json(kept.deleted)
                check("deleted, its file is in the Trash and no longer in its library", response(kept.deleted).status === 200 && gone.trash !== ""
                      && testInput.exists(gone.trash) && !testInput.exists(library("Made Here").path + "/Copy To Delete.pro"), response(kept.deleted).body)
                return 200
            }
        ].concat(theRest(), [
            () => {
                // ---- MCP, as its earlier revisions begin and as its current one stands by itself
                kept.initialize = send("POST", "/mcp", JSON.stringify({ jsonrpc: "2.0", id: 1, method: "initialize",
                                                                        params: { protocolVersion: "2025-06-18", capabilities: {}, clientInfo: { name: "test", version: "1" } } }))
                kept.ready = send("POST", "/mcp", JSON.stringify({ jsonrpc: "2.0", method: "notifications/initialized" }))
                kept.tools = send("POST", "/mcp", JSON.stringify({ jsonrpc: "2.0", id: 2, method: "tools/list" }), { "MCP-Protocol-Version": "2025-06-18" })
                kept.called = send("POST", "/mcp", JSON.stringify({ jsonrpc: "2.0", id: 3, method: "tools/call",
                                                                    params: { name: "update_slide", arguments: { presentation: kept.uuid, slide: 4, notes: "Said through MCP" } } }))
                kept.discover = mcp("d", "server/discover")
                kept.modern = mcp(5, "tools/call", { name: "get_presentation", arguments: { presentation: "Renamed By API" } })
                kept.toolError = mcp(6, "tools/call", { name: "get_presentation", arguments: { presentation: "No Such Presentation" } })
                kept.mismatch = send("POST", "/mcp", JSON.stringify({ jsonrpc: "2.0", id: 7, method: "tools/list", params: {
                    _meta: { "io.modelcontextprotocol/protocolVersion": "2026-07-28", "io.modelcontextprotocol/clientCapabilities": {} } } }),
                                     { "MCP-Protocol-Version": "2026-07-28", "Mcp-Method": "tools/call" })
                kept.mcpGet = send("GET", "/mcp")
                kept.mcpElsewhere = send("POST", "/mcp", "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"ping\"}", { Origin: "https://example.com" })
                return 1100
            },
            () => {
                const began = json(kept.initialize).result
                check("MCP begun as its earlier revisions begin: the revision asked for, tools, what the server is, and words on using them",
                      response(kept.initialize).status === 200 && began.protocolVersion === "2025-06-18" && began.capabilities.tools !== undefined
                      && began.serverInfo.name === "simplepresenter" && began.serverInfo.version === Qt.application.version && began.instructions.length > 100
                      && response(kept.ready).status === 202 && response(kept.ready).body === "", response(kept.initialize).body.slice(0, 200))
                const tools = json(kept.tools).result.tools
                check("its tools are the extended API's operations", tools.length > 40 && tools.some(tool => tool.name === "create_presentation"
                      && tool.inputSchema.properties.library !== undefined && tool.inputSchema.required.includes("name"))
                      && tools.some(tool => tool.name === "get_workspace" && tool.annotations.readOnlyHint === true), tools.length + " tools")
                const called = json(kept.called).result
                check("a tool called changes the workspace as the API does, and answers with what is now so", called.isError === false
                      && JSON.parse(called.content[0].text).notes === "Said through MCP" && document.slides.some(slide => slide.notesText === "Said through MCP"),
                      response(kept.called).body.slice(0, 300))
                const discovered = json(kept.discover).result
                check("the current revision, each request by itself: the server discovered, a tool called, a tool's failure said as its result",
                      discovered.resultType === "complete" && discovered.supportedVersions.includes("2026-07-28")
                      && json(kept.modern).result.resultType === "complete" && JSON.parse(json(kept.modern).result.content[0].text).uuid === kept.uuid
                      && response(kept.toolError).status === 200 && json(kept.toolError).result.isError === true
                      && json(kept.toolError).result.content[0].text.includes("No Such Presentation"), response(kept.toolError).body.slice(0, 300))
                check("headers that say other than the body, a GET, and a web page from elsewhere are refused", response(kept.mismatch).status === 400
                      && json(kept.mismatch).error.code === -32020 && response(kept.mcpGet).status === 405 && response(kept.mcpGet).headers["allow"] === "POST"
                      && response(kept.mcpElsewhere).status === 403, response(kept.mismatch).body)
                // ---- The log, and off again
                const changes = logged(/network\s+(POST|PATCH|PUT|DELETE) \/sp\/v1\//)
                check("the log has each request that changed the workspace, with who asked and what it answered; and MCP's",
                      changes.some(line => /POST \/sp\/v1\/libraries from 127\.0\.0\.1: 201/.test(line))
                      && changes.some(line => /PATCH \/sp\/v1\/presentation\/.*\/slide\/0 from 127\.0\.0\.1: 409/.test(line))
                      && logged(/MCP update_slide from 127\.0\.0\.1: 200/).length === 1 && logged(/GET \/sp\/v1\/\S+ from \S+: \d/).length === 0
                      && logged(/web page from elsewhere/).length >= 2, changes.length + " changes logged")
                check("and of what was saved, the presentation's own file and not the copies it was worked on in",
                      logged(/saved\s+.*\/Made Here\/Made By API\.pro/).length > 3 && logged(/saved\s+.*SimplePresenter-api-/).length === 0)
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "network"
                return 600
            },
            () => {
                click(centre(named("extendedSwitch")))
                return 700
            },
            () => {
                kept.offAgain = api("GET", "workspace")
                return 600
            },
            () => {
                check("switched off again, it answers as it did before it was on, and ProPresenter's API goes on", !HttpApi.extendedEnabled
                      && response(kept.offAgain).status === 404 && HttpApi.listening, response(kept.offAgain).raw)
                settingsOpen = false
                return 300
            }
        ])
        next()
    }
}
