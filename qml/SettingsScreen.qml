import QtQuick
import QtQuick.Controls.Basic
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

    // The hotkeys the workspace has for groups that are not in the list here, in words
    property string otherHotkeys: ""
    // How solid the icons on the slides are: see Main.qml
    property real actionIconOpacity: 0.8

    // What sends each screen that goes over NDI, by the screen's id: see ScreensSettings
    property var senders: ({})

    signal groupsEdited(var groups)
    signal screensFailed(string text)
    signal actionIconOpacityEdited(real opacity)
    signal useX11Edited(bool useX11)
    signal closed

    // What the app is running on now: "wayland", "xcb" (X11), "windows", "cocoa", ...
    readonly property string platform: Qt.platform.pluginName
    readonly property var sections: Qt.platform.os === "linux"
        ? [{ name: "Groups", path: "groups" }, { name: "Slides", path: "slides" }, { name: "Screens", path: "screens" },
           { name: "Windows", path: "windows" }, { name: "About", path: "about" }]
        : [{ name: "Groups", path: "groups" }, { name: "Slides", path: "slides" }, { name: "Screens", path: "screens" },
           { name: "About", path: "about" }]
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
                        + "describing it to an AI model, which wrote the code. Nobody supports it, nothing about "
                        + "it is promised, and it comes with no warranty of any kind. Use it at your own risk, "
                        + "and give it a copy of your ProPresenter folder, never your only one."
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "It is free software, under the GNU Lesser General Public License, version 3, and has "
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
        }
    }
}
