import QtQuick
import SimplePresenterApp

// Import PowerPoint: the panel put up once a .pptx has been chosen (the Import
// PowerPoint… row of the + beside Libraries, see showLibraryMenu in Main.qml), over the
// operator window. It says what the file holds, has the one option (leaving the slides'
// backgrounds out, kept for the next time), the library the presentation goes into and
// the playlist it is added to (both as they were the last time something came in to this
// workspace, as the lyrics import has them), and Import; the import runs on a thread of
// its own (PowerPointImport) with how far it has got and a Stop, and at the end the panel
// says what was made and what could not be carried over. Cancel, Esc or a click outside
// closes it, except while it is importing.
//
// ProPresenter has File > Import > PowerPoint as Presentation (since 21) and a switch for
// the backgrounds (22); what else its own panel asks has not been looked at.
Rectangle {
    id: panel

    required property var win
    // The file, "" while the panel is shut
    property string file: ""
    // What it holds (PowerPointImport.describe)
    property var found: ({})
    // The library's path, and the playlist's id or "" for none
    property string library: ""
    property string playlist: ""
    // "choose", "importing" or "done"
    property string stage: "choose"
    property var result: null
    readonly property var libraries: win.catalog.libraries
    // (The playlists alone: a folder holds no rows)
    readonly property var playlists: win.catalog.playlists.filter(node => !node.folder)
    readonly property string fileName: file.substring(file.lastIndexOf("/") + 1)
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

    signal closed

    function showFile(path) {
        const described = PowerPointImport.describe(path)
        if (described.error !== "") {
            win.report(described.error)
            return false
        }
        found = described
        result = null
        stage = "choose"
        // Last time's choices, where they still are
        const savedLibrary = win.savedSelection("importLibrary")
        library = libraries.some(l => l.path === savedLibrary) ? savedLibrary
                : libraries.some(l => l.path === win.libraryPath) ? win.libraryPath
                : libraries.length > 0 ? libraries[0].path : ""
        const savedPlaylist = win.savedSelection("importPlaylist")
        playlist = playlists.some(p => p.path === savedPlaylist) ? savedPlaylist : ""
        file = path
        forceActiveFocus()
        return true
    }

    function close() {
        if (stage === "importing")
            return
        file = ""
        closed()
    }

    function importIt() {
        if (library === "" || stage !== "choose")
            return
        if (!PowerPointImport.start(file, win.catalog.workspacePath, library))
            return
        stage = "importing"
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
            panel.stage = "done"
            if (made.ok)
                panel.win.importedPowerPoint(made, panel.library, panel.playlist, panel.file)
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

            Text {
                objectName: "importPowerPointFound"
                width: parent.width
                wrapMode: Text.Wrap
                color: "#b4b7bd"
                font.pixelSize: 13
                text: "“" + panel.fileName + "”: " + (panel.found.slides ?? 0) + ((panel.found.slides ?? 0) === 1 ? " slide" : " slides")
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
                    model: ["None"].concat(panel.playlists.map(node => "  ".repeat(node.depth) + node.name))
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
                color: panel.result && panel.result.ok ? "#f0f0f0" : "#ff8a80"
                font.pixelSize: 13
                text: !panel.result ? "" : panel.result.ok
                      ? panel.result.slides + (panel.result.slides === 1 ? " slide" : " slides") + " made, as “" + panel.result.name + "” in the library “"
                        + (panel.libraries.find(l => l.path === panel.library) ?? { name: "" }).name + "”"
                        + (panel.playlist !== "" ? ", and added to the playlist" : "") + "."
                      : panel.result.error
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
                        model: panel.result ? panel.result.notes ?? [] : []

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
                AppButton {
                    objectName: "importPowerPointStop"
                    visible: panel.stage === "importing"
                    text: "Stop"
                    onClicked: PowerPointImport.stop()
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
