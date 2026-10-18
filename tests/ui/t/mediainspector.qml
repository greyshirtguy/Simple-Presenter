import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The Media Inspector (qml/MediaInspector.qml, src/mediasettings.h), on a picture of four colours (red, green, blue and
// white from the top left) put on a slide ProPresenter wrote, and on a short video: chosen from the slide's menu, it
// shows the picture as the output does; flipping, turning a quarter, cropping, stretching and an effect each show in
// its preview as they should, by the colour at known points, and are each in the file at once, while the output keeps
// what it had; shown again, the slide's picture is drawn as now set on the output and in the operator's preview, and
// the grid's thumbnail is asked for cropped. Another file in its place (a video) keeps how it was fitted and its
// effects but not its crop; the video's in point, out point and picture are set from the inspector's player, and the
// video, shown, starts at its in point and stops at its out point. A row of the media bin flipped; an element's
// picture fill turned in the editor, drawn so in the editor, and undone with the editor's undo.
QtObject {
    id: t

    readonly property string media: "@WORKSPACES@".replace(/WorkSpaces$/, "media")
    readonly property string made: "@MADE@"

//COMMON
    function panel() {
        return mediaInspectorLoader.item
    }

    // Whether a colour ("#rrggbb") is near enough to one given as [r, g, b]
    function near(colour, rgb) {
        const c = String(colour)
        const parts = [1, 3, 5].map(i => parseInt(c.substring(i, i + 2), 16))
        return parts.every((part, i) => Math.abs(part - rgb[i]) < 50)
    }

    readonly property var red: [255, 0, 0]
    readonly property var green: [0, 255, 0]
    readonly property var blue: [0, 0, 255]
    readonly property var white: [255, 255, 255]
    readonly property var black: [0, 0, 0]
    readonly property var magenta: [255, 0, 255]

    // The colour at a point of the inspector's preview, as fractions of it, as the operator window shows it
    function previewAt(fx, fy) {
        const box = named("inspectorPreview")
        const p = box.mapToItem(null, fx * box.width, fy * box.height)
        return testInput.windowPixel(p.x, p.y)
    }

    // The colour at a point of the output, as fractions of it
    function outputAt(fx, fy) {
        return testInput.pixel(false, fx, fy)
    }

    // The colour at a point of the screen in the operator's preview, as fractions of the screen
    function operatorPreviewAt(fx, fy) {
        const picture = Lib.find(win.contentItem, item => item.namePrefix === "preview" && item.screenId !== undefined)
        const aspect = output.width / output.height
        const w = Math.min(picture.width, picture.height * aspect)
        const h = w / aspect
        const p = picture.mapToItem(null, (picture.width - w) / 2 + fx * w, (picture.height - h) / 2 + fy * h)
        return testInput.windowPixel(p.x, p.y)
    }

    // All of the slide's media's settings, as the file has them
    function settingsOnDisk() {
        return catalog.slideMediaSettings(kept.entry.file, kept.id)
    }

    // Clicks an item of the inspector by its name, scrolled into view first: the tabs'
    // pages are taller than the panel.
    function clickNamed(name) {
        const item = named(name)
        let page = item.parent
        while (page && page.contentY === undefined)
            page = page.parent
        if (page) {
            const at = item.mapToItem(page.contentItem, 0, 0).y
            page.contentY = Math.max(0, Math.min(at - 40, page.contentHeight - page.height))
        }
        click(centre(item))
    }

    function run() {
        steps = [
            () => {
                // ---- A slide with no media and no action of its own, and the picture of four colours put on it
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = catalog.open(d.file).slides
                        const at = slides.findIndex(s => s.media === undefined && s.actions.length === 0)
                        if (at >= 0 && slides.length > 1) {
                            kept.entry = d
                            kept.index = at
                            kept.id = slides[at].id
                            break
                        }
                    }
                    if (kept.entry)
                        break
                }
                check("(a slide with no media of its own)", kept.entry !== undefined, kept.entry ? kept.entry.name + " slide " + (kept.index + 1) : "none")
                check("(the picture of four colours put on it)", catalog.setSlideMedia(kept.entry.file, kept.id, media + "/Quarters.png", false) === "")
                openEntry(kept.entry)
                return 700
            },
            () => {
                const slide = document.slides[kept.index]
                check("set to nothing, the slide's map has no settings for it, and the file says nothing is set", slide.media !== undefined
                      && slide.media.settings === undefined && settingsOnDisk().drawn === false && settingsOnDisk().scale === 0,
                      slide.media ? JSON.stringify(settingsOnDisk()) : "no media")
                goLive(kept.index)
                return 1500
            },
            () => {
                check("on the output, the picture as it is", near(outputAt(0.25, 0.25), red) && near(outputAt(0.75, 0.25), green)
                      && near(outputAt(0.25, 0.75), blue) && near(outputAt(0.75, 0.75), white),
                      [outputAt(0.25, 0.25), outputAt(0.75, 0.25), outputAt(0.25, 0.75), outputAt(0.75, 0.75)].join(" "))
                showSlideMenu(kept.index, slideCell(kept.index), 10, 10)
                return 300
            },
            () => {
                check("a slide's menu has Inspector, under Media", labels().includes("Remove Media, Inspector, [Slide]"), labels())
                click(menuRow("Inspector"))
                return 900
            },
            () => {
                check("the Media Inspector comes up on the slide's media", panel() !== null && panel().open && panel().media.name === "Quarters.png"
                      && named("mediaInspector") !== null && panel().about.startsWith("Slide " + (kept.index + 1)))
                check("its preview shows the picture as the output does", near(previewAt(0.25, 0.25), red) && near(previewAt(0.75, 0.25), green)
                      && near(previewAt(0.25, 0.75), blue) && near(previewAt(0.75, 0.75), white),
                      [previewAt(0.25, 0.25), previewAt(0.75, 0.25), previewAt(0.25, 0.75), previewAt(0.75, 0.75)].join(" "))
                check("the log says so", /the Media Inspector opened on Quarters\.png/.test(testInput.readText(Log.path)))
                testInput.grab("m01-inspector")
                clickNamed("inspectorTab-properties")
                return 400
            },
            () => {
                clickNamed("inspectorFlipH")
                return 700
            },
            () => {
                // ---- Flipped across
                check("Flip Horizontal shows it flipped across", near(previewAt(0.25, 0.25), green) && near(previewAt(0.75, 0.25), red),
                      [previewAt(0.25, 0.25), previewAt(0.75, 0.25)].join(" "))
                check("and is in the file at once", settingsOnDisk().flipH === true)
                check("while the output keeps what it had", near(outputAt(0.25, 0.25), red))
                testInput.grab("m02-flipped")
                clickNamed("inspectorFlipH")
                return 400
            },
            () => {
                clickNamed("inspectorRotate90")
                return 700
            },
            () => {
                // ---- Turned a quarter: 180 by 320 now, in the middle of the preview, as high as it is
                const box = named("inspectorPreview")
                const shown = box.height * 180 / 320 / box.width
                const left = 0.5 - shown / 4
                const right = 0.5 + shown / 4
                check("turned a quarter clockwise, what was at its bottom left is at its top left", near(previewAt(left, 0.25), blue)
                      && near(previewAt(right, 0.25), red) && near(previewAt(left, 0.75), white) && near(previewAt(right, 0.75), green),
                      [previewAt(left, 0.25), previewAt(right, 0.25), previewAt(left, 0.75), previewAt(right, 0.75)].join(" "))
                check("with black beside it", near(previewAt(0.05, 0.5), black), previewAt(0.05, 0.5))
                check("in the file: turned 90, and flipped no longer", settingsOnDisk().rotation === 90 && settingsOnDisk().flipH === false)
                testInput.grab("m03-turned")
                clickNamed("inspectorRotate0")
                return 400
            },
            () => {
                // ---- Cropped: its left half, 160 of its 320 pixels, typed
                clickNamed("inspector-cropLeft")
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("160")
                testInput.key(Qt.Key_Return)
                return 700
            },
            () => {
                const settings = settingsOnDisk()
                check("a crop typed is in the file, and switches cropping on", settings.cropOn === true && settings.cropLeft === 160,
                      JSON.stringify([settings.cropOn, settings.cropLeft]))
                check("the right half alone is shown, fitted", near(previewAt(0.5, 0.25), green) && near(previewAt(0.5, 0.75), white)
                      && near(previewAt(0.1, 0.5), black), [previewAt(0.5, 0.25), previewAt(0.5, 0.75), previewAt(0.1, 0.5)].join(" "))
                named("inspectorScaling").activated(2)
                return 700
            },
            () => {
                check("Stretch to Fill stretches what is left to the whole screen", near(previewAt(0.06, 0.25), green) && near(previewAt(0.06, 0.75), white)
                      && near(previewAt(0.94, 0.25), green), [previewAt(0.06, 0.25), previewAt(0.06, 0.75), previewAt(0.94, 0.25)].join(" "))
                check("and is in the file", settingsOnDisk().scale === 2)
                testInput.grab("m04-cropped")
                clickNamed("inspectorTab-effects")
                return 400
            },
            () => {
                clickNamed("inspectorAddEffect")
                return 400
            },
            () => {
                check("Add Effect lists ProPresenter's effects, but the two drawn from what is behind", named("inspectorChoice-Color Invert") !== null
                      && named("inspectorChoice-Vignette") !== null && named("inspectorChoice-Background Blur") === null)
                clickNamed("inspectorChoice-Color Invert")
                return 900
            },
            () => {
                const effects = settingsOnDisk().effects ?? []
                check("an effect added is in the file, on, by ProPresenter's id for it", effects.length === 1 && effects[0].name === "Color Invert"
                      && effects[0].on && effects[0].renderId === "EDF55F11-1A1E-4BF3-9718-FA2D3731626D", JSON.stringify(effects.map(e => [e.name, e.on])))
                check("and is drawn: green inverted is magenta, white black", near(previewAt(0.5, 0.25), magenta) && near(previewAt(0.5, 0.75), black),
                      [previewAt(0.5, 0.25), previewAt(0.5, 0.75)].join(" "))
                testInput.grab("m05-effect")
                // Each of the effects drawn here in turn, shown and not kept, on the picture as it is set: each draws
                // something (its shader is there and works), and none leaves the preview empty.
                kept.choices = panel().effectChoices.filter(choice => choice.drawn)
                kept.kept = panel().settings
                kept.empty = []
                for (let i = 0; i < kept.choices.length; ++i) {
                    steps.splice(at + 2 * i, 0, () => {
                        panel().settings = Object.assign({}, kept.kept, { effects: [catalog.newMediaEffect(kept.choices[i].renderId)] })
                        return 250
                    }, () => {
                        const seen = [[0.3, 0.25], [0.7, 0.25], [0.3, 0.75], [0.7, 0.75]].map(p => previewAt(p[0], p[1]))
                        if (seen.every(colour => near(colour, black)))
                            kept.empty.push(kept.choices[i].name)
                        return 10
                    })
                }
                return 300
            },
            () => {
                check("every one of ProPresenter's effects drawn here draws on the preview", kept.choices.length === 38 && kept.empty.length === 0,
                      kept.choices.length + " effects; empty: " + kept.empty.join(", "))
                panel().settings = kept.kept
                kept.cornerBefore = previewAt(0.02, 0.04)
                clickNamed("inspectorAddEffect")
                return 400
            },
            () => {
                clickNamed("inspectorChoice-Vignette")
                return 700
            },
            () => {
                // A slider dragged: shown as it goes, kept when it is let go
                const slider = named("inspectorVariable-Radius")
                check("an effect added after another is picked, with what can be adjusted about it", slider !== null && panel().picked.name === "Vignette")
                const from = slider.mapToItem(null, slider.leftPadding + 0.5 * slider.availableWidth, slider.height / 2)
                const to = slider.mapToItem(null, slider.leftPadding + 0.95 * slider.availableWidth, slider.height / 2)
                drag(from, to)
                return 700
            },
            () => {
                const vignette = (settingsOnDisk().effects ?? []).find(effect => effect.name === "Vignette")
                const radius = vignette ? vignette.variables.find(variable => variable.name === "Radius") : undefined
                check("a slider dragged across is kept when let go", radius !== undefined && radius.value > 85, radius ? radius.value : "none")
                check("and the vignette darkens the corner more", near(previewAt(0.02, 0.04), black) && !near(kept.cornerBefore, black),
                      kept.cornerBefore + " to " + previewAt(0.02, 0.04))
                // Taken off again, with the button on its row, for what follows
                const row = named("inspectorEffect-Vignette")
                click(centre(Lib.find(row, item => item.text === "Remove")))
                return 600
            },
            () => {
                check("Remove takes an effect off", (settingsOnDisk().effects ?? []).map(effect => effect.name).join() === "Color Invert",
                      (settingsOnDisk().effects ?? []).map(effect => effect.name).join())
                clickNamed("inspectorDone")
                return 900
            },
            () => {
                // ---- Put away, and shown again
                check("Done puts it away", !panel().open)
                check("the grid has the slide as it is now set", document.slides[kept.index].media.settings.cropOn === true
                      && document.slides[kept.index].media.settings.effects.length === 1)
                check("the output still has what it had", near(outputAt(0.25, 0.25), red))
                const cell = slideCell(kept.index)
                const thumbnail = Lib.find(cell, item => item.source !== undefined && String(item.source).startsWith("image://thumbnail/"))
                check("the grid's thumbnail is asked for cropped", thumbnail !== null && String(thumbnail.source).includes("&c=160,0,0,0"),
                      thumbnail ? String(thumbnail.source) : "none")
                kept.gridThumbnail = thumbnail
                return 900
            },
            () => {
                // Cropped in the thumbnail made, and with its effect and its stretching over it
                const at = (fx, fy) => {
                    const p = kept.gridThumbnail.mapToItem(null, fx * kept.gridThumbnail.width, fy * kept.gridThumbnail.height)
                    return testInput.windowPixel(p.x, p.y)
                }
                check("and is drawn so: the right half alone, stretched, inverted: magenta over black", near(at(0.1, 0.12), magenta)
                      && near(at(0.9, 0.12), magenta) && near(at(0.5, 0.9), black), [at(0.1, 0.12), at(0.9, 0.12), at(0.5, 0.9)].join(" "))
                goLive(kept.index)
                return 1500
            },
            () => {
                check("shown again, the output has it as now set: the right half, stretched, inverted", near(outputAt(0.1, 0.25), magenta)
                      && near(outputAt(0.9, 0.25), magenta) && near(outputAt(0.5, 0.75), black),
                      [outputAt(0.1, 0.25), outputAt(0.9, 0.25), outputAt(0.5, 0.75)].join(" "))
                check("and so has the operator's preview", near(operatorPreviewAt(0.3, 0.25), magenta) && near(operatorPreviewAt(0.7, 0.75), black),
                      [operatorPreviewAt(0.3, 0.25), operatorPreviewAt(0.7, 0.75)].join(" "))
                testInput.grabOutput("m06-output")
                // ---- Another file in its place: a short video
                inspectSlideMedia(kept.index)
                return 600
            },
            () => {
                check("(the inspector up again)", panel().open)
                const error = panel().keeper.replace(made + "/b-short-4s.mp4")
                panel().reread()
                const settings = panel().settings
                check("another file in its place is the video", error === "" && panel().media.video === true && panel().media.name === "b-short-4s.mp4", error)
                check("and keeps how it was fitted and its effect, but not its crop", settings.scale === 2 && settings.effects.length === 1
                      && settings.cropOn === false, JSON.stringify([settings.scale, settings.effects.length, settings.cropOn]))
                // Fitted again, its effect taken off, and flipped across
                panel().change({ scale: 0, effects: [], flipH: true }, false)
                panel().tab = "properties"
                return 1800
            },
            () => {
                check("its player has the video", panel().duration > 3000, panel().duration)
                check("and the details have its size, from its first frame", named("inspectorSize").text === "1280 × 720", named("inspectorSize").text)
                // (It comes in from black: a second in, it is upright bars of colour, red at its left and cyan at its right.)
                panel().player.position = 1000
                return 900
            },
            () => {
                check("moved through while paused, its preview shows where it is, flipped across: cyan at the left",
                      near(previewAt(0.1, 0.3), [0, 255, 255]) && near(previewAt(0.9, 0.3), red), [previewAt(0.1, 0.3), previewAt(0.9, 0.3)].join(" "))
                return 100
            },
            () => {
                clickNamed("inspectorSetIn")
                panel().player.position = 2500
                return 700
            },
            () => {
                clickNamed("inspectorSetOut")
                named("inspectorPlayback").activated(0)
                panel().tab = "information"
                panel().player.position = 2000
                return 700
            },
            () => {
                clickNamed("inspectorSetThumbnail")
                return 600
            },
            () => {
                const settings = settingsOnDisk()
                check("its in point and out point, set from where its player was, are in the file", Math.abs(settings.inPoint - 1) < 0.2
                      && Math.abs(settings.outPoint - 2.5) < 0.2 && settings.trimmedOut === true && settings.endPoint > 3.5,
                      JSON.stringify([settings.inPoint, settings.outPoint, settings.endPoint, settings.trimmedOut]))
                check("and the moment its picture is taken from, and that it stops", Math.abs(settings.thumbnail - 2) < 0.2 && settings.playback === 0,
                      JSON.stringify([settings.thumbnail, settings.playback]))
                const thumbnail = Lib.find(named("mediaInspector"), item => item.objectName === "inspectorThumbnail")
                check("its thumbnail is asked for at that moment", thumbnail !== null && /&t=[0-9.]+/.test(String(thumbnail.source)), thumbnail ? String(thumbnail.source) : "none")
                check("the bar under its player marks the part that plays", named("inspectorTrimBar").opacity === 1)
                testInput.grab("m07-trimmed")
                clickNamed("inspectorDone")
                return 700
            },
            () => {
                goLive(kept.index)
                return 1300
            },
            () => {
                kept.startedAt = player() ? player().position : -1
                check("shown, the video starts at its in point", player() !== null && kept.startedAt >= 950 && kept.startedAt < 2400, kept.startedAt)
                check("flipped across on the output too", near(outputAt(0.1, 0.3), [0, 255, 255]) && near(outputAt(0.9, 0.3), red),
                      [outputAt(0.1, 0.3), outputAt(0.9, 0.3)].join(" "))
                return 2500
            },
            () => {
                const at = player() ? player().position : -1
                check("and stops at its out point", player() !== null && player().playbackState !== 1 && at >= 2300 && at <= 2900,
                      player() ? at + " " + player().playbackState : "none")
                check("the log says so", /has played to its out point/.test(testInput.readText(Log.path)))
                // ---- A row of the media bin
                const playlist = catalog.mediaPlaylists.find(p => !p.folder)
                check("(a media playlist)", playlist !== undefined)
                kept.playlist = playlist.path
                check("(the picture added to it)", catalog.addMedia(playlist.path, ["file://" + media + "/Quarters.png"]) === "")
                openMediaPlaylist(playlist.path)
                return 700
            },
            () => {
                const row = mediaFiles.find(m => m.name === "Quarters.png")
                check("(its row)", row !== undefined)
                kept.row = row.id
                showMediaItemMenu(row, binCell(m => m.id === row.id))
                return 300
            },
            () => {
                check("a row's menu has Inspector", labels().includes("Inspector"), labels())
                click(menuRow("Inspector"))
                return 800
            },
            () => {
                check("the inspector comes up on the row", panel().open && panel().about.startsWith("The media bin") && panel().media.name === "Quarters.png")
                clickNamed("inspectorTab-properties")
                return 300
            },
            () => {
                clickNamed("inspectorFlipV")
                return 700
            },
            () => {
                const row = catalog.mediaIn(kept.playlist).find(m => m.id === kept.row)
                check("flipped up and down, the row's file has it", row !== undefined && row.settings !== undefined && row.settings.flipV === true
                      && catalog.mediaItemSettings(kept.row).flipV === true)
                check("shown so in the preview", near(previewAt(0.25, 0.25), blue) && near(previewAt(0.25, 0.75), red),
                      [previewAt(0.25, 0.25), previewAt(0.25, 0.75)].join(" "))
                testInput.key(Qt.Key_Escape)
                return 700
            },
            () => {
                check("Esc puts it away", !panel().open)
                const cell = binCell(m => m.id === kept.row)
                const thumbnail = cell ? Lib.find(cell, item => item.source !== undefined && String(item.source).startsWith("image://thumbnail/")) : null
                check("and the bin's thumbnail is asked for flipped", thumbnail !== null && String(thumbnail.source).includes("&f=v"),
                      thumbnail ? String(thumbnail.source) : "none")
                kept.binThumbnail = thumbnail
                return 900
            },
            () => {
                const at = (fx, fy) => {
                    const p = kept.binThumbnail.mapToItem(null, fx * kept.binThumbnail.width, fy * kept.binThumbnail.height)
                    return testInput.windowPixel(p.x, p.y)
                }
                check("and is drawn so: blue at its top left, red at its bottom left", near(at(0.2, 0.2), blue) && near(at(0.2, 0.8), red),
                      [at(0.2, 0.2), at(0.2, 0.8)].join(" "))
                // ---- An element filled with the picture, in the editor
                startEditing(kept.entry, catalog.open(kept.entry.file).slides.find((s, i) => i !== kept.index).id)
                return 900
            },
            () => {
                editScreen.canvas.addMedia("file://" + media + "/Quarters.png")
                return 900
            },
            () => {
                const element = editScreen.canvas.selected
                check("(a picture added in the editor, and picked)", element !== null && element.fillKind === "media", element ? element.fillKind : "none")
                kept.element = element.id
                editScreen.inspectFill()
                return 800
            },
            () => {
                check("the inspector comes up on the element's fill", panel().open && panel().about.startsWith("The fill of"))
                clickNamed("inspectorTab-properties")
                return 300
            },
            () => {
                clickNamed("inspectorRotate180")
                return 600
            },
            () => {
                const canvas = editScreen.canvas
                const element = canvas.editor.slideAt(canvas.row).elements.find(e => e.id === kept.element)
                check("turned half round, the editor has it", element.fillMediaSettings !== undefined && element.fillMediaSettings.rotation === 180
                      && canvas.editor.fillMediaSettings(canvas.row, kept.element).rotation === 180)
                clickNamed("inspectorDone")
                return 700
            },
            () => {
                const canvas = editScreen.canvas
                const element = canvas.slide.elements.find(e => e.id === kept.element)
                const at = (fx, fy) => {
                    const p = canvas.mapToItem(null, canvas.originX + (element.x + fx * element.width) * canvas.u,
                                               canvas.originY + (element.y + fy * element.height) * canvas.u)
                    return testInput.windowPixel(p.x, p.y)
                }
                check("and draws it so: what was at the bottom right at the top left", near(at(0.25, 0.25), white) && near(at(0.75, 0.75), red),
                      [at(0.25, 0.25), at(0.75, 0.75)].join(" "))
                testInput.grab("m08-fill")
                canvas.undo()
                return 600
            },
            () => {
                const canvas = editScreen.canvas
                const element = canvas.editor.slideAt(canvas.row).elements.find(e => e.id === kept.element)
                check("the editor's undo takes it back, to media set to nothing", element !== undefined && element.fillMediaSettings === undefined
                      && canvas.editor.fillMediaSettings(canvas.row, kept.element).rotation === 0)
                stopEditing()
                return 600
            },
            () => {
                check("the log has each change kept", /Media Inspector: flipH of Quarters\.png kept/.test(testInput.readText(Log.path))
                      && /Media Inspector: effects of Quarters\.png kept/.test(testInput.readText(Log.path)))
                check("nothing went wrong on the way", !/PROBLEM|ERROR/.test(testInput.readText(Log.path)),
                      testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
