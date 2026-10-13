import QtQuick
import SimplePresenterApp

// The arrangement editor: two thin rows over the slides, which the Arrangements button
// in the line over the slides opens and shuts (see SlideGrid.qml).
//
// An arrangement is an order to go through a presentation's groups in, in which a
// group may come up several times and need not come up at all (src/arrangementedit.h
// says how the file keeps one). With none chosen the groups run in the order the
// presentation has them, which ProPresenter calls Master.
//
//   - The top row is Master: every group of the presentation, in that order, each a
//     small rounded label in the group's colour. It is what there is to make
//     arrangements from, and is not changed here.
//   - The row under it begins with the picker of the arrangement being viewed (Master,
//     or one of the presentation's own), whose menu also makes a new one and opens the
//     list of them (ArrangementsPanel.qml). Beside it is that arrangement: the same
//     labels, in its order. For Master there is nothing there: Master is the row above.
//
// An arrangement is made by dragging. A group dragged down from the top row goes into
// the arrangement where it is let go, and a group of the arrangement is dragged along
// it to another place; a line shows where it would land. The pointer over a group of
// the arrangement brings up an ✕ on it, which takes that one coming up of the group
// out. Every change is saved in the presentation's file at once, and the slides below
// are laid out again as the arrangement now has them.
//
// It was made from the maintainer's description of ProPresenter's, which this follows,
// and from ProPresenter's help. GUESSES, where neither says: a group with no name is
// labelled "(no name)"; a drag let go anywhere but on the arrangement's row does
// nothing; and how a label looks while it is held is this app's own, after its lists.
//
// Nothing of this is made until the button is first pressed (it is behind a Loader),
// and what is shown only while something is held is made only then.
Item {
    id: bar

    // The operator window
    required property var win
    readonly property var document: win.document
    // Master: the presentation's groups, as the file has them
    readonly property var groups: document && document.groups ? document.groups : []
    // The arrangement being viewed, as the presentation's list of them has it; null for
    // Master
    readonly property var arrangement: document && document.arrangement !== "" && document.arrangementList
                                       ? document.arrangementList.find(candidate => candidate.name === document.arrangement) ?? null : null
    // What it goes through, a group for each place; null for a place whose group the
    // presentation no longer has, which is shown too, so that a place here is the same
    // place in the file
    readonly property var run: arrangement ? arrangement.groups.map(id => bar.groups.find(group => group.id === id) ?? null) : []

    // The colour a group's label has: the colour its slides' frames have in the grid
    // below, which is the app's own for a group of that name where it has one, and
    // failing that the colour the presentation's file gives the group (see
    // groupColor() and configuredGroup() in Main.qml). A group with neither is grey,
    // as is a place whose group is gone.
    function colourOf(group) {
        if (group === null)
            return "#5a5d66"
        const known = win.configuredGroup({ group: group.name })
        return known ? known.color : group.color !== "" ? group.color : "#5a5d66"
    }

    readonly property real rowHeight: 26
    // Where the labels of both rows begin: after the picker, and the caption over it
    readonly property real labelsX: 168
    readonly property color accentColor: "#ff8a1f"

    // What is being dragged: { from, place, group }, where `from` is "master" or
    // "run", and `place` is where in the arrangement it is, for one of the
    // arrangement's own; null while nothing is. Where the pointer has it, as a point
    // of this bar, and where the press that may become a drag was. And where it would
    // land: the place of the arrangement it would go before, or the number of places
    // for after the last; -1 for nowhere.
    property var carried: null
    property point carriedAt
    property point pressedAt
    // Where in the label the press was: it is carried by that point of it, as a thing
    // picked up is
    property point pressedIn
    property int landing: -1
    // Which way the arrangement's row is going under something held near one of its
    // ends, to bring what is beyond into reach, in points a second (rightwards counted
    // up); 0 where it is not held near either, or there is no further to go.
    readonly property real carriedScroll: {
        if (carried === null || landing < 0)
            return 0
        const edge = 30
        const furthest = Math.max(0, runRow.contentWidth - runRow.width)
        const x = carriedAt.x - runRow.x
        if (x > runRow.width - edge && runRow.contentX < furthest)
            return 380
        if (x < edge && runRow.contentX > 0)
            return -380
        return 0
    }

    // A press on a label, which may become a drag of it: at a point of the bar, which
    // is this point of the label.
    function press(at, within) {
        pressedAt = at
        pressedIn = within
    }

    // A label moved with the button down, to this point of the bar. Once that is a
    // little way from where it was pressed it is being dragged.
    function drag(from, place, group, at) {
        if (carried === null && Math.abs(at.x - pressedAt.x) + Math.abs(at.y - pressedAt.y) <= 8)
            return
        if (carried === null)
            carried = { from: from, place: place, group: group }
        carriedAt = at
        landing = landingAt(at)
    }

    // The gap of the arrangement's row that a point of the bar is nearest, counted in
    // its places; -1 for a point that is not on that row, or while Master is viewed,
    // which has no row to land on.
    function landingAt(at) {
        if (arrangement === null)
            return -1
        if (at.y < runRow.y - 6 || at.y > runRow.y + runRow.height + 12 || at.x < runRow.x - 24 || at.x > runRow.x + runRow.width + 24)
            return -1
        const x = at.x - runRow.x + runRow.contentX - runPills.x
        let gap = 0
        for (let i = 0; i < runLabels.count; ++i) {
            const label = runLabels.itemAt(i)
            if (label && x > label.x + label.width / 2)
                gap = i + 1
        }
        return gap
    }

    // Where the line for a landing is drawn, along the arrangement's row: in the
    // middle of the gap, or at the very start
    function landingX(gap) {
        if (runLabels.count === 0 || gap <= 0)
            return 0
        const before = runLabels.itemAt(Math.min(gap, runLabels.count) - 1)
        return before ? runPills.x + before.x + before.width + 1.5 : 0
    }

    // It has been let go: a group from Master goes into the arrangement there, and one
    // of the arrangement's own moves there.
    function release() {
        const held = carried
        const gap = landing
        letGo()
        if (held === null || gap < 0)
            return
        if (held.from === "master")
            win.arrangementInsert(held.group.id, gap)
        else
            win.arrangementMove(held.place, gap)
    }

    function letGo() {
        carried = null
        landing = -1
    }

    function showPicker(item) {
        const entry = win.currentEntry()
        // (Under a caption, as the same rows are in the menu of a presentation's name:
        // a menu with a caption sets its rows in, which is where the tick of the one
        // viewed goes. Without it the tick was on the first letter of the name.)
        const items = [{ header: "Arrangement" },
                       { label: "Master", current: arrangement === null, run: () => win.setArrangement(entry, "") }]
        for (const candidate of document.arrangementList) {
            items.push({ label: candidate.name, current: arrangement !== null && arrangement.id === candidate.id,
                         run: () => win.setArrangement(entry, candidate.name) })
        }
        items.push({ divider: true })
        items.push({ label: "New Arrangement", run: () => win.newArrangement() })
        items.push({ label: "Edit Arrangements…", run: () => win.openArrangements() })
        win.showMenu(items, item)
    }

    height: 2 * rowHeight + 6

    // The presentation has changed under what was held (another was opened, or the
    // file was changed from elsewhere): it is let go of, and nothing is moved.
    onRunChanged: letGo()
    onGroupsChanged: letGo()

    // A group's label: a small rounded rectangle in the group's colour, with its name
    component GroupLabel: Rectangle {
        id: groupLabel

        // { id, name, color, slides }; null for a place whose group is gone
        property var group: null
        // The colour it is drawn in: see colourOf()
        property color ground: "#5a5d66"
        // Dark words on a light colour, and light ones on a dark
        readonly property bool pale: 0.299 * ground.r + 0.587 * ground.g + 0.114 * ground.b > 0.6
        readonly property string name: group === null ? "(missing group)" : group.name !== "" ? group.name : "(no name)"

        width: words.width + 18
        height: 20
        radius: 6
        color: ground

        Text {
            id: words

            anchors.centerIn: parent
            width: Math.min(implicitWidth, 150)
            elide: Text.ElideRight
            color: groupLabel.pale ? "#1b1c1f" : "white"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            text: groupLabel.name
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: 6
        radius: 6
        color: "#25272b"
    }

    // ---- Master
    Text {
        x: 10
        y: (bar.rowHeight - height) / 2
        color: bar.win.dimTextColor
        font.pixelSize: 11
        font.bold: true
        font.capitalization: Font.AllUppercase
        text: "Groups"
    }

    Flickable {
        id: masterRow

        objectName: "arrangementMaster"
        x: bar.labelsX
        width: bar.width - bar.labelsX - 8
        height: bar.rowHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick
        contentWidth: masterLabels.width + 4
        contentHeight: height

        Row {
            id: masterLabels

            x: 2
            y: 3
            spacing: 5

            Repeater {
                model: bar.groups

                delegate: GroupLabel {
                    id: masterLabel

                    required property var modelData
                    required property int index

                    objectName: "arrangementGroup"
                    group: modelData
                    ground: bar.colourOf(modelData)

                    // It can be dragged down into the arrangement. (The row would
                    // otherwise take the drag for itself, to scroll by.)
                    MouseArea {
                        anchors.fill: parent
                        preventStealing: true
                        onPressed: (mouse) => bar.press(mapToItem(bar, mouse.x, mouse.y), Qt.point(mouse.x, mouse.y))
                        onPositionChanged: (mouse) => {
                            if (pressed)
                                bar.drag("master", -1, masterLabel.modelData, mapToItem(bar, mouse.x, mouse.y))
                        }
                        onReleased: bar.release()
                        onCanceled: bar.letGo()
                    }
                }
            }
        }

        // A wheel turned over a row moves along it.
        WheelHandler {
            onWheel: (wheel) => {
                const furthest = Math.max(0, masterRow.contentWidth - masterRow.width)
                masterRow.contentX = Math.max(0, Math.min(furthest, masterRow.contentX - (wheel.angleDelta.y + wheel.angleDelta.x) / 2))
            }
        }
    }

    // ---- The arrangement being viewed
    Rectangle {
        id: picker

        objectName: "arrangementPicker"
        x: 8
        y: bar.rowHeight + (bar.rowHeight - height) / 2
        width: bar.labelsX - 18
        height: 22
        radius: 6
        color: pickerMouse.pressed ? "#50535a" : pickerMouse.containsMouse ? "#45484e" : "#3a3c42"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: "#e6e6e6"
            font.pixelSize: 12
            text: bar.arrangement ? bar.arrangement.name : "Master"
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#9a9da3"
            font.pixelSize: 10
            text: "▾"
        }

        MouseArea {
            id: pickerMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                bar.win.takeFocus()
                bar.showPicker(picker)
            }
        }
    }

    Flickable {
        id: runRow

        objectName: "arrangementRun"
        x: bar.labelsX
        y: bar.rowHeight
        width: bar.width - bar.labelsX - 8
        height: bar.rowHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick
        contentWidth: runPills.width + 4
        contentHeight: height

        Row {
            id: runPills

            x: 2
            y: 3
            spacing: 5

            Repeater {
                id: runLabels

                model: bar.run

                delegate: GroupLabel {
                    id: runLabel

                    required property var modelData
                    required property int index

                    objectName: "arrangementPlace"
                    group: modelData
                    ground: bar.colourOf(modelData)
                    // The one being dragged is fainter where it was, until it is let go.
                    opacity: bar.carried !== null && bar.carried.from === "run" && bar.carried.place === index ? 0.35 : 1

                    MouseArea {
                        id: runMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        preventStealing: true
                        onPressed: (mouse) => bar.press(mapToItem(bar, mouse.x, mouse.y), Qt.point(mouse.x, mouse.y))
                        onPositionChanged: (mouse) => {
                            if (pressed)
                                bar.drag("run", runLabel.index, runLabel.modelData, mapToItem(bar, mouse.x, mouse.y))
                        }
                        onReleased: bar.release()
                        onCanceled: bar.letGo()
                    }

                    // Under the pointer it has an ✕, which takes this coming up of
                    // the group out of the arrangement.
                    Rectangle {
                        objectName: "arrangementTakeOut"
                        anchors.right: parent.right
                        anchors.rightMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        radius: 8
                        visible: bar.carried === null && (runMouse.containsMouse || outMouse.containsMouse)
                        color: outMouse.containsMouse ? "#e0000000" : "#a0000000"

                        Text {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: -0.5
                            color: "white"
                            font.pixelSize: 9
                            font.bold: true
                            text: "✕"
                        }

                        MouseArea {
                            id: outMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: bar.win.arrangementTakeOut(runLabel.index)
                        }
                    }
                }
            }
        }

        WheelHandler {
            onWheel: (wheel) => {
                const furthest = Math.max(0, runRow.contentWidth - runRow.width)
                runRow.contentX = Math.max(0, Math.min(furthest, runRow.contentX - (wheel.angleDelta.y + wheel.angleDelta.x) / 2))
            }
        }
    }

    // Master has no row of its own here: it is the row above. While a group is being
    // dragged with Master viewed, this says why there is nowhere to put it.
    Text {
        x: bar.labelsX + 4
        y: bar.rowHeight + (bar.rowHeight - height) / 2
        width: runRow.width - 8
        visible: bar.arrangement === null && bar.carried !== null
        elide: Text.ElideRight
        color: bar.win.dimTextColor
        font.pixelSize: 12
        text: "Master cannot be changed here. Pick an arrangement, or make one with New Arrangement, to put groups in it."
    }

    // What is shown while something is held, and what goes on then: made when a label
    // is dragged and gone when it is let go.
    Loader {
        anchors.fill: parent
        z: 10
        active: bar.carried !== null
        sourceComponent: Item {
            // The label itself, under the pointer, held by the point of it that was
            // pressed: as a thing picked up is.
            GroupLabel {
                objectName: "arrangementCarried"
                x: bar.carriedAt.x - bar.pressedIn.x
                y: bar.carriedAt.y - bar.pressedIn.y
                group: bar.carried ? bar.carried.group : null
                ground: bar.colourOf(bar.carried ? bar.carried.group : null)
                opacity: 0.85
                border.width: 1
                border.color: bar.accentColor
            }

            // Where it would land: a line in the gap between two groups of the
            // arrangement, before the first or after the last. It is drawn over the
            // label that is held, which would otherwise be on top of it, and only
            // where the arrangement's row is.
            Rectangle {
                objectName: "arrangementLanding"
                x: runRow.x + bar.landingX(bar.landing) - runRow.contentX
                y: runRow.y + 1
                width: 2
                height: bar.rowHeight - 2
                radius: 1
                visible: bar.landing >= 0 && x >= runRow.x - 1 && x <= runRow.x + runRow.width
                color: bar.accentColor
            }

            // Moves the arrangement's row along under what is held near one of its
            // ends, by the time that has really passed; and where it would land, the
            // pointer not having moved, is worked out again each time. Nothing runs
            // while nothing is held there.
            Timer {
                property real last: 0

                interval: 16
                repeat: true
                running: bar.carriedScroll !== 0
                onRunningChanged: last = Date.now()
                onTriggered: {
                    const now = Date.now()
                    const furthest = Math.max(0, runRow.contentWidth - runRow.width)
                    runRow.contentX = Math.max(0, Math.min(furthest, runRow.contentX + bar.carriedScroll * Math.min(100, now - last) / 1000))
                    last = now
                    bar.landing = bar.landingAt(bar.carriedAt)
                }
            }
        }
    }
}
