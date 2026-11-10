import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Import PowerPoint (the maintainer, 2026-11-09: a PowerPoint file as editable slides, "textboxes for text, images for
// images"): the row of the + beside Libraries, the panel that says what the file holds and takes the library and the
// playlist, the import on its thread, and what it made: a presentation of three slides in the library, opened, with the
// sections as groups, the hidden slide disabled, the words in text boxes, the notes, the background colour and the
// background picture on the output where they should be and the picture on its slide where it was, and the pictures
// under the workspace's Media/Imported. The file the import made is looked at as a file in run.py (after_pptximport).
QtObject {
    id: t

//COMMON
    function panel() {
        return importPowerPointPanel
    }

    // A point of the output, by slide coordinates (the output is a floating window with its own title bar)
    function pixel(x, y) {
        return String(testInput.pixel(false, x / 1920, (20.5 + y * 300 / 1920) / 189))
    }

    function near(a, b) {
        const ca = Qt.color(a), cb = Qt.color(b)
        return Math.abs(ca.r - cb.r) < 0.06 && Math.abs(ca.g - cb.g) < 0.06 && Math.abs(ca.b - cb.b) < 0.06
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                kept.library = catalog.libraries[0]
                kept.playlist = catalog.playlists.find(node => !node.folder)
                kept.before = documents.length
                check("(a library and a playlist to import into)", kept.library !== undefined && kept.playlist !== undefined)
                // ---- The row of the + beside Libraries
                showLibraryMenu(named("slideGrid"))
                return 300
            },
            () => {
                check("the + beside Libraries offers Import PowerPoint, after the other imports", menu.opened && labels().endsWith("Import Lyrics from Clipboard, Import PowerPoint…"), labels())
                testInput.key(Qt.Key_Escape)
                // ---- The panel, given the file (the file dialog is the desktop's, so the test hands the file over as it would)
                kept.file = "@WORKSPACES@/Demo/Deck.pptx"
                const opened = panel().showFile(kept.file)
                check("the panel opens for a PowerPoint file, saying what it holds", opened && panel().visible && panel().found.slides === 3
                      && panel().found.width === 1920 && panel().found.height === 1080, JSON.stringify(panel().found))
                const found = named("importPowerPointFound")
                check("in words: the file, its slides and its shape", found !== null && found.text.startsWith("“Deck.pptx”: 3 slides, 16:9"), found ? found.text : "")
                check("the library is the one open, and no playlist, to begin with", panel().library === kept.library.path && panel().playlist === "", panel().library + " " + panel().playlist)
                check("the switch for the backgrounds starts off", PowerPointImport.leaveOutBackgrounds === false && named("importPowerPointBackgrounds").checked === false)
                panel().playlist = kept.playlist.path
                testInput.grab("1-panel")
                click(centre(named("importPowerPointButton")))
                return 200
            },
            () => {
                check("Import starts the import, on its thread, with how far it has got", panel().stage === "importing" || panel().stage === "done", panel().stage)
                return 2500
            },
            () => {
                const made = panel().result
                check("the import finished: three slides, as “Deck”", panel().stage === "done" && made !== null && made.ok === true && made.slides === 3 && made.name === "Deck",
                      JSON.stringify(made))
                const result = named("importPowerPointResult")
                check("the panel says what was made, and where", result !== null && result.text.startsWith("3 slides made, as “Deck” in the library “Demo”, and added to the playlist."),
                      result ? result.text : "")
                const notes = (made ? made.notes : []).map(note => note.message)
                check("and what could not be kept: the lists' markers, and the fonts not installed here", notes.some(note => note.startsWith("Lists are written as their markers"))
                      && notes.some(note => note.includes("is not installed here")), notes.join(" | "))
                // ---- What was made, as the app shows it
                check("the presentation is in the library, and opened", documents.length === kept.before + 1 && documents.some(d => d.name === "Deck") && document !== null
                      && document.name === "Deck" && libraryPath === kept.library.path, documents.length + " " + (document ? document.name : "none"))
                check("three slides, the third disabled (PowerPoint's hidden slide)", document.slides.length === 3 && document.slides[2].disabled === true
                      && document.slides[0].disabled !== true, JSON.stringify(document.slides.map(s => s.disabled === true)))
                check("the sections as groups", document.groups.map(g => g.name).join("|") === "Opening|Close" && document.slides[0].group === "Opening"
                      && document.slides[2].group === "Close", document.groups.map(g => g.name).join("|"))
                check("the words, in text boxes that can be edited", document.slides[0].plainText.includes("Welcome to the Service") && document.slides[0].plainText.includes("Songs of praise")
                      && document.slides[2].plainText.includes("Hidden slide") && document.slides[0].elements.length === 2 && document.slides[0].elements.every(e => e.textBox),
                      document.slides[0].plainText + " | " + document.slides[0].elements.length)
                check("a bulleted point has its marker written into its words", document.slides[0].plainText.includes("• Songs of praise"), document.slides[0].plainText)
                check("the speaker notes are the slide's notes", document.slides[0].notesText === "Speaker notes here", document.slides[0].notesText)
                check("the second slide's background picture is its background media, from the workspace's Media/Imported folder",
                      document.slides[1].mediaName === "image1.png" && document.slides[1].mediaForeground === false
                      && String(document.slides[1].media.path).includes("/Media/Imported/Deck/image1.png"), document.slides[1].mediaName + " " + document.slides[1].media.path)
                const picture = document.slides[1].elements.find(e => e.fillKind === "media")
                check("the picture on it is an element filled with its file, where it was (1200, 180, 480 by 480)", picture !== undefined && picture.fillMediaName === "image2.png"
                      && Math.abs(picture.x - 1200) < 1 && Math.abs(picture.y - 180) < 1 && Math.abs(picture.width - 480) < 1 && Math.abs(picture.height - 480) < 1,
                      picture ? JSON.stringify([picture.x, picture.y, picture.width, picture.height]) : "none")
                check("both pictures were written under Media/Imported/Deck", testInput.exists("@WORKSPACES@/Demo/Media/Imported/Deck/image1.png")
                      && testInput.exists("@WORKSPACES@/Demo/Media/Imported/Deck/image2.png"))
                check("added to the playlist", catalog.playlistItems(kept.playlist.path).some(row => row.name === "Deck"))
                check("the library and the playlist are remembered for next time", savedSelection("importLibrary") === kept.library.path && savedSelection("importPlaylist") === kept.playlist.path)
                check("the log says what came in", logged(/import +The PowerPoint file "Deck.pptx" came in as "Deck" in the library "Demo", 3 slides, and added to the playlist/).length === 1)
                click(centre(named("importPowerPointDone")))
                return 300
            },
            () => {
                check("Done shuts the panel", !panel().visible)
                // ---- On the output
                goLive(0)
                return 600
            },
            () => {
                check("the first slide's background colour fills the output", near(pixel(1800, 1000), "#204060") && near(pixel(100, 1000), "#204060"), pixel(1800, 1000) + " " + pixel(100, 1000))
                // (The output is a small window here, and its letters a pixel or two wide: light pixels, not white ones, are counted)
                check("with its title drawn in white over it", testInput.outputShare(0.1, 0.15, 0.6, 0.15, "#ffffff", 160) > 0.01, testInput.outputShare(0.1, 0.15, 0.6, 0.15, "#ffffff", 160))
                testInput.grabOutput("2-first")
                goLive(1)
                return 600
            },
            () => {
                check("the second slide's background picture is behind it, green", near(pixel(300, 900), "#00ff00"), pixel(300, 900))
                check("and its own picture is on it, red, where it was", near(pixel(1440, 420), "#ff0000") && near(pixel(1250, 230), "#ff0000") && near(pixel(1700, 420), "#00ff00"),
                      pixel(1440, 420) + " " + pixel(1250, 230) + " " + pixel(1700, 420))
                testInput.grabOutput("3-second")
                step(1)
                return 500
            },
            () => {
                check("Next from the second slide skips the disabled third: stays", liveIndex === 1, liveIndex)
                // ---- Leaving the backgrounds out: the switch, kept in the settings
                PowerPointImport.leaveOutBackgrounds = true
                check("the switch is kept in the settings", String(settings.value("pptxImport/leaveOutBackgrounds")) === "true", settings.value("pptxImport/leaveOutBackgrounds"))
                kept.before = documents.length
                const shown = panel().showFile(kept.file)
                panel().playlist = ""
                check("the panel opens again, ready", shown === true && panel().visible && panel().stage === "choose" && panel().library !== "",
                      shown + " " + panel().visible + " " + panel().stage + " " + panel().library)
                // (A frame for the panel to be laid out afresh: shown again after its report, it is a different height)
                return 300
            },
            () => {
                click(centre(named("importPowerPointButton")))
                return 3500
            },
            () => {
                const made = panel().result
                check("imported again, with the backgrounds left out: a second presentation, numbered", panel().stage === "done" && made && made.ok && made.name === "Deck 2"
                      && documents.length === kept.before + 1, JSON.stringify(made))
                check("its first slide draws no background colour, and its second has no background media", document.name === "Deck 2" && document.slides[1].mediaName === ""
                      && document.slides[1].elements.some(e => e.fillKind === "media"), document.slides[1].mediaName)
                check("its pictures are in a folder of its own", testInput.exists("@WORKSPACES@/Demo/Media/Imported/Deck 2/image2.png")
                      && !testInput.exists("@WORKSPACES@/Demo/Media/Imported/Deck 2/image1.png"))
                PowerPointImport.leaveOutBackgrounds = false
                click(centre(named("importPowerPointDone")))
                // (The first deck's background picture is still up on the media layer, as a
                // background stays until it is cleared)
                clearAll()
                goLive(0)
                return 600
            },
            () => {
                check("on the output the first slide of that one is black behind its words", near(pixel(1800, 1000), "#000000"), pixel(1800, 1000))
                // ---- A file that is not a PowerPoint file
                const refused = panel().showFile("@WORKSPACES@/Demo/Libraries/Demo/" + documents[0].name + ".pro")
                check("a file that is not a PowerPoint file is refused, and the panel does not open", refused === false && !panel().visible)
                clearAll()
                return 300
            },
            () => {
                // (The refusal of the .pro, shown to the user, is the one problem the test asked for)
                const wrong = logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme") && !l.includes("is not a PowerPoint file"))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
            }
        ]
        next()
    }
}
