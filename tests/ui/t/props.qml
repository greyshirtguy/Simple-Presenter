import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Props: made from nothing in a workspace with none, edited in the editor, turned on and
// off over the slides, layered in the order they were turned on, cleared; collections,
// one at a time, moving, renaming, copying and removing.
QtObject {
    id: t

//COMMON
    function tabs() {
        return Lib.findAll(sidePanel, item => item.chosen !== undefined && item.modelData !== undefined && item.modelData.id !== undefined)
    }

    function list() {
        return named("propList")
    }

    function panel() {
        return list().parent
    }

    function rows() {
        return Lib.findAll(list(), item => item.modelData !== undefined && item.modelData !== null && item.naming !== undefined)
    }

    // A prop's row: its collection is shown first, the tab showing one at a time.
    function row(name) {
        const holds = Props.collections.find(c => c.props.some(p => p.name === name))
        if (holds && panel().collectionId !== holds.id) {
            panel().chosen = holds.id
            list().forceLayout()
        }
        return rows().find(r => r.modelData.name === name)
    }

    // The menu of a collection, from the button beside the drop-down
    function collectionMenu(name) {
        panel().chosen = collection(name).id
        panel().showCollectionMenu(named("propCollectionMenu"))
    }

    function all() {
        const found = []
        for (const collection of Props.collections) {
            for (const prop of collection.props)
                found.push(prop)
        }
        return found
    }

    function prop(name) {
        return all().find(p => p.name === name)
    }

    function collection(name) {
        return Props.collections.find(c => c.name === name)
    }

    function doneButton() {
        return named("showButton")
    }

    // The props the output is drawing, by id, the one at the back first
    function drawn() {
        return Lib.findAll(output.contentItem, item => item.propId !== undefined && item.on !== undefined)
                  .filter(item => item.on).sort((a, b) => a.z - b.z).map(item => item.propId)
    }

    // A box that fills part of the output with a colour, on the prop the editor is showing
    function paint(x, y, colour, words) {
        const canvas = editScreen.canvas
        canvas.addText()
        testInput.type(words)
        testInput.key(Qt.Key_Escape)
        canvas.setProperties({ x: x, y: y, width: 400, height: 200, fillOn: true, fillColor: colour }, false)
    }

    // The colour of the output at a point of the slide. The output is a small window
    // here, 300 by 189 with a bar of 20 across its top, and the slide fills its width.
    function pixel(x, y) {
        return testInput.pixel(false, x / 1920, (20.5 + y * 300 / 1920) / 189)
    }

    // How much of the output under its bar is lit
    function lit() {
        return testInput.lit(false, 0, 22 / 189, 1, 167 / 189)
    }

    function leftovers() {
        return Lib.findAll(output.contentItem, item => item.propId !== undefined && item.on !== undefined)
                  .map(item => item.propId.substring(0, 4) + " on=" + item.on + " made=" + item.made + " opacity=" + item.opacity).join("; ")
    }

    function run() {
        const near = (a, b) => Math.abs(a - b) < 1.5
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                check("a workspace with no props file has no props", Props.collections.length === 0 && !testInput.exists(Props.path), JSON.stringify(Props.collections))
                check("and nothing of them is on", liveProps.length === 0 && shownProps.length === 0 && !named("clearProps").live)
                click(centre(tabs()[1]))
                return 400
            },
            () => {
                const note = Lib.find(sidePanel, item => item.text !== undefined && String(item.text).startsWith("No props") && item.visible)
                check("the Props tab says there are none", sidePanel.showControlTab === "props" && list().count === 0 && note !== null)
                testInput.grab("1-empty")
                click(centre(named("showControlAdd")))
                return 400
            },
            () => {
                check("the + offers a prop or a collection", menu.opened && labels() === "New Prop, New Collection", labels())
                click(menuRow("New Prop"))
                return 900
            },
            () => {
                // ---- a new prop, in the editor
                check("a new prop opens in the editor", editing && editScreen.editor.kind === "props" && editScreen.editor.count === 1, editScreen.editor.kind + " " + editScreen.editor.count)
                check("the editor says what is being edited", editScreen.subject === "Props" && win.title === ("Simple Presenter " + Qt.application.version), editScreen.subject)
                check("the file has been made, with the prop in a collection", testInput.exists(Props.path) && Props.collections.length === 1
                      && Props.collections[0].name === "Default Collection" && all().length === 1 && all()[0].name === "Prop", JSON.stringify(Props.collections.map(c => c.name)))
                const title = Lib.find(editScreen, item => item.text === "Props" && item.font !== undefined && item.font.capitalization === Font.AllUppercase)
                check("the editor's list is of props", title !== null && named("editorAddRow").visible)
                const strip = Lib.find(editScreen, item => item.text === "Props   ·   Prop 1 of 1")
                check("and it says which prop this is", strip !== null)
                check("a new prop has nothing on it", editScreen.canvas.elements.length === 0 && editScreen.canvas.slide.width === 1920 && editScreen.canvas.slide.height === 1080)
                kept.a = all()[0].id
                paint(400, 400, "#ff0000", "A")
                return 500
            },
            () => {
                const canvas = editScreen.canvas
                check("a text box added to it is saved in the props file", canvas.elements.length === 1 && testInput.plain(canvas.elements[0].text) === "A"
                      && canvas.elements[0].fillEnabled && canvas.elements[0].x === 400 && editScreen.editor.changed, JSON.stringify(canvas.elements.map(e => [e.x, e.y, e.fillEnabled])))
                check("and can be undone", editScreen.editor.canUndo)
                testInput.grab("2-editor")
                doneButton().clicked()
                return 600
            },
            () => {
                check("Done goes back to the show", !editing && named("gridTitle").text === "Move Of God", named("gridTitle").text)
                check("with the prop as it was left", all().length === 1 && all()[0].slide.elements.length === 1 && all()[0].slide.elements[0].fillEnabled)
                check("listed in its collection, which the drop-down over the list names", list().count === 1 && panel().collection.name === "Default Collection"
                      && named("propCollection").model.join() === "Default Collection" && rows()[0].modelData.name === "Prop", rows().map(r => r.modelData.name).join(" | "))
                check("the keyboard is back with the show", keys.activeFocus)
                // ---- a second one, from the collection's own menu
                collectionMenu("Default Collection")
                return 400
            },
            () => {
                check("a collection's menu", labels() === "[Default Collection], New Prop, Rename, One at a Time, Remove with its Props…, [Collections], New Collection", labels())
                click(menuRow("New Prop"))
                return 900
            },
            () => {
                check("adds a prop to it, named apart from the first", editing && editScreen.editor.count === 2 && all().length === 2 && all()[1].name === "Prop 2"
                      && collection("Default Collection").props.length === 2, all().map(p => p.name).join())
                check("and the editor opens on that one", editScreen.canvas.row === 1 && editScreen.canvas.slide.id === all()[1].id)
                kept.b = all()[1].id
                paint(600, 500, "#0000ff", "B")
                return 500
            },
            () => {
                doneButton().clicked()
                return 600
            },
            () => {
                // ---- turning them on
                check("two props, neither on", !editing && all().length === 2 && liveProps.length === 0 && drawn().length === 0)
                check("the output is dark", lit() === 0, lit())
                click(centre(row("Prop")))
                return 1200
            },
            () => {
                check("a click turns a prop on", liveProps.length === 1 && liveProps[0] === kept.a && shownProps.length === 1 && row("Prop").on)
                check("it is drawn on the output", drawn().join() === kept.a && pixel(500, 450) === "#ff0000", pixel(500, 450))
                check("the button that clears props is red, and so is the one that clears everything", named("clearProps").live && named("clearAll").live
                      && !named("clearSlide").live)
                testInput.grab("3-one-on")
                testInput.grabOutput("3-output-one")
                goLive(1)
                return 1500
            },
            () => {
                check("a slide going live leaves it there", !cleared && liveProps.length === 1 && pixel(500, 450) === "#ff0000", pixel(500, 450))
                check("over the slide, whose words are there too", testInput.lit(false, 0, 30 / 189, 1, 50 / 189) > 0.01, testInput.lit(false, 0, 30 / 189, 1, 50 / 189))
                testInput.grabOutput("4-output-over-slide")
                click(centre(row("Prop 2")))
                return 1200
            },
            () => {
                check("a second is on as well, in front of the first", liveProps.join() === kept.a + "," + kept.b && drawn().join() === kept.a + "," + kept.b)
                check("where they overlap, the later one shows", pixel(700, 550) === "#0000ff" && pixel(450, 420) === "#ff0000" && pixel(950, 680) === "#0000ff",
                      pixel(700, 550) + " " + pixel(450, 420) + " " + pixel(950, 680))
                testInput.grabOutput("5-output-two")
                testInput.grab("5-two-on")
                click(centre(row("Prop")))
                return 1200
            },
            () => {
                check("a click on one that is on turns it off, and leaves the other", liveProps.join() === kept.b && drawn().join() === kept.b && pixel(450, 420) !== "#ff0000"
                      && pixel(950, 680) === "#0000ff", liveProps.join() + " " + pixel(450, 420))
                click(centre(row("Prop")))
                return 1200
            },
            () => {
                check("turned on again it is the latest, so it is the one in front", liveProps.join() === kept.b + "," + kept.a && drawn().join() === kept.b + "," + kept.a
                      && pixel(700, 550) === "#ff0000", pixel(700, 550))
                clearSlide()
                return 900
            },
            () => {
                check("clearing the slide leaves the props", cleared && liveProps.length === 2 && pixel(700, 550) === "#ff0000")
                testInput.key(Qt.Key_F4)
                return 1200
            },
            () => {
                check("F4 clears the props", liveProps.length === 0 && drawn().length === 0 && pixel(700, 550) === "#000000" && !named("clearProps").live, pixel(700, 550))
                check("and once they have faded there is nothing left of them to draw",
                      Lib.findAll(output.contentItem, item => item.propId !== undefined && item.on !== undefined).length === 0, leftovers())
                click(centre(row("Prop")))
                goLive(2)
                return 1200
            },
            () => {
                check("the props button clears them too, and nothing else", liveProps.length === 1 && !cleared)
                click(centre(named("clearProps")))
                return 1000
            },
            () => {
                check("(it did)", liveProps.length === 0 && !cleared && pixel(500, 450) !== "#ff0000")
                click(centre(row("Prop 2")))
                return 900
            },
            () => {
                testInput.key(Qt.Key_F1)
                return 1000
            },
            () => {
                check("clearing everything clears the props with the rest", liveProps.length === 0 && cleared && liveMedia === null)
                // ---- one at a time
                collectionMenu("Default Collection")
                return 400
            },
            () => {
                click(menuRow("One at a Time"))
                return 500
            },
            () => {
                check("a collection can be set to show one prop at a time", collection("Default Collection").single === true)
                check("and says so", named("propCollection").model[0].includes("one at a time"), named("propCollection").model[0])
                click(centre(row("Prop")))
                return 700
            },
            () => {
                click(centre(row("Prop 2")))
                return 1200
            },
            () => {
                check("then turning one on turns the other off", liveProps.join() === kept.b && drawn().join() === kept.b, liveProps.join())
                testInput.grab("6-one-at-a-time")
                // ---- collections
                click(centre(named("showControlAdd")))
                return 400
            },
            () => {
                click(menuRow("New Collection"))
                return 600
            },
            () => {
                check("a new collection is named in place", Props.collections.length === 2 && Props.collections[1].name === "New Collection" && sidePanel.renaming
                      && Lib.find(panel(), item => item.selectByMouse !== undefined && item.activeFocus) !== null && panel().collectionId === Props.collections[1].id, Props.collections.map(c => c.name).join())
                testInput.type("Lower Thirds\n")
                return 500
            },
            () => {
                check("with the name typed", Props.collections[1].name === "Lower Thirds" && !sidePanel.renaming && keys.activeFocus, Props.collections.map(c => c.name).join())
                check("the prop that was on is still on", liveProps.join() === kept.b)
                click(centre(row("Prop 2")), Qt.RightButton)
                return 400
            },
            () => {
                check("a prop's menu", labels() === "Edit, Rename, Duplicate, Move to, Remove…", labels())
                click(menuRow("Move to"))
                return 300
            },
            () => {
                check("Move to leads to the other collections", labels() === "[Move to], Lower Thirds", labels())
                click(menuRow("Lower Thirds"))
                return 500
            },
            () => {
                check("moves it to another collection", collection("Default Collection").props.length === 1 && collection("Lower Thirds").props.length === 1
                      && collection("Lower Thirds").props[0].id === kept.b, JSON.stringify(Props.collections.map(c => c.props.length)))
                check("which leaves it on", liveProps.join() === kept.b && pixel(950, 680) === "#0000ff")
                click(centre(row("Prop")))
                return 1200
            },
            () => {
                check("props of different collections are on together, whatever one of them says of its own", liveProps.join() === kept.b + "," + kept.a
                      && pixel(700, 550) === "#ff0000", liveProps.join())
                // ---- renaming a prop
                click(centre(row("Prop")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Rename"))
                return 500
            },
            () => {
                testInput.type("Banner\n")
                return 500
            },
            () => {
                check("a prop is renamed in place", prop("Banner") !== undefined && prop("Banner").id === kept.a && prop("Prop") === undefined, all().map(p => p.name).join())
                check("and stays on through it", liveProps.includes(kept.a) && pixel(450, 420) === "#ff0000")
                click(centre(row("Banner")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Duplicate"))
                return 500
            },
            () => {
                check("a copy goes after it, under a name of its own, with ids of its own", collection("Default Collection").props.map(p => p.name).join() === "Banner,Banner 2"
                      && prop("Banner 2").id !== kept.a && prop("Banner 2").slide.elements.length === 1
                      && prop("Banner 2").slide.elements[0].id !== prop("Banner").slide.elements[0].id === false
                      , collection("Default Collection").props.map(p => p.name).join())
                check("the copy is not on", !liveProps.includes(prop("Banner 2").id))
                testInput.snapshot(Props.path, "Props-mid")
                // ---- a prop that is on, edited
                click(centre(row("Banner")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Edit"))
                return 900
            },
            () => {
                check("Edit opens the editor at that prop", editing && editScreen.editor.kind === "props" && editScreen.canvas.slide.id === kept.a && editScreen.editor.count === 3)
                editScreen.canvas.pick(editScreen.canvas.elements[0].id)
                editScreen.canvas.setProperties({ fillColor: "#00ff00" }, false)
                return 500
            },
            () => {
                check("F4 still clears the props from in the editor", liveProps.length === 2)
                doneButton().clicked()
                return 1200
            },
            () => {
                check("what was changed shows on the output once done, the prop still on", !editing && liveProps.includes(kept.a) && pixel(450, 420) === "#00ff00", pixel(450, 420))
                // ---- removing
                click(centre(row("Banner 2")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Remove…"))
                return 500
            },
            () => {
                check("removing asks first", menu.opened && menu.items.length === 2 && menu.items[0].note !== undefined && menu.items[0].note.includes("Banner 2"), JSON.stringify(menu.items.map(i => i.note ?? i.label)))
                click(menuRow("Remove"))
                return 500
            },
            () => {
                check("and then removes it", prop("Banner 2") === undefined && all().length === 2)
                click(centre(row("Banner")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Remove…"))
                return 500
            },
            () => {
                click(menuRow("Remove"))
                return 1200
            },
            () => {
                check("a prop that is on goes from the output when it is removed", prop("Banner") === undefined && liveProps.join() === kept.b && drawn().join() === kept.b
                      && pixel(450, 420) === "#000000", liveProps.join() + " " + pixel(450, 420))
                testInput.grab("7-after-removing")
                collectionMenu("Lower Thirds")
                return 400
            },
            () => {
                click(menuRow("Remove with its Props…"))
                return 500
            },
            () => {
                check("removing a collection with props in it says they go too", menu.opened && menu.shown[0].note !== undefined && menu.shown[0].note.includes("the prop in it"), menu.shown[0].note)
                click(menuRow("Remove"))
                return 1200
            },
            () => {
                check("and they do", Props.collections.length === 1 && all().length === 0 && liveProps.length === 0 && drawn().length === 0, JSON.stringify(Props.collections.map(c => c.name)))
                check("leaving the output dark", lit() === 0, lit())
                return 100
            }
        ]
        next()
    }
}
