import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Bible mode (BiblePanel.qml, src/bibles.h): come to by the toolbar's button and by Ctrl+Alt+B, and left by Show; an XML
// Bible imported, and a passage looked up in it as it is typed, made into slides that are shown in the grid and go live
// as any slides do; Next Verse and Previous Verse; a whole chapter flowed evenly, and filled; Break on New Verse and each
// slide's own reference; a theme's look; the slides saved as a presentation, saved to a playlist and put into the
// presentation that was open; api.bible, played by a stand-in on this computer (tests/ui/run.py), refusing a wrong key
// and then giving a passage, with its words of Jesus in red and its copyright said; and the settings' Bibles section,
// where the imported Bible is taken out again. Every verse is made up by the test runner: no Bible's words are in the
// tests. What is looked at is what is drawn where the thing is drawn: the slides in the grid and on the output.
QtObject {
    id: t

//COMMON
    function panel() {
        return named("biblePanel").item
    }

    // The words of a slide's text boxes, and its runs
    function wordsOf(slide) {
        return (slide.elements ?? []).filter(e => e.textBox).map(e => testInput.plain(e.text)).join(" | ")
    }

    function runsOf(slide) {
        let all = []
        for (const element of slide.elements ?? []) {
            if (element.textBox)
                all = all.concat(testInput.runsOf(element.text))
        }
        return all
    }

    // How many words of the passage a slide holds (its reference and verse numbers left out)
    function countWords(slide) {
        const box = (slide.elements ?? []).find(e => e.textBox && testInput.plain(e.text).trim() !== "")
        const text = box ? testInput.plain(box.text).split("\n")[0] : ""
        return text.split(/\s+/).filter(w => w !== "" && !/^\d+ ?$/.test(w)).length
    }

    // What is drawn at the left end of each of the bar's controls, a little in from it: the
    // controls' own colour while nothing is drawn over them
    function barPixels() {
        return ["bibleTranslation", "biblePassage", "bibleTheme", "bibleVerseBox", "bibleReferences", "bibleFlow"].map(name => {
            const item = named(name)
            const at = item.mapToItem(null, 3, item.height / 2)
            return name + " " + testInput.windowPixel(at.x, at.y)
        }).join(", ")
    }

    // Whether every slide's words, laid out as they are drawn, fit inside their box: what
    // the slides were measured by is not what draws them
    function allFit() {
        const over = []
        slides().forEach((slide, index) => {
            for (const e of slide.elements ?? []) {
                if (!e.textBox || testInput.plain(e.text).trim() === "")
                    continue
                const room = e.height - e.marginTop - e.marginBottom
                const high = testInput.laidOutHeight(e.text, e.width - e.marginLeft - e.marginRight)
                if (high > room + 0.5)
                    over.push((index + 1) + ": " + Math.round(high) + " in " + Math.round(room))
            }
        })
        return over
    }

    function slides() {
        return win.document ? win.document.slides : []
    }

    function find(passage) {
        const field = named("biblePassage")
        field.forceActiveFocus()
        field.selectAll()
        testInput.type(passage)
        testInput.key(Qt.Key_Return)
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                kept.library = catalog.libraries[0].path
                kept.entry = documents.find(d => openable(d) && catalog.open(d.file).slides.length > 0)
                openEntry(documents.find(d => openable(d) && d !== kept.entry))
                openEntry(kept.entry)
                kept.before = catalog.open(kept.entry.file).slides.length
                return 600
            },
            () => {
                check("(a presentation is open)", win.document !== null && win.documentKey === kept.entry.path && !win.bibleMode, kept.entry.name)
                kept.remembered = String(settings.value(selectionKey("presentation"), ""))
                check("(and remembered as the one open)", kept.remembered !== "" && kept.entry.path.endsWith(kept.remembered), kept.remembered)
                kept.gridTop = named("slideGrid").mapToItem(null, 0, 0).y
                // (A click of the test's own does not reach a button of the toolbar, which is also where the window is
                // dragged from, Search's no more than this one's: the button's own signal is sent, which is what its
                // click sends.)
                named("bibleButton").clicked()
                return 800
            },
            () => {
                const bar = named("biblePanel")
                check("the toolbar's Bible button comes to Bible mode: its bar is over the slides", win.bibleMode && bar.visible && bar.item !== null && bar.height > 60,
                      bar.height)
                check("and the Show button is no longer the one lit", !named("showButton").on && named("bibleButton").on)
                check("the slides are under the bar", named("slideGrid").mapToItem(null, 0, 0).y >= kept.gridTop + bar.height - 1,
                      kept.gridTop + " then " + named("slideGrid").mapToItem(null, 0, 0).y)
                kept.barHeight = bar.height
                kept.barPixels = barPixels()
                testInput.grab("1-bible-mode")
                testInput.key(Qt.Key_B, Qt.ControlModifier | Qt.AltModifier)
                return 400
            },
            () => {
                check("Ctrl+Alt+B leaves it, back at the presentation that was open", !win.bibleMode && win.documentKey === kept.entry.path)
                testInput.key(Qt.Key_B, Qt.ControlModifier | Qt.AltModifier)
                return 400
            },
            () => {
                check("and comes to it again, with its bar as high as it was", win.bibleMode && named("biblePanel").height === kept.barHeight
                      && named("biblePanel").item.height === kept.barHeight, kept.barHeight + " then " + named("biblePanel").height)
                // ---- An XML Bible imported
                Bibles.importFile("file://" + catalog.workspacePath + "/Made Up Bible.xml")
                return 1500
            },
            () => {
                const made = Bibles.translations.find(b => b.name === "Made Up Version")
                check("an XML Bible imported is offered, under its own name and short name, and is the one chosen",
                      made !== undefined && made.abbreviation === "MUV" && made.source === "file" && panel().settings.translation === made.id,
                      JSON.stringify(Bibles.translations))
                kept.file = made ? made.id : ""
                find("john 3 16 18")
                return 1500
            },
            () => {
                const p = panel()
                check("a passage typed as people type them is found, and written out", p.passage !== null && p.passage.error === ""
                      && p.passage.reference === "John 3:16–18" && p.passage.verses.length === 3, p.passage ? p.passage.reference + " " + p.passage.error : "none")
                check("its slides are what the grid shows, as a presentation of its own", win.documentKey === win.bibleKey && win.bibleKey !== "" && slides().length >= 1,
                      slides().length)
                const remembered = String(settings.value(selectionKey("presentation"), ""))
                check("which is not remembered as the presentation that was open, its file going when the app closes",
                      remembered === kept.remembered, remembered)
                const all = slides().map(s => wordsOf(s)).join(" ")
                check("they hold the passage's words, with each verse's number", all.includes("said in red by a speaker") && all.includes("16 ") && all.includes("18 "),
                      all.slice(0, 160))
                const last = wordsOf(slides()[slides().length - 1])
                check("the passage's reference, with the translation, after the words of the last slide (Passage Last)", last.endsWith("John 3:16–18 MUV"), last.slice(-40))
                const red = runsOf(slides()[0]).find(r => r.text.includes("said in red"))
                check("red letters kept in red (Preserve Font Color)", red !== undefined && red.color !== "#ffffff", red ? red.color : "none")
                check("and the copyright said with the passage", named("bibleStatus").text.includes("Made up for the tests"), named("bibleStatus").text)
                testInput.grab("2-a-passage")
                click(centre(slideCell(0)))
                return 900
            },
            () => {
                check("a click on a slide puts it live", Show.currentSlide !== undefined && wordsOf(Show.currentSlide).includes("Made up words"),
                      wordsOf(Show.currentSlide).slice(0, 60))
                const lit = testInput.lit(false, 0.05, 0.05, 0.9, 0.9)
                check("and it is on the output, drawn", lit > 0.005, lit)
                testInput.grabOutput("3-on-the-output")
                // ---- Next Verse and Previous Verse
                click(centre(named("bibleNext")))
                return 1200
            },
            () => {
                check("Next Verse: the verse after the passage", panel().passage.reference === "John 3:19" && panel().passage.verses.length === 1, panel().passage.reference)
                click(centre(named("biblePrevious")))
                return 1200
            },
            () => {
                check("Previous Verse: the one before that", panel().passage.reference === "John 3:18", panel().passage.reference)
                find("John 3:20")
                return 1000
            },
            () => {
                click(centre(named("bibleNext")))
                return 1200
            },
            () => {
                check("Next Verse past a chapter's end: the next chapter's first", panel().passage.reference === "John 4:1", panel().passage.reference)
                // ---- A whole chapter, flowed
                find("John 3")
                return 1800
            },
            () => {
                const counts = slides().map(s => countWords(s))
                kept.evenCounts = counts
                check("a whole chapter is several slides", counts.length > 1, counts.join(" "))
                check("and none of them is drawn over the bar, which shows as it did before there were any",
                      barPixels() === kept.barPixels && slideCell(0).mapToItem(null, 0, 0).y >= named("biblePanel").mapToItem(null, 0, named("biblePanel").height).y,
                      barPixels() + " against " + kept.barPixels)
                const least = Math.min(...counts)
                const most = Math.max(...counts)
                check("flowed evenly: no slide is left with a scrap", least >= most * 0.5, counts.join(" "))
                check("and every slide's words fit in its box, laid out as they are drawn", allFit().length === 0, allFit().join(", "))
                testInput.grab("4-even")
                panel().edit({ flow: "fill" })
                return 1500
            },
            () => {
                const counts = slides().map(s => countWords(s))
                check("filled, every slide's words fit in its box too", allFit().length === 0, allFit().join(", "))
                check("filled as ProPresenter fills them: as many slides, each as full as goes", counts.length === kept.evenCounts.length
                      && counts[0] >= kept.evenCounts[0], counts.join(" ") + " against " + kept.evenCounts.join(" "))
                panel().edit({ flow: "even", breakOnNewVerse: true, references: "verse" })
                find("John 3:16-18")
                return 1500
            },
            () => {
                check("Break on New Verse: a slide for each verse, labelled with it", slides().length === 3
                      && slides().map(s => s.label).join(",") === "John 3:16,John 3:17,John 3:18", slides().map(s => s.label).join(","))
                check("Verse References: each slide says its own verse", wordsOf(slides()[1]).endsWith("John 3:17 MUV"), wordsOf(slides()[1]).slice(-30))
                panel().edit({ breakOnNewVerse: false, references: "last" })
                // ---- A theme's look
                kept.theme = Themes.add("Bible Look")
                return 600
            },
            () => {
                const theme = Themes.theme(kept.theme.place)
                check("(a theme is made, with a slide)", theme !== null && theme.slides.length === 1, JSON.stringify(kept.theme))
                panel().edit({ theme: kept.theme.place, themeSlide: theme.slides[0].id })
                return 1500
            },
            () => {
                const box = (slides()[0].elements ?? []).find(e => e.textBox && testInput.plain(e.text).includes("Made up"))
                const runs = box ? testInput.runsOf(box.text) : []
                check("the slides are made in the theme slide's look: its text box, where it is and how large, its font's size",
                      panel().verseBox === "Text" && box !== undefined && box.name === "Text" && Math.round(box.width) === 1700 && Math.round(box.y) === 190
                      && runs.some(r => Math.round(r.size) === 90), box ? box.name + " " + box.width + " " + box.y + " " + JSON.stringify(runs[1] ?? {}) : "none")
                testInput.grab("5-in-a-theme")
                kept.made = slides().length
                // ---- Kept: as a presentation, to a playlist, and into the one that was open
                panel().saveIn(kept.library, "")
                return 800
            },
            () => {
                const saved = documents.find(d => d.name === "John 3:16–18 (MUV)")
                check("Save as Presentation: a presentation of the passage in the library, its slides the ones made", saved !== undefined
                      && catalog.open(saved.file).slides.length === kept.made, (saved ? catalog.open(saved.file).slides.length : -1) + " of " + kept.made)
                kept.playlist = catalog.createPlaylist("Bible Test", "")
                return 400
            },
            () => {
                panel().saveIn(kept.library, kept.playlist.id)
                return 800
            },
            () => {
                // (The library has the second copy, and the playlist a row for it, which opening the playlist shows.)
                check("Save to Playlist: a second presentation of it saved", documents.filter(d => d.name.startsWith("John 3:16–18 (MUV)")).length === 2,
                      documents.map(d => d.name).filter(n => n.startsWith("John")).join(", "))
                win.openPlaylist(kept.playlist.id)
                return 600
            },
            () => {
                check("and a row for it in the playlist", documents.length === 1 && documents[0].name.startsWith("John 3:16–18 (MUV)"), documents.map(d => d.name).join(", "))
                openLibrary(kept.library)
                panel().copyToCurrent()
                return 800
            },
            () => {
                const now = catalog.open(kept.entry.file).slides.length
                check("Copy to Current Presentation: its slides put into the presentation that was open", now === kept.before + kept.made, kept.before + " then " + now + ", " + kept.made + " made")
                // ---- api.bible, played by the stand-in
                win.apiBibleKey = "a-wrong-key"
                Bibles.refreshApi()
                return 1500
            },
            () => {
                check("a key api.bible does not take: said, in its words", Bibles.apiProblem.includes("did not take the key") && Bibles.apiProblem.includes("Invalid API key"),
                      Bibles.apiProblem)
                win.apiBibleKey = "a-made-up-test-key"
                Bibles.refreshApi()
                return 1500
            },
            () => {
                const online = Bibles.translations.find(b => b.source === "api")
                check("with a key it takes, the Bibles it gives are offered after the imported ones", online !== undefined && online.name === "Made Up Online"
                      && Bibles.translations[0].source === "file", JSON.stringify(Bibles.translations.map(b => b.name)))
                panel().edit({ translation: online ? online.id : "", theme: "", themeSlide: "" })
                find("John 3:16")
                return 1500
            },
            () => {
                const p = panel()
                check("a passage asked of api.bible, found", p.passage !== null && p.passage.error === "" && p.passage.reference === "John 3:16",
                      p.passage ? p.passage.reference + " " + p.passage.error : "none")
                const words = slides().map(s => wordsOf(s)).join(" ")
                const red = slides().length > 0 ? runsOf(slides()[0]).find(r => r.text.includes("said in red online")) : undefined
                check("its words on the slides, those of Jesus in red", words.includes("said in red online") && red !== undefined && red.color !== "#ffffff", words.slice(0, 80))
                check("and its copyright said", named("bibleStatus").text.includes("Made up online for the tests"), named("bibleStatus").text)
                click(centre(named("bibleNext")))
                return 1500
            },
            () => {
                check("Next Verse, asked of api.bible", panel().passage.reference === "John 3:17", panel().passage.reference)
                testInput.grab("6-api-bible")
                panel().edit({ translation: kept.file })
                return 1200
            },
            () => {
                check("another Bible chosen: the same passage, looked up in that one", panel().passage.reference === "John 3:17" && panel().passage.translation === "MUV"
                      && slides().map(s => wordsOf(s)).join(" ").includes("Made up words of verse 17"), panel().passage.reference + " " + panel().passage.translation)
                // ---- Left by Show
                named("showButton").clicked()
                return 600
            },
            () => {
                check("the Show button leaves Bible mode, back at the presentation that was open", !win.bibleMode && win.documentKey === kept.entry.path && !named("biblePanel").visible)
                // ---- The settings
                settingsOpen = true
                kept.settings = Lib.find(win.contentItem, item => item.apiBibleKeyEdited !== undefined)
                kept.settings.section = "bibles"
                return 600
            },
            () => {
                const remove = named("removeBible:Made Up Version")
                check("the settings list the imported Bible, with a way to take it out", named("bibleSettings") !== null && remove !== null && remove.visible)
                testInput.grab("7-settings")
                click(centre(remove))
                return 600
            },
            () => {
                check("taken out, it is offered no more", !Bibles.translations.some(b => b.name === "Made Up Version"))
                panel().edit({ translation: "file:Made Up Version.xml" })
                check("and a Bible chosen that is there no more gives way to one that is, with nothing to choose (api.bible's first in English)",
                      panel().translation !== null && panel().translation.name === "Made Up Online" && named("bibleTranslation").value === "MUO",
                      named("bibleTranslation").value)
                kept.settings.section = "groups"
                settingsOpen = false
                return 300
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
