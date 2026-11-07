import QtQuick
import SimplePresenterApp

// The panel a song's words from SongSelect are imported from: what the text was found
// to hold, how many lines of words go on a slide, the theme to dress the slides in, the
// library the song goes into, the playlist it is added to, and Import. It is put up
// once the words are to hand, from a lyrics file SongSelect gave out or from the
// clipboard (see importLyricsFile and importLyricsFromClipboard in Main.qml), over
// the operator window; Cancel, Esc or a click outside leaves the library as it was.
//
// Two lines to a slide is what a new song starts at, being what most lyric slides are,
// every time. The theme, the library and the playlist start as they were the last time
// a song came in to this workspace (the app's own settings, see savedImport in
// Main.qml), the library as the one that is open until then, the theme and the
// playlist as none; so that a run of songs for a service comes in with three clicks
// each. After a song is in, its slides are ordinary slides and are changed in the
// editor like any others, and a theme can be put on them afterwards like on any other.
//
// The maintainer asked for these controls (2026-11-07). ProPresenter's own import of a
// SongSelect song, from its SongSelect window, has not been watched: whether it asks the
// same is not known.
Rectangle {
    id: panel

    required property var win
    // The words, "" while the panel is shut
    property string text: ""
    // Where they came from, for the panel and the log: "the file Worthy Of It All-lyrics.txt", "the clipboard"
    property string source: ""
    // What they hold (Catalog.describeLyrics)
    property var found: ({})
    property int lines: 2
    // The theme slide to dress the slides in: the theme's place and the slide's id, or
    // "" for none
    property string themePlace: ""
    property string themeSlideId: ""
    // The library's path, and the playlist's id or "" for none
    property string library: ""
    property string playlist: ""
    readonly property var libraries: win.catalog.libraries
    // (The playlists alone: a folder holds no rows)
    readonly property var playlists: win.catalog.playlists.filter(node => !node.folder)
    readonly property int slideCount: (found.sections ?? []).reduce((sum, part) => sum + Math.ceil(part.lines / Math.max(1, lines)), 0)
    readonly property var themed: themePlace !== "" ? Themes.slide(themePlace, themeSlideId) : null
    readonly property string themeName: {
        const theme = themePlace !== "" ? Themes.theme(themePlace) : null
        return theme && themed ? theme.name + " \u203a " + themed.name : "None"
    }

    signal closed

    // Words from the clipboard, or anywhere else
    function show(words, from) {
        return open(win.catalog.describeLyrics(words), words, from)
    }

    // A lyrics file
    function showFile(path) {
        const described = win.catalog.describeLyricsFile(path)
        return open(described, described.text ?? "", "the file " + path.substring(path.lastIndexOf("/") + 1))
    }

    function open(described, words, from) {
        if (described.error !== "") {
            win.report(described.error)
            return false
        }
        found = described
        lines = 2
        // Last time's choices, where they still are
        const place = win.savedImport("themePlace"), slideId = win.savedImport("themeSlide")
        chooseTheme(place !== "" && Themes.slide(place, slideId) ? place : "", slideId)
        const savedLibrary = win.savedSelection("importLibrary")
        library = libraries.some(l => l.path === savedLibrary) ? savedLibrary
                : libraries.some(l => l.path === win.libraryPath) ? win.libraryPath
                : libraries.length > 0 ? libraries[0].path : ""
        const savedPlaylist = win.savedSelection("importPlaylist")
        playlist = playlists.some(p => p.path === savedPlaylist) ? savedPlaylist : ""
        text = words
        source = from
        forceActiveFocus()
        return true
    }

    function chooseTheme(place, slideId) {
        themePlace = place
        themeSlideId = place === "" ? "" : slideId
    }

    function close() {
        text = ""
        closed()
    }

    function importIt() {
        if (library === "")
            return
        const words = text, from = source
        text = ""
        win.importLyrics(words, lines, themePlace, themeSlideId, library, playlist, from)
        closed()
    }

    visible: text !== ""
    color: "#99000000"
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape)
            close()
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
            importIt()
        event.accepted = true
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    Rectangle {
        objectName: "importLyricsPanel"
        anchors.centerIn: parent
        width: 480
        height: content.height + 36
        radius: 10
        color: "#2b2d31"
        border.width: 1
        border.color: "#45484e"

        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: content

            x: 20
            y: 18
            width: parent.width - 40
            spacing: 12

            Text {
                width: parent.width
                elide: Text.ElideRight
                color: "#f0f0f0"
                font.pixelSize: 17
                font.bold: true
                text: (panel.found.title ?? "") !== "" ? panel.found.title : "Lyrics"
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#b4b7bd"
                font.pixelSize: 13
                text: {
                    const parts = panel.found.sections ?? []
                    const named = parts.filter(part => part.name !== "").map(part => part.name)
                    return ["From " + panel.source,
                            (panel.found.author ?? "") !== "" ? panel.found.author : "",
                            parts.length + (parts.length === 1 ? " part" : " parts") + (named.length > 0 ? ": " + named.join(", ") : ""),
                            (panel.found.songNumber ?? "") !== "" ? "CCLI song " + panel.found.songNumber : ""].filter(line => line !== "").join("\n")
                }
            }

            Row {
                spacing: 10

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "Lines on a slide"
                }

                NumberField {
                    objectName: "lyricsLinesField"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    value: panel.lines
                    from: 1
                    to: 12
                    onEdited: (value) => panel.lines = value
                    onFinished: panel.forceActiveFocus()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: "which makes " + panel.slideCount + (panel.slideCount === 1 ? " slide" : " slides")
                }
            }

            Row {
                spacing: 10

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "Theme"
                }

                // The themes' menu, as a slide's Theme has it, with None first
                AppButton {
                    id: themeButton

                    objectName: "lyricsThemeButton"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 300
                    text: panel.themeName
                    onClicked: panel.win.pickThemeWith(themeButton, panel.themePlace, (place, slideId) => panel.chooseTheme(place, slideId))
                }
            }

            Row {
                spacing: 10

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "Library"
                }

                AppComboBox {
                    objectName: "lyricsLibraryBox"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 300
                    model: panel.libraries.map(library => library.name)
                    currentIndex: Math.max(0, panel.libraries.findIndex(library => library.path === panel.library))
                    onActivated: (index) => panel.library = panel.libraries[index].path
                }
            }

            Row {
                spacing: 10

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "Add to playlist"
                }

                AppComboBox {
                    objectName: "lyricsPlaylistBox"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 300
                    // (Each playlist set in by how deep in the folders it is)
                    model: ["None"].concat(panel.playlists.map(node => "\u2007\u2007".repeat(node.depth) + node.name))
                    currentIndex: 1 + panel.playlists.findIndex(node => node.path === panel.playlist)
                    onActivated: (index) => panel.playlist = index === 0 ? "" : panel.playlists[index - 1].path
                }
            }

            Row {
                anchors.right: parent.right
                spacing: 10

                AppButton {
                    text: "Cancel"
                    onClicked: panel.close()
                }

                AppButton {
                    objectName: "importLyricsButton"
                    text: "Import"
                    enabled: panel.library !== ""
                    onClicked: panel.importIt()
                }
            }
        }
    }
}
