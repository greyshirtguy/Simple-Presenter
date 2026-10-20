import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Clear groups: the buttons under the clears of the layers (one for each group that is not kept out of sight, with
// its picture, its tint and, where there is room, its name), what a click on one and F1 clear, the window the groups
// are set up in (the layers, the picture, the tint, the button, a new group, the order, removing one), and a group's
// button dragged onto a slide. The workspace is given a file of four groups as ProPresenter writes them (run.py,
// prepare_cleargroups); what the files then hold is looked at when the test is over, by after_cleargroups.
QtObject {
    id: t

//COMMON
    readonly property string file: catalog.workspacePath + "/Configuration/ClearGroups"

    function groupButtons() {
        return Lib.findAll(sidePanel, item => item.objectName === "clearGroup")
    }

    function buttonOf(name) {
        return groupButtons().find(button => button.modelData.name === name) ?? null
    }

    function group(name) {
        return ClearGroups.groups.find(candidate => candidate.name === name)
    }

    function near(a, b) {
        return Math.abs(a - b) <= 1.5
    }

    function part(c, i) {
        return parseInt(String(c).substring(1 + 2 * i, 3 + 2 * i), 16)
    }

    function red(c) {
        return part(c, 0) > 150 && part(c, 1) < 90 && part(c, 2) < 90
    }

    function like(a, b, slack) {
        return [0, 1, 2].every(i => Math.abs(part(a, i) - part(b, i)) <= slack)
    }

    // The colour the window draws just inside the top of a button, clear of what is on it: red while it has something
    // to clear. (Its picture is in the middle of it, as wide as a slim column lets it be.)
    function ground(button) {
        const at = button.mapToItem(null, button.width / 2, 2.5)
        return String(testInput.windowPixel(at.x, at.y))
    }

    // Whether a picture is drawn in a colour: whether any point along its middle, across and down, is near that colour
    function inked(item, color) {
        for (let i = 1; i < item.width; ++i) {
            const across = item.mapToItem(null, i, item.height / 2)
            const down = item.mapToItem(null, item.width / 2, i)
            if (like(String(testInput.windowPixel(across.x, across.y)), color, 40) || like(String(testInput.windowPixel(down.x, down.y)), color, 40))
                return true
        }
        return false
    }

    // Whether a group's button has its picture in a tint. On a button with nothing to clear the tint is drawn faint,
    // which is the colour partly gone into the grey of the button.
    function tinted(button, color) {
        const faint = "#" + [0, 1, 2].map(i => Math.round(0.45 * part(color, i) + 0.55 * part("#2b2d31", i)).toString(16).padStart(2, "0")).join("")
        return inked(pictureOf(button), button.live ? color : faint)
    }

    function pictureOf(button) {
        return Lib.find(button, item => item.objectName === "clearGroupPicture")
    }

    // One of the pictures to choose from, in the window: it is drawn there in the group's tint when it is the one chosen.
    // (While the window is up, what is behind it is drawn darker, the buttons among it: so a tint is looked for here.)
    function choice(icon) {
        return Lib.find(named("clearPicture:" + icon), item => item.icon !== undefined && item.ink !== undefined)
    }

    function rows() {
        return Lib.findAll(named("clearGroupsList"), item => item.objectName === "clearGroupRow")
    }

    function rowOf(name) {
        return rows().find(row => row.modelData.name === name) ?? null
    }

    function layerBox(key) {
        return named("clearLayer:" + key)
    }

    function layersShown() {
        return ClearGroups.layers.map(layer => (layerBox(layer.key).checked ? "+" : "-") + layer.name + (layerBox(layer.key).available ? "" : "(grey)")).join(" ")
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function dragTo(from, to) {
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 12; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
        kept.to = to
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("When Wind Meets Fire"))
                return 700
            },
            // ---- The buttons
            () => {
                kept.hash = testInput.fileHash(file)
                check("the workspace's clear groups are read, in their order", ClearGroups.groups.map(g => g.name + ":" + g.layers.join("+")).join(" | ")
                      === "Clear All:audio+audioEffects+messages+props+announcements+slide+media+videoInput | Words:messages+slide | Out Of Sight:props | Own:media",
                      ClearGroups.groups.map(g => g.name + ":" + g.layers.join("+")).join(" | "))
                const buttons = groupButtons()
                const layers = named("clearAudio")
                const top = layers.mapToItem(sidePanel, 0, 0).y
                const bottom = named("clearMedia").mapToItem(sidePanel, 0, named("clearMedia").height).y
                check("each has a button, in a column beside the clears of the layers, but the one that is kept out of sight", buttons.map(b => b.modelData.name).join("|") === "Clear All|Words|Own"
                      && buttons.every(b => b.visible && b.mapToItem(sidePanel, b.width, 0).x < layers.mapToItem(sidePanel, 0, 0).x),
                      buttons.map(b => b.modelData.name).join("|"))
                check("the buttons share the height of their column between them, which is the layers' column's", near(buttons[0].height, buttons[1].height)
                      && near(buttons[1].height, buttons[2].height) && near(buttons[0].mapToItem(sidePanel, 0, 0).y, top)
                      && near(buttons[2].mapToItem(sidePanel, 0, buttons[2].height).y, bottom) && buttons.every((b, i) => i === 0 || b.mapToItem(sidePanel, 0, 0).y > buttons[i - 1].mapToItem(sidePanel, 0, 0).y),
                      buttons.map(b => b.height.toFixed(1)).join(", ") + " from " + top + " to " + bottom)
                check("with nothing on the output none of them has anything to clear, and they are grey", cleared && liveMedia === null && liveProps.length === 0
                      && buttons.every(b => !b.live && !red(ground(b))), buttons.map(ground).join())
                check("a group's picture is drawn in its tint, the bell of the second in its blue (faint, there being nothing to clear), and the first in none",
                      pictureOf(buttons[1]).icon === 20 && tinted(buttons[1], "#1e88e5") && !tinted(buttons[0], "#1e88e5") && !inked(pictureOf(buttons[1]), "#1e88e5"))
                const own = pictureOf(buttons[2])
                const middle = own.mapToItem(null, own.width / 2, own.height / 2)
                kept.own = String(testInput.windowPixel(middle.x, middle.y))
                check("a picture of the user's own, kept in the file, is drawn as it is", own.icon === 0 && own.image !== "" && like(kept.own, "#ff00ff", 6), kept.own)
                kept.video = document.slides.findIndex(s => s.mediaName !== "" && s.mediaVideo && s.media !== undefined)
                kept.prop = Props.collections[0].props[0]
                goLive(kept.video)
                toggleProp(kept.prop.id)
                return 1500
            },
            () => {
                const buttons = groupButtons()
                check("(a slide, its video and a prop are on the output)", !cleared && liveMedia !== null && liveProps.length === 1)
                check("each group that has a layer with something on it is red: all three, for the slide, its media and the props among them",
                      buttons.every(b => b.live && red(ground(b))), buttons.map(b => b.modelData.name + ":" + b.live + ":" + ground(b)).join())
                check("and a tint is drawn at its full colour on a button that has something to clear", inked(pictureOf(buttons[1]), "#1e88e5"))
                testInput.grab("1-buttons")
                click(centre(buttonOf("Words")))
                return 600
            },
            () => {
                check("a click on a group clears its layers and no others: the slide has gone, the media and the prop have not", cleared && liveMedia !== null
                      && liveProps.length === 1 && liveSlide === null)
                check("its other layer is one this app has not, and nothing is done for it but to say what was asked", logged(/  clear +the clear group "Words" asked for/).length === 1)
                check("that button is grey now, and the two that still have something to clear are red", !buttonOf("Words").live && !red(ground(buttonOf("Words")))
                      && red(ground(buttonOf("Clear All"))) && red(ground(buttonOf("Own"))), groupButtons().map(ground).join())
                testInput.key(Qt.Key_F1)
                return 700
            },
            () => {
                check("F1 clears the first group, which here is all there is", cleared && liveMedia === null && liveProps.length === 0
                      && logged(/  clear +the clear group "Clear All" asked for/).length === 1 && groupButtons().every(b => !red(ground(b))))
                // ---- The window they are set up in
                click(centre(named("clearSlide")), Qt.RightButton)
                return 400
            },
            () => {
                check("a right click on the clear button of a layer has the way to the window the groups are set up in", labels() === "Configure Clear Groups…", labels())
                click(menuRow("Configure Clear Groups…"))
                return 700
            },
            () => {
                const panel = named("clearGroupsPanel")
                const title = named("clearGroupsTitle")
                check("which comes up over the window, on the first group", clearGroupsOpen && panel !== null && panel.visible && title.text === "Clear All"
                      && strokes(title).light >= 3, title ? title.text : "no window")
                const tags = rows().map(row => row.modelData.name + (Lib.findAll(row, i => i.font !== undefined && i.visible && (i.text === "F1" || i.text === "hidden")).map(i => "/" + i.text).join("")))
                check("its list has every group, the first marked as the one F1 clears and the one out of sight as hidden",
                      tags.join("|") === "Clear All/F1|Words|Out Of Sight/hidden|Own", tags.join("|"))
                check("the layers are ProPresenter's eight in its order; the seven this app has can be changed and the other is greyed, as the group has them",
                      layersShown() === "+Audio +Audio Effects +Messages +Props +Announcements +Slide +Media +Video Input(grey)", layersShown())
                const greyed = layerBox("videoInput")
                const ours = layerBox("slide")
                check("drawn so: a greyed one is fainter than one of ours", greyed.opacity < 0.6 && ours.opacity === 1 && strokes(ours).light >= 3)
                check("its picture is the one called All, picked out among the thirty-two", Lib.findAll(named("clearPictures"), i => String(i.objectName).startsWith("clearPicture:")).length === 32
                      && named("clearPicture:11").chosen && !named("clearPicture:20").chosen && !named("clearTint").checked && !named("clearTintColor").available)
                testInput.grab("2-window")
                click(centre(rowOf("Words")))
                return 400
            },
            () => {
                check("another group picked shows what that one has: its two layers, its bell, and its tint switched on, in its blue", named("clearGroupsTitle").text === "Words"
                      && layersShown() === "-Audio -Audio Effects +Messages -Props -Announcements +Slide -Media -Video Input(grey)"
                      && named("clearPicture:20").chosen && named("clearTint").checked && named("clearTintColor").available
                      && like(String(named("clearTintColor").value), "#1e88e5", 2), layersShown() + " " + named("clearTintColor").value)
                testInput.grab("3-second-group")
                click(centre(layerBox("media")))
                return 500
            },
            () => {
                check("a layer ticked is one the group clears, put where ProPresenter's order has it", group("Words").layers.join("+") === "messages+slide+media"
                      && layerBox("media").checked && logged(/  edit +the clear group "Words" clears Media/).length === 1, group("Words").layers.join("+"))
                click(centre(layerBox("media")))
                return 500
            },
            () => {
                check("unticked again, the file is byte for byte what it was", group("Words").layers.join("+") === "messages+slide" && testInput.fileHash(file) === kept.hash)
                click(centre(layerBox("videoInput")))
                return 400
            },
            () => {
                check("a greyed layer is not changed by a click", group("Words").layers.join("+") === "messages+slide" && testInput.fileHash(file) === kept.hash)
                click(centre(named("clearPicture:18")))
                return 500
            },
            () => {
                const button = pictureOf(buttonOf("Words"))
                check("a picture clicked is the group's: the star, on its button too, drawn in its tint", group("Words").icon === 18 && group("Words").picture === "Star"
                      && named("clearPicture:18").chosen && !named("clearPicture:20").chosen && button.icon === 18 && inked(choice(18), "#1e88e5")
                      && !inked(choice(20), "#1e88e5"), group("Words").picture)
                click(centre(named("clearTint")))
                return 500
            },
            () => {
                check("the tint switched off, the picture is drawn plain, and the colour is kept for when it is switched on again", !group("Words").tinted
                      && group("Words").tint === "#1e88e5" && !inked(choice(18), "#1e88e5") && !named("clearTintColor").available)
                click(centre(named("clearTint")))
                return 400
            },
            () => {
                check("(switched on again)", group("Words").tinted && inked(choice(18), "#1e88e5"))
                // The swatch leads to the app's own colour picker; what that hands over is the tint.
                click(centre(named("clearTintColor")))
                return 500
            },
            () => {
                // (The swatch is edged in orange for as long as its colours are up.)
                check("the swatch beside it brings up the colours to choose from", String(named("clearTintColor").border.color) === "#ff8a1f",
                      String(named("clearTintColor").border.color))
                click(centre(named("clearGroupsTitle")))
                return 400
            },
            () => {
                check("(a click beside them puts them away, and the window stays)", String(named("clearTintColor").border.color) !== "#ff8a1f" && clearGroupsOpen)
                clearGroupsPanel.setTint(true, "#43a047")
                return 500
            },
            () => {
                check("a colour chosen is the tint", group("Words").tint === "#43a047" && inked(choice(18), "#43a047") && !inked(choice(18), "#1e88e5")
                      && logged(/  edit +the clear group "Words" tinted #43a047/).length === 1, group("Words").tint)
                // ---- Kept out of sight
                click(centre(rowOf("Out Of Sight")))
                return 400
            },
            () => {
                check("(the group that is out of sight: its box is not ticked)", named("clearGroupsTitle").text === "Out Of Sight" && !named("clearShown").checked)
                click(centre(named("clearShown")))
                return 500
            },
            () => {
                const bottom = named("clearMedia").mapToItem(sidePanel, 0, named("clearMedia").height).y
                check("ticked, it has a button with the others, which make room for it", !group("Out Of Sight").hidden && groupButtons().length === 4
                      && groupButtons().map(b => b.modelData.name).join("|") === "Clear All|Words|Out Of Sight|Own" && near(groupButtons()[0].height, groupButtons()[3].height)
                      && near(groupButtons()[3].mapToItem(sidePanel, 0, groupButtons()[3].height).y, bottom), groupButtons().map(b => b.height.toFixed(1)).join())
                click(centre(named("clearShown")))
                return 500
            },
            () => {
                check("unticked, its button is gone again", group("Out Of Sight").hidden && groupButtons().length === 3)
                // ---- A new group
                click(centre(named("clearGroupsAdd")))
                return 500
            },
            () => {
                const made = ClearGroups.groups[ClearGroups.groups.length - 1]
                const box = Lib.find(rowOf(made.name), item => item.objectName === "clearGroupNameBox")
                check("the + makes a group, last in the list, picked, with its name to be typed over", ClearGroups.groups.length === 5 && made.name === "Clear Group"
                      && named("clearGroupsTitle").text === "Clear Group" && box !== null && box.activeFocus && box.selectedText === "Clear Group", made.name)
                check("it clears nothing yet, and its picture is the first digit no group has", made.layers.length === 0 && made.icon === 1 && made.picture === "1" && !made.tinted
                      && layersShown() === "-Audio -Audio Effects -Messages -Props -Announcements -Slide -Media -Video Input(grey)", layersShown())
                testInput.type("Offering")
                testInput.key(Qt.Key_Return)
                return 500
            },
            () => {
                check("named", group("Offering") !== undefined && named("clearGroupsTitle").text === "Offering" && rowOf("Offering") !== null
                      && buttonOf("Offering") !== null && groupButtons().length === 4)
                click(centre(layerBox("props")))
                return 500
            },
            () => {
                check("and given the props to clear", group("Offering").layers.join("+") === "props")
                click(centre(rowOf("Offering")), Qt.RightButton)
                return 400
            },
            () => {
                check("a group's menu has what can be done with it; the last cannot go further down", labels() === "[Offering], Rename, Move Up, Move Down(off), Delete…", labels())
                click(menuRow("Move Up"))
                return 500
            },
            () => {
                check("moved up, it is before the one that was above it, in the list and among the buttons", ClearGroups.groups.map(g => g.name).join("|")
                      === "Clear All|Words|Out Of Sight|Offering|Own" && groupButtons().map(b => b.modelData.name).join("|") === "Clear All|Words|Offering|Own",
                      ClearGroups.groups.map(g => g.name).join("|"))
                click(centre(named("clearGroupsAdd")))
                return 500
            },
            () => {
                testInput.type("Not Wanted")
                testInput.key(Qt.Key_Return)
                return 500
            },
            () => {
                check("(another is made)", ClearGroups.groups.length === 6 && group("Not Wanted") !== undefined && group("Not Wanted").icon === 2)
                click(centre(rowOf("Not Wanted")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Delete…"))
                return 400
            },
            () => {
                check("Delete asks first", labels() === "[Delete “Not Wanted”?], Delete Clear Group", labels())
                click(menuRow("Delete Clear Group"))
                return 500
            },
            () => {
                check("and then it is gone, and the one that was above it is picked", ClearGroups.groups.length === 5 && group("Not Wanted") === undefined
                      && named("clearGroupsTitle").text === "Own", named("clearGroupsTitle").text)
                testInput.grab("4-after-changes")
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("Esc puts the window away, and the keys run the show again", !clearGroupsOpen && !named("clearGroupsPanel").visible)
                // ---- What the new group does
                goLive(kept.video)
                toggleProp(kept.prop.id)
                return 1200
            },
            () => {
                check("(a slide, its video and a prop are on the output again; the new group's button is red, for the prop)", !cleared && liveMedia !== null
                      && liveProps.length === 1 && buttonOf("Offering").live && red(ground(buttonOf("Offering"))))
                const changed = pictureOf(buttonOf("Words"))
                check("the group that was changed has its new picture on its button, in its new tint", changed.icon === 18 && inked(changed, "#43a047")
                      && !inked(changed, "#1e88e5"))
                check("and the new group its digit, and not its name, as none of the buttons has", pictureOf(buttonOf("Offering")).icon === 1
                      && Lib.findAll(buttonOf("Offering"), item => item.text === "Offering").length === 0)
                testInput.grab("5-buttons-after")
                click(centre(buttonOf("Offering")))
                return 500
            },
            () => {
                check("its button clears the props and nothing else", liveProps.length === 0 && !cleared && liveMedia !== null && !red(ground(buttonOf("Offering"))))
                click(centre(buttonOf("Words")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Configure Clear Groups…"))
                return 600
            },
            () => {
                check("a right click on a group's button opens the window on that group", clearGroupsOpen && named("clearGroupsTitle").text === "Words")
                click(centre(named("clearGroupsClose")))
                return 400
            },
            () => {
                check("(its ✕ puts it away)", !clearGroupsOpen)
                // ---- A group's button dragged onto a slide
                kept.target = document.slides.findIndex((slide, index) => index !== kept.video && slide.actions.length === 0)
                testInput.snapshot(document.path, "song-before")
                dragTo(centre(buttonOf("Offering")), centre(slideCell(kept.target)))
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 700
            },
            () => {
                const actions = document.slides[kept.target].actions
                check("dragged onto a slide, a group's button gives the slide the action that clears that group", actions.length === 1 && actions[0].kind === "clearGroup"
                      && actions[0].groupId === group("Offering").id && actions[0].groupName === "Offering" && actions[0].title === "Clear the group “Offering”" && actions[0].done,
                      JSON.stringify(actions))
                toggleProp(kept.prop.id)
                return 500
            },
            () => {
                check("(a prop is on)", liveProps.length === 1)
                goLive(kept.target)
                return 700
            },
            () => {
                check("the slide going live clears its group: the prop has gone, and the slide is up", liveProps.length === 0 && !cleared && liveIndex === kept.target
                      && logged(/  action +Clear the group “Offering”/).length === 1)
                // (The pointer is taken away from where the menu comes up, which would otherwise open the row under it.)
                testInput.mouse(1, 4, 4)
                showAddActionMenu({ slide: kept.target }, slideCell(kept.target), 10, 10)
                return 400
            },
            () => {
                check("the menu of actions to add has Clear Group", labels().includes("Clear, Clear Group, Stage…"), labels())
                click(menuRow("Clear Group"))
                return 400
            },
            () => {
                check("which lists the workspace's groups, those out of sight among them", labels() === "[Clear Group], Clear All, Words, Out Of Sight, Offering, Own", labels())
                menu.close()
            }
        ]
        next()
    }
}
