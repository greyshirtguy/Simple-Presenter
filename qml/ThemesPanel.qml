import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The themes, under the toolbar's Themes button: the workspace's themes to browse, and
// to dress the presentation that is open in.
//
// A theme is a set of slides that show how slides should look (see Themes). They are
// shown as they are kept: folders of themes as folders, a theme as its first slide.
// A click on a folder goes into it, and on a theme shows the theme's slides; a click on
// one of those dresses every slide of the open presentation in it. (One slide is
// dressed from its own menu, under Theme.) The line along the top says where this is,
// and each part of it goes back there.
Rectangle {
    id: panel

    // The operator window
    required property var win
    // Where this is: the place of the folder being looked into, "" for the top; and the
    // place of the theme whose slides are shown, "" while it is folders and themes
    property string folder: ""
    property string theme: ""
    // A name being typed for a new theme
    property bool naming: false

    signal closed

    // What is shown: the entries of the folder, or the slides of the theme
    readonly property var entries: {
        if (theme !== "") {
            const found = Themes.themes.find(candidate => candidate.place === theme)
            return found ? found.slides.map(slide => ({ kind: "slide", name: slide.name, id: slide.id, slide: slide.slide })) : []
        }
        let items = Themes.tree
        if (folder !== "") {
            for (const part of folder.split("/")) {
                const inside = items.find(entry => entry.kind === "folder" && entry.name === part)
                items = inside ? inside.items : []
            }
        }
        return items
    }
    // The line along the top: [{ label, folder }], the last being where this is
    readonly property var path: {
        const parts = [{ label: "Themes", folder: "" }]
        const place = theme !== "" ? theme : folder
        const names = place === "" ? [] : place.split("/")
        names.forEach((name, at) => parts.push({ label: name, folder: names.slice(0, at + 1).join("/") }))
        return parts
    }

    function open() {
        naming = false
        // (Where it was last, if that is still there.)
        if (theme !== "" && Themes.theme(theme) === null)
            theme = ""
    }

    function pick(entry) {
        if (entry.kind === "folder") {
            folder = entry.place
        } else if (entry.kind === "theme") {
            theme = entry.place
        } else {
            win.applyTheme([], theme, entry.id)
            closed()
        }
    }

    color: "#25272b"
    radius: 8
    border.width: 1
    border.color: "#4a4d55"

    // (A click in it is not a click on what is under it.)
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
    }

    // ---- Where this is
    Row {
        id: crumbs

        x: 12
        y: 10
        spacing: 4

        Repeater {
            model: panel.path

            Row {
                id: crumb

                required property var modelData
                required property int index
                readonly property bool last: index === panel.path.length - 1

                spacing: 4

                Text {
                    objectName: "themesCrumb"
                    color: crumb.last ? "#e6e6e6" : crumbMouse.containsMouse ? "#ff8a1f" : "#9a9da3"
                    font.pixelSize: 13
                    font.bold: crumb.last
                    text: crumb.modelData.label

                    MouseArea {
                        id: crumbMouse

                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        enabled: !crumb.last
                        onClicked: {
                            panel.theme = ""
                            panel.folder = crumb.modelData.folder
                        }
                    }
                }

                Text {
                    visible: !crumb.last
                    color: "#6c6f75"
                    font.pixelSize: 13
                    text: "›"
                }
            }
        }
    }

    // ---- A new theme, and changing the one being looked at
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 6
        spacing: 6

        AppTextField {
            id: nameField

            objectName: "themeNameField"
            width: 170
            height: 26
            font.pixelSize: 12
            visible: panel.naming
            placeholderText: "Name of the new theme"
            onAccepted: {
                const made = Themes.add(text)
                if (panel.win.report(made.error)) {
                    panel.naming = false
                    panel.folder = ""
                    panel.theme = made.place
                    text = ""
                }
            }
            Keys.onEscapePressed: panel.naming = false
        }

        AppButton {
            objectName: "themeNew"
            height: 26
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 12
            visible: panel.theme === "" && !panel.naming
            text: "+ New Theme"
            onClicked: {
                panel.naming = true
                nameField.forceActiveFocus()
            }
        }

        AppButton {
            objectName: "themeAddSlide"
            height: 26
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 12
            visible: panel.theme !== ""
            text: "+ Slide"
            onClicked: panel.win.report(Themes.addSlide(panel.theme).error)
        }

        AppButton {
            objectName: "themeEdit"
            height: 26
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 12
            visible: panel.theme !== ""
            text: "Edit"
            onClicked: {
                const place = panel.theme
                panel.closed()
                panel.win.startEditingTheme(place, "")
            }
        }

        AppButton {
            objectName: "themeRemove"
            height: 26
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 12
            visible: panel.theme !== ""
            text: "Delete Theme…"
            onClicked: panel.win.showMenu([
                { header: "Delete the theme “" + panel.theme.split("/").pop() + "”?" },
                { label: "Delete It", danger: true, run: () => {
                    const place = panel.theme
                    panel.theme = ""
                    panel.win.report(Themes.remove(place))
                } },
                { label: "Keep It", run: () => {} }
            ], this)
        }
    }

    // ---- What is here
    GridView {
        id: grid

        objectName: "themesGrid"
        anchors.fill: parent
        anchors.margins: 10
        anchors.topMargin: 40
        anchors.bottomMargin: 34
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        cellWidth: Math.floor(width / Math.max(2, Math.floor(width / 150)))
        cellHeight: Math.round((cellWidth - 12) * 9 / 16) + 30
        model: panel.entries

        delegate: Item {
            id: tile

            required property var modelData
            // What is drawn of it: a theme's first slide, or a theme slide itself
            readonly property var shown: modelData.kind === "slide" ? modelData.slide
                                       : modelData.kind === "theme" && modelData.slides.length > 0 ? modelData.slides[0].slide : null

            objectName: "themesTile"
            width: grid.cellWidth
            height: grid.cellHeight

            Rectangle {
                id: picture

                x: 6
                y: 4
                width: parent.width - 12
                height: Math.round(width * 9 / 16)
                color: tile.modelData.kind === "folder" ? "#2f3136" : "black"
                border.width: tileMouse.containsMouse ? 2 : 1
                border.color: tileMouse.containsMouse ? "#ff8a1f" : "#4a4d55"

                Slide {
                    anchors.fill: parent
                    anchors.margins: 2
                    visible: tile.shown !== null
                    slide: tile.shown
                    effects: false
                }

                // A folder
                Item {
                    anchors.centerIn: parent
                    width: 44
                    height: 32
                    visible: tile.modelData.kind === "folder"

                    Rectangle {
                        width: 20
                        height: 8
                        radius: 2
                        color: "#c9a25a"
                    }

                    Rectangle {
                        y: 5
                        width: 44
                        height: 27
                        radius: 3
                        color: "#e0b666"
                    }
                }
            }

            Text {
                anchors.top: picture.bottom
                anchors.topMargin: 4
                anchors.horizontalCenter: picture.horizontalCenter
                width: picture.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                color: "#e6e6e6"
                font.pixelSize: 12
                text: tile.modelData.name
            }

            MouseArea {
                id: tileMouse

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton) {
                        panel.pick(tile.modelData)
                    } else if (tile.modelData.kind === "slide") {
                        const place = panel.theme
                        const id = tile.modelData.id
                        panel.win.showMenu([
                            { header: tile.modelData.name },
                            { label: "Edit", run: () => {
                                panel.closed()
                                panel.win.startEditingTheme(place, id)
                            } },
                            { label: "Delete Slide", danger: true, run: () => panel.win.report(Themes.removeSlide(place, id)) }
                        ], tile, mouse.x, mouse.y)
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: grid
        visible: panel.entries.length === 0
        color: "#8a8d93"
        font.pixelSize: 13
        text: panel.theme !== "" ? "This theme has no slides." : panel.folder !== "" ? "Nothing in this folder." : "This workspace has no themes yet."
    }

    Text {
        objectName: "themesHint"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        elide: Text.ElideRight
        color: "#8a8d93"
        font.pixelSize: 12
        text: panel.theme === "" ? "Pick a theme to see its slides."
            : panel.win.document === null || panel.win.currentEntry() === undefined ? "Open a presentation to dress it in one of these."
            : "A click dresses every slide of “" + panel.win.document.name + "” in that one. For a single slide, use the slide’s own menu."
    }
}
