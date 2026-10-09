import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Themes: ProPresenter's own, browsed from the toolbar's button; a presentation dressed
// in a theme slide, and one slide from its menu; what dressing keeps and what it
// replaces; and a theme made, edited in the editor, and removed.
QtObject {
    id: t

//COMMON
    function tiles() {
        return Lib.findAll(named("themesGrid"), item => String(item.objectName) === "themesTile").map(tile => tile.modelData)
    }

    function shown(name) {
        const item = named(name)
        if (!item)
            return false
        for (let at = item; at; at = at.parent) {
            if (!at.visible)
                return false
        }
        return true
    }

    function worded(slide) {
        return slide.elements.filter(e => e.text !== undefined && testInput.plain(e.text).trim() !== "")
    }

    function wordsOf(slides) {
        return slides.map(s => worded(s).map(e => testInput.plain(e.text)).join(" / "))
    }

    function themeSlide(place, name) {
        return Themes.theme(place).slides.find(s => s.name === name)
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("All Hail King Jesus"))
                kept.path = document.path
                kept.words = wordsOf(document.slides)
                kept.count = document.slides.length
                check("the themes are not up until asked for", !themesOpen && !shown("themesPanel"))
                named("themesButton").clicked()
                return 600
            },
            () => {
                const top = tiles()
                check("the toolbar's button shows the themes as they are kept: folders, and themes by their first slide", themesOpen && shown("themesPanel")
                      && top.some(e => e.kind === "folder" && e.name === "Samples") && top.some(e => e.kind === "theme" && e.name === "New Life Chapel" && e.slides.length === 10),
                      top.map(e => e.kind + " " + e.name).join(", "))
                testInput.grab("1-themes")
                themesPanel.pick(top.find(e => e.kind === "folder"))
                return 400
            },
            () => {
                check("a folder is gone into", themesPanel.folder === "Samples" && tiles().length >= 9 && tiles().every(e => e.kind === "theme")
                      && tiles().some(e => e.name === "Black Box"), tiles().map(e => e.name).join(", "))
                themesPanel.pick(tiles().find(e => e.name === "Black Box"))
                return 400
            },
            () => {
                check("and a theme's slides are shown, by name", themesPanel.theme === "Samples/Black Box" && tiles().length === 6 && tiles().every(e => e.kind === "slide")
                      && tiles()[0].name === "Two Lines" && named("themesHint").text.includes("All Hail King Jesus"), tiles().map(e => e.name).join(", "))
                testInput.grab("2-theme-slides")
                kept.two = themeSlide("Samples/Black Box", "Two Lines")
                themesPanel.pick(tiles()[0])
                return 900
            },
            () => {
                const box = worded(kept.two.slide)[0]
                const themed = testInput.runsOf(box.text)[0]
                const all = document.slides.filter(s => worded(s).length > 0)
                check("a click on a theme slide dresses every slide of the presentation in it: the theme's box", !themesOpen && all.length > 5
                      && all.every(s => worded(s).length === 1 && worded(s)[0].x === box.x && worded(s)[0].y === box.y
                                        && worded(s)[0].width === box.width && worded(s)[0].height === box.height),
                      all.length + " slides; " + JSON.stringify([box.x, box.y, box.width, box.height]))
                check("the theme's font, size and colour", all.every(s => { const f = testInput.runsOf(worded(s)[0].text)[0]
                                                                            return f.family === themed.family && f.size === themed.size && f.color === themed.color }),
                      themed.family + " " + themed.size + " " + themed.color)
                check("and every slide's own words, in as many slides", document.slides.length === kept.count
                      && JSON.stringify(wordsOf(document.slides)) === JSON.stringify(kept.words))
                check("it is in the file, and the operator is told where the file as it was is kept", JSON.stringify(wordsOf(catalog.open(kept.path).slides)) === JSON.stringify(kept.words)
                      && worded(catalog.open(kept.path).slides.find(s => worded(s).length > 0))[0].y === box.y && notice.includes("Two Lines") && notice.includes("backups") && !noticeIsError, notice)
                testInput.grab("3-dressed")
                // ---- one slide, from its menu
                kept.at = document.slides.findIndex(s => worded(s).length === 1)
                const items = themeMenuItems([document.slides[kept.at].id])
                const samples = items.find(i => i.label === "Samples")
                const life = items.find(i => i.label === "New Life Chapel")
                check("a slide's menu has the themes: folders by name, themes by the look of their first slide, then their slides",
                      items[0].header === "Theme" && samples !== undefined && samples.preview === undefined && samples.items.some(i => i.label === "Black Box" && i.preview)
                      && life !== undefined && life.preview && life.items.some(i => i.label === "Lower 3rd" && i.preview && typeof i.run === "function"),
                      items.map(i => i.label ?? i.header).join(", "))
                kept.picture = themeSlide("New Life Chapel", "Theme Slide")
                life.items.find(i => i.label === "Theme Slide").run()
                return 900
            },
            () => {
                const one = document.slides[kept.at]
                const theme = kept.picture.slide
                const brought = theme.elements.filter(e => e.text === undefined || testInput.plain(e.text).trim() === "" || true)
                check("one slide is dressed from its menu, and the others left", notice.startsWith("The slide is now in")
                      && worded(document.slides[kept.at + 1])[0].y === worded(kept.two.slide)[0].y
                      && JSON.stringify(wordsOf(document.slides)) === JSON.stringify(kept.words), notice)
                check("what else the theme slide has comes with it: its picture, and its text boxes that got no words, empty",
                      one.elements.length === theme.elements.length && worded(one).length === 1
                      && one.elements.filter(e => !worded(one).includes(e)).every(e => theme.elements.some(th => th.id === e.id)),
                      one.elements.length + " elements for the theme's " + theme.elements.length)
                // The bigger of the theme's two text boxes has the words
                const boxes = theme.elements.filter(e => e.name === "Verse" || e.name === "Reference")
                const bigger = boxes.reduce((a, b) => a.width * a.height >= b.width * b.height ? a : b)
                check("the words went into the larger of the theme's boxes, there being no name to go by", worded(one)[0].y === bigger.y && worded(one)[0].height === bigger.height,
                      worded(one)[0].name + " at " + worded(one)[0].y)
                kept.id = one.id
                applyTheme([one.id], "New Life Chapel", themeSlide("New Life Chapel", "Centre").id)
                return 900
            },
            () => {
                const one = document.slides.find(s => s.id === kept.id)
                const centre = themeSlide("New Life Chapel", "Centre").slide
                check("dressed in another theme, nothing of the first is left: one box, the words", one.elements.length === centre.elements.length && worded(one).length === 1
                      && worded(one)[0].y === worded(centre)[0].y && testInput.plain(worded(one)[0].text) === kept.words[kept.at].split(" / ")[0],
                      one.elements.length + " elements")
                // ---- making one
                named("themesButton").clicked()
                return 400
            },
            () => {
                themesPanel.theme = ""
                themesPanel.folder = ""
                named("themeNew").clicked()
                testInput.type("Plain Words\n")
                return 700
            },
            () => {
                const made = Themes.theme("Plain Words")
                check("a theme is made by name, with a slide to start from, and is in the workspace's Themes folder", made !== null && made.slides.length === 1
                      && worded(made.slides[0].slide).length === 1 && testInput.exists(catalog.workspacePath + "/Themes/Plain Words/Theme")
                      && themesPanel.theme === "Plain Words" && tiles().length === 1)
                named("themeAddSlide").clicked()
                return 500
            },
            () => {
                check("it is given another slide", Themes.theme("Plain Words").slides.length === 2 && tiles().length === 2 && tiles()[1].name === "Theme Slide 2")
                named("themeEdit").clicked()
                return 900
            },
            () => {
                check("Edit opens the editor on the theme, its slides in the list", editing && !themesOpen && editScreen.kind === "theme" && editScreen.editor.count === 2
                      && editScreen.subject === "Theme: Plain Words" && editScreen.rowWord === "Theme Slide", editScreen.kind + " " + editScreen.editor.count)
                testInput.grab("4-editor")
                // Its text box moved and made smaller, as the editor does it
                const box = editScreen.canvas.slide.elements[0]
                kept.box = box.id
                editScreen.editor.setProperties(editScreen.canvas.row, box.id, { y: 800, height: 200 })
                editScreen.renameRow(editScreen.canvas.slide.id, "Low")
                return 600
            },
            () => {
                check("a theme slide is renamed there", editScreen.editor.slideAt(0).label === "Low", editScreen.editor.slideAt(0).label)
                stopEditing()
                return 500
            },
            () => {
                const low = themeSlide("Plain Words", "Low")
                check("what was changed in the editor is the theme's", !editing && low !== undefined && low.slide.elements[0].y === 800 && low.slide.elements[0].height === 200,
                      low ? low.slide.elements[0].y : "no such slide")
                applyTheme([kept.id], "Plain Words", low.id)
                return 800
            },
            () => {
                const one = document.slides.find(s => s.id === kept.id)
                check("and slides can be dressed in it", worded(one)[0].y === 800 && worded(one)[0].height === 200)
                check("a theme's slide is removed, but not its last", Themes.removeSlide("Plain Words", themeSlide("Plain Words", "Theme Slide 2").id) === ""
                      && Themes.theme("Plain Words").slides.length === 1 && Themes.removeSlide("Plain Words", themeSlide("Plain Words", "Low").id) !== "")
                check("a theme is removed, folder and all", Themes.remove("Plain Words") === "" && Themes.theme("Plain Words") === null
                      && !testInput.exists(catalog.workspacePath + "/Themes/Plain Words") && testInput.exists(catalog.workspacePath + "/Themes/Samples/Black Box/Theme"))
                check("a name that is taken, or is no name, is refused", Themes.add("Samples").error !== "" && Themes.add(" ").error !== "" && Themes.add("a/b").error !== "")
                return 200
            }
        ]
        next()
    }
}
