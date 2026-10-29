import QtCore
import QtQuick
import QtQuick.Dialogs
import SimplePresenterApp

// Export to PowerPoint: the panel put up by a right click on a presentation (see
// showPresentationMenu in Main.qml), over the operator window. It shows the options, which
// are ProToPPTX's (the maintainer's Mac app this export is a port of: see pptxexport.h)
// and are kept for the next time; Export… asks where the file is to go, and then the
// export runs (on a thread of its own, PptxExport) with how far it has got and a Stop;
// at the end it says where the file is and what could not be carried over, the report's
// notes, with Show in Folder. Cancel, Esc or a click outside closes it, except while it
// is exporting.
//
// This is the app's own, not ProPresenter's, which exports no PowerPoint; the words are
// ProToPPTX's, where it has them.
Rectangle {
    id: panel

    required property var win
    property bool open: false
    // The presentation's row: its file, name and arrangement
    property var entry: null
    // "choose", "exporting" or "done"
    property string stage: "choose"
    property var options: ({})
    property var result: null

    signal closed

    function show(row) {
        entry = row
        options = PptxExport.options()
        stage = "choose"
        result = null
        open = true
        forceActiveFocus()
    }

    function close() {
        if (stage === "exporting")
            return
        open = false
        closed()
    }

    function set(name, value) {
        PptxExport.setOption(name, value)
        options = PptxExport.options()
    }

    // Into this file: what Export… does once a file is chosen, and what the tests call
    function exportTo(path) {
        if (!entry || !PptxExport.start(entry.file, win.catalog.workspacePath, options.followArrangement ? (entry.arrangement ?? "") : "", path))
            return
        stage = "exporting"
    }

    function chooseFile() {
        const suggested = PptxExport.suggestedFile(entry.name)
        saveDialog.currentFolder = "file://" + suggested.substring(0, suggested.lastIndexOf("/"))
        saveDialog.selectedFile = "file://" + suggested
        saveDialog.open()
    }

    function choiceMenu(item, name, rows) {
        win.showMenu(rows.map(row => ({ label: row[1], current: options[name] === row[0], run: () => set(name, row[0]) })), item)
    }

    visible: open
    color: "#99000000"
    Keys.onEscapePressed: close()

    Connections {
        target: PptxExport

        function onFinished(made) {
            if (!panel.open)
                return
            panel.result = made
            panel.stage = "done"
        }
    }

    FileDialog {
        id: saveDialog

        title: "Export to PowerPoint"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PowerPoint (*.pptx)"]
        defaultSuffix: "pptx"
        onAccepted: panel.exportTo(selectedFile.toString())
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    Rectangle {
        objectName: "exportPowerPointPanel"
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
            spacing: 10

            Text {
                width: parent.width
                color: "#f0f0f0"
                font.pixelSize: 17
                font.bold: true
                text: "Export to PowerPoint"
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#b4b7bd"
                font.pixelSize: 13
                text: !panel.entry ? "" : "“" + panel.entry.name + "”, slide for slide as it is shown, its words as text that can be edited"
                      + (panel.options.followArrangement && (panel.entry.arrangement ?? "") !== "" ? ", in the order of its arrangement “" + panel.entry.arrangement + "”." : ".")
            }

            // The options
            Column {
                visible: panel.stage === "choose"
                width: parent.width
                spacing: 4

                AppCheck {
                    objectName: "pptxCarryBackground"
                    checked: panel.options.carryBackgroundForward === true
                    text: "Keep background media on following slides"
                    onToggled: (checked) => panel.set("carryBackgroundForward", checked)
                }
                AppCheck {
                    objectName: "pptxSkipDisabled"
                    checked: panel.options.skipDisabledSlides === true
                    text: "Leave out slides turned off"
                    onToggled: (checked) => panel.set("skipDisabledSlides", checked)
                }
                AppCheck {
                    objectName: "pptxNotes"
                    checked: panel.options.includeNotes === true
                    text: "Put slide notes into speaker notes"
                    onToggled: (checked) => panel.set("includeNotes", checked)
                }
                AppCheck {
                    objectName: "pptxSections"
                    checked: panel.options.groupsAsSections === true
                    text: "Make groups into PowerPoint sections"
                    onToggled: (checked) => panel.set("groupsAsSections", checked)
                }
                AppCheck {
                    objectName: "pptxArrangement"
                    checked: panel.options.followArrangement === true
                    text: "Follow the chosen arrangement"
                    onToggled: (checked) => panel.set("followArrangement", checked)
                }

                Row {
                    spacing: 8
                    topPadding: 6

                    Text {
                        width: 150
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#b4b7bd"
                        font.pixelSize: 13
                        text: "Background pictures"
                    }
                    AppButton {
                        id: placementButton

                        objectName: "pptxBackgroundPlacement"
                        font.pixelSize: 13
                        text: (panel.options.backgroundPlacement === "pictureAtBack" ? "As a picture at the back" : "As the slide's background") + "  ▾"
                        onClicked: panel.choiceMenu(placementButton, "backgroundPlacement",
                                                    [["slideBackground", "As the slide's background"], ["pictureAtBack", "As a picture at the back"]])
                    }
                }

                Row {
                    spacing: 8

                    Text {
                        width: 150
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#b4b7bd"
                        font.pixelSize: 13
                        text: "Videos"
                    }
                    AppButton {
                        id: videosButton

                        objectName: "pptxVideos"
                        font.pixelSize: 13
                        text: (panel.options.videos === "stillOnly" ? "A still only" : panel.options.videos === "leaveOut" ? "Leave out" : "Put the video in") + "  ▾"
                        onClicked: panel.choiceMenu(videosButton, "videos",
                                                    [["embed", "Put the video in"], ["stillOnly", "A still only"], ["leaveOut", "Leave out"]])
                    }
                }
            }

            // How far it has got
            Item {
                visible: panel.stage === "exporting"
                width: parent.width
                height: 22

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 6
                    radius: 3
                    color: "#15161a"

                    Rectangle {
                        objectName: "pptxProgress"
                        width: parent.width * PptxExport.progress
                        height: parent.height
                        radius: 3
                        color: "#ff8a1f"
                    }
                }
            }

            // What came of it
            Text {
                objectName: "pptxResult"
                visible: panel.stage === "done"
                width: parent.width
                wrapMode: Text.Wrap
                color: panel.result && panel.result.ok ? "#f0f0f0" : "#ff8a80"
                font.pixelSize: 13
                text: !panel.result ? "" : panel.result.ok ? panel.result.slides + (panel.result.slides === 1 ? " slide" : " slides") + " written to " + panel.result.path
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

                            objectName: "pptxNote"
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
                    objectName: "pptxExport"
                    visible: panel.stage === "choose"
                    text: "Export…"
                    onClicked: panel.chooseFile()
                }
                AppButton {
                    objectName: "pptxStop"
                    visible: panel.stage === "exporting"
                    text: "Stop"
                    onClicked: PptxExport.stop()
                }
                AppButton {
                    visible: panel.stage === "done" && panel.result && panel.result.ok
                    text: "Show in Folder"
                    onClicked: Qt.openUrlExternally("file://" + panel.result.path.substring(0, panel.result.path.lastIndexOf("/")))
                }
                AppButton {
                    objectName: "pptxDone"
                    visible: panel.stage === "done"
                    text: "Done"
                    onClicked: panel.close()
                }
            }
        }
    }
}
