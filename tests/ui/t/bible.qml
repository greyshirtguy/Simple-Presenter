import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Bible mode (BiblePanel.qml, src/bibles.h): come to by the toolbar's button and by Ctrl+Alt+B, and left by Show, the
// grid showing nothing at first and then the slides it made last; an XML Bible imported, and a passage looked up in it
// as it is typed, made into slides that are shown in the grid and go live as any slides do, ringed as live; Next Verse
// and Previous Verse growing the passage, the slide that is live kept as it was and still ringed, and stopping at a
// book's end and start; a whole chapter flowed evenly, and filled; the panel of options behind the cog, with Break on
// New Verse and each slide's own reference; a theme's look; the menus' ticks clear of their words; Save under the
// slides, and the slides saved as a presentation, saved to a playlist and put into the presentation that was open;
// api.bible, played by a stand-in on this computer (tests/ui/run.py), refusing a wrong key and then giving a passage,
// with its words of Jesus in red and its copyright said, and only the verse added asked for by Next Verse; Bible mode
// come to from a playlist, its slide ringed as live there too; and the settings' Bibles section, where the imported
// Bible is taken out again. Every verse is made up by the test runner: no Bible's words are in the
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
        return ["bibleTranslation", "biblePassage", "bibleTheme", "bibleVerseBox", "bibleReferenceBox", "bibleOptions"].map(name => {
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

    // The passage's words on a slide, its reference left out (the first line of its box)
    function versesOn(slide) {
        const box = (slide.elements ?? []).find(e => e.textBox && testInput.plain(e.text).trim() !== "")
        return box ? testInput.plain(box.text).split("\n")[0] : ""
    }

    // A presentation's row in the list at the left
    function rowOf(name) {
        const list = named("presentationList")
        for (let i = 0; i < list.count; ++i) {
            if (list.model[i].name === name) {
                list.positionViewAtIndex(i, ListView.Contain)
                return list.itemAtIndex(i)
            }
        }
        return null
    }

    // A control of the panel of options, which is in the window's overlay
    function option(name) {
        return Lib.find(panel().optionsPanel.contentItem, item => item.objectName === name)
    }

    // Whether a slide in the grid has the live ring drawn round it: orange, a few points
    // outside its frame, which is drawn 6 in from the cell
    function ringed(index) {
        const cell = slideCell(index)
        if (!cell)
            return false
        const at = cell.mapToItem(null, 3, cell.height / 2)
        const c = String(testInput.windowPixel(at.x, at.y))
        const r = parseInt(c.substring(1, 3), 16), g = parseInt(c.substring(3, 5), 16), b = parseInt(c.substring(5, 7), 16)
        return r > 150 && r > g + 40 && g > b
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
                check("the slides are under the bar", named("slideGrid").mapToItem(null, 0, 0).y >= bar.mapToItem(null, 0, bar.height).y - 1,
                      bar.mapToItem(null, 0, bar.height).y + " then " + named("slideGrid").mapToItem(null, 0, 0).y)
                check("the grid shows nothing yet: not the slides of the presentation that was open", win.document === null && named("slideGrid").count === 0,
                      win.documentKey)
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
                check("and comes to it again, with its bar as high as it was, and still nothing in the grid", win.bibleMode && named("biblePanel").height === kept.barHeight
                      && named("biblePanel").item.height === kept.barHeight && win.document === null, kept.barHeight + " then " + named("biblePanel").height)
                // ---- The Bibles that come with the app
                check("the three Bibles that come with the app are offered, the KJV chosen to begin with",
                      Bibles.translations.filter(t => t.source === "included").map(t => t.abbreviation).join(" ") === "KJV YLT BBE"
                      && named("bibleTranslation").value === "KJV", JSON.stringify(Bibles.translations.map(t => t.abbreviation)))
                panel().translationMenu(named("bibleTranslation"))
                return 400
            },
            () => {
                check("the Bible menu has them first, as Included", labels().startsWith("[Included], *King James Version (KJV), Young's Literal Translation (YLT), "
                      + "Bible in Basic English (BBE), --, [Imported]"), labels())
                menu.close()
                find("John 3:16")
                return 2500
            },
            () => {
                // (Nothing of its words is looked at: only that it was found, and made into slides.)
                const p = panel()
                check("a passage found in the KJV, made into slides, its public domain said", p.passage !== null && p.passage.error === "" && p.passage.translation === "KJV"
                      && p.passage.verses.length === 1 && slides().length === 1 && named("bibleStatus").text.includes("Public domain"),
                      p.passage ? p.passage.reference + " " + p.passage.error + " / " + named("bibleStatus").text : "none")
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
                check("with no theme, the words go in the plain slide's Verse box", named("bibleVerseBox").value === "Verse"
                      && named("bibleReferenceBox").value === "With Verse", named("bibleVerseBox").value)
                testInput.grab("2-a-passage")
                kept.first = versesOn(slides()[0])
                kept.firstPath = win.bibleKey
                click(centre(slideCell(0)))
                return 900
            },
            () => {
                check("a click on a slide puts it live", Show.currentSlide !== undefined && wordsOf(Show.currentSlide).includes("Made up words"),
                      wordsOf(Show.currentSlide).slice(0, 60))
                const lit = testInput.lit(false, 0.05, 0.05, 0.9, 0.9)
                check("and it is on the output, drawn", lit > 0.005, lit)
                check("and ringed in the grid as the live slide", viewingLive && liveIndex === 0 && ringed(0))
                const before = named("biblePrevious"), after = named("bibleNext")
                const transition = Lib.find(named("gridFooter"), item => item.toString().startsWith("TransitionControls"))
                const zoom = Lib.find(named("gridFooter"), item => item.toString().startsWith("ZoomButtons"))
                const from = transition.mapToItem(null, transition.width, 0).x, to = zoom.mapToItem(null, 0, 0).x
                const middle = (before.mapToItem(null, 0, 0).x + after.mapToItem(null, after.width, 0).x) / 2
                check("← Verse and Verse → are under the slides, midway between the transition and the size of the thumbnails",
                      before.text === "← Verse" && after.text === "Verse →" && before.mapToItem(null, 0, 0).x > from && after.mapToItem(null, after.width, 0).x < to
                      && Math.abs(middle - (from + to) / 2) < 3, from + " " + middle + " " + to)
                check("and the bar has no Find: Enter finds", named("bibleFind") === null)
                testInput.grabOutput("3-on-the-output")
                // ---- Next Verse and Previous Verse: the passage grows
                click(centre(named("bibleNext")))
                return 1200
            },
            () => {
                check("Next Verse: the verse after the passage added to it", panel().passage.reference === "John 3:16–19" && panel().passage.verses.length === 4,
                      panel().passage.reference)
                check("the slide that is live kept as it was, first, and still the live one, ringed", win.bibleKey !== kept.firstPath && Show.liveKey === win.bibleKey
                      && liveIndex === 0 && versesOn(slides()[0]) === kept.first && viewingLive && ringed(0), versesOn(slides()[0]).slice(-40) + " / " + liveIndex)
                check("its reference gone, it being no longer the last slide (Passage Last), and on the new last", !wordsOf(slides()[0]).includes("John 3:16")
                      && wordsOf(slides()[slides().length - 1]).endsWith("John 3:16–19 MUV"), wordsOf(slides()[slides().length - 1]).slice(-30))
                check("the verse added on the slides after it", slides().length >= 2 && slides().slice(1).map(s => wordsOf(s)).join(" ").includes("verse 19"),
                      slides().length)
                testInput.grab("3b-grown")
                click(centre(named("biblePrevious")))
                return 1200
            },
            () => {
                const at = slides().findIndex(s => versesOn(s) === kept.first)
                check("Previous Verse: the verse before it added, the live slide still as it was and still live, further on now",
                      panel().passage.reference === "John 3:15–19" && at >= 1 && liveIndex === at && viewingLive && ringed(at)
                      && wordsOf(slides()[0]).includes("verse 15"), panel().passage.reference + " / " + at + " / " + liveIndex)
                // With nothing live, the whole passage is flowed again
                clearSlide()
                return 400
            },
            () => {
                click(centre(named("bibleNext")))
                return 1200
            },
            () => {
                check("grown with nothing live: as the same passage typed would be", panel().passage.reference === "John 3:15–20" && panel().made.live === -1,
                      panel().passage.reference)
                find("John 3:20")
                return 1000
            },
            () => {
                click(centre(named("bibleNext")))
                return 1200
            },
            () => {
                check("Next Verse past a chapter's end: the next chapter's first, added", panel().passage.reference === "John 3:20–4:1", panel().passage.reference)
                find("Jude 3")
                return 1000
            },
            () => {
                click(centre(named("bibleNext")))
                return 1200
            },
            () => {
                check("at the end of a book, Next Verse says so and the passage stays", panel().problem === "That is the end of Jude."
                      && panel().passage.reference === "Jude 3" && named("bibleStatus").text === "That is the end of Jude.", panel().problem)
                find("John 3:1")
                return 1000
            },
            () => {
                click(centre(named("biblePrevious")))
                return 1200
            },
            () => {
                check("and at its start, Previous Verse (the made-up Bible has Genesis before John, and no John 2)",
                      panel().problem === "That is the start of John." && panel().passage.reference === "John 3:1", panel().problem)
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
                panel().edit({ flow: "even", references: "verse" })
                // ---- The options, behind the cog
                click(centre(named("bibleOptions")))
                return 500
            },
            () => {
                const options = panel().optionsPanel
                const cog = named("bibleOptions")
                check("the cog opens the panel of how the slides are made, under it: the four Bible Slide Options, References and Flow",
                      options.opened && ["bibleVerseNumbers", "bibleBreakOnNewVerse", "bibleDisplayTranslation", "biblePreserveColor", "bibleReferences", "bibleFlow"]
                      .every(n => option(n) !== null && option(n).visible) && option("bibleReferences").currentText === "Verse References"
                      && options.contentItem.mapToItem(null, 0, 0).y > cog.mapToItem(null, 0, cog.height).y)
                testInput.grab("4b-options")
                click(centre(option("bibleBreakOnNewVerse")))
                return 300
            },
            () => {
                check("a tick in it changes what it says", panel().settings.breakOnNewVerse === true && option("bibleBreakOnNewVerse").checked)
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("and Esc closes it", !panel().optionsPanel.opened)
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
                      panel().verseBoxName === "Text" && box !== undefined && box.name === "Text" && Math.round(box.width) === 1700 && Math.round(box.y) === 190
                      && runs.some(r => Math.round(r.size) === 90), box ? box.name + " " + box.width + " " + box.y + " " + JSON.stringify(runs[1] ?? {}) : "none")
                testInput.grab("5-in-a-theme")
                kept.made = slides().length
                // ---- The menus' ticks
                panel().themeMenu(named("bibleTheme"))
                return 400
            },
            () => {
                const row = Lib.find(menu.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.current === true)
                const words = row ? Lib.find(row, item => item.elide !== undefined && item.text === row.modelData.label) : null
                const tick = row ? Lib.find(row, item => item.text === "✓") : null
                check("a menu's tick is drawn clear of the words of its row", row !== null && words !== null && tick !== null && tick.visible
                      && tick.mapToItem(null, tick.width, 0).x + 4 <= words.mapToItem(null, 0, 0).x, labels())
                menu.close()
                // ---- Kept: Save, under the slides, at the left of the transition
                const save = named("bibleSave")
                const transition = Lib.find(named("gridFooter"), item => item.toString().startsWith("TransitionControls"))
                check("Save is under the slides, at the left of the transition", save !== null && save.visible && save.enabled && transition !== null
                      && save.mapToItem(null, save.width, 0).x < transition.mapToItem(null, 0, 0).x, transition ? "" : "no transition")
                click(centre(save))
                return 400
            },
            () => {
                check("and opens the three ways to keep the slides", labels().startsWith("Save as Presentation, Copy to Current Presentation, Save to Playlist"), labels())
                menu.close()
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
                // ---- A presentation clicked in Bible mode: Bible mode's target, not shown
                kept.other = documents.find(d => openable(d) && d.path !== kept.entry.path && catalog.open(d.file).slides.length > 0)
                kept.otherBefore = catalog.open(kept.other.file).slides.length
                kept.bibleDoc = win.documentKey
                click(centre(rowOf(kept.other.name)))
                return 600
            },
            () => {
                check("a presentation clicked in Bible mode is not shown: the grid keeps Bible mode's slides", win.bibleMode && win.documentKey === kept.bibleDoc
                      && slides().length === kept.made, win.documentKey)
                check("it is marked in the list, the one that was open no longer", listed(kept.other.name).picked && !listed(kept.entry.name).picked)
                check("the presentation's own buttons are not over Bible mode's slides: no Destination, Slide Show or Arrangements",
                      !named("destinationButton").visible && !named("slideShowButton").visible && !named("arrangementToggle").visible)
                panel().copyToCurrent()
                return 800
            },
            () => {
                const now = catalog.open(kept.other.file).slides.length
                check("Copy to Current Presentation puts them into the one clicked", now === kept.otherBefore + kept.made, kept.otherBefore + " then " + now)
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
                check("with a key it takes, the Bibles it gives are offered after those that come with the app and those imported", online !== undefined
                      && online.name === "Made Up Online" && Bibles.translations.map(b => b.source).join(" ") === "included included included file api",
                      JSON.stringify(Bibles.translations.map(b => b.name)))
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
                check("Next Verse, asked of api.bible: the verse added", panel().passage.reference === "John 3:16–17" && panel().passage.verses.length === 2,
                      panel().passage.reference)
                testInput.grab("6-api-bible")
                panel().edit({ translation: kept.file })
                return 1200
            },
            () => {
                check("another Bible chosen: the same passage, looked up in that one", panel().passage.reference === "John 3:16–17" && panel().passage.translation === "MUV"
                      && slides().map(s => wordsOf(s)).join(" ").includes("Made up words of verse 17"), panel().passage.reference + " " + panel().passage.translation)
                kept.lastMade = win.bibleKey
                // ---- Left by Show
                named("showButton").clicked()
                return 600
            },
            () => {
                check("the Show button leaves Bible mode for the presentation clicked in it", !win.bibleMode && win.documentKey === kept.other.path && !named("biblePanel").visible,
                      win.documentKey)
                // ---- Come to from a playlist being browsed
                kept.demoPlaylist = catalog.playlists.find(p => p.name === "Demo Playlist")
                win.openPlaylist(kept.demoPlaylist.path)
                return 600
            },
            () => {
                openEntry(documents.find(d => openable(d)))
                return 400
            },
            () => {
                check("(a playlist is being browsed)", playlistId !== "" && win.document !== null, playlistId)
                testInput.key(Qt.Key_B, Qt.ControlModifier | Qt.AltModifier)
                return 600
            },
            () => {
                check("Bible mode come to again shows the slides it made last", win.bibleMode && win.documentKey === kept.lastMade && slides().length > 0, win.documentKey)
                click(centre(slideCell(0)))
                return 900
            },
            () => {
                check("and one clicked there is live, and ringed, with a playlist being browsed", Show.liveKey === kept.lastMade && viewingLive && ringed(0),
                      Show.liveKey + " / " + playlistId)
                kept.target = bibleReturnEntry
                // (Its signal, as for the Bible button: see above)
                named("editButton").clicked()
                return 900
            },
            () => {
                check("Edit leaves Bible mode for the editor, on Bible mode's target", editing && !win.bibleMode && kept.target !== null && win.documentKey === kept.target.path,
                      win.documentKey)
                stopEditing()
                return 600
            },
            () => {
                check("(and left)", !win.bibleMode && !editing)
                openLibrary(kept.library)
                // ---- The settings
                settingsOpen = true
                kept.settings = Lib.find(win.contentItem, item => item.apiBibleKeyEdited !== undefined)
                kept.settings.section = "bibles"
                return 600
            },
            () => {
                const remove = named("removeBible:Made Up Version")
                check("the settings list the imported Bible, with a way to take it out", named("bibleSettings") !== null && remove !== null && remove.visible)
                check("and the three that come with the app, which cannot be", ["KJV", "YLT", "BBE"].every(a => named("includedBible:" + a) !== null && named("includedBible:" + a).visible)
                      && named("removeBible:King James Version") === null)
                testInput.grab("7-settings")
                click(centre(remove))
                return 600
            },
            () => {
                check("taken out, it is offered no more", !Bibles.translations.some(b => b.name === "Made Up Version"))
                panel().edit({ translation: "file:Made Up Version.xml" })
                check("and a Bible chosen that is there no more gives way to one that is, with nothing to choose (the KJV that comes with the app)",
                      panel().translation !== null && panel().translation.source === "included" && named("bibleTranslation").value === "KJV",
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
