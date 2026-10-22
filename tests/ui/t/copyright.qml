import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The copyright display (src/ccli.h, CopyrightDisplay, the show's rule in show::State::goLive), on the Act workspace,
// which has no copyright display's file. Settings → Copyright: ProPresenter's layout to begin with and its example
// line; the display enabled (which makes the file), a licence number, the first and last slides; the layout changed (the
// publisher taken out, the CCLI song number added); Edit Template, the editor on the template with its box linked to
// the copyright, the box moved. The editor's Copyright tab for a presentation: made-up details typed, and Display
// Copyright ticked. Then the show: the line over the first slide on the output and in the operator's preview, drawn;
// none on the second; on the last again; none once the slide is cleared. after_copyright in run.py reads the files.
QtObject {
    id: t

//COMMON
    function texts(root) {
        return Lib.findAll(root, item => item.text !== undefined && item.font !== undefined && item.visible && typeof item.text === "string" && item.text !== "").map(item => item.text)
    }

    function token(index) {
        return named("copyrightToken:" + index)
    }

    // The template's box as drawn somewhere, with the line it shows
    function lineIn(root) {
        return Lib.find(root, item => item.source !== undefined && item.liveLinkText !== undefined && item.source !== null
                        && item.source.linkKind === "copyright" && item.visible)
    }

    function lit(fx, fy, fw, fh) {
        return testInput.lit(false, fx, fy, fw, fh)
    }

    function run() {
        steps = [
            () => {
                settingsOpen = true
                settingsScreen.section = "copyright"
                return 800
            },
            () => {
                check("Settings has Copyright", settingsScreen.sections.map(s => s.name).includes("Copyright"))
                check("a workspace with none starts off, on the first slide, with ProPresenter's layout", !named("copyrightEnabled").checked
                      && named("copyrightDisplay").currentText === "First Slide"
                      && CopyrightDisplay.settings.tokens.map(t => t.kind).join() === "title,text,artist,text,publisher,text,year,text,license",
                      CopyrightDisplay.settings.tokens.map(t => t.kind).join())
                check("its example is the line made of made-up details", named("copyrightExample").text === "Song Title\nArtist\nPublisher© 2026\n123456",
                      JSON.stringify(named("copyrightExample").text))
                check("and there is no file yet", !testInput.exists(catalog.workspacePath + "/Configuration/CCLI"))
                named("copyrightEnabled").toggled(true)
                return 500
            },
            () => {
                check("enabled, the file is made", CopyrightDisplay.settings.enabled === true && testInput.exists(catalog.workspacePath + "/Configuration/CCLI"))
                named("copyrightLicense").text = "998877"
                named("copyrightLicense").editingFinished()
                named("copyrightDisplay").activated(2)
                return 500
            },
            () => {
                check("the licence number and the first and last slides", CopyrightDisplay.settings.license === "998877" && CopyrightDisplay.settings.display === "firstAndLast"
                      && named("copyrightDisplay").currentText === "First and Last Slide", JSON.stringify(CopyrightDisplay.settings))
                check("the example has the licence number", named("copyrightExample").text.endsWith("998877"), JSON.stringify(named("copyrightExample").text))
                check("the layout's chips: the details, the line breaks and the words", texts(token(0)).join() === "Title" && texts(token(1)).join() === "New Line"
                      && texts(token(4)).join() === "Publisher" && texts(token(5)).join() === "© ", [0, 1, 4, 5].map(i => texts(token(i)).join()).join(" | "))
                click(centre(token(4)))
                return 300
            },
            () => {
                check("a chip's menu", labels() === "[Publisher], Move Left, Move Right, Remove", labels())
                click(menuRow("Remove"))
                return 500
            },
            () => {
                check("the publisher taken out", CopyrightDisplay.settings.tokens.map(t => t.kind).join() === "title,text,artist,text,text,year,text,license")
                click(centre(named("copyrightAddToken")))
                return 300
            },
            () => {
                check("+ has the details, words, and a new line", labels() === "[Add], Title, Artist, Author, Publisher, Copyright Year, CCLI Song Number, License Number, --, Text, New Line",
                      labels())
                click(menuRow("CCLI Song Number"))
                return 500
            },
            () => {
                check("the CCLI song number added, last", CopyrightDisplay.settings.tokens.map(t => t.kind).join() === "title,text,artist,text,text,year,text,license,songNumber")
                click(centre(named("copyrightEditTemplate")))
                return 1000
            },
            // ---- The template, in the editor
            () => {
                check("Edit Template opens the editor on it: one slide, its box linked to the copyright", editing && editScreen.editor.kind === "copyright"
                      && editScreen.editor.count === 1 && !settingsOpen && editScreen.canvas.elements.some(e => e.linkKind === "copyright"),
                      editScreen.editor.kind + " " + editScreen.editor.count)
                check("with no + to add another", named("editorAddRow") === null || !named("editorAddRow").visible)
                kept.box = editScreen.canvas.elements.find(e => e.linkKind === "copyright")
                editScreen.canvas.pick(kept.box.id)
                return 400
            },
            () => {
                editScreen.inspector.setProperties({ y: 700 }, false)
                return 500
            },
            () => {
                stopEditing()
                return 800
            },
            () => {
                const box = Array.from(CopyrightDisplay.templateSlide.elements).find(e => e.id === kept.box.id)
                check("the box moved, as the screens will draw it", box !== undefined && box.y === 700, box ? box.y : "no box")
                // ---- A presentation's details, in the editor's Copyright tab
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    const found = documents.find(d => openable(d) && catalog.open(d.file).slides.length > 3)
                    if (found) {
                        kept.entry = found
                        break
                    }
                }
                openEntry(kept.entry)
                return 600
            },
            () => {
                kept.count = document.slides.length
                startEditing(kept.entry, document.slides[0].id)
                return 900
            },
            () => {
                check("the editor has a Copyright tab for a presentation", named("tabCopyright") !== null && named("tabCopyright").visible)
                click(centre(named("tabCopyright")))
                return 500
            },
            () => {
                const set = (key, value) => {
                    const field = named("copyright-" + key)
                    field.text = value
                    field.editingFinished()
                }
                set("title", "A Made Up Song")
                set("artist", "Someone")
                set("author", "")
                set("publisher", "")
                set("year", "2001")
                set("songNumber", "")
                set("album", "")
                named("copyrightShown").toggled(true)
                return 700
            },
            () => {
                const c = editScreen.editor.copyright
                check("the details typed, and shown", c.title === "A Made Up Song" && c.artist === "Someone" && c.year === 2001 && c.display === true
                      && c.publisher === "" && c.songNumber === 0, JSON.stringify(c))
                check("undone as one step", editScreen.editor.canUndo)
                stopEditing()
                return 800
            },
            // ---- The show
            () => {
                openEntry(kept.entry)
                return 500
            },
            () => {
                goLive(0)
                return 900
            },
            () => {
                kept.line = "A Made Up Song\nSomeone\n© 2001\n998877"
                check("the first slide has the line", Show.copyrightText === kept.line, JSON.stringify(Show.copyrightText))
                const onOutput = lineIn(output.contentItem)
                check("drawn over it on the output, in the template's box", onOutput !== null && onOutput.liveLinkText === kept.line, onOutput ? onOutput.liveLinkText : "none")
                const preview = Lib.find(win.contentItem, i => i.objectName === "previewCopyright")
                check("and in the operator's preview", preview !== null && preview.active && lineIn(preview) !== null)
                // (Where the box is, from y 700, lit by its words)
                kept.lit = lit(0.03, 700 / 1080, 0.94, 180 / 1080)
                check("its words are drawn there", kept.lit > 0.003, kept.lit)
                testInput.grabOutput("1-first")
                testInput.grab("1-operator")
                goLive(1)
                return 900
            },
            () => {
                check("the second slide has none", Show.copyrightText === "" && lineIn(output.contentItem) === null)
                goLive(kept.count - 1)
                return 900
            },
            () => {
                check("the last has it again", Show.copyrightText === kept.line && lineIn(output.contentItem) !== null)
                clearSlide()
                return 900
            },
            () => {
                check("and it goes with the slide when it is cleared", Show.copyrightText === "" && lineIn(output.contentItem) === null)
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
