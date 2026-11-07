import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A song's words from SongSelect made a presentation (ImportLyrics.qml): from the lyrics file SongSelect gives out,
// and from the clipboard; what the panel says it found; the lines on a slide, the theme the slides are dressed in,
// the library the song goes into and the playlist it is added to, each as chosen and each remembered for the next
// song (the settings, by the workspace); the song opened in its library, its slides in their groups and dressed in the
// theme, drawn so, and its details where the copyright display looks for them; and what is said of nothing to import.
// The words are made up (LYRICS_SONG in run.py): a song's own are its owner's. The workspace is one with themes.
QtObject {
    id: t

    readonly property string file: "@WORKSPACES@/Act/Morning Song-lyrics.txt"
    readonly property string pasted: "Evening Song\n\nFirst line of the evening\nSecond line of it\n\nChorus\nSing the chorus\nSing it again\n"

//COMMON
    function themeSlide(place, name) {
        return Themes.theme(place).slides.find(s => s.name === name)
    }

    function parts(found) {
        return (found.sections ?? []).map(s => s.name + ":" + s.lines).join(" ")
    }

    // A combo box given its choice as a click on a row would give it
    function choose(box, label) {
        const index = box.model.indexOf(label)
        box.currentIndex = index
        box.activated(index)
        return index
    }

    function grey(colour) {
        const c = String(colour)
        return (parseInt(c.substring(1, 3), 16) * 299 + parseInt(c.substring(3, 5), 16) * 587 + parseInt(c.substring(5, 7), 16) * 114) / 1000
    }

    // What is drawn of a slide's thumbnail, part of the way across and down its cell
    function cellGrey(index, across, down) {
        const cell = slideCell(index)
        const p = cell.mapToItem(null, cell.width * across, cell.height * down)
        return grey(testInput.windowPixel(p.x, p.y))
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                // A second library and a playlist, for the song to be sent to
                const library = catalog.createLibrary("Hymns")
                check("a library Hymns is made beside the workspace's own", library.error === "" && catalog.libraries.some(l => l.name === "Hymns"), library.error)
                const playlist = catalog.createPlaylist("Sunday", "")
                check("and a playlist Sunday, with nothing in it", playlist.error === "" && catalog.playlistItems(playlist.id).length === 0, playlist.error)
                kept.playlist = playlist.id
                kept.hymns = catalog.libraries.find(l => l.name === "Hymns").path
                click(centre(named("addLibraryButton")))
                return 400
            },
            () => {
                check("the + beside Libraries offers a lyrics file and the clipboard, after the ChordPro file",
                      labels() === "New Presentation…, New Library, Import ChordPro File…, Import Lyrics File…, Import Lyrics from Clipboard", labels())
                menu.close()
                importLyricsPanel.showFile(file)
                return 400
            },
            () => {
                const found = importLyricsPanel.found
                check("the file is read: its title, its authors, its number and its parts, under their labels", importLyricsPanel.visible && found.title === "Morning Song"
                      && found.author === "Nobody Inparticular, Another Nobody" && found.songNumber === "1234567" && found.year === 2001
                      && parts(found) === "Verse 1:4 Chorus:3 Verse 2:4 Bridge:2 Ending:1", JSON.stringify(found))
                check("and the panel says where the words came from", importLyricsPanel.source === "the file Morning Song-lyrics.txt", importLyricsPanel.source)
                check("two lines to a slide, which makes eight; no theme; the library that is open; no playlist", importLyricsPanel.lines === 2 && importLyricsPanel.slideCount === 8
                      && importLyricsPanel.themeName === "None" && named("lyricsThemeButton").text === "None" && importLyricsPanel.library === libraryPath
                      && importLyricsPanel.playlist === "" && named("lyricsPlaylistBox").currentIndex === 0,
                      importLyricsPanel.slideCount + " " + importLyricsPanel.themeName + " " + importLyricsPanel.library)
                testInput.grab("1-import")
                importLyricsPanel.lines = 3
                check("three lines to a slide makes seven", importLyricsPanel.slideCount === 7, importLyricsPanel.slideCount)
                importLyricsPanel.chooseTheme("Samples/White Box", themeSlide("Samples/White Box", "Two Lines").id)
                check("a theme slide chosen is named on the button", importLyricsPanel.themeName === "White Box \u203a Two Lines" && named("lyricsThemeButton").text === importLyricsPanel.themeName,
                      importLyricsPanel.themeName)
                choose(named("lyricsLibraryBox"), "Hymns")
                check("the library box sends the song to Hymns", importLyricsPanel.library === kept.hymns, importLyricsPanel.library)
                check("the playlist box lists the playlist after None", named("lyricsPlaylistBox").model.join("|") === "None|Sunday", named("lyricsPlaylistBox").model.join("|"))
                choose(named("lyricsPlaylistBox"), "Sunday")
                check("and adds the song to Sunday", importLyricsPanel.playlist === kept.playlist, importLyricsPanel.playlist)
                testInput.grab("2-import-set")
                click(centre(named("importLyricsButton")))
                return 900
            },
            () => {
                check("the song is made in Hymns, which is browsed, and is opened", document !== null && document.name === "Morning Song"
                      && document.path === kept.hymns + "/Morning Song.pro" && libraryPath === kept.hymns && documents.some(d => d.name === "Morning Song"),
                      (document ? document.path : "none") + " in " + libraryPath)
                const row = listed("Morning Song")
                check("its row is in view and picked out, with its name drawn", row.inView && row.picked && row.light >= 3 && row.dark >= 3, JSON.stringify(row))
                check("a slide for every three lines, in a group for each part, by the file's labels", document.slides.length === 7
                      && document.slides.map(s => s.group).join("|") === "Verse 1|Verse 1|Chorus|Verse 2|Verse 2|Bridge|Ending", document.slides.map(s => s.group).join("|"))
                const first = document.slides[0].elements[0], chorus = document.slides[2].elements[0]
                check("the words on the slides, three lines then one, and the chorus's three", testInput.plain(first.text) === "The first line of the first verse\nThe second line of it\nThe third line of it"
                      && testInput.plain(document.slides[1].elements[0].text) === "The fourth line of it" && testInput.plain(chorus.text).split("\n").length === 3,
                      testInput.plain(first.text))
                const themed = themeSlide("Samples/White Box", "Two Lines").slide.elements[0]
                check("every slide dressed in the theme slide: its one text box is the theme's, by name, place and size, in the theme's font",
                      document.slides.every(s => s.elements.length === 1 && s.elements[0].name === "Text" && Math.abs(s.elements[0].x - themed.x) < 0.5
                                                 && Math.abs(s.elements[0].width - themed.width) < 0.5)
                      && testInput.runsOf(first.text)[0].family === "Helvetica" && testInput.runsOf(first.text)[0].size === 65,
                      JSON.stringify({ name: first.name, x: first.x, width: first.width, font: testInput.runsOf(first.text)[0].family, size: testInput.runsOf(first.text)[0].size }))
                check("its details are where the copyright display looks: title, authors, publishers one a line, year, number, shown", document.ccli.title === "Morning Song"
                      && document.ccli.author === "Nobody Inparticular, Another Nobody" && document.ccli.publisher === "Example Music\nSample Songs Publishing"
                      && document.ccli.year === 2001 && document.ccli.songNumber === 1234567 && document.ccli.display === true, JSON.stringify(document.ccli))
                const rows = catalog.playlistItems(kept.playlist)
                check("and it is the one row of the playlist Sunday", rows.length === 1 && rows[0].name === "Morning Song" && rows[0].file === document.path,
                      JSON.stringify(rows.map(r => r.name)))
                check("the choices are kept for the next song, by the workspace", settings.value("workspaces/Act/import/themePlace") === "Samples/White Box"
                      && settings.value("workspaces/Act/import/themeSlide") === themeSlide("Samples/White Box", "Two Lines").id
                      && settings.value("workspaces/Act/importLibrary") === "Libraries/Hymns" && settings.value("workspaces/Act/importPlaylist") === kept.playlist,
                      [settings.value("workspaces/Act/import/themePlace"), settings.value("workspaces/Act/importLibrary"), settings.value("workspaces/Act/importPlaylist")].join(" "))
                return 400
            },
            () => {
                // Drawn: the theme's pale box across the first slide's thumbnail, over black (looked at towards its
                // left end, where the words, which are dark, are not)
                const box = cellGrey(0, 0.22, 0.42), top = cellGrey(0, 0.5, 0.1)
                check("the first slide's thumbnail is drawn in the theme: the pale box across its middle, black above it", box > 120 && top < 60, box + " and " + top)
                testInput.grab("3-imported")
                click(centre(named("addLibraryButton")))
                return 400
            },
            () => {
                // From the clipboard, words with no labels and no details. (Set in the same step as it is read: on
                // the test desktop a clipboard set with no key or click just before it is let go of by the next turn
                // of the event loop, as the pasting tests have found too.)
                testInput.setClipboard(pasted)
                click(menuRow("Import Lyrics from Clipboard"))
                return 400
            },
            () => {
                const found = importLyricsPanel.found
                check("the clipboard's words are read: a part with no label, then the chorus, and no details", importLyricsPanel.visible && importLyricsPanel.source === "the clipboard"
                      && found.title === "Evening Song" && parts(found) === ":2 Chorus:2" && found.author === "" && found.songNumber === "", JSON.stringify(found))
                check("the theme, the library and the playlist are as they were for the last song, and the lines two again", importLyricsPanel.themeName === "White Box \u203a Two Lines"
                      && importLyricsPanel.library === kept.hymns && importLyricsPanel.playlist === kept.playlist && importLyricsPanel.lines === 2
                      && named("lyricsLibraryBox").currentText === "Hymns" && named("lyricsPlaylistBox").currentText === "Sunday",
                      importLyricsPanel.themeName + " " + named("lyricsLibraryBox").currentText + " " + named("lyricsPlaylistBox").currentText)
                choose(named("lyricsPlaylistBox"), "None")
                check("None takes the playlist off", importLyricsPanel.playlist === "", importLyricsPanel.playlist)
                // Return imports, as the panel has the keyboard
                testInput.key(Qt.Key_Return)
                return 900
            },
            () => {
                check("the second song is made in Hymns and opened: a group with no name, then the chorus, each slide dressed in the theme", document !== null
                      && document.name === "Evening Song" && document.path === kept.hymns + "/Evening Song.pro" && document.slides.length === 2
                      && document.slides.map(s => s.group).join("|") === "|Chorus" && document.slides.every(s => s.elements.length === 1 && s.elements[0].name === "Text"),
                      (document ? document.path + " " + document.slides.map(s => s.group).join("|") : "none"))
                check("with no details, having none, but its title, and the copyright display off", document.ccli.author === "" && document.ccli.songNumber === 0
                      && document.ccli.publisher === "" && document.ccli.title === "Evening Song" && document.ccli.display === false, JSON.stringify(document.ccli))
                check("and the playlist still has the one row", catalog.playlistItems(kept.playlist).length === 1, catalog.playlistItems(kept.playlist).length)
                check("the log says where each came from", /A song came in from the file Morning Song-lyrics.txt as "Morning Song" in the library "Hymns", 3 lines to a slide, dressed in "Two Lines" of the theme "White Box", and added to the playlist "Sunday"/.test(testInput.readText(Log.path))
                      && /A song came in from the clipboard as "Evening Song" in the library "Hymns", 2 lines to a slide, dressed in "Two Lines" of the theme "White Box"\n/.test(testInput.readText(Log.path)),
                      testInput.readText(Log.path).split("\n").filter(l => l.includes("came in")).join(" | "))
                check("the playlist is remembered as none now", settings.value("workspaces/Act/importPlaylist") === "", settings.value("workspaces/Act/importPlaylist"))
                // Nothing to import: an empty clipboard, and a file that is not there
                testInput.setClipboard("  \n ")
                importLyricsFromClipboard()
                check("an empty clipboard is said so, and no panel", notice === "There are no words on the clipboard" && !importLyricsPanel.visible, notice)
                importLyricsPanel.showFile("@WORKSPACES@/Act/not-here.txt")
                check("a file that cannot be read is said so", notice.startsWith("Cannot read not-here.txt") && !importLyricsPanel.visible, notice)
                importLyricsPanel.show("A Title\n\nCCLI Song #1\n", "a test")
                check("and words with no lines of a song in them", notice === "A Title has no words in it" && !importLyricsPanel.visible, notice)
                kept.count = documents.length
                importLyricsPanel.show("A Title\n\nA line\n", "a test")
                check("the panel is up for words that will do", importLyricsPanel.visible)
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc puts the panel away and makes nothing", !importLyricsPanel.visible && documents.length === kept.count && !documents.some(d => d.name === "A Title"), documents.length)
                // (But for the three things the test asked to be told of, which the log has as problems shown)
                const told = /Shown to the user: (There are no words on the clipboard|Cannot read not-here.txt|A Title has no words in it)/
                const lines = testInput.readText(Log.path).split("\n").filter(l => !l.includes("qt.qpa.theme") && !told.test(l))
                check("nothing else went wrong on the way", !/PROBLEM|WARNING|ERROR/.test(lines.join("\n")), lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
