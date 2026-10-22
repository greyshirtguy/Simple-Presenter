import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import SimplePresenterApp

// The Calendar: the workspace's events, each coming due at a date and time, once or every
// week on chosen days, and running its action then (see CalendarEvents and
// src/calendarfile.h). A window of its own, as ProPresenter's is (the maintainer's
// choice, 2026-10-22), laid out as ProPresenter's, from the maintainer's pictures of it:
//
//   - on the left, the list: "Calendar Enabled" over it; the events, each with its name
//     and when ("27 Oct 2026 at 7:00:00 pm"), those whose time has come red; at its foot,
//     + for a new event, the pin, which takes the list to now, and the arrows going
//     round, which show each time a repeating event comes due (under Current, the last
//     time each came due, and under Upcoming, those to come) or each event once.
//   - on the right, the event picked: its name, its Description, then Schedule (when it
//     starts, the days of the week it repeats on, and when it ends), and Action (what it
//     runs: a playlist or one of its presentations, which goes live on its first slide,
//     or a macro), with Test, which runs it now.
//
// Every change is written at once, to ProPresenter's own file. A right click on an event
// has Delete; so has Delete, with the list picked. Typed dates and times are read as the
// list writes them (CalendarEvents.withDate and withTime), and one that is no date or
// time is said and left as it was.
//
// GUESSES at ProPresenter, where its pictures do not say: the words of the Action menu
// (None, Playlists, Macros, and Capture, which is shown and not offered, this app having
// nothing of it), the whole playlist being one of its choices, and where Delete is;
// "ends on", ticked, starting a year after the event's start, as the file looked at had
// it; and "ends on" being for an event that repeats.
Window {
    id: calendar

    // The operator window: its catalog, its log, and what runs an event
    required property var owner

    // The event picked, by its id, and which of its times was clicked in the list
    property string picked: ""
    property real pickedAt: 0
    // Each time an event comes due, or each event once
    property bool recurring: remembered.recurring
    // The event picked as the calendar now has it, and whether there is one
    readonly property var event: CalendarEvents.events.length >= 0 && picked !== "" ? CalendarEvents.find(picked) : ({})
    readonly property bool has: event.id !== undefined
    readonly property bool repeats: has && event.days.length > 0
    readonly property var action: has ? event.action : ({ kind: "none" })
    // The list's rows, asked again as the calendar changes and as the clock is looked at.
    // (What is read is used: a bare read of a property, for its signal alone, is left
    // out by the compiler, and the list was then never asked again.)
    readonly property var rows: CalendarEvents.tick >= 0 && CalendarEvents.events.length >= 0 ? CalendarEvents.listing(recurring) : []
    readonly property var days: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]
    // Its menu, for scripts to read
    readonly property alias popup: menu

    function report(error) {
        return owner.report(error)
    }

    function change(changes) {
        if (has)
            report(CalendarEvents.change(event.id, changes))
    }

    function addEvent() {
        const made = CalendarEvents.add()
        if (!report(made.error))
            return
        picked = made.id
        pickedAt = CalendarEvents.find(made.id).start
        Log.note("calendar", "a new event, \"New Event\"")
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    function deleteEvent(id, item, x, y) {
        const target = CalendarEvents.find(id)
        menu.show([
            { note: "“" + target.name + "” will be deleted from the calendar. That cannot be undone." },
            { label: "Delete", danger: true, run: () => {
                if (report(CalendarEvents.remove(id))) {
                    Log.note("calendar", "the event \"" + target.name + "\" deleted")
                    if (picked === id)
                        picked = ""
                }
            } },
            { label: "Cancel", run: () => {} }
        ], item, x, y)
    }

    // The list taken to now: to the first time to come, or the last that has come
    function toNow() {
        let index = rows.findIndex(row => !row.past)
        if (index < 0)
            index = rows.length - 1
        if (index >= 0)
            list.positionViewAtIndex(index, ListView.Beginning)
    }

    // "Ends on", ticked: a year after it starts, on the clock
    function aYearOn(start) {
        const date = new Date(start * 1000)
        date.setFullYear(date.getFullYear() + 1)
        return Math.round(date.getTime() / 1000)
    }

    // What the event runs, in words, by the names the workspace has for it now, or else
    // those the file has
    function actionWords(action) {
        if (action.kind === "playlist") {
            const node = owner.catalog.playlists.find(p => p.path === action.playlistId)
            const playlist = node ? node.name : action.name || "A playlist not here"
            const item = action.itemId !== "" ? owner.catalog.playlistItems(action.playlistId).find(r => r.path === action.itemId) : null
            return action.itemId === "" ? playlist : playlist + " › " + (item ? item.name : "a presentation not here")
        }
        if (action.kind === "macro") {
            const macro = Macros.find(action.macroId, action.macroName)
            return macro && macro.name ? macro.name : action.macroName || action.name
        }
        return action.kind === "capture" ? "Capture" : action.kind === "other" ? "Something this app has not" : "None"
    }

    function setAction(asked, words) {
        change({ action: asked })
        Log.note("calendar", "the event \"" + event.name + "\" given the action " + words)
    }

    // The Action menu's Playlists: the folders and playlists of the workspace, each
    // playlist leading to the whole of it and to each of its presentations
    function playlistRows(parent) {
        return owner.catalog.playlists.filter(node => node.parent === parent).map(node => node.folder
            ? { label: node.name, items: () => playlistRows(node.path) }
            : { label: node.name, current: action.kind === "playlist" && action.playlistId === node.path, items: () => presentationRows(node) })
    }

    function presentationRows(node) {
        const chosen = action.kind === "playlist" && action.playlistId === node.path
        const rows = owner.catalog.playlistItems(node.path).filter(row => row.kind === "presentation")
        return [{ header: node.name },
                { label: "The Whole Playlist", current: chosen && action.itemId === "",
                  run: () => setAction({ kind: "playlist", playlistId: node.path, playlistName: node.name, itemId: "" }, "the playlist \"" + node.name + "\"") },
                { divider: true }].concat(rows.map(row => ({
                    label: row.name, current: chosen && action.itemId === row.path,
                    run: () => setAction({ kind: "playlist", playlistId: node.path, playlistName: node.name, itemId: row.path },
                                         "\"" + row.name + "\" of the playlist \"" + node.name + "\"") })))
    }

    function macroRows(collection) {
        return [{ header: collection.name }].concat(collection.macros.map(macro => ({
            label: macro.name, glyph: "macro", current: action.kind === "macro" && action.macroId === macro.id,
            run: () => setAction({ kind: "macro", macroId: macro.id, macroName: macro.name, collectionId: collection.id,
                                   collectionName: collection.id === "" ? "" : collection.name }, "the macro \"" + macro.name + "\"") })))
    }

    function showActionMenu(item) {
        const collections = Macros.collections.filter(collection => collection.macros.length > 0)
        menu.show([
            { header: "Action" },
            { label: "None", current: action.kind === "none", run: () => setAction({ kind: "none" }, "None") },
            { label: "Playlists", disabled: owner.catalog.playlists.length === 0, items: () => playlistRows("") },
            { label: "Macros", disabled: collections.length === 0,
              items: () => collections.length === 1 ? macroRows(collections[0])
                                                    : collections.map(collection => ({ label: collection.name, items: () => macroRows(collection) })) },
            { label: "Capture", current: action.kind === "capture", disabled: true }
        ], item)
    }

    title: "Calendar"
    width: remembered.width
    height: remembered.height
    minimumWidth: 640
    minimumHeight: 380
    color: "#1e1f23"
    flags: Qt.Window
    // In front of the operator window, as its other windows are
    transientParent: owner

    onWidthChanged: if (visible) remembered.width = width
    onHeightChanged: if (visible) remembered.height = height
    onRecurringChanged: remembered.recurring = recurring
    onVisibleChanged: if (visible) Qt.callLater(toNow)

    Settings {
        id: remembered

        category: "calendarWindow"
        property real width: 780
        property real height: 470
        property bool recurring: true
    }

    PopupMenu {
        id: menu

        onClosed: list.forceActiveFocus()
    }

    // Delete, with an event picked and nothing being typed
    Shortcut {
        sequences: [StandardKey.Delete, "Backspace"]
        enabled: calendar.has && !(calendar.activeFocusItem && calendar.activeFocusItem.cursorPosition !== undefined)
        onActivated: calendar.deleteEvent(calendar.picked, list, list.width / 2, 40)
    }

    // -------- The list
    Rectangle {
        id: side

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 250
        color: "#26272c"

        AppCheck {
            objectName: "calendarEnabled"
            anchors.right: parent.right
            anchors.rightMargin: 12
            y: 10
            text: "Calendar Enabled"
            checked: CalendarEvents.enabled
            onToggled: (checked) => calendar.report(CalendarEvents.setEnabled(checked))
        }

        ListView {
            id: list

            objectName: "calendarList"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: foot.top
            anchors.topMargin: 40
            clip: true
            focus: true
            boundsBehavior: Flickable.StopAtBounds
            model: calendar.rows

            ScrollBar.vertical: ScrollBar {}

            section.property: "section"
            section.delegate: Item {
                required property string section

                width: ListView.view.width
                height: section === "" ? 0 : 34

                Text {
                    x: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    visible: parent.section !== ""
                    color: "#c9cbd0"
                    font.pixelSize: 14
                    text: parent.section === "current" ? "Current" : "Upcoming"
                }
            }

            delegate: Rectangle {
                id: row

                required property var modelData
                required property int index
                readonly property bool chosen: modelData.id === calendar.picked && modelData.at === calendar.pickedAt

                objectName: "calendarRow:" + index
                width: ListView.view.width
                height: 48
                // Picked blue; come due red, as ProPresenter marks them
                color: chosen ? "#1f5fa8" : modelData.past ? "#6b2a2f" : rowMouse.containsMouse ? "#2e3035" : "transparent"

                Text {
                    x: 16
                    y: 6
                    width: parent.width - 28
                    elide: Text.ElideRight
                    color: "#f0f0f0"
                    font.pixelSize: 15
                    text: row.modelData.name !== "" ? row.modelData.name : "(no name)"
                }

                Text {
                    x: 16
                    y: 28
                    color: "#c0c2c8"
                    font.pixelSize: 11
                    text: row.modelData.when
                }

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        calendar.picked = row.modelData.id
                        calendar.pickedAt = row.modelData.at
                        list.forceActiveFocus()
                        if (mouse.button === Qt.RightButton)
                            menu.show([
                                { header: row.modelData.name },
                                { label: "Delete Event…", danger: true, run: () => calendar.deleteEvent(row.modelData.id, row, mouse.x, mouse.y) }
                            ], row, mouse.x, mouse.y)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#8d9097"
                font.pixelSize: 13
                text: CalendarEvents.events.length === 0 ? "No events. Add one with the + below." : "Nothing still to come."
            }
        }

        // + at the left; the pin and the arrows going round at the right
        Rectangle {
            id: foot

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 34
            color: "#222327"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#34363c"
            }

            component FootButton: Rectangle {
                id: footButton

                property bool lit: false

                signal clicked

                width: 30
                height: 26
                radius: 5
                color: footMouse.containsMouse ? "#3a3c42" : "transparent"

                MouseArea {
                    id: footMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: footButton.clicked()
                }
            }

            FootButton {
                objectName: "calendarAdd"
                x: 6
                anchors.verticalCenter: parent.verticalCenter
                onClicked: calendar.addEvent()

                Text {
                    anchors.centerIn: parent
                    color: "#d6d8dc"
                    font.pixelSize: 20
                    text: "+"
                }
            }

            // The pin: the list taken to now
            FootButton {
                objectName: "calendarNow"
                anchors.right: recurringButton.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                onClicked: calendar.toNow()

                Rectangle {
                    x: 10
                    y: 4
                    width: 10
                    height: 10
                    radius: 5
                    color: "transparent"
                    border.width: 1.5
                    border.color: "#d6d8dc"
                }

                Rectangle {
                    x: 14.2
                    y: 13
                    width: 1.6
                    height: 8
                    radius: 0.8
                    color: "#d6d8dc"
                }
            }

            // The arrows going round: each time a repeating event comes due, or not,
            // blue while they are shown
            FootButton {
                id: recurringButton

                objectName: "calendarRecurring"
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                onClicked: calendar.recurring = !calendar.recurring

                Rectangle {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    radius: 9
                    color: calendar.recurring ? "#2f7fe0" : "transparent"
                    border.width: 2
                    border.color: calendar.recurring ? "#2f7fe0" : "#d6d8dc"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 8
                        height: 8
                        radius: 4
                        color: calendar.recurring ? "#26272c" : "#2f7fe0"
                    }
                }
            }
        }
    }

    // -------- The event picked
    Flickable {
        anchors.left: side.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        contentHeight: details.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        visible: calendar.has

        Column {
            id: details

            width: parent.width
            spacing: 0

            Column {
                x: 20
                width: parent.width - 40
                topPadding: 16
                bottomPadding: 12
                spacing: 4

                AppTextField {
                    id: nameField

                    objectName: "calendarName"
                    width: parent.width
                    height: 36
                    font.pixelSize: 20
                    font.bold: true
                    placeholderText: "Name"
                    text: calendar.has ? calendar.event.name : ""
                    onEditingFinished: {
                        if (calendar.has && text !== calendar.event.name)
                            calendar.change({ name: text })
                        text = Qt.binding(() => calendar.has ? calendar.event.name : "")
                    }
                }

                AppTextField {
                    objectName: "calendarDescription"
                    width: parent.width
                    height: 30
                    font.pixelSize: 13
                    placeholderText: "Description"
                    text: calendar.has ? calendar.event.description : ""
                    onEditingFinished: {
                        if (calendar.has && text !== calendar.event.description)
                            calendar.change({ description: text })
                        text = Qt.binding(() => calendar.has ? calendar.event.description : "")
                    }
                }
            }

            // A date, or a time of day, typed: `at` is the moment shown, and `moved`
            // says the one typed
            component Moment: AppTextField {
                id: moment

                property real at: 0
                property bool day: true

                signal moved(real at)

                function shown() {
                    return at > 0 ? (day ? CalendarEvents.dateText(at) : CalendarEvents.timeText(at)) : ""
                }

                width: day ? 112 : 112
                height: 28
                font.pixelSize: 13
                text: shown()
                onEditingFinished: {
                    const typed = text
                    const now = day ? CalendarEvents.withDate(at, typed) : CalendarEvents.withTime(at, typed)
                    text = Qt.binding(() => moment.shown())
                    if (now < 0)
                        calendar.report("“" + typed + "” is not " + (day ? "a date" : "a time of day"))
                    else if (now !== at)
                        moved(now)
                }
            }

            // -------- Schedule
            Rectangle {
                width: parent.width
                height: schedule.height + 24
                color: "#2a2c31"

                Column {
                    id: schedule

                    x: 20
                    y: 12
                    width: parent.width - 40
                    spacing: 10

                    Text {
                        color: "#eeeeee"
                        font.pixelSize: 18
                        text: "Schedule"
                    }

                    Row {
                        spacing: 8

                        Text {
                            width: 58
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            color: "#c9cbd0"
                            font.pixelSize: 13
                            text: "starts:"
                        }

                        Moment {
                            objectName: "calendarStartDate"
                            at: calendar.has ? calendar.event.start : 0
                            onMoved: (at) => calendar.change({ start: at })
                        }

                        Moment {
                            objectName: "calendarStartTime"
                            day: false
                            at: calendar.has ? calendar.event.start : 0
                            onMoved: (at) => calendar.change({ start: at })
                        }
                    }

                    Row {
                        spacing: 8

                        Text {
                            width: 58
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            color: "#c9cbd0"
                            font.pixelSize: 13
                            text: "repeat:"
                        }

                        // The days of the week, each on or off, as ProPresenter's buttons
                        Rectangle {
                            width: dayRow.width
                            height: 28
                            radius: 6
                            color: "#3a3c42"

                            Row {
                                id: dayRow

                                Repeater {
                                    model: 7

                                    delegate: Rectangle {
                                        id: dayButton

                                        required property int index
                                        readonly property int number: index + 1
                                        readonly property bool chosen: calendar.has && calendar.event.days.includes(number)

                                        objectName: "calendarDay:" + number
                                        width: 46
                                        height: 28
                                        radius: 6
                                        color: chosen ? "#2f7fe0" : dayMouse.containsMouse ? "#45484e" : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            color: dayButton.chosen ? "white" : "#e6e6e6"
                                            font.pixelSize: 13
                                            text: calendar.days[dayButton.index]
                                        }

                                        MouseArea {
                                            id: dayMouse

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                const days = calendar.event.days.filter(d => d !== dayButton.number)
                                                calendar.change({ days: dayButton.chosen ? days : days.concat([dayButton.number]) })
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Row {
                        spacing: 8
                        opacity: calendar.repeats ? 1 : 0.45
                        enabled: calendar.repeats

                        Item {
                            width: 58
                            height: 1
                        }

                        AppCheck {
                            objectName: "calendarEnds"
                            anchors.verticalCenter: parent.verticalCenter
                            text: "ends on:"
                            checked: calendar.has && calendar.event.ends > 0
                            available: calendar.repeats
                            onToggled: (checked) => calendar.change({ ends: checked ? calendar.aYearOn(calendar.event.start) : 0 })
                        }

                        Moment {
                            objectName: "calendarEndDate"
                            visible: calendar.has && calendar.event.ends > 0
                            at: calendar.has ? calendar.event.ends : 0
                            onMoved: (at) => calendar.change({ ends: at })
                        }

                        Moment {
                            objectName: "calendarEndTime"
                            day: false
                            visible: calendar.has && calendar.event.ends > 0
                            at: calendar.has ? calendar.event.ends : 0
                            onMoved: (at) => calendar.change({ ends: at })
                        }
                    }
                }
            }

            // -------- Action
            Column {
                x: 20
                width: parent.width - 40
                topPadding: 16
                spacing: 10

                Text {
                    color: "#eeeeee"
                    font.pixelSize: 18
                    text: "Action"
                }

                Row {
                    x: 66
                    spacing: 10

                    // What it runs: a click has the choices
                    Rectangle {
                        id: actionChoice

                        objectName: "calendarAction"
                        width: 260
                        height: 30
                        radius: 6
                        color: actionMouse.containsMouse ? "#45484e" : "#3a3c42"

                        ActionGlyph {
                            id: actionGlyph

                            x: 10
                            anchors.verticalCenter: parent.verticalCenter
                            visible: calendar.action.kind === "macro"
                            kind: "macro"
                            ink: "#b48cff"
                        }

                        RowIcon {
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            visible: calendar.action.kind === "playlist"
                            kind: "playlist"
                        }

                        Text {
                            objectName: "calendarActionWords"
                            anchors.left: parent.left
                            anchors.leftMargin: calendar.action.kind === "macro" || calendar.action.kind === "playlist" ? 32 : 12
                            anchors.right: chevron.left
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            color: "#e6e6e6"
                            font.pixelSize: 13
                            text: calendar.has ? calendar.actionWords(calendar.action) : ""
                        }

                        Text {
                            id: chevron

                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#9a9da3"
                            font.pixelSize: 11
                            text: "▾"
                        }

                        MouseArea {
                            id: actionMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: calendar.showActionMenu(actionChoice)
                        }
                    }

                    AppButton {
                        objectName: "calendarTest"
                        width: 110
                        height: 30
                        text: "Test"
                        enabled: calendar.action.kind === "playlist" || calendar.action.kind === "macro"
                        opacity: enabled ? 1 : 0.45
                        onClicked: calendar.owner.runCalendarEvent(calendar.event, true)
                    }
                }
            }
        }
    }

    Text {
        anchors.left: side.right
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: !calendar.has
        horizontalAlignment: Text.AlignHCenter
        color: "#8d9097"
        font.pixelSize: 14
        text: "Pick an event in the list, or add one with the +."
    }
}
