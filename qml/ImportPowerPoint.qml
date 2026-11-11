import QtQuick
import SimplePresenterApp

// Import PowerPoint: the panel put up once a .pptx has been chosen (the Import
// PowerPoint… row of the + beside Libraries, see showLibraryMenu in Main.qml), or
// dropped from another application on a library or a playlist (Main's dropPowerPoint),
// over the operator window. Chosen, it says what the file holds, has the one option
// (leaving the slides' backgrounds out, kept for the next time), the library the
// presentation goes into and the playlist it is added to (both as they were the last
// time something came in to this workspace, as the lyrics import has them), and Import;
// the import runs on a thread of its own (PowerPointImport) with how far it has got and
// a Stop, and at the end the panel says what was made and what could not be carried
// over. Dropped, the files are not asked about: they come in at once, into the library or
// the playlist they were dropped on, one after another when there are several, and the
// panel shows how far it has got and then what came of each. Cancel, Esc or a click
// outside closes it, except while it is importing.
//
// ProPresenter has File > Import > PowerPoint as Presentation (since 21) and a switch for
// the backgrounds (22), and takes a PowerPoint file dropped on a playlist or a library;
// what else its own panel asks, and whether a drop asks anything, has not been looked at.
Rectangle {
    id: panel

    required property var win
    // The file being looked at or imported, "" while the panel is shut
    property string file: ""
    // What it holds (PowerPointImport.describe)
    property var found: ({})
    // The files still to come after it, of a drop of several
    property var queue: []
    // How many files were given, for "2 of 3"
    property int given: 0
    // The library's path, and the playlist's id or "" for none
    property string library: ""
    property string playlist: ""
    // "choose", "importing" or "done"
    property string stage: "choose"
    // What came of the file last imported (PowerPointImport.finished), and of each file
    // done, in their order, as [{ file, made }]
    property var result: null
    property var results: []
    readonly property var libraries: win.catalog.libraries
    // (The playlists alone: a folder holds no rows)
    readonly property var playlists: win.catalog.playlists.filter(node => !node.folder)
    readonly property string fileName: nameOf(file)
    readonly property string libraryName: (libraries.find(l => l.path === library) ?? { name: "" }).name
    // The deck's shape, as the panel says it
    readonly property string shape: {
        const w = Number(found.width ?? 1920), h = Number(found.height ?? 1080)
        if (h <= 0)
            return ""
        const ratio = w / h
        if (Math.abs(ratio - 16 / 9) < 0.02)
            return "16:9"
        if (Math.abs(ratio - 4 / 3) < 0.02)
            return "4:3, to be fitted into the slide with the sides left black"
        if (Math.abs(ratio - 16 / 10) < 0.02)
            return "16:10, to be fitted into the slide"
        return "of another shape, to be fitted into the slide"
    }
    // What came of it all, in words: for one file what was made and where, or why not;
    // for several, a line for each, and how many of them were made
    readonly property string outcome: {
        if (results.length === 0)
            return ""
        const where = " in the library “" + libraryName + "”" + (playlist !== "" ? ", and added to the playlist" : "") + "."
        const madeText = made => made.slides + (made.slides === 1 ? " slide" : " slides") + " made, as “" + made.name + "”"
        if (results.length === 1)
            return results[0].made.ok ? madeText(results[0].made) + where : results[0].made.error
        const lines = results.map(r => "“" + nameOf(r.file) + "”: " + (r.made.ok ? madeText(r.made) : r.made.error))
        return lines.join("\n") + "\n" + results.filter(r => r.made.ok).length + " of " + results.length + " made" + where
    }
    // What could not be carried over, of every file, with the file's name first when
    // there were several
    readonly property var notes: {
        let all = []
        for (const r of results) {
            const own = r.made.notes ?? []
            all = all.concat(results.length === 1 ? own
                             : own.map(note => ({ message: "“" + nameOf(r.file) + "”: " + note.message, slides: note.slides })))
        }
        return all
    }

    signal closed

    function nameOf(path) {
        return path.substring(path.lastIndexOf("/") + 1)
    }

    // Looks at a file (PowerPointImport.describe) and makes it the panel's: "" when it
    // could be, else why not
    function lookAt(path) {
        const described = PowerPointImport.describe(path)
        if (described.error !== "")
            return described.error
        found = described
        file = path
        return ""
    }

    // The library and the playlist as they were the last time something came in; the
    // library open, or the first, if none has
    function lastLibrary() {
        const saved = win.savedSelection("importLibrary")
        return libraries.some(l => l.path === saved) ? saved
             : libraries.some(l => l.path === win.libraryPath) ? win.libraryPath
             : libraries.length > 0 ? libraries[0].path : ""
    }

    function lastPlaylist() {
        const saved = win.savedSelection("importPlaylist")
        return playlists.some(p => p.path === saved) ? saved : ""
    }

    // Opens the panel for a file chosen in the file dialog: ready to import, the library
    // and the playlist as they were last time. False, and the reason in a notice, if it
    // is not a PowerPoint file.
    function showFile(path) {
        return showFiles([path], "", "", false)
    }

    // Opens the panel for `paths`, into `intoLibrary` and `intoPlaylist` where given, and
    // else as they were last time (a drop's playlist is the one it was dropped on, or
    // none). `atOnce` starts the import without asking, as a drop does, and goes on to the
    // next file as each is done, a file that cannot be read counted as not made. False
    // if nothing could be started: no file of them is a PowerPoint file, or an import is
    // going on; a notice says which.
    function showFiles(paths, intoLibrary, intoPlaylist, atOnce) {
        if (PowerPointImport.busy) {
            win.report("A PowerPoint file is still coming in: the next can be dropped when it is done")
            return false
        }
        library = libraries.some(l => l.path === intoLibrary) ? intoLibrary : lastLibrary()
        playlist = playlists.some(p => p.path === intoPlaylist) ? intoPlaylist : atOnce ? "" : lastPlaylist()
        results = []
        result = null
        given = paths.length
        const rest = paths.slice()
        let started = false
        while (rest.length > 0 && !started) {
            const next = rest.shift()
            const error = lookAt(next)
            if (error === "")
                started = true
            else if (atOnce)
                results = results.concat([{ file: next, made: { ok: false, error: error } }])
            else {
                win.report(error)
                return false
            }
        }
        if (!started) {
            win.report(results[0].made.error)
            results = []
            return false
        }
        queue = rest
        stage = "choose"
        forceActiveFocus()
        if (atOnce)
            importIt()
        return true
    }

    function close() {
        if (stage === "importing")
            return
        file = ""
        queue = []
        closed()
    }

    function importIt() {
        if (library === "" || stage !== "choose")
            return
        if (!PowerPointImport.start(file, win.catalog.workspacePath, library))
            return
        stage = "importing"
    }

    // The next file of a drop of several, if one is left that can be read; else done
    function goOn() {
        while (queue.length > 0) {
            const next = queue[0]
            queue = queue.slice(1)
            const error = lookAt(next)
            if (error !== "") {
                results = results.concat([{ file: next, made: { ok: false, error: error } }])
                continue
            }
            stage = "choose"
            importIt()
            if (stage === "importing")
                return
            results = results.concat([{ file: next, made: { ok: false, error: "The import could not be started" } }])
        }
        stage = "done"
    }

    visible: file !== ""
    color: "#99000000"
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape)
            close()
        else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && stage === "choose")
            importIt()
        else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && stage === "done")
            close()
        event.accepted = true
    }

    Connections {
        target: PowerPointImport

        function onFinished(made) {
            if (panel.file === "")
                return
            panel.result = made
            panel.results = panel.results.concat([{ file: panel.file, made: made }])
            if (made.ok)
                panel.win.importedPowerPoint(made, panel.library, panel.playlist, panel.file)
            panel.goOn()
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    Rectangle {
        objectName: "importPowerPointPanel"
        anchors.centerIn: parent
        width: 500
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
                text: "Import PowerPoint"
            }

            // The file being looked at or imported: with its place among them for a drop
            // of several
            Text {
                objectName: "importPowerPointFound"
                width: parent.width
                wrapMode: Text.Wrap
                color: "#b4b7bd"
                font.pixelSize: 13
                text: "“" + panel.fileName + "”" + (panel.given > 1 ? " (" + Math.min(panel.results.length + 1, panel.given) + " of " + panel.given + ")" : "")
                      + ": " + (panel.found.slides ?? 0) + ((panel.found.slides ?? 0) === 1 ? " slide" : " slides")
                      + (panel.shape !== "" ? ", " + panel.shape : "") + ". Its words come in as text that can be edited, its pictures as pictures, "
                      + "and its slide backgrounds as the slides' background media."
            }

            AppCheck {
                objectName: "importPowerPointBackgrounds"
                visible: panel.stage === "choose"
                checked: PowerPointImport.leaveOutBackgrounds
                text: "Leave out the slides' backgrounds"
                onToggled: (checked) => PowerPointImport.leaveOutBackgrounds = checked
            }

            Row {
                visible: panel.stage === "choose"
                spacing: 10

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "Library"
                }

                AppComboBox {
                    objectName: "importPowerPointLibraryBox"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 320
                    model: panel.libraries.map(library => library.name)
                    currentIndex: Math.max(0, panel.libraries.findIndex(library => library.path === panel.library))
                    onActivated: (index) => panel.library = panel.libraries[index].path
                }
            }

            Row {
                visible: panel.stage === "choose"
                spacing: 10

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "Add to playlist"
                }

                AppComboBox {
                    objectName: "importPowerPointPlaylistBox"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 320
                    // (Each playlist set in by how deep in the folders it is)
                    model: ["None"].concat(panel.playlists.map(node => "  ".repeat(node.depth) + node.name))
                    currentIndex: 1 + panel.playlists.findIndex(node => node.path === panel.playlist)
                    onActivated: (index) => panel.playlist = index === 0 ? "" : panel.playlists[index - 1].path
                }
            }

            // How far it has got
            Item {
                visible: panel.stage === "importing"
                width: parent.width
                height: 22

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 6
                    radius: 3
                    color: "#15161a"

                    Rectangle {
                        objectName: "importPowerPointProgress"
                        width: parent.width * PowerPointImport.progress
                        height: parent.height
                        radius: 3
                        color: "#ff8a1f"
                    }
                }
            }

            // What came of it
            Text {
                objectName: "importPowerPointResult"
                visible: panel.stage === "done"
                width: parent.width
                wrapMode: Text.Wrap
                color: panel.results.some(r => r.made.ok) ? "#f0f0f0" : "#ff8a80"
                font.pixelSize: 13
                text: panel.outcome
            }

            Flickable {
                visible: panel.stage === "done" && notesColumn.height > 0
                width: parent.width
                height: Math.min(notesColumn.height, 220)
                contentHeight: notesColumn.height
                clip: true

                Column {
                    id: notesColumn

                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: panel.notes

                        Text {
                            required property var modelData

                            objectName: "importPowerPointNote"
                            width: notesColumn.width
                            wrapMode: Text.Wrap
                            color: "#b4b7bd"
                            font.pixelSize: 12
                            text: "• " + modelData.message + (modelData.slides !== "" ? " (" + modelData.slides + ")" : "")
                        }
                    }
                }
            }

            Row {
                anchors.right: parent.right
                spacing: 10

                AppButton {
                    visible: panel.stage === "choose"
                    text: "Cancel"
                    onClicked: panel.close()
                }
                AppButton {
                    objectName: "importPowerPointButton"
                    visible: panel.stage === "choose"
                    text: "Import"
                    enabled: panel.library !== ""
                    onClicked: panel.importIt()
                }
                // (Stops the file being imported, and the rest of a drop with it)
                AppButton {
                    objectName: "importPowerPointStop"
                    visible: panel.stage === "importing"
                    text: "Stop"
                    onClicked: {
                        panel.queue = []
                        PowerPointImport.stop()
                    }
                }
                AppButton {
                    objectName: "importPowerPointDone"
                    visible: panel.stage === "done"
                    text: "Done"
                    onClicked: panel.close()
                }
            }
        }
    }
}
