import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import SimplePresenterApp

// Settings → Inputs: the workspace's video inputs (src/videoinputs.h), as ProPresenter's
// Video Inputs page has them. A row for each, with its thumbnail, its name, and which of
// this computer's devices it is; a click on it opens what it is set to: its preview (the
// eye switches it off, as ProPresenter's does), its Name, its Device, the Mode it is
// captured in, its thumbnail (taken from the input, or a picture of the operator's), and
// its Sound, which is the device's own, another of the computer's audio inputs, or none,
// and how loud. + adds an input, last in the list.
//
// The list and the names are the workspace's, in ProPresenter's own file; the devices,
// the modes, the sound and the thumbnails are this computer's, and are kept in the app's
// settings (see VideoInputs). Every change is kept at once.
//
// The preview here holds the input while it is shown, which is what has it captured
// (see VideoInputView); going back to the list, or leaving the settings, lets it go.
Item {
    id: page

    // Shows a menu (see PopupMenu) by an item
    property var showMenu: (rows, item, x, y) => {}
    // The input being looked at, by id, "" for the list; and whether its preview is on
    property string editing: ""
    property bool previewOn: true

    // (From the list itself, so that it follows every change to the input)
    readonly property var input: editing === "" ? null : VideoInputs.inputs.find(i => i.id === editing) ?? null
    readonly property var devices: editing === "" ? [] : VideoInputs.devices
    readonly property var device: input === null ? null : devices.find(d => d.id === input.device) ?? null
    readonly property int deviceAt: input === null ? -1 : devices.findIndex(d => d.id === input.device)

    function summary(input) {
        if (input.problem !== "")
            return input.problem
        if (input.device === "")
            return "No device on this computer" + (input.proDevice !== "" ? " (in ProPresenter, " + input.proDevice + ")" : "")
        if (input.missing)
            return input.deviceName + ", which is not on this computer now"
        return input.deviceName + (input.modeLabel !== "" ? ", " + input.modeLabel : "")
    }

    // ---- The list of inputs
    Flickable {
        anchors.fill: parent
        visible: page.editing === ""
        contentHeight: listColumn.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: listColumn

            width: parent.width - 16
            spacing: 8

            Repeater {
                model: VideoInputs.inputs

                delegate: Rectangle {
                    id: row

                    required property var modelData

                    objectName: "inputRow:" + modelData.name
                    width: listColumn.width
                    height: 64
                    radius: 8
                    color: rowMouse.containsMouse ? "#383b41" : "#313338"

                    MouseArea {
                        id: rowMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: page.editing = row.modelData.id
                    }

                    // Its thumbnail, or its colour
                    Rectangle {
                        id: picture

                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 80
                        height: 45
                        radius: 4
                        color: row.modelData.color !== "" ? row.modelData.color : "#202226"
                        clip: true

                        Image {
                            anchors.fill: parent
                            visible: row.modelData.thumbnail !== ""
                            source: row.modelData.thumbnail
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                        }
                    }

                    Column {
                        anchors.left: picture.right
                        anchors.leftMargin: 14
                        anchors.right: chevron.left
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3

                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            color: "#e6e6e6"
                            font.pixelSize: 14
                            font.bold: true
                            text: row.modelData.name
                        }

                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            color: row.modelData.problem !== "" || row.modelData.missing ? "#ff8a80" : "#9a9da3"
                            font.pixelSize: 12
                            text: page.summary(row.modelData)
                        }
                    }

                    Text {
                        id: chevron

                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#9a9da3"
                        font.pixelSize: 18
                        text: "›"
                    }
                }
            }

            Text {
                width: listColumn.width
                visible: VideoInputs.inputs.length === 0
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "No video inputs yet. A video input is live video coming into this computer, from a camera or a capture "
                      + "card, which the show can put on the screens: from the media bin's Video Inputs, from a slide or a "
                      + "macro, or as the fill of a shape."
            }

            AppButton {
                objectName: "inputAdd"
                height: 30
                font.pixelSize: 14
                text: "+  Add Video Input"
                onClicked: {
                    const made = VideoInputs.add("")
                    if (made.error === "")
                        page.editing = made.id
                }
            }
        }
    }

    // ---- One input
    Flickable {
        anchors.fill: parent
        visible: page.input !== null
        contentHeight: inputColumn.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: inputColumn

            width: parent.width - 16
            spacing: 12

            AppButton {
                objectName: "inputBack"
                height: 28
                font.pixelSize: 13
                text: "‹  Video Inputs"
                onClicked: page.editing = ""
            }

            // The preview, and the eye that switches it off
            Row {
                spacing: 10

                Rectangle {
                    objectName: "inputPreview"
                    width: 320
                    height: 180
                    color: "black"
                    border.width: 1
                    border.color: "#3a3c42"

                    Loader {
                        anchors.fill: parent
                        anchors.margins: 1
                        active: page.input !== null && page.previewOn

                        sourceComponent: VideoInputView {
                            inputId: page.editing
                            interval: 66
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 30
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        visible: page.input !== null && (!page.previewOn || !page.input.capturing)
                        color: "#8d9097"
                        font.pixelSize: 12
                        text: !page.previewOn ? "The preview is off" : page.input && page.input.device === "" ? "Choose a device" : "No picture yet"
                    }
                }

                IconButton {
                    objectName: "inputPreviewShown"
                    anchors.top: parent.top
                    width: 34
                    height: 30
                    kind: "eye"
                    on: page.previewOn
                    onClicked: page.previewOn = !page.previewOn
                }
            }

            component Line: Item {
                property alias caption: label.text
                default property alias controls: row.data

                width: inputColumn.width
                height: 34

                Text {
                    id: label

                    width: 110
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 13
                }

                Row {
                    id: row

                    x: 120
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                }
            }

            Line {
                caption: "Name"

                AppTextField {
                    objectName: "inputName"
                    width: Math.min(360, inputColumn.width - 130)
                    height: 30
                    font.pixelSize: 14
                    text: page.input ? page.input.name : ""
                    // (And the name it has afterwards, which is the one given unless it
                    // could not be, follows the input again)
                    onEditingFinished: {
                        if (page.input && text.trim() !== "" && text.trim() !== page.input.name)
                            VideoInputs.rename(page.editing, text.trim())
                        text = Qt.binding(() => page.input ? page.input.name : "")
                    }
                }
            }

            Line {
                caption: "Device"

                AppComboBox {
                    objectName: "inputDevice"
                    width: Math.min(360, inputColumn.width - 130)
                    model: ["None"].concat(page.devices.map(d => d.name))
                            .concat(page.input && page.input.device !== "" && page.device === null ? [page.input.deviceName + " (not here)"] : [])
                    currentIndex: page.input === null || page.input.device === "" ? 0
                                : page.deviceAt >= 0 ? page.deviceAt + 1 : page.devices.length + 1
                    onActivated: (index) => {
                        if (index <= page.devices.length)
                            VideoInputs.setDevice(page.editing, index === 0 ? "" : page.devices[index - 1].id)
                    }
                }
            }

            Line {
                caption: "Mode"
                visible: page.device !== null

                AppComboBox {
                    objectName: "inputMode"
                    width: Math.min(360, inputColumn.width - 130)
                    model: page.device === null ? [] : ["Automatic" + (page.input.mode === "" && page.input.modeLabel !== "" ? " (" + page.input.modeLabel + ")" : "")]
                                                       .concat(page.device.modes.map(m => m.label))
                    currentIndex: page.device === null || page.input.mode === "" ? 0 : Math.max(0, page.device.modes.findIndex(m => m.key === page.input.mode) + 1)
                    onActivated: (index) => VideoInputs.setMode(page.editing, index === 0 ? "" : page.device.modes[index - 1].key)
                }
            }

            Line {
                caption: "Thumbnail"
                height: 52

                Rectangle {
                    width: 80
                    height: 45
                    radius: 4
                    color: page.input && page.input.color !== "" ? page.input.color : "#202226"
                    clip: true

                    Image {
                        anchors.fill: parent
                        visible: page.input !== null && page.input.thumbnail !== ""
                        source: page.input ? page.input.thumbnail : ""
                        fillMode: Image.PreserveAspectCrop
                        cache: false
                    }
                }

                AppButton {
                    objectName: "inputThumbnailTake"
                    anchors.verticalCenter: parent.verticalCenter
                    height: 28
                    font.pixelSize: 13
                    enabled: page.input !== null && page.input.device !== ""
                    text: "Take from the Input"
                    onClicked: VideoInputs.takeThumbnail(page.editing)
                }

                AppButton {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 28
                    font.pixelSize: 13
                    text: "Choose a Picture…"
                    onClicked: pictureDialog.open()
                }

                AppButton {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 28
                    font.pixelSize: 13
                    visible: page.input !== null && page.input.thumbnail !== ""
                    text: "None"
                    onClicked: VideoInputs.clearThumbnail(page.editing)
                }
            }

            Rectangle {
                width: inputColumn.width
                height: 1
                color: "#3a3c42"
            }

            Line {
                caption: "Sound"

                AppComboBox {
                    id: sources

                    readonly property var choices: [{ id: "auto", name: "Automatic: the device's own" }, { id: "none", name: "No Audio Source" }]
                                                    .concat(page.input === null ? [] : VideoInputs.audioSources)

                    objectName: "inputAudio"
                    width: Math.min(360, inputColumn.width - 130)
                    model: choices.map(c => c.name)
                    currentIndex: page.input === null ? 0 : Math.max(0, choices.findIndex(c => c.id === page.input.audio))
                    onActivated: (index) => VideoInputs.setAudio(page.editing, choices[index].id)
                }
            }

            Line {
                caption: "Volume"
                visible: page.input !== null && page.input.audio !== "none"

                AppSlider {
                    objectName: "inputVolume"
                    width: Math.min(240, inputColumn.width - 200)
                    from: 0
                    to: 1
                    value: page.input ? page.input.volume : 1
                    onMoved: VideoInputs.setVolume(page.editing, value)
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: page.input ? Math.round(page.input.volume * 100) + "%" : ""
                }
            }

            Text {
                width: inputColumn.width
                wrapMode: Text.Wrap
                color: "#8d9097"
                font.pixelSize: 12
                text: "The sound plays while the input is on an audience screen, where the app's audio plays. Automatic is the "
                      + "sound of the device itself, where it has any. ProPresenter's Delay, channel levels and routing are not "
                      + "here yet."
            }

            AppButton {
                objectName: "inputRemove"
                height: 28
                font.pixelSize: 13
                text: "Remove"
                onClicked: page.showMenu([
                    { note: "“" + (page.input ? page.input.name : "") + "” will be removed from the workspace's video inputs. Slides "
                            + "and shapes that show it, by its place in the list, will show the one that takes its place." },
                    { label: "Remove", danger: true, run: () => {
                        const id = page.editing
                        page.editing = ""
                        VideoInputs.remove(id)
                    } }], this, 0, height)
            }
        }
    }

    FileDialog {
        id: pictureDialog

        title: "Choose a Thumbnail"
        nameFilters: ["Pictures (*.png *.jpg *.jpeg *.bmp *.gif *.webp)", "All files (*)"]
        onAccepted: VideoInputs.setThumbnail(page.editing, selectedFile)
    }
}
