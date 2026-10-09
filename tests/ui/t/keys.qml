import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// The hotkeys of groups in the workspace's key mappings; show mode and edit mode; the icons on the slides and how solid they are;
// and the editor's buttons. Run with settings as an earlier version left them (hotkeys in them), on a workspace whose key
// mappings hold other things.
QtObject {
    id: t

//COMMON
    readonly property var canvas: editScreen.canvas
    readonly property var editor: editScreen.editor

    function keysNow() {
        return groups.filter(g => g.key).map(g => g.name + "=" + g.key).join(" ")
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                const inFile = GroupKeys.keys.map(h => h.label + "=" + h.key).join(" ")
                check("the workspace's own list of groups gives the hotkeys", GroupKeys.exists && inFile === "Verse=A Verse 2=S Chorus=C", inFile)
                check("the groups of the settings have the ones under their own names, and not those an earlier version kept in the settings", keysNow() === "Verse=A Chorus=C", keysNow())
                check("a plain key mapped to a group in the key mappings is a hotkey too", JSON.stringify(GroupKeys.mappedKeys.map(h => h.label + "=" + h.key)) === '["Tag=Q"]'
                      && hotkeys.length === 4, JSON.stringify(GroupKeys.mappedKeys))
                check("the app's settings no longer hold hotkeys", !String(settings.value("groups", "")).includes("key"), String(settings.value("groups", "")).slice(0, 80))
                check("the settings screen is told of the hotkeys of groups it does not list", otherHotkeys === "S for Verse 2", otherHotkeys)
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("The Joy"))
                return 500
            },
            () => {
                // The example has "Verse 1", "Verse 2", "Chorus 1"...: A goes to the first verse, having no "Verse 1" of its own; S to the second
                const starts = name => document.slides.findIndex(s => s.groupStart && s.group === name)
                kept.verse1 = starts("Verse 1")
                kept.verse2 = starts("Verse 2")
                check("the presentation has a first and a second verse", kept.verse1 >= 0 && kept.verse2 > kept.verse1, kept.verse1 + " " + kept.verse2)
                check("each slide a key goes to is marked with it", groupKeyAt[kept.verse1] === "A" && groupKeyAt[kept.verse2] === "S", JSON.stringify(groupKeyAt))
                goLive(0)
                testInput.key(Qt.Key_S, 0, "s")
                check("S goes to the second verse, which is ProPresenter's own hotkey for it", liveIndex === kept.verse2, liveIndex)
                testInput.key(Qt.Key_A, 0, "a")
                check("and A to the first", liveIndex === kept.verse1, liveIndex)
                testInput.key(Qt.Key_V, 0, "v")
                check("V, which the settings of an earlier version had for the verse, is no hotkey here", liveIndex === kept.verse1 && !hotkeys.some(h => h.key === "V"))
                // ---- a hotkey changed on the settings screen
                settingsOpen = true
                return 400
            },
            () => {
                const screen = Lib.find(win.contentItem, i => i.keyEdited !== undefined)
                const bridge = groups.findIndex(g => g.name === "Bridge")
                const before = testInput.fileHash(GroupKeys.path)
                screen.groupsEdited(groups.map((g, i) => i === 0 ? Object.assign({}, g, { color: "#123456" }) : g))
                check("a change of colour alone does not touch the file", testInput.fileHash(GroupKeys.path) === before && groups[0].color === "#123456")
                screen.keyEdited(bridge, "X")
                check("a hotkey set on the settings screen goes into the list of groups, the group with it if it was not there",
                      GroupKeys.keys.map(h => h.label + "=" + h.key).join(" ") === "Verse=A Verse 2=S Chorus=C Bridge=X" && groups[bridge].key === "X", JSON.stringify(GroupKeys.keys))
                screen.keyEdited(bridge, "C")
                check("a key is one group's only, among those of the settings", GroupKeys.keys.map(h => h.label + "=" + h.key).join(" ") === "Verse=A Verse 2=S Bridge=C", JSON.stringify(GroupKeys.keys))
                screen.keyEdited(bridge, "")
                check("and a hotkey can be taken away", GroupKeys.keys.map(h => h.label + "=" + h.key).join(" ") === "Verse=A Verse 2=S" && keysNow() === "Verse=A", keysNow())
                // ---- how solid the icons are
                screen.section = "slides"
                return 300
            },
            () => {
                const slider = named("actionIconOpacity")
                check("the settings have a slider for how solid the slides' icons are, from 5% to all", slider !== null && Math.abs(slider.from - 0.05) < 1e-6 && slider.to === 1 && Math.abs(slider.value - 0.8) < 1e-6,
                      slider ? slider.from + ".." + slider.to + " at " + slider.value : "none")
                testInput.grab("1-settings-slides")
                const screen = Lib.find(win.contentItem, i => i.keyEdited !== undefined)
                screen.actionIconOpacityEdited(0.3)
                check("which is the app's own setting", Math.abs(actionIconOpacity - 0.3) < 1e-6 && Math.abs(Number(settings.value("actionIconOpacity")) - 0.3) < 1e-6, actionIconOpacity)
                screen.actionIconOpacityEdited(0)
                check("and goes no lower than 5%", Math.abs(actionIconOpacity - 0.05) < 1e-6, actionIconOpacity)
                screen.actionIconOpacityEdited(0.3)
                settingsOpen = false
                takeFocus()
                return 400
            },
            () => {
                const key = Lib.find(slideCell(kept.verse1), i => i.objectName === "hotkeyBadge")
                const media = Lib.find(slideCell(0), i => i.objectName === "mediaBadge")
                check("the hotkey's icon is smaller, with a thin black edge, and as solid as was set", key !== null && key.width === 18 && key.height === 18 && key.border.width === 1
                      && String(key.border.color) === "#000000" && Math.abs(key.opacity - 0.3) < 1e-6, key ? key.width + " " + key.opacity : "none")
                check("the media's icon is the same", media !== null && media.width === 21 && media.height === 18 && media.border.width === 1 && Math.abs(media.opacity - 0.3) < 1e-6, media ? media.width + " " + media.opacity : "none")
                actionIconOpacity = 0.8
                slideCell(0)
                return 300
            },
            () => {
                testInput.grab("2-icons")
                // ---- show mode and edit mode
                const show = named("showButton")
                const edit = named("editButton")
                check("the toolbar has Show beside Edit, and Show is lit", show !== null && show.kind === "show" && show.on && !edit.on && show.x < edit.x, show ? show.x + " " + edit.x : "none")
                check("Ctrl+E is taken as a shortcut", testInput.shortcut(Qt.Key_E, Qt.ControlModifier))
                return 600
            },
            () => {
                check("and goes into the editor", editing && named("editButton").on && !named("showButton").on)
                check("the editor has no Done button of its own", Lib.find(editScreen, i => i.text === "Done") === null)
                check("nor a + Text button over the elements", Lib.find(editScreen, i => i.text === "+ Text") === null)
                testInput.grab("3-editor")
                kept.rows = editor.count
                kept.row = canvas.row
                const add = named("editorAddRow")
                check("the slides have a + over them", add !== null && add.visible)
                add.clicked()
                return 600
            },
            () => {
                check("which adds a slide with nothing on it after the one being worked on", editor.count === kept.rows + 1 && canvas.row === kept.row + 1 && canvas.slide.elements.length === 0, canvas.row + " of " + editor.count)
                // the pointer over a corner handle
                canvas.addShape("rectangle")
                const handles = Lib.findAll(canvas, i => i.hx !== undefined && i.hy !== undefined)
                const corner = handles.find(i => i.hx === 1 && i.hy === 1)
                const side = handles.find(i => i.hx === 1 && i.hy === 0.5)
                check("a corner handle resizes, to begin with", !corner.turns && corner.lie === 1 && side.lie === 0, corner.lie + " " + side.lie)
                testInput.keyDown(Qt.Key_Control)
                check("with Ctrl down it turns, and has the pointer for that; a side handle still resizes", Cursors.ctrlHeld && corner.turns && !side.turns)
                testInput.keyUp(Qt.Key_Control)
                check("and with Ctrl up it resizes again", !Cursors.ctrlHeld && !corner.turns)
                canvas.setProperties({ rotation: 45 }, false)
                check("the arrows of a turned element's handles lie as it is turned", corner.lie === 2 && side.lie === 1, corner.lie + " " + side.lie)
                canvas.editText(canvas.selectedId)
                testInput.type("x")
                check("Ctrl+S is taken as a shortcut, even while typing", testInput.shortcut(Qt.Key_S, Qt.ControlModifier))
                return 600
            },
            () => {
                check("and goes back to showing, what was typed kept", !editing && named("showButton").on && document.slides.some(s => s.elements.length === 1 && s.elements[0].words === "x"))
                startEditingProps(Props.add("").id)
                return 500
            },
            () => {
                check("the props' editor is up", editing && editor.kind === "props")
                named("showButton").clicked()
                return 500
            },
            () => {
                check("and the Show button comes out of that too", !editing)
                const lines = testInput.readText(Log.path).split("\n")
                check("the log says what became of the hotkeys an earlier version kept", lines.some(l => l.includes("are left behind: the workspace's list of groups has its own")))
                check("nothing went wrong on the way", lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme") && !l.includes("was not found in the workspace")).length === 0,
                      lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme")).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
