import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import SimplePresenterApp

// The settings screen, laid over the operator window: sections on the left, the selected
// section's settings on the right. Edits are reported as they are made; nothing here
// stores anything. The last section sets nothing: it says what the app is (an
// experiment, and not a product, which whoever is using it should not have to find out
// from anywhere else), which version this is, and where the log is.
Rectangle {
    id: screen

    // [{ name, color }]: the groups a slide's group name is matched against for its colour.
    property var groups: []

    // Whether to run through X11 on a Wayland desktop from the next launch.
    property bool useX11: false
    // Who decodes video, "auto", "processor" or "hardware": see the Video section
    property string videoDecoding: "auto"
    // The colour of a chord's name on the chord sheet, and of a chord in the ChordPro
    // text, each as "#rrggbb"; "" for the one it comes with. See the Chords section.
    property string chordSheetColor: ""
    // The device sound is played through, by its id as the system has it; "" for
    // wherever the system sends sound
    property string audioDevice: ""
    property string chordTextColor: ""
    // And of the chord sheet's highlighted line, the same way; and whether the sheet
    // draws sharps and flats as ♯ and ♭
    property string chordLineColor: ""
    // The person's own api.bible key, "" for the one the app was built with, if any
    property string apiBibleKey: ""
    property bool chordSigns: true
    readonly property color usualSheetChord: "#ff8a1f"
    readonly property color usualTextChord: "#ffb45e"
    readonly property color usualSheetLine: "#ff8a1f"

    // The hotkeys the workspace has for groups that are not in the list here, in words
    property string otherHotkeys: ""
    // How solid the icons on the slides are: see Main.qml
    property real actionIconOpacity: 0.8

    // Whether Simple View leaves the pane at the right where it is: see Main.qml
    property bool simpleKeepsSidePanel: false

    // What sends each screen that goes over NDI, by the screen's id: see ScreensSettings
    property var senders: ({})

    signal groupsEdited(var groups)
    signal screensFailed(string text)
    // The menu of where a screen can go is to be shown (see ScreensSettings)
    signal screenMenuWanted(var rows, var item, real x, real y)
    signal actionIconOpacityEdited(real opacity)
    signal useX11Edited(bool useX11)
    signal videoDecodingEdited(string mode)
    signal chordSheetColorEdited(string color)
    signal audioDeviceEdited(string id)
    signal chordTextColorEdited(string color)
    signal chordLineColorEdited(string color)
    signal chordSignsEdited(bool on)
    signal apiBibleKeyEdited(string key)
    signal simpleKeepsSidePanelEdited(bool keeps)
    // A label kept ready is to be made (index -1) or changed, in the panel a label is
    // made in; or taken out of the list
    signal labelEdited(int index)
    signal labelRemoved(int index)
    signal closed

    // One of the three ways video can be decoded (the Video section): a round mark, what
    // it is called, and what it does
    component DecodingChoice: Item {
        id: choice

        property string mode
        property string title
        property string detail
        readonly property bool chosen: screen.videoDecoding === mode

        objectName: "decodingChoice:" + mode
        width: parent ? parent.width : 0
        height: Math.max(22, words.height)

        Rectangle {
            y: 2
            width: 18
            height: 18
            radius: 9
            color: "#15161a"
            border.width: 1.5
            border.color: choice.chosen ? "#ff8a1f" : choiceMouse.containsMouse ? "#8b8f98" : "#5c5f66"

            Rectangle {
                anchors.centerIn: parent
                width: 10
                height: 10
                radius: 5
                visible: choice.chosen
                color: "#ff8a1f"
            }
        }

        Column {
            id: words

            x: 30
            width: parent.width - 30
            spacing: 2

            Text {
                color: "#e6e6e6"
                font.pixelSize: 15
                text: choice.title
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: choice.detail
            }
        }

        MouseArea {
            id: choiceMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: screen.videoDecodingEdited(choice.mode)
        }
    }

    // One editor's colour: its name, what it is the colour of where the name does not
    // say, the swatch to choose it from, the way back to the usual one, and at the right
    // what a chord (or a line) then looks like.
    component ChordColour: Item {
        id: colour

        property string title
        property string says
        // The colour chosen, "" for the usual one, and what that is
        property color usual
        property string chosen
        readonly property color shown: chosen !== "" ? chosen : usual
        default property alias sample: sampled.data

        signal edited(string color)

        width: parent.width
        height: Math.max(78, buttons.y + buttons.height)

        Text {
            id: colourTitle

            color: "#e6e6e6"
            font.pixelSize: 14
            font.bold: true
            text: colour.title
        }

        Text {
            id: colourSays

            anchors.top: colourTitle.bottom
            anchors.topMargin: 4
            width: parent.width - sampled.width - 40
            visible: colour.says !== ""
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 13
            text: colour.says
        }

        Row {
            id: buttons

            anchors.top: colourSays.visible ? colourSays.bottom : colourTitle.bottom
            anchors.topMargin: 12
            spacing: 10

            ColorButton {
                objectName: "chordColour:" + colour.title
                anchors.verticalCenter: parent.verticalCenter
                value: colour.shown
                hasAlpha: false
                onChanging: (value) => colour.edited(String(value))
                onPicked: (value) => colour.edited(String(value))
            }

            AppButton {
                objectName: "usualColour:" + colour.title
                anchors.verticalCenter: parent.verticalCenter
                height: 26
                font.pixelSize: 12
                enabled: colour.chosen !== ""
                text: "Usual Colour"
                onClicked: colour.edited("")
            }
        }

        Item {
            id: sampled

            objectName: "chordSample:" + colour.title
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 200
            height: 70
        }
    }

    // What the app is running on now: "wayland", "xcb" (X11), "windows", "cocoa", ...
    readonly property string platform: Qt.platform.pluginName
    readonly property var sections: Qt.platform.os === "linux"
        ? [{ name: "Groups", path: "groups" }, { name: "Labels", path: "labels" }, { name: "Slides", path: "slides" },
           { name: "Chords", path: "chords" }, { name: "Bibles", path: "bibles" }, { name: "Simple View", path: "simple" }, { name: "Screens", path: "screens" },
           { name: "Video", path: "video" }, { name: "Audio", path: "audio" }, { name: "Windows", path: "windows" }, { name: "About", path: "about" }]
        : [{ name: "Groups", path: "groups" }, { name: "Labels", path: "labels" }, { name: "Slides", path: "slides" },
           { name: "Chords", path: "chords" }, { name: "Bibles", path: "bibles" }, { name: "Simple View", path: "simple" }, { name: "Screens", path: "screens" },
           { name: "Video", path: "video" }, { name: "Audio", path: "audio" }, { name: "About", path: "about" }]
    property string section: "groups"
    readonly property var palette: [
        "#e53935", "#d81b60", "#8e24aa", "#5e35b1", "#3949ab", "#1e88e5",
        "#039be5", "#00acc1", "#00897b", "#43a047", "#7cb342", "#c0ca33",
        "#fdd835", "#ffb300", "#fb8c00", "#f4511e", "#6d4c41", "#757575"
    ]

    function edited(index, change) {
        groupsEdited(groups.map((group, i) => i === index ? Object.assign({}, group, change) : group))
    }

    // Gives a group its hotkey, "" for none. A key is one group's only.
    function keyEdited(index, key) {
        groupsEdited(groups.map((group, i) => i === index ? Object.assign({}, group, { key: key })
                                             : key !== "" && group.key === key ? Object.assign({}, group, { key: "" }) : group))
    }

    color: "#b0000000"

    // Swallow clicks and scrolling so nothing behind the screen reacts.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        objectName: "settingsSheet"
        anchors.centerIn: parent
        width: Math.min(760, parent.width - 60)
        height: Math.min(600, parent.height - 60)
        radius: 8
        color: "#1e1f22"
        border.width: 1
        border.color: "#45484e"

        Text {
            id: title

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 18
            color: "#e6e6e6"
            font.pixelSize: 18
            text: "Settings"
        }

        AppButton {
            id: done

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            text: "Done"
            onClicked: screen.closed()
        }

        SidebarList {
            id: sectionList

            anchors.left: parent.left
            anchors.top: done.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 1
            anchors.topMargin: 12
            width: 180
            model: screen.sections
            selectedPath: screen.section
            onPicked: (entry) => screen.section = entry.path
        }

        // Groups
        Item {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            visible: screen.section === "groups"

            Text {
                id: groupsHelp

                anchors.left: parent.left
                anchors.right: parent.right
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "Slides are framed in the colour of the group they belong to. A slide's group is "
                    + "matched by name, ignoring case and a trailing number, so “Verse” also colours "
                    + "“Verse 1” and “Verse 2” unless those have entries of their own.\n\n"
                    + "A group can have a hotkey, in the box after its name: a letter or a digit that, "
                    + "pressed while showing, goes to the first slide of that group in the presentation "
                    + "(where the group first comes up, if it comes up more than once). Click the box "
                    + "and press the key; Backspace takes it away. The hotkeys are kept with the workspace, "
                    + "in its list of groups, where ProPresenter keeps them."
                    + (screen.otherHotkeys !== "" ? "\n\nThe workspace's own groups have hotkeys besides: " + screen.otherHotkeys + "." : "")
            }

            ListView {
                id: groupList

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: groupsHelp.bottom
                anchors.bottom: addGroup.top
                anchors.topMargin: 12
                anchors.bottomMargin: 12
                clip: true
                spacing: 6
                boundsBehavior: Flickable.StopAtBounds
                model: screen.groups

                ScrollBar.vertical: ScrollBar {}

                KineticWheel {}

                delegate: Item {
                    id: row

                    required property var modelData
                    required property int index

                    width: ListView.view.width - 14
                    height: 34

                    Rectangle {
                        id: swatch

                        width: 34
                        height: 34
                        radius: 6
                        color: row.modelData.color
                        border.width: 1
                        border.color: "#45484e"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: colors.open()
                        }

                        Popup {
                            id: colors

                            y: swatch.height + 4
                            // Kept inside the window
                            margins: 6
                            padding: 10
                            focus: true

                            background: Rectangle {
                                radius: 8
                                color: "#2b2d31"
                                border.width: 1
                                border.color: "#45484e"
                            }

                            contentItem: Column {
                                spacing: 10

                                Grid {
                                    columns: 6
                                    spacing: 6

                                    Repeater {
                                        model: screen.palette

                                        delegate: Rectangle {
                                            required property string modelData

                                            width: 28
                                            height: 28
                                            radius: 5
                                            color: modelData
                                            border.width: Qt.colorEqual(modelData, row.modelData.color) ? 2 : 0
                                            border.color: "white"

                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: {
                                                    colors.close()
                                                    screen.edited(row.index, { color: parent.modelData })
                                                }
                                            }
                                        }
                                    }
                                }

                                // Any other colour, as #rrggbb
                                AppTextField {
                                    width: 6 * 28 + 5 * 6
                                    text: row.modelData.color
                                    onEditingFinished: {
                                        const value = text.trim()
                                        if (/^#[0-9a-fA-F]{6}$/.test(value)) {
                                            colors.close()
                                            screen.edited(row.index, { color: value.toLowerCase() })
                                        } else {
                                            text = row.modelData.color
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // The group's hotkey: click, then press the letter or digit it is to
                    // be. A key that another group has is taken from that group.
                    Rectangle {
                        id: keyBox

                        objectName: "groupKey"
                        readonly property string key: row.modelData.key ?? ""

                        anchors.right: remove.left
                        anchors.rightMargin: 8
                        width: 46
                        height: 34
                        radius: 6
                        color: activeFocus ? "#3a3c42" : "#23252b"
                        border.width: activeFocus ? 2 : 1
                        border.color: activeFocus ? "#ff8a1f" : "#45484e"
                        activeFocusOnTab: true
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
                                screen.keyEdited(row.index, "")
                            } else if (/^[a-z0-9]$/i.test(event.text) && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
                                screen.keyEdited(row.index, event.text.toUpperCase())
                            } else if (event.key !== Qt.Key_Escape) {
                                return
                            }
                            focus = false
                            event.accepted = true
                        }

                        Text {
                            anchors.centerIn: parent
                            color: keyBox.key !== "" ? "#e6e6e6" : "#6c6f75"
                            font.pixelSize: keyBox.key !== "" ? 15 : 12
                            font.bold: keyBox.key !== ""
                            text: keyBox.activeFocus ? "…" : keyBox.key !== "" ? keyBox.key : "key"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: keyBox.forceActiveFocus()
                        }
                    }

                    AppTextField {
                        anchors.left: swatch.right
                        anchors.right: keyBox.left
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        text: row.modelData.name
                        onEditingFinished: {
                            if (text.trim() === "")
                                text = row.modelData.name
                            else if (text.trim() !== row.modelData.name)
                                screen.edited(row.index, { name: text.trim() })
                        }
                    }

                    AppButton {
                        id: remove

                        anchors.right: parent.right
                        text: "Remove"
                        onClicked: screen.groupsEdited(screen.groups.filter((group, i) => i !== row.index))
                    }
                }
            }

            AppButton {
                id: addGroup

                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: "Add Group"
                onClicked: {
                    screen.groupsEdited(screen.groups.concat([{ name: "New Group", color: "#757575" }]))
                    groupList.positionViewAtEnd()
                }
            }
        }

        // Slides: how the thumbnails of slides look
        Column {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            spacing: 14
            visible: screen.section === "slides"

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#b0b3b8"
                font.pixelSize: 13
                text: "A slide's thumbnail has small icons in its top left corner for what comes with the slide: the hotkey that goes to it, and the media it brings. They can be made fainter, so that more of the slide shows through them, or more solid, so that they stand out."
            }

            Row {
                spacing: 12

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "Icon opacity"
                }

                AppSlider {
                    objectName: "actionIconOpacity"
                    width: 260
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0.05
                    to: 1
                    stepSize: 0.05
                    value: screen.actionIconOpacity
                    onMoved: screen.actionIconOpacityEdited(Math.round(value * 100) / 100)
                }

                Text {
                    width: 44
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: Math.round(screen.actionIconOpacity * 100) + "%"
                }
            }

            // How they look at that, over something light and something dark
            Row {
                spacing: 12

                Repeater {
                    model: ["#d9dde3", "#ff8a1f", "#15161a"]

                    delegate: Rectangle {
                        id: sample

                        required property string modelData

                        width: 150
                        height: 84
                        color: modelData
                        border.width: 1
                        border.color: "#45484e"

                        Row {
                            x: 4
                            y: 4
                            spacing: 3

                            ActionIcon {
                                color: "#ff8a1f"
                                strength: screen.actionIconOpacity

                                Text {
                                    anchors.centerIn: parent
                                    color: "#15161a"
                                    font.pixelSize: 11
                                    font.bold: true
                                    text: "C"
                                }
                            }

                            ActionIcon {
                                width: 21
                                strength: screen.actionIconOpacity

                                MediaBadge {
                                    anchors.centerIn: parent
                                    size: 0.8
                                    color: "transparent"
                                }
                            }
                        }
                    }
                }
            }
        }

        // Labels: the ones the workspace keeps ready for its slides
        Column {
            objectName: "labelsSettings"
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            spacing: 8
            visible: screen.section === "labels"

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#b0b3b8"
                font.pixelSize: 13
                bottomPadding: 6
                text: "A slide can be given a label: a few words, and a colour if wanted, shown under its thumbnail. These are the labels kept ready, which a slide's Label menu offers. They are the workspace's own list, the one ProPresenter keeps. Changing one here does not change the slides that were given it."
            }

            Repeater {
                model: Labels.presets

                delegate: Row {
                    id: presetRow

                    required property var modelData
                    required property int index

                    objectName: "labelPreset"
                    spacing: 10

                    // The label as a slide shows it: a tag of its colour, or plain words
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 260
                        height: 26
                        radius: 4
                        color: "#15161a"
                        border.width: 1
                        border.color: "#3a3c42"

                        Rectangle {
                            x: 6
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(parent.width - 12, tagText.implicitWidth + 14)
                            height: 18
                            radius: 3
                            color: presetRow.modelData.color !== "" ? presetRow.modelData.color : "transparent"

                            Text {
                                id: tagText

                                anchors.fill: parent
                                anchors.leftMargin: 7
                                anchors.rightMargin: 7
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                color: {
                                    const c = presetRow.modelData.color
                                    const part = i => parseInt(c.substring(1 + 2 * i, 3 + 2 * i), 16) / 255
                                    return c !== "" && 0.299 * part(0) + 0.587 * part(1) + 0.114 * part(2) > 0.6 ? "black" : "#e6e6e6"
                                }
                                font.pixelSize: 12
                                text: presetRow.modelData.text
                            }
                        }
                    }

                    AppButton {
                        objectName: "labelPresetEdit"
                        height: 26
                        font.pixelSize: 12
                        text: "Edit…"
                        onClicked: screen.labelEdited(presetRow.index)
                    }

                    AppButton {
                        objectName: "labelPresetRemove"
                        height: 26
                        font.pixelSize: 12
                        text: "Remove"
                        onClicked: screen.labelRemoved(presetRow.index)
                    }
                }
            }

            Text {
                visible: Labels.presets.length === 0
                color: "#8d9097"
                font.pixelSize: 13
                text: "No labels are kept ready in this workspace yet."
            }

            AppButton {
                objectName: "labelPresetNew"
                height: 28
                font.pixelSize: 13
                text: "+ New Label"
                onClicked: screen.labelEdited(-1)
            }
        }

        // Simple View: what it leaves in the window
        Column {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            spacing: 14
            visible: screen.section === "simple"

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#b0b3b8"
                font.pixelSize: 13
                text: "Simple View takes away what is round the slides, so that more of them can be seen at once: the toolbar, the lists at the left, the media bin, and the previews and controls at the right. It is switched from the toolbar, or by holding the ~ key."
            }

            AppCheck {
                objectName: "simpleKeepsSidePanel"
                checked: screen.simpleKeepsSidePanel
                text: "Keep the previews and controls at the right"
                onToggled: (checked) => screen.simpleKeepsSidePanelEdited(checked)
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#8d9097"
                font.pixelSize: 12
                text: "With this on, Simple View leaves the pane at the right where it is: the previews of the output and the stage, the clears, and the timers, props, stage layouts and macros. The slides have the rest of the window."
            }
        }

        // Screens: the workspace's, and what each is sent out through here
        ScreensSettings {
            objectName: "screensSettings"
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            visible: screen.section === "screens"
            senders: screen.senders
            onFailed: (text) => screen.screensFailed(text)
            onNdiWanted: ndiSetup.visible = true
            onMenuWanted: (rows, item, x, y) => screen.screenMenuWanted(rows, item, x, y)
        }

        // (The looks, which say what each of those screens gets, have a window of their
        // own, as in ProPresenter: the toolbar's Looks button leads to it. See LooksPanel.)

        // Video: who decodes it. (How it is drawn is not a setting: the graphics chip
        // draws it, whoever worked its pictures out.) One setting of three values, which
        // decodingchoice.h says the meaning of; it is acted on as the app starts.
        //
        // This part is all words, and on a computer the guard holds there is a paragraph
        // more of them: more than the sheet has room for in a short window. So it scrolls
        // where it has to, which the other parts have no need to.
        Flickable {
            id: videoPart

            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            visible: screen.section === "video"
            clip: true
            contentHeight: videoWords.height
            boundsBehavior: Flickable.StopAtBounds

            // The bar is kept in sight while there is more than is shown: where the words
            // stop at the end of a paragraph, nothing else would say that there is more.
            //
            // It is always on, and hidden where it is not wanted, and not switched on and
            // off. A bar that is switched off fades away, and a fade is frames drawn. This
            // part is laid out as the app starts, unseen, and for a moment while that is
            // done there is more than fits; switching the bar by that had the operator
            // window drawing for an eighth of a second longer at every start, with
            // nothing to show for it. The benchmark found it.
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AlwaysOn
                visible: videoPart.contentHeight > videoPart.height
            }

            KineticWheel {}

            Column {
                id: videoWords

                objectName: "videoSettings"
                // (Room is left for the scroll bar, as the list of groups leaves it.)
                width: videoPart.width - 14
                spacing: 10

                Text {
                    color: "#8d9097"
                    font.pixelSize: 11
                    font.bold: true
                    text: "VIDEO IS DECODED"
                }

                DecodingChoice {
                    mode: "auto"
                    title: "Automatically"
                    detail: "By the graphics chip, where its driver offers that: it is what keeps a 4K video cheap. By the processor "
                          + "where there is no such driver, and on a computer whose graphics driver is known to hang when its "
                          + "decoder is tried."
                }

                DecodingChoice {
                    mode: "processor"
                    title: "By the processor only"
                    detail: "No hardware video decoder is tried. For a computer on which a video shows nothing, or which hangs when a "
                          + "video starts or is changed for another: switch to this, start the app again, and see whether that goes away."
                }

                DecodingChoice {
                    mode: "hardware"
                    title: "By the graphics chip, whatever the app makes of this computer"
                    detail: "Hardware decoders are tried even where Automatically would leave them alone. For a computer the app is "
                          + "wrong about, or one whose graphics driver has since been put right."
                }

                Text {
                    objectName: "videoDecodingNote"
                    width: parent.width
                    wrapMode: Text.Wrap
                    topPadding: 6
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: "Whichever it is, the pictures are drawn by the graphics chip, with the same transitions: this is only "
                        + "about where they are worked out. A change takes effect the next time the app starts.\n\n"
                        + "Decoding now: " + VideoDecoding.inWords + "."
                        // What the app makes of this computer, where the guard holds it. (Not
                        // said twice, where how it is decoding now has said it already.)
                        + (VideoDecoding.guarded
                           ? "\n\n" + (VideoDecoding.inWords.includes(VideoDecoding.guardReason) ? "" : "On this computer " + VideoDecoding.guardReason + ". ")
                             + "Trying that decoder has been seen to hang such a computer, which is why Automatically leaves it alone "
                             + "here. With the firmware put in (the README says where that is explained), it is tried again without "
                             + "anything being changed here."
                           : "")
                }
            }
        }

        // Audio: what sound is played through, for everything the app plays: the audio
        // bin's music and sound effects, and a video's sound. The system's own choice,
        // which is wherever it sends sound and follows it if that changes, or one of the
        // devices it has. It is this computer's, kept with the app's own settings:
        // ProPresenter's file of a workspace names a device too, one of the computer it
        // was written on, and that is left as it is.
        //
        // (Made when the section is turned to: listing the devices means asking the
        // system's sound service, which nothing waits for as the app starts.)
        Loader {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            active: screen.section === "audio"
            visible: active

            sourceComponent: Flickable {
                id: audioPart

                clip: true
                contentHeight: audioWords.height
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOn
                    visible: audioPart.contentHeight > audioPart.height
                }

                KineticWheel {}

                // One thing sound can be played through: a round mark, what it is
                // called, and a word more about it
                component DeviceChoice: Item {
                    id: choice

                    property string device
                    property string title
                    property string detail
                    readonly property bool chosen: screen.audioDevice === device

                    width: parent ? parent.width : 0
                    height: Math.max(22, words.height)

                    Rectangle {
                        y: 2
                        width: 18
                        height: 18
                        radius: 9
                        color: "#15161a"
                        border.width: 1.5
                        border.color: choice.chosen ? "#ff8a1f" : choiceMouse.containsMouse ? "#8b8f98" : "#5c5f66"

                        Rectangle {
                            anchors.centerIn: parent
                            width: 10
                            height: 10
                            radius: 5
                            visible: choice.chosen
                            color: "#ff8a1f"
                        }
                    }

                    Column {
                        id: words

                        x: 30
                        width: parent.width - 30
                        spacing: 2

                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            color: "#e6e6e6"
                            font.pixelSize: 15
                            text: choice.title
                        }

                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            visible: text !== ""
                            color: "#9a9da3"
                            font.pixelSize: 13
                            text: choice.detail
                        }
                    }

                    MouseArea {
                        id: choiceMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: screen.audioDeviceEdited(choice.device)
                    }
                }

                Column {
                    id: audioWords

                    objectName: "audioSettings"
                    width: audioPart.width - 14
                    spacing: 10

                    Text {
                        color: "#8d9097"
                        font.pixelSize: 11
                        font.bold: true
                        text: "SOUND IS PLAYED THROUGH"
                    }

                    DeviceChoice {
                        objectName: "audioDevice:"
                        device: ""
                        title: "The system's own choice"
                        detail: {
                            const usual = AudioLayer.devices.find(device => device.usual)
                            return "Wherever this computer sends sound" + (usual ? ", which is now " + usual.name : "")
                                   + ". If that is changed in the system's settings, the app goes with it."
                        }
                    }

                    Repeater {
                        model: AudioLayer.devices

                        DeviceChoice {
                            required property var modelData

                            objectName: "audioDevice:" + modelData.name
                            device: modelData.id
                            title: modelData.name
                            detail: ""
                        }
                    }

                    // A device that was chosen and is not there now (unplugged, say)
                    DeviceChoice {
                        objectName: "audioDeviceGone"
                        visible: screen.audioDevice !== "" && !AudioLayer.devices.some(device => device.id === screen.audioDevice)
                        device: screen.audioDevice
                        title: "The device that was chosen"
                        detail: "It is not there now. Until it is, sound goes wherever this computer sends it."
                    }

                    Text {
                        objectName: "audioDeviceNote"
                        width: parent.width
                        wrapMode: Text.Wrap
                        topPadding: 6
                        color: "#9a9da3"
                        font.pixelSize: 13
                        text: "This is for everything the app plays: the music and the sound effects of the audio bin, and the sound "
                            + "of a video. What is playing goes over to a newly chosen device at once, but for a video, which goes "
                            + "over when it is next started. How loud each is on that device is the system's to set."
                    }
                }
            }
        }

        // Chords: how a chord is coloured in each of the two chord editors, each with what
        // it then looks like and a way back to the colour it comes with. They are only
        // how this app draws a chord in its editors, kept with the app's own settings:
        // nothing in a song's file says a colour.
        //
        // (Made when the section is turned to, and not with the rest as the app starts:
        // a swatch brings a whole colour picker with it, and most starts of the app
        // never come here.)
        Loader {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            active: screen.section === "chords"
            visible: active

            sourceComponent: Column {
                objectName: "chordSettings"
                spacing: 18

                ChordColour {
                    id: sheetColour

                    title: "On the chord sheet"
                    chosen: screen.chordSheetColor
                    usual: screen.usualSheetChord
                    onEdited: (color) => screen.chordSheetColorEdited(color)

                    // A chord as the sheet draws one: its name in a bubble, over a word
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: "#222428"

                        Rectangle {
                            x: 12
                            y: 9
                            width: sampleName.implicitWidth + 14
                            height: 22
                            radius: 6
                            color: "#34363c"
                            border.width: 1
                            border.color: "#5c5f66"

                            Text {
                                id: sampleName

                                anchors.centerIn: parent
                                color: sheetColour.shown
                                font.pixelSize: 13
                                font.bold: true
                                text: "Em7"
                            }
                        }

                        Rectangle {
                            x: 15
                            y: 30
                            width: 2
                            height: 8
                            color: "#5c5f66"
                        }

                        Text {
                            x: 12
                            y: 36
                            color: "#e6e6e6"
                            font.pixelSize: 17
                            text: "Amazing grace"
                        }
                    }
                }

                Rectangle {
                    width: parent.width - 14
                    height: 1
                    color: "#3a3c42"
                }

                // The highlighted line of the sheet: the line the person is at, whose
                // words take this colour (ChordSheet.qml, `highlighted`)
                ChordColour {
                    id: lineColour

                    title: "The highlighted line"
                    says: "The line the pointer is on, on the chord sheet: where Ctrl+C copies from and Ctrl+V pastes to."
                    chosen: screen.chordLineColor
                    usual: screen.usualSheetLine
                    onEdited: (color) => screen.chordLineColorEdited(color)

                    // Two lines as the sheet draws them, the second the highlighted one
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: "#222428"

                        Column {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            Text {
                                color: "#e6e6e6"
                                font.pixelSize: 17
                                text: "Amazing grace"
                            }

                            Text {
                                color: lineColour.shown
                                font.pixelSize: 17
                                text: "How sweet the sound"
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width - 14
                    height: 1
                    color: "#3a3c42"
                }

                ChordColour {
                    id: textColour

                    title: "In ChordPro text"
                    chosen: screen.chordTextColor
                    usual: screen.usualTextChord
                    onEdited: (color) => screen.chordTextColorEdited(color)

                    // A line as the text shows one: the chords in their colour, bold, among
                    // the words
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: "#1b1c1f"
                        border.width: 1
                        border.color: "#3a3c42"

                        Row {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter

                            Repeater {
                                model: [{ text: "[Em7]", chord: true }, { text: "Amazing ", chord: false }, { text: "[D]", chord: true }, { text: "grace", chord: false }]

                                Text {
                                    required property var modelData

                                    color: modelData.chord ? textColour.shown : "#e6e6e6"
                                    font.family: "monospace"
                                    font.pixelSize: 14
                                    font.bold: modelData.chord
                                    text: modelData.text
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width - 14
                    height: 1
                    color: "#3a3c42"
                }

                // Sharps and flats drawn as the signs on the chord sheet (and in the
                // song's two keys over it), with three chords as it then draws them
                Item {
                    width: parent.width
                    height: 70

                    Column {
                        width: parent.width - signsSample.width - 40
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        AppCheck {
                            objectName: "chordSigns"
                            checked: screen.chordSigns
                            text: "Show sharps and flats as ♯ and ♭"
                            onToggled: (checked) => screen.chordSignsEdited(checked)
                        }

                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: "#9a9da3"
                            font.pixelSize: 13
                            text: "On the chord sheet. Songs are saved with # and b either way."
                        }
                    }

                    Rectangle {
                        id: signsSample

                        objectName: "chordSignsSample"
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        width: 200
                        height: 44
                        radius: 4
                        color: "#222428"

                        Row {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8

                            Repeater {
                                model: ["Bbm7", "F#", "Eb/G"]

                                Rectangle {
                                    required property string modelData

                                    width: signedName.implicitWidth + 14
                                    height: 22
                                    radius: 6
                                    color: "#34363c"
                                    border.width: 1
                                    border.color: "#5c5f66"

                                    Text {
                                        id: signedName

                                        anchors.centerIn: parent
                                        color: sheetColour.shown
                                        font.pixelSize: 13
                                        font.bold: true
                                        text: screen.chordSigns ? Chords.withSigns(parent.modelData) : parent.modelData
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    width: parent.width - 14
                    wrapMode: Text.Wrap
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: "These are only how a song looks in the two chord editors. Nothing in a song's file says any of them: the stage "
                        + "shows chords in the colour its layout gives them, and ProPresenter in its own."
                }
            }
        }

        // Bibles: the Bibles imported from XML files, and api.bible's key (src/bibles.h).
        // Made when the section is turned to: nothing of Bibles is made before it is
        // wanted.
        Loader {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            active: screen.section === "bibles"
            visible: active

            sourceComponent: Column {
                objectName: "bibleSettings"
                spacing: 12

                Text {
                    width: parent.width - 14
                    wrapMode: Text.Wrap
                    color: "#b0b3b8"
                    font.pixelSize: 13
                    text: "Bible mode (the Bible button in the toolbar, or Ctrl+Alt+B) makes slides of a passage. It reads from three Bibles in "
                        + "the public domain that come with the app, from Bibles imported here from XML files (Zefania, OSIS, Beblia and "
                        + "OpenSong), and from api.bible over the internet. Most other translations are copyright, and an imported file is the "
                        + "importer's to answer for."
                }

                SectionTitle {
                    text: "Included"
                }

                Repeater {
                    model: Bibles.translations.filter(t => t.source === "included")

                    Text {
                        required property var modelData

                        objectName: "includedBible:" + modelData.abbreviation
                        width: parent.width - 14
                        height: 28
                        verticalAlignment: Text.AlignVCenter
                        color: "#e6e6e6"
                        font.pixelSize: 14
                        text: modelData.name + "  (" + modelData.abbreviation + ")"
                    }
                }

                Text {
                    width: parent.width - 14
                    wrapMode: Text.Wrap
                    color: "#7d8088"
                    font.pixelSize: 12
                    text: Bibles.translations.some(t => t.source === "included")
                          ? "From eBible.org, which has them as public domain: the KJV except in the United Kingdom, where the Crown's letters "
                            + "patent apply, and the BBE in the United States."
                          : "Not found beside the app: it was built or installed without them."
                }

                SectionTitle {
                    text: "Imported"
                }

                Repeater {
                    model: Bibles.translations.filter(t => t.source === "file")

                    Item {
                        required property var modelData

                        width: parent.width - 14
                        height: 32

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#e6e6e6"
                            font.pixelSize: 14
                            text: parent.modelData.name + (parent.modelData.abbreviation !== "" ? "  (" + parent.modelData.abbreviation + ")" : "")
                        }

                        AppButton {
                            objectName: "removeBible:" + parent.modelData.name
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 28
                            font.pixelSize: 12
                            text: "Remove"
                            onClicked: {
                                const error = Bibles.remove(parent.modelData.id)
                                if (error !== "")
                                    screen.screensFailed(error)
                            }
                        }
                    }
                }

                Text {
                    visible: !Bibles.translations.some(t => t.source === "file")
                    color: "#7d8088"
                    font.pixelSize: 13
                    text: "None yet."
                }

                AppButton {
                    objectName: "importBible"
                    height: 30
                    font.pixelSize: 13
                    text: "Import XML Bible…"
                    onClicked: bibleDialog.open()
                }

                FileDialog {
                    id: bibleDialog

                    title: "Import XML Bible"
                    nameFilters: ["XML Bibles (*.xml *.osis *.zefania)", "All files (*)"]
                    onAccepted: Bibles.importFile(selectedFile)
                }

                Connections {
                    target: Bibles

                    function onImported(error, id) {
                        if (error !== "")
                            screen.screensFailed(error)
                    }
                }

                SectionTitle {
                    text: "api.bible"
                }

                Text {
                    width: parent.width - 14
                    wrapMode: Text.Wrap
                    color: "#b0b3b8"
                    font.pixelSize: 13
                    // (The app comes with no key: each person registers for their own, as the
                    // maintainer settled on 2026-10-15. One can still be built in: README.)
                    text: (Bibles.builtInKey ? "This copy of the app was built with a key for api.bible, which gives the Bibles that key has been allowed. "
                                               + "A key of your own can be typed here instead. "
                                             : "api.bible needs a key of your own: register at api.bible for one (it is free for use that is not commercial), "
                                               + "ask there for the Bibles you want, and type the key here. ")
                        + "Each passage is asked for when it is wanted, and nothing of it is kept but in slides you save."
                }

                Row {
                    spacing: 10

                    AppTextField {
                        id: keyField

                        objectName: "apiBibleKey"
                        width: 380
                        height: 30
                        font.pixelSize: 13
                        placeholderText: "Your own api.bible key"
                        echoMode: TextInput.Password
                        text: screen.apiBibleKey
                        onEditingFinished: screen.apiBibleKeyEdited(text.trim())
                    }

                    AppButton {
                        height: 30
                        font.pixelSize: 12
                        text: "Clear"
                        enabled: screen.apiBibleKey !== ""
                        onClicked: {
                            keyField.text = ""
                            screen.apiBibleKeyEdited("")
                        }
                    }
                }
            }
        }

        // Windows
        Column {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            spacing: 14
            visible: screen.section === "windows"

            Row {
                spacing: 12

                // A switch
                Rectangle {
                    width: 44
                    height: 24
                    radius: 12
                    color: screen.useX11 ? "#ff8a1f" : "#45484e"

                    Rectangle {
                        x: screen.useX11 ? parent.width - width - 3 : 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18
                        height: 18
                        radius: 9
                        color: "#e6e6e6"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: screen.useX11Edited(!screen.useX11)
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#e6e6e6"
                    font.pixelSize: 15
                    text: "Remember window positions (run through X11)"
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "A Wayland desktop does not let an application place its own windows, so the "
                    + "output and stage windows come back wherever the desktop puts them. Running "
                    + "through X11 lets their positions be remembered and keeps them above every "
                    + "other window.\n\nThe cost: on a display scaled to anything other than 100% or "
                    + "200%, everything is drawn at 200% and scaled down by the desktop, which is more "
                    + "work for the graphics hardware and slightly softens the output.\n\n"
                    + "Takes effect the next time the app starts. Running now on: "
                    + (screen.platform === "xcb" ? "X11" : screen.platform === "wayland" ? "Wayland" : screen.platform) + "."
            }

            Text {
                objectName: "altTabNote"
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "Alt+Tab. The output and stage windows are there to be looked at, not switched to, so "
                    + "they are kept out of the desktop's window switcher. Through X11 the app sees to that "
                    + "itself. Through Wayland only the desktop can: on GNOME, switch on the extension "
                    + "“Simple Presenter windows”, which comes with the app (the README says how)."
            }
        }

        // About: what this is, which version, and where the log is
        Column {
            anchors.left: sectionList.right
            anchors.right: parent.right
            anchors.top: done.bottom
            anchors.margins: 18
            anchors.topMargin: 12
            spacing: 14
            visible: screen.section === "about"

            Text {
                color: "#e6e6e6"
                font.pixelSize: 17
                text: "Simple Presenter " + Qt.application.version
            }

            // What it is, where it cannot be missed
            Rectangle {
                objectName: "whatThisIs"
                width: parent.width
                height: whatThisIs.implicitHeight + 24
                radius: 6
                color: "#2b2318"
                border.width: 1
                border.color: "#ff8a1f"

                Text {
                    id: whatThisIs

                    x: 12
                    y: 12
                    width: parent.width - 24
                    wrapMode: Text.Wrap
                    textFormat: Text.StyledText
                    color: "#e6e6e6"
                    font.pixelSize: 14
                    text: "<b>This is a personal experiment, not a product.</b><br><br>"
                        + "Simple Presenter is one person's hobby project, and it is vibe coded: it was built by "
                        + "describing it to an AI model, which wrote the code. It is shared as an example of what "
                        + "vibe coding can do in the hands of a hobby programmer, and is not a product for use. "
                        + "Nobody supports it, nothing about it is promised, and it comes with no warranty of any "
                        + "kind: anything done with it is at your own risk. If you run it, give it a copy of your "
                        + "ProPresenter folder, never your only one."
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "It is free software, under the MIT licence: do with it what you like. It has "
                    + "nothing to do with Renewed Vision, the makers of ProPresenter.\n\n"
                    + "Each time the app runs it keeps a log: a text file that says what it was running on and "
                    + "what it did, for working out what happened when something has gone wrong. It holds the "
                    + "names of files, presentations and playlists, and nothing of what is in them. The twenty "
                    + "most recent are kept.\n\n"
                    + (Log.path !== "" ? "This run's log is “" + Log.path.substring(Log.path.lastIndexOf("/") + 1) + "”, in "
                                         + Log.folder + "."
                                       : "No log is being kept this time: the folder for it could not be written to.")
            }

            AppButton {
                objectName: "showLogsFolder"
                height: 30
                font.pixelSize: 13
                text: "Show the Logs Folder"
                enabled: Log.folder !== ""
                onClicked: Log.showFolder()
            }

            // What of other people's is used, and whose it is
            Text {
                objectName: "aboutNdi"
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                textFormat: Text.StyledText
                linkColor: "#6fb3ff"
                onLinkActivated: (link) => Qt.openUrlExternally(link)
                text: "A screen can be sent over the network with NDI®, by NDI's own library, which is not part of this app "
                    + "(Screens has how to get it). NDI® is a registered trademark of Vizrt NDI AB. "
                    + "<a href=\"https://ndi.video\">ndi.video</a>"
            }
        }
    }

    // How to get NDI's library, when a screen is set to NDI without it
    NdiSetup {
        id: ndiSetup

        anchors.fill: parent
        visible: false
        onClosed: visible = false
    }
}
