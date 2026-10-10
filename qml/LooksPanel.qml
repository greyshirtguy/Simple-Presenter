import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes
import SimplePresenterApp

// The Looks window: the workspace's looks down the left, and on the right what the one
// picked gives each audience screen, as a table with the screens across the top and the
// layers of the show down the side. It comes up over the operator window from the Looks
// button of the toolbar (Edit Looks…), and its own ✕, Esc or a click outside puts it
// away.
//
// It is laid out as ProPresenter's own Looks window is, because the looks are
// ProPresenter's (see lookfile.h), and someone who knows the one should find the other
// where they expect it:
//
//   - Picking a look in the list is for seeing what it says, and for changing it. It
//     does not make it live. Make Live, at the top right, does that.
//   - The live look is a look of its own, and is first in the list, apart from the
//     saved ones. It is what the screens are showing. Making a saved look live copies
//     that look into it; after that it can be changed by itself, here, and the screens
//     follow at once, while the saved look stays as it was. So it can come to differ
//     from the look it was made from, which the list then says ("changed"). Two things
//     end that: Save, which is at the top right in place of Make Live once the live
//     look has been changed, keeps the live look as that saved look; and making that
//     look live again puts the live look back as the saved look has it. (An action on
//     a slide that goes over to a look does the second, which is how a change made to
//     the live look by hand comes to be undone by the next song.) The live look
//     cannot be removed or renamed: there is always something the screens are
//     showing. The saved look it came from has a green dot.
//   - The layers are in the order they lie on a screen, the top one first: masks,
//     messages, props, announcements, the presentation (its slides, then its media),
//     video inputs. This app has the props, the slides and the media; the others are
//     ProPresenter's, and are shown greyed, as the look has them in the file, and
//     cannot be changed here. Nothing a look says of them is lost.
//   - "Presentation" is not a layer but the theme the screen's slides are dressed in as
//     they are shown (ProPresenter's alternate theme): none, which is a circle with a
//     line through it, or a theme slide, chosen from the same menu of themes a slide's
//     own menu has.
//
// A new look (the +) is a copy of the one that is picked, so that the live look as it
// has been changed can be kept as a saved look, and a look can be made from one near it.
Rectangle {
    id: panel

    // The operator window
    required property var win
    // What is picked in the list: `liveKey` for the live look, or a saved look's id
    readonly property string liveKey: "<live>"
    property string picked: liveKey
    readonly property bool onLive: picked === liveKey
    readonly property var look: onLive ? Looks.live : (Looks.looks.find(candidate => candidate.id === picked) ?? null)
    // The saved look whose name is being typed in place, by id
    property string renaming: ""
    readonly property var screens: Screens.audience
    readonly property color accentColor: "#ff8a1f"
    readonly property color liveColor: "#3ddc68"
    // The layers, the top one first. `ours` are the ones this app has, which can be
    // changed; `kind` is what stands in a screen's cell: a switch, the theme, or a mask.
    readonly property var layers: [
        { key: "mask", label: "Masks", kind: "mask", ours: false },
        { key: "messages", label: "Messages", kind: "switch", ours: false },
        { key: "props", label: "Props", kind: "switch", ours: true },
        { key: "announcements", label: "Announcements", kind: "switch", ours: false },
        { key: "theme", label: "Presentation", kind: "theme", ours: true },
        { key: "slide", label: "Slide", kind: "switch", ours: true },
        { key: "media", label: "Media", kind: "switch", ours: true },
        { key: "videoInput", label: "Video Input", kind: "switch", ours: false }
    ]
    // What a look gives a screen it says nothing of: everything, as it is
    readonly property var everything: ({ slide: true, media: true, props: true, theme: "", themeSlide: "", messages: true,
                                         announcements: true, videoInput: true, mask: "" })

    signal closed

    function open() {
        picked = liveKey
        renaming = ""
        forceActiveFocus()
    }

    // Gives the window the keyboard, unless a look's name is being typed, which has it.
    function takeKeys() {
        if (renaming === "")
            forceActiveFocus()
    }

    function say(error) {
        if (error !== "")
            win.report(error)
        return error === ""
    }

    // What the look that is picked gives a screen
    function lineOf(screenId) {
        return look && look.screens && look.screens[screenId] !== undefined ? look.screens[screenId] : everything
    }

    // Changes what the look that is picked gives a screen. For the live look the
    // screens show it at once. The log is told, with the names: what a look gave a
    // screen, and since when, is the first thing wanted when a screen is not showing
    // what was expected.
    function set(screenId, changes) {
        if (!say(Looks.setScreen(onLive ? Looks.live.id : picked, screenId, changes)))
            return
        const screen = screens.find(candidate => candidate.id === screenId)
        const layerNames = { slide: "the slides", media: "the media", props: "the props" }
        const done = []
        for (const layer of ["slide", "media", "props"]) {
            if (changes[layer] !== undefined)
                done.push((changes[layer] ? "gets " : "no longer gets ") + layerNames[layer])
        }
        if (changes.theme !== undefined)
            done.push(changes.theme === "" ? "is given no theme" : "is given the theme slide \"" + themeName(changes) + "\"")
        Log.note("looks", (onLive ? "the live look" : "the saved look \"" + (look ? look.name : "") + "\"") + ": the screen \""
                 + (screen ? screen.name : screenId) + "\" " + done.join(", "))
    }

    function makeLive() {
        if (!onLive && look)
            Show.lookId = picked
    }

    // Keeps the live look, as it has been changed, as the saved look it was made from.
    function save() {
        say(Looks.saveLive())
    }

    // A new saved look, a copy of the one that is picked, to be named at once.
    function add() {
        let name = "Look"
        for (let number = 2; Looks.looks.some(candidate => candidate.name === name); ++number)
            name = "Look " + number
        const made = Looks.add(name, screens.map(screen => screen.id), onLive ? Looks.live.id : picked)
        if (say(made.error)) {
            picked = made.id
            renaming = made.id
        }
    }

    function duplicate(id) {
        const from = Looks.looks.find(candidate => candidate.id === id)
        if (!from)
            return
        let name = from.name + " Copy"
        for (let number = 2; Looks.looks.some(candidate => candidate.name === name); ++number)
            name = from.name + " Copy " + number
        const made = Looks.add(name, [], id)
        if (say(made.error))
            picked = made.id
    }

    function remove(id) {
        if (say(Looks.remove(id)) && picked === id)
            picked = liveKey
    }

    function rename(id, name) {
        renaming = ""
        const from = Looks.looks.find(candidate => candidate.id === id)
        if (from && name.trim() !== "" && name.trim() !== from.name)
            say(Looks.rename(id, name))
        forceActiveFocus()
    }

    function showLookMenu(entry, item, x, y) {
        win.showMenu([
            { header: entry.name },
            { label: "Make Live", run: () => Show.lookId = entry.id },
            { label: "Rename", run: () => { picked = entry.id; renaming = entry.id } },
            { label: "Duplicate", run: () => duplicate(entry.id) },
            { label: "Delete…", items: [{ header: "Delete “" + entry.name + "”?" },
                                        { label: "Delete Look", danger: true, run: () => remove(entry.id) }] }
        ], item, x, y)
    }

    // The themes to choose one of for a screen, as a slide's own menu has them, with
    // none first.
    function showThemeMenu(screenId, item) {
        const line = lineOf(screenId)
        win.showMenu([{ header: "Presentation" },
                      { label: "No Theme", current: line.theme === "", run: () => set(screenId, { theme: "", themeSlide: "" }) },
                      { header: "" }]
                     .concat(win.themePickItems((place, slideId) => set(screenId, { theme: place, themeSlide: slideId })).slice(1)),
                     item, 0, item.height + 2)
    }

    // The name of the theme slide a line dresses its screen in, for under its picture
    function themeName(line) {
        if (line.theme === "")
            return ""
        const theme = Themes.themes.find(candidate => candidate.place === line.theme)
        const slide = theme ? theme.slides.find(candidate => candidate.id === line.themeSlide) : undefined
        return slide ? slide.name : theme ? theme.name : line.theme.substring(line.theme.lastIndexOf("/") + 1)
    }

    color: "#99000000"
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape)
            closed()
        // (Nothing typed here is a key of the show's.)
        event.accepted = true
    }

    Connections {
        target: Looks

        // A look that has gone is not picked any more.
        function onChanged() {
            if (!panel.onLive && !Looks.looks.some(candidate => candidate.id === panel.picked))
                panel.picked = panel.liveKey
        }
    }

    // A click outside the window puts it away; nothing behind it is clicked or scrolled.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.closed()
    }

    // The sign for nothing: a circle with a line through it
    component Nothing: Shape {
        id: nothing

        property color ink: "#8d9097"

        width: 18
        height: 18
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: nothing.ink
            strokeWidth: 1.6
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: 9
                centerY: 9
                radiusX: 7
                radiusY: 7
                startAngle: 0
                sweepAngle: 360
            }

            PathMove {
                x: 4.1
                y: 13.9
            }

            PathLine {
                x: 13.9
                y: 4.1
            }
        }
    }

    // A row of the list: the live look, or a saved one
    component LookRow: Rectangle {
        id: row

        property string key
        property string name
        // Of the live look: that it is no longer what the look it came from is. Of a
        // saved look: that it is the one the live look came from.
        property bool changed: false
        property bool origin: false
        property bool isLive: false
        readonly property bool chosen: panel.picked === key
        readonly property bool typing: !isLive && panel.renaming === key

        signal menuAsked(real x, real y)

        width: parent ? parent.width : 0
        height: 30
        radius: 5
        color: chosen ? "#3f4249" : rowMouse.containsMouse ? "#33353a" : "transparent"

        Rectangle {
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: 8
            radius: 4
            visible: row.isLive || row.origin
            color: panel.liveColor
        }

        Text {
            x: 26
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 34 - (tag.visible ? tag.implicitWidth + 8 : 0)
            visible: !row.typing
            elide: Text.ElideRight
            color: "#e6e6e6"
            font.pixelSize: 13
            font.bold: row.isLive
            text: row.name
        }

        Text {
            id: tag

            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            visible: row.changed && !row.typing
            color: "#9a9da3"
            font.pixelSize: 11
            text: "changed"
        }

        MouseArea {
            id: rowMouse

            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: (mouse) => {
                panel.picked = row.key
                if (mouse.button === Qt.RightButton && !row.isLive)
                    row.menuAsked(mouse.x, mouse.y)
            }
            onDoubleClicked: (mouse) => {
                if (mouse.button === Qt.LeftButton && !row.isLive)
                    panel.renaming = row.key
            }
        }

        // The name, to be typed over
        Rectangle {
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 26
            height: 24
            radius: 4
            visible: row.typing
            color: "#15161a"
            border.width: 1
            border.color: panel.accentColor

            TextInput {
                id: nameField

                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: "#f0f0f0"
                selectionColor: panel.accentColor
                selectedTextColor: "black"
                font.pixelSize: 13
                onVisibleChanged: {
                    if (visible) {
                        text = row.name
                        forceActiveFocus()
                        selectAll()
                    }
                }
                onAccepted: panel.rename(row.key, text)
                onActiveFocusChanged: {
                    if (!activeFocus && row.typing)
                        panel.rename(row.key, text)
                }
                Keys.onEscapePressed: {
                    panel.renaming = ""
                    panel.forceActiveFocus()
                }
            }
        }
    }

    Rectangle {
        id: sheet

        objectName: "looksPanel"
        anchors.centerIn: parent
        width: Math.min(panel.width - 40, Math.max(640, list.width + 178 + panel.screens.length * table.columnWidth + 44))
        height: Math.min(panel.height - 40, 468)
        radius: 10
        color: "#25272b"
        border.width: 1
        border.color: "#4a4d55"

        // (A click in the window is not a click outside it.)
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: {
                // A click on the window finishes a name that was being typed.
                if (panel.renaming !== "")
                    panel.forceActiveFocus()
            }
        }

        // ---- The looks
        Item {
            id: list

            objectName: "looksList"
            x: 10
            y: 10
            width: 214
            height: parent.height - 20

            Text {
                id: liveCaption

                x: 10
                y: 4
                color: "#8d9097"
                font.pixelSize: 11
                font.bold: true
                text: "LIVE"
            }

            LookRow {
                id: liveRow

                objectName: "lookRow:live"
                y: liveCaption.y + liveCaption.height + 6
                key: panel.liveKey
                isLive: true
                // A workspace that has never had a look made live shows everything.
                name: Looks.live.name !== undefined && Looks.live.name !== "" ? Looks.live.name : "Everything"
                changed: Looks.live.changed === true
            }

            Rectangle {
                id: listRule

                y: liveRow.y + liveRow.height + 10
                width: parent.width
                height: 1
                color: "#3a3c42"
            }

            Text {
                id: savedCaption

                x: 10
                y: listRule.y + 12
                color: "#8d9097"
                font.pixelSize: 11
                font.bold: true
                text: "LOOKS"
            }

            // A new look, a copy of the one that is picked
            Rectangle {
                id: addButton

                objectName: "looksAdd"
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: savedCaption.verticalCenter
                width: 26
                height: 22
                radius: 5
                color: addMouse.pressed ? "#5c5f67" : addMouse.containsMouse ? "#53565e" : "#474a51"

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -1
                    color: "#e6e6e6"
                    font.pixelSize: 16
                    text: "+"
                }

                MouseArea {
                    id: addMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: panel.add()
                }
            }

            Flickable {
                id: savedView

                y: savedCaption.y + savedCaption.height + 8
                width: parent.width
                height: parent.height - y
                clip: true
                contentHeight: saved.height
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                KineticWheel {}

                Column {
                    id: saved

                    width: savedView.width
                    spacing: 2

                    Repeater {
                        model: Looks.looks

                        LookRow {
                            id: savedRow

                            required property var modelData

                            objectName: "lookRow:" + modelData.id
                            key: modelData.id
                            name: modelData.name
                            origin: Looks.live.origin === modelData.id
                            onMenuAsked: (x, y) => panel.showLookMenu(modelData, savedRow, x, y)
                        }
                    }

                    EmptyNote {
                        width: parent.width
                        visible: Looks.looks.length === 0
                        text: "No saved looks yet. The + keeps the live look as one."
                    }
                }
            }
        }

        Rectangle {
            id: listEdge

            x: list.x + list.width + 10
            y: 0
            width: 1
            height: parent.height
            color: "#3a3c42"
        }

        // ---- What the look that is picked gives each screen
        Item {
            id: detail

            anchors.left: listEdge.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            Text {
                objectName: "looksTitle"
                x: 18
                y: 16
                width: (makeLive.visible ? makeLive.x : save.visible ? save.x : close.x) - 30
                elide: Text.ElideRight
                color: "#f0f0f0"
                font.pixelSize: 16
                font.bold: true
                text: !panel.look ? "" : panel.onLive ? "Live: " + liveRow.name : panel.look.name
            }

            // Puts the window away, as Esc and a click outside it do. Drawn as the
            // operator window's own close button is.
            AppButton {
                id: close

                objectName: "looksClose"
                anchors.right: parent.right
                anchors.rightMargin: 10
                y: 10
                width: 34
                leftPadding: 0
                rightPadding: 0
                font.pixelSize: 15
                text: "✕"
                onClicked: panel.closed()
            }

            // What is done with the look that is picked, at the top right as in
            // ProPresenter. A saved look is made live. The live look is live already,
            // so it has no such button; once it has been changed it has Save in the
            // same place, which keeps it as the saved look it was made from.
            AppButton {
                id: makeLive

                objectName: "looksMakeLive"
                anchors.right: close.left
                anchors.rightMargin: 8
                y: 10
                text: "Make Live"
                visible: !panel.onLive && panel.look !== null
                onClicked: panel.makeLive()
            }

            AppButton {
                id: save

                objectName: "looksSave"
                anchors.right: close.left
                anchors.rightMargin: 8
                y: 10
                text: "Save"
                visible: panel.onLive && Looks.live.changed === true
                onClicked: panel.save()
            }

            Text {
                id: lead

                x: 18
                y: 46
                width: parent.width - 36
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 12
                text: panel.onLive
                      ? "What the screens are showing. A change here is seen at once, and is not a change to any saved look."
                        + (Looks.live.changed === true ? " It is no longer what the look it was made from is: Save keeps it as that look." : "")
                      : Looks.live.origin === panel.picked
                        ? "The look the live one was made from. A change here is to the saved look, and is not seen until it is made live again."
                        : "A saved look. Nothing changes on the screens until it is made live."
            }

            Flickable {
                id: tableView

                x: 18
                y: lead.y + lead.height + 12
                width: parent.width - 36
                height: parent.height - y - foot.height - 22
                clip: true
                contentWidth: table.width
                contentHeight: table.height
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.horizontal: ScrollBar {}
                ScrollBar.vertical: ScrollBar {}

                Column {
                    id: table

                    readonly property real labelWidth: 132
                    readonly property real columnWidth: 118
                    readonly property real rowHeight: 34

                    width: labelWidth + Math.max(1, panel.screens.length) * columnWidth

                    // The screens, across the top
                    Row {
                        height: 30

                        Item {
                            width: table.labelWidth
                            height: 1
                        }

                        Repeater {
                            model: panel.screens

                            Text {
                                required property var modelData

                                width: table.columnWidth
                                height: 30
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                color: "#e6e6e6"
                                font.pixelSize: 13
                                font.bold: true
                                text: modelData.name
                            }
                        }
                    }

                    Repeater {
                        model: panel.layers

                        Item {
                            id: layerRow

                            required property var modelData
                            required property int index

                            objectName: "lookLayer:" + modelData.key
                            width: table.width
                            height: table.rowHeight

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: "#34363c"
                            }

                            Text {
                                x: 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: layerRow.modelData.ours ? "#e6e6e6" : "#6c6f75"
                                font.pixelSize: 13
                                text: layerRow.modelData.label
                            }

                            Repeater {
                                model: panel.screens

                                Item {
                                    id: cell

                                    required property var modelData
                                    required property int index
                                    readonly property var line: panel.look ? panel.lineOf(modelData.id) : panel.everything

                                    objectName: "lookCell:" + layerRow.modelData.key + ":" + modelData.id
                                    x: table.labelWidth + index * table.columnWidth
                                    width: table.columnWidth
                                    height: table.rowHeight

                                    // A layer: on or off for this screen
                                    AppCheck {
                                        anchors.centerIn: parent
                                        visible: layerRow.modelData.kind === "switch"
                                        available: layerRow.modelData.ours
                                        checked: cell.line[layerRow.modelData.key] === true
                                        onToggled: (checked) => {
                                            if (layerRow.modelData.ours)
                                                panel.set(cell.modelData.id, { [layerRow.modelData.key]: checked })
                                        }
                                    }

                                    // The mask, which is not drawn here: none, or that there is one
                                    Nothing {
                                        anchors.centerIn: parent
                                        visible: layerRow.modelData.kind === "mask"
                                        opacity: 0.45
                                        ink: cell.line.mask !== "" ? "#c9cbd0" : "#8d9097"
                                    }

                                    // The theme: none, or the theme slide, by name; a click
                                    // gives the themes to choose from
                                    Rectangle {
                                        id: themeButton

                                        anchors.centerIn: parent
                                        width: parent.width - 12
                                        height: parent.height - 6
                                        radius: 5
                                        visible: layerRow.modelData.kind === "theme"
                                        color: themeMouse.pressed ? "#50535a" : themeMouse.containsMouse ? "#3a3c42" : "transparent"

                                        Nothing {
                                            anchors.centerIn: parent
                                            visible: cell.line.theme === ""
                                        }

                                        Row {
                                            anchors.centerIn: parent
                                            visible: cell.line.theme !== ""
                                            spacing: 5

                                            // The themes' own sign, as on the toolbar
                                            Shape {
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 18
                                                height: 14
                                                preferredRendererType: Shape.CurveRenderer

                                                ShapePath {
                                                    fillColor: "transparent"
                                                    strokeColor: panel.accentColor
                                                    strokeWidth: 1.5
                                                    joinStyle: ShapePath.RoundJoin

                                                    PathMultiline {
                                                        paths: [
                                                            [Qt.point(1.5, 3.5), Qt.point(13, 3.5), Qt.point(13, 12.5), Qt.point(1.5, 12.5), Qt.point(1.5, 3.5)],
                                                            [Qt.point(4.5, 1), Qt.point(16.5, 1), Qt.point(16.5, 9.5)],
                                                            [Qt.point(4, 7), Qt.point(10.5, 7)],
                                                            [Qt.point(4, 9.7), Qt.point(8.5, 9.7)]
                                                        ]
                                                    }
                                                }
                                            }

                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: Math.min(implicitWidth, themeButton.width - 32)
                                                elide: Text.ElideRight
                                                color: "#e6e6e6"
                                                font.pixelSize: 11
                                                text: panel.themeName(cell.line)
                                            }
                                        }

                                        MouseArea {
                                            id: themeMouse

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: panel.showThemeMenu(cell.modelData.id, themeButton)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#34363c"
                    }
                }
            }

            EmptyNote {
                anchors.centerIn: tableView
                width: tableView.width
                visible: panel.screens.length === 0
                text: "There are no audience screens. They are added under Settings, Screens."
            }

            Text {
                id: foot

                x: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                width: parent.width - 36
                wrapMode: Text.Wrap
                color: "#6c6f75"
                font.pixelSize: 11
                text: "The layers are in the order they lie on a screen. The greyed ones are ProPresenter's, which this app has not yet: "
                      + "what a look says of them is shown, and kept as it is."
            }
        }
    }
}
