import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Video inputs (src/videoinputfile.h, src/videoinputs.h, qml/VideoInputView.qml), on the Act workspace, whose
// ProPresenter has one, "Input 1", with no device and no sound, and on the pretend device the app has for tests
// (SP_TEST_VIDEOINPUT), a picture of one colour that the test changes. What is looked at is what is drawn.
//
// Settings → Inputs: ProPresenter's input read from its file; one added with + and renamed; the pretend device chosen
// for Input 1, its preview in the page, a thumbnail taken of it; letting go of it when the page is left. The media
// bin's Video Inputs, first in its list: the inputs as cells, one clicked onto the video input layer of the output and
// of the operator's preview, ringed; the picture following the device; a look that switches the layer off for the
// output, and on again; the layer's clear button; and the input let go once nothing shows it. Actions: Video Input
// from Add Action, an input dragged out of the bin onto a slide, the input changed from the action's menu, and the
// slides shown and cleared. The fill: Video Input chosen in the editor's Fill menu, in the file with the rest of the
// element as it was, drawn live in the editor and marked; another input and a scale chosen; all three undone, the file
// byte for byte as it was; then the slide on the output, with the input drawn in its shape there and in the operator's
// preview, and in no thumbnail. after_videoinputs in run.py reads the files.
QtObject {
    id: t

//COMMON
    function texts(root) {
        return Lib.findAll(root, item => item.text !== undefined && item.font !== undefined && item.visible && typeof item.text === "string" && item.text !== "").map(item => item.text)
    }

    // A live picture of an input, and a still one (a thumbnail's)
    function isView(item) {
        return item.sending !== undefined && item.fitting !== undefined && item.interval !== undefined && item.inputId !== undefined && !item.still
    }

    function isStill(item) {
        return item.sending !== undefined && item.fitting !== undefined && item.still === true
    }

    function onDisk() {
        return catalog.open(kept.entry.file).slides[kept.index].elements.find(e => e.id === kept.element.id)
    }

    // An element as the file has it, but for its fill
    function restOf(e) {
        const rest = {}
        for (const key of Object.keys(e).sort()) {
            if (!key.startsWith("fill") && key !== "visible" && key !== "displayText" && key !== "text")
                rest[key] = e[key]
        }
        return JSON.stringify(rest) + testInput.plain(e.text)
    }

    function listRow(list, name) {
        const view = named(list)
        for (let i = 0; i < view.count; ++i) {
            if (view.model[i].name === name) {
                view.positionViewAtIndex(i, ListView.Contain)
                return view.itemAtIndex(i)
            }
        }
        return null
    }

    function hover(label) {
        const row = menuRow(label)
        testInput.mouse(4, row.x, row.y)
    }

    function dragTo(from, to) {
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 12; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
        kept.to = to
    }

    function badge(slide, kind) {
        return Lib.findAll(slideCell(slide), item => item.objectName === "actionBadge" && item.modelData.kind === kind)[0] ?? null
    }

    function actionsOf(slide, kind) {
        return document.slides[slide].actions.filter(action => action.kind === kind)
    }

    function outputAt(fx, fy) {
        return String(testInput.pixel(false, fx, fy))
    }

    function previewAt(fx, fy) {
        const picture = Lib.find(win.contentItem, item => item.namePrefix === "preview" && item.screenId !== undefined)
        const aspect = output.width / output.height
        const w = Math.min(picture.width, picture.height * aspect)
        const h = w / aspect
        const p = picture.mapToItem(null, (picture.width - w) / 2 + fx * w, (picture.height - h) / 2 + fy * h)
        return String(testInput.windowPixel(p.x, p.y))
    }

    // The colour at the middle of an item in the operator window
    function middleOf(item) {
        const p = item.mapToItem(null, item.width / 2, item.height / 2)
        return String(testInput.windowPixel(p.x, p.y))
    }

    function reddish(c) {
        return parseInt(c.substring(1, 3), 16) > 200 && parseInt(c.substring(3, 5), 16) < 60 && parseInt(c.substring(5, 7), 16) < 60
    }

    function greenish(c) {
        return parseInt(c.substring(1, 3), 16) < 60 && parseInt(c.substring(3, 5), 16) > 200 && parseInt(c.substring(5, 7), 16) < 60
    }

    function dark(c) {
        return parseInt(c.substring(1, 3), 16) < 40 && parseInt(c.substring(3, 5), 16) < 40 && parseInt(c.substring(5, 7), 16) < 40
    }

    // ProPresenter's HTTP API, asked as a client on the network asks it (see httpapi.qml)
    readonly property int port: 50125

    function send(method, path) {
        return testInput.httpSend(port, method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" + port + "\r\n\r\n")
    }

    function response(id) {
        const raw = testInput.httpReceived(id)
        const end = raw.indexOf("\r\n\r\n")
        return end < 0 ? { status: 0, body: "", raw: raw } : { status: Number(raw.slice(0, end).split(" ")[1]), body: raw.slice(end + 4), raw: raw }
    }

    function input(name) {
        return VideoInputs.inputs.find(i => i.name === name)
    }

    function run() {
        steps = [
            // ---- Settings → Inputs
            () => {
                const first = VideoInputs.inputs[0]
                check("ProPresenter's own input is read from its file: Input 1, with no device on this computer and no sound",
                      VideoInputs.inputs.length === 1 && first.name === "Input 1" && first.index === 0 && first.device === "" && first.audio === "none"
                      && first.color === "#0c2c5a" && !first.capturing, JSON.stringify(VideoInputs.inputs))
                settingsOpen = true
                settingsScreen.section = "inputs"
                return 800
            },
            () => {
                check("the settings have Inputs, after Devices", settingsScreen.sections.map(s => s.name).join().includes("Devices,Inputs,"),
                      settingsScreen.sections.map(s => s.name).join())
                const row = named("inputRow:Input 1")
                check("the page lists it, saying it has no device here", row !== null && row.visible && texts(row).includes("No device on this computer"),
                      row ? texts(row).join(" | ") : "none")
                click(centre(named("inputAdd")))
                return 600
            },
            () => {
                const made = input("Input 2")
                check("+ adds Input 2 to the workspace, and opens it", VideoInputs.inputs.length === 2 && made !== undefined && made.index === 1
                      && named("inputName") !== null && named("inputName").visible && named("inputName").text === "Input 2",
                      VideoInputs.inputs.map(i => i.name).join())
                named("inputName").text = "Camera Two"
                named("inputName").editingFinished()
                return 400
            },
            () => {
                check("and it is renamed", input("Camera Two") !== undefined && input("Input 2") === undefined, VideoInputs.inputs.map(i => i.name).join())
                click(centre(named("inputBack")))
                return 400
            },
            () => {
                click(centre(named("inputRow:Input 1")))
                return 400
            },
            () => {
                const device = named("inputDevice")
                const choices = Array.from(device.model)
                check("Input 1's page: its Device is None, and the computer's devices are offered, the pretend one among them",
                      device.visible && device.currentIndex === 0 && choices[0] === "None" && choices.includes("Test Pattern"), choices.join(", "))
                check("its sound is No Audio Source, as ProPresenter's file has it", named("inputAudio").currentText === "No Audio Source", named("inputAudio").currentText)
                kept.first = input("Input 1").id
                kept.second = input("Camera Two").id
                device.activated(choices.indexOf("Test Pattern"))
                return 1500
            },
            () => {
                const now = input("Input 1")
                check("the pretend device chosen, it is captured, for the preview in the page", now.device === "test:pattern" && now.capturing, JSON.stringify(now))
                check("the page says so", named("inputDevice").currentText === "Test Pattern", named("inputDevice").currentText)
                check("and its sound is still none, as ProPresenter's file has it, until another is chosen", now.audio === "none"
                      && named("inputAudio").currentText === "No Audio Source", now.audio + " " + named("inputAudio").currentText)
                check("and the preview shows its picture, red", reddish(middleOf(named("inputPreview"))), middleOf(named("inputPreview")))
                testInput.grab("1-settings")
                click(centre(named("inputThumbnailTake")))
                return 1000
            },
            () => {
                check("a thumbnail taken of it", input("Input 1").thumbnail.startsWith("file://") && input("Input 1").thumbnail.includes("/Video Inputs/"),
                      input("Input 1").thumbnail)
                click(centre(named("inputBack")))
                return 400
            },
            () => {
                const row = named("inputRow:Input 1")
                const picture = row ? Lib.find(row, i => i.status !== undefined && i.source !== undefined && i.visible && String(i.source).includes("/Video Inputs/")) : null
                check("the list shows it by its thumbnail, red, and says which device it is", picture !== null && picture.status === Image.Ready
                      && reddish(middleOf(picture)) && texts(row).some(x => x.startsWith("Test Pattern")), row ? texts(row).join(" | ") : "none")
                settingsOpen = false
                return 2500
            },
            () => {
                check("with the settings closed nothing shows it, and it is let go", !input("Input 1").capturing)
                // ---- The media bin's Video Inputs
                const row = listRow("mediaPlaylistList", "Video Inputs")
                check("the media bin's list has Video Inputs, first", row !== null && named("mediaPlaylistList").model[0].name === "Video Inputs")
                click(centre(row))
                return 600
            },
            () => {
                const cells = Lib.findAll(named("videoInputGrid"), i => i.objectName.startsWith("videoInputCell:") && i.visible)
                check("and no word of the media grid's is over them", !texts(named("mediaBin") ?? win.contentItem).some(x => x.startsWith("This media playlist")),
                      texts(named("mediaBin") ?? win.contentItem).filter(x => x.startsWith("This")).join(" | "))
                check("picked, the bin shows the inputs in their order, in place of a playlist's media",
                      named("videoInputGrid").visible && !named("mediaGrid").visible && cells.length === 2
                      && cells.map(c => c.modelData.name).join() === "Input 1,Camera Two", cells.map(c => c.modelData.name).join())
                click(centre(named("videoInputCell:Input 1")))
                return 1500
            },
            () => {
                check("a click on one puts it on the video input layer", Show.liveVideoInput === 0 && input("Input 1").capturing, Show.liveVideoInput)
                check("the output shows it", reddish(outputAt(0.5, 0.5)), outputAt(0.5, 0.5))
                check("and so does the operator's preview", reddish(previewAt(0.5, 0.5)), previewAt(0.5, 0.5))
                const cell = named("videoInputCell:Input 1")
                check("the cell is ringed as live, and the bin's row too", cell.live && Lib.findAll(cell, i => i.background !== undefined && i.visible).length > 0
                      && named("mediaPlaylistList").livePath === videoInputsNode)
                check("the clear button of the layer is lit", named("clearVideoInput").live === true)
                testInput.grabOutput("2-output")
                testInput.grab("2-operator")
                VideoInputs.setTestColor("#00ff00")
                return 800
            },
            () => {
                check("what the device sends next is what is shown", greenish(outputAt(0.5, 0.5)) && greenish(previewAt(0.5, 0.5)),
                      outputAt(0.5, 0.5) + " " + previewAt(0.5, 0.5))
                kept.lookError = Looks.setScreen(Looks.live.id, Screens.audience[0].id, { videoInput: false })
                return 1200
            },
            () => {
                check("a look that switches the layer off for the output: it is gone from the output and from the preview",
                      kept.lookError === "" && dark(outputAt(0.5, 0.5)) && dark(previewAt(0.5, 0.5)) && Show.liveVideoInput === 0,
                      kept.lookError + " " + outputAt(0.5, 0.5) + " " + previewAt(0.5, 0.5))
                const hidden = Lib.findAll(output.contentItem, isView)
                check("and the output's picture of it is handed no frames while it is not seen, the input still held", hidden.length === 1
                      && !hidden[0].shown && input("Input 1").capturing, hidden.length + " " + (hidden.length ? hidden[0].shown : ""))
                kept.lookError = Looks.setScreen(Looks.live.id, Screens.audience[0].id, { videoInput: true })
                return 1200
            },
            () => {
                check("and on again", kept.lookError === "" && greenish(outputAt(0.5, 0.5)), outputAt(0.5, 0.5))
                click(centre(named("clearVideoInput")))
                return 1000
            },
            () => {
                check("its clear button takes it off the layer", Show.liveVideoInput === -1 && dark(outputAt(0.5, 0.5)) && dark(previewAt(0.5, 0.5)),
                      Show.liveVideoInput + " " + outputAt(0.5, 0.5))
                // ---- ProPresenter's HTTP API: the list, an input triggered by its name, the layer's status, and its clear
                win.networkPort = t.port
                win.networkEnabled = true
                return 800
            },
            () => {
                check("(the API is listening)", HttpApi.listening)
                kept.list = send("GET", "/v1/video_inputs")
                kept.trigger = send("GET", "/v1/video_inputs/Input%201/trigger")
                return 1000
            },
            () => {
                const list = response(kept.list)
                const body = list.status === 200 ? JSON.parse(list.body) : null
                check("the API lists the inputs, each by its id, name and place", body !== null && body.length === 2
                      && Object.keys(body[0]).join() === "id" && body[0].id.uuid === kept.first && body[0].id.name === "Input 1" && body[0].id.index === 0
                      && body[1].id.name === "Camera Two" && body[1].id.index === 1, list.raw.slice(0, 400))
                check("an input triggered by its name is put on the layer", response(kept.trigger).status === 204 && Show.liveVideoInput === 0
                      && greenish(outputAt(0.5, 0.5)), response(kept.trigger).raw.slice(0, 100) + " " + Show.liveVideoInput + " " + outputAt(0.5, 0.5))
                kept.layers = send("GET", "/v1/status/layers")
                return 600
            },
            () => {
                const layers = response(kept.layers)
                check("the layers' status says the video input is on", layers.status === 200 && JSON.parse(layers.body).video_input === true, layers.raw.slice(0, 300))
                kept.cleared = send("GET", "/v1/clear/layer/video_input")
                kept.missing = send("GET", "/v1/video_inputs/No%20Such%20Input/trigger")
                return 800
            },
            () => {
                check("clearing the layer takes it off", response(kept.cleared).status === 204 && Show.liveVideoInput === -1 && dark(outputAt(0.5, 0.5)),
                      response(kept.cleared).raw.slice(0, 100) + " " + Show.liveVideoInput)
                check("and an input it has not got is not found", response(kept.missing).status === 404, response(kept.missing).raw.slice(0, 100))
                win.networkEnabled = false
                return 2000
            },
            () => {
                check("and once nothing shows it, it is let go", !input("Input 1").capturing)
                // ---- Actions, on a presentation with a box filled with a colour, for the fill after them
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = catalog.open(d.file).slides
                        const plain = slides.map((s, i) => s.media === undefined && s.actions.length === 0 ? i : -1).filter(i => i >= 0)
                        const at = plain.find(i => slides[i].elements.some(e => e.fillOn && e.fillKind === "color" && !e.fillLinesOnly && e.width > 300 && !e.hidden))
                        if (at !== undefined && plain.length >= 3 && !kept.entry) {
                            kept.entry = d
                            kept.index = at
                            kept.element = slides[at].elements.find(e => e.fillOn && e.fillKind === "color" && !e.fillLinesOnly && e.width > 300 && !e.hidden)
                            kept.plain = plain.filter(i => i !== at)
                        }
                    }
                    if (kept.entry)
                        break
                }
                check("(a presentation with a box filled with a colour, and two more slides with no media or action)", kept.entry !== undefined,
                      kept.entry ? kept.entry.name : "none")
                testInput.say("PRESENTATION " + kept.entry.file)
                openEntry(kept.entry)
                return 800
            },
            () => {
                showAddActionMenu({ slide: kept.plain[0] }, slideCell(kept.plain[0]), 10, 10)
                return 300
            },
            () => {
                check("Add Action has Video Input, after Media Playlist", /Media Playlist, Video Input, Audio Playlist/.test(labels()), labels())
                hover("Video Input")
                return 500
            },
            () => {
                check("which has the workspace's inputs", labels() === "[Video Input], Input 1, Camera Two", labels())
                click(menuRow("Input 1"))
                return 600
            },
            () => {
                const made = actionsOf(kept.plain[0], "videoInput")[0]
                check("the slide shows Input 1, by its place", made !== undefined && made.inputIndex === 0, JSON.stringify(document.slides[kept.plain[0]].actions))
                const icon = badge(kept.plain[0], "videoInput")
                check("its icon is on the slide: ProPresenter's camera", icon !== null && icon.visible
                      && Lib.find(icon, i => i.inkName !== undefined && i.name === "VideoInput" && i.status === Image.Ready) !== null)
                dragTo(centre(named("videoInputCell:Camera Two")), centre(slideCell(kept.plain[1])))
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 600
            },
            () => {
                const made = actionsOf(kept.plain[1], "videoInput")[0]
                check("an input dragged out of the bin onto a slide gives it the action that shows it", made !== undefined && made.inputIndex === 1,
                      JSON.stringify(document.slides[kept.plain[1]].actions))
                check("and was not shown by being dragged", Show.liveVideoInput === -1)
                click(centre(badge(kept.plain[0], "videoInput")), Qt.RightButton)
                return 300
            },
            () => {
                check("the icon's menu offers another input", labels().includes("Video Input"), labels())
                hover("Video Input")
                return 500
            },
            () => {
                check("(the workspace's inputs, the one it shows marked)", labels() === "[Video Input], *Input 1, Camera Two", labels())
                click(menuRow("Camera Two"))
                return 600
            },
            () => {
                const made = actionsOf(kept.plain[0], "videoInput")
                check("the action shows Camera Two now, and is still the one action", made.length === 1 && made[0].inputIndex === 1,
                      JSON.stringify(document.slides[kept.plain[0]].actions))
                VideoInputs.setDevice(kept.second, "test:pattern")
                click(centre(slideCell(kept.plain[0])))
                return 1500
            },
            () => {
                check("the slide shows it on the output", Show.liveVideoInput === 1 && greenish(outputAt(0.95, 0.95)) && input("Camera Two").capturing,
                      Show.liveVideoInput + " " + outputAt(0.95, 0.95))
                click(centre(named("clearGroup")))
                return 1000
            },
            () => {
                check("Clear All takes it off", Show.liveVideoInput === -1 && dark(outputAt(0.95, 0.95)), Show.liveVideoInput + " " + outputAt(0.95, 0.95))
                // ---- The fill, in the editor
                kept.hash = testInput.fileHash(kept.entry.file)
                kept.rest = restOf(kept.element)
                startEditing(kept.entry, document.slides[kept.index].id)
                return 800
            },
            () => {
                editScreen.canvas.pick(kept.element.id)
                editScreen.inspector.tab = "shape"
                return 400
            },
            () => {
                check("the Fill menu has Video Input", Array.from(named("fillKind").model).indexOf("Video Input") === 4, Array.from(named("fillKind").model).join(", "))
                named("fillKind").chosen(4)
                return 1500
            },
            () => {
                const e = onDisk()
                check("choosing it makes the fill the first input, in the file", e !== undefined && e.fillKind === "input" && e.fillInputIndex === 0 && e.fillShown,
                      e ? JSON.stringify([e.fillKind, e.fillInputIndex, e.fillShown]) : "none")
                check("with the rest of the element as it was", e !== undefined && restOf(e) === kept.rest)
                check("the inspector says which input", named("fillInput") !== null && named("fillInput").visible && named("fillInput").currentText === "Input 1",
                      named("fillInput") ? named("fillInput").currentText : "none")
                const mark = Lib.findAll(editScreen, item => item.objectName === "linkMark" && item.visible && item.caption.startsWith("Video Input"))
                check("the canvas marks the element with it", mark.length === 1 && mark[0].caption === "Video Input: Input 1", mark.map(m => m.caption).join(" | "))
                const views = Lib.findAll(editScreen, isView)
                check("and draws it live, a frame a tenth of a second apart", views.length === 1 && views[0].sending && views[0].interval === 100
                      && greenish(middleOf(views[0])), views.length + (views.length ? " " + views[0].interval + " " + middleOf(views[0]) : ""))
                testInput.grab("3-editor")
                named("fillInput").chosen(1)
                return 600
            },
            () => {
                const e = onDisk()
                check("another input chosen is in the file", e !== undefined && e.fillKind === "input" && e.fillInputIndex === 1, e ? e.fillInputIndex : "none")
                named("mediaScale").chosen(1)
                return 600
            },
            () => {
                const e = onDisk()
                check("and Scale to Fill, which leaves it a video input", e !== undefined && e.fillKind === "input" && e.fillMediaScale === 1 && e.fillInputIndex === 1,
                      e ? JSON.stringify([e.fillKind, e.fillMediaScale]) : "none")
                kept.undone = [editScreen.editor.undo(), editScreen.editor.undo(), editScreen.editor.undo()]
                return 600
            },
            () => {
                const e = onDisk()
                check("all three undone, the file is byte for byte as it was", kept.undone.every(x => x === "") && e.fillKind === "color"
                      && testInput.fileHash(kept.entry.file) === kept.hash, JSON.stringify(kept.undone))
                named("fillKind").chosen(4)
                return 600
            },
            () => {
                stopEditing()
                return 800
            },
            () => {
                // ---- On the output, under a look that gives it no theme (a theme's boxes are drawn in place of the slide's)
                const plain = Looks.looks.find(look => Looks.of(look.id, Screens.audience[0].id).theme === "")
                check("(a look that gives the output no theme)", plain !== undefined, Looks.looks.map(look => look.name).join(", "))
                Show.lookId = plain.id
                openEntry(kept.entry)
                goLive(kept.index)
                return 1500
            },
            () => {
                // (The middle of the shape as it is drawn on the output, as shares of the output)
                const drawn = Lib.find(output.contentItem, item => item.source !== undefined && item.boxFilled !== undefined && item.source !== null
                                       && item.source.id === kept.element.id && item.visible)
                const middle = drawn.mapToItem(output.contentItem, drawn.width / 2, drawn.height / 2)
                const fx = middle.x / output.contentItem.width
                const fy = middle.y / output.contentItem.height
                const onOutput = Lib.findAll(output.contentItem, isView)
                const seen = Lib.findAll(win.contentItem, isView)
                check("on the output the shape has the input in it, every frame", onOutput.length === 1 && onOutput[0].interval === 0 && greenish(outputAt(fx, fy)),
                      onOutput.length + " " + outputAt(fx, fy))
                check("and so has the operator's preview, a frame a tenth of a second apart, and no thumbnail has it live",
                      seen.length === 1 && seen[0].interval === 100 && greenish(previewAt(fx, fy)), seen.length + " " + previewAt(fx, fy))
                const still = Lib.findAll(slideCell(kept.index), isStill)
                const picture = still.length === 1 ? Lib.find(still[0], i => i.status !== undefined && i.visible && String(i.source).includes("/Video Inputs/")) : null
                check("the slide's thumbnail has the input's own thumbnail in the shape, red, holding nothing", picture !== null && picture.status === Image.Ready
                      && reddish(middleOf(picture)) && !still[0].sending, still.length + " " + (picture ? middleOf(picture) : "no picture"))
                check("its sound is not the shape's to play", onOutput.every(v => v.sound === false))
                testInput.grabOutput("4-fill-output")
                testInput.grab("4-fill-operator")
                Show.clearSlide()
                return 2500
            },
            () => {
                check("the slide cleared, nothing draws an input and none is captured", Lib.findAll(output.contentItem, isView).length === 0
                      && Lib.findAll(win.contentItem, isView).length === 0 && VideoInputs.inputs.every(i => !i.capturing),
                      JSON.stringify(VideoInputs.inputs.map(i => i.capturing)))
                const wrong = testInput.readText(Log.path).split("\n").filter(l => / input .*(could not|problem|error)/i.test(l))
                check("the log says nothing went wrong with the inputs", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
