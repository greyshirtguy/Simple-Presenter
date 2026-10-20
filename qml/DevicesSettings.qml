import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// Settings → Devices: the workspace's communication devices (src/devices.h), as
// ProPresenter's Devices page has them. A row for each, with its state (a green mark
// when connected), what it is, and Connect; a click on it opens what it is set to. Under
// the list, + adds a device of one of the three kinds there are here (MIDI, RossTalk,
// HTTP Requests), and ProPresenter's own devices for the workspace that are not here yet
// are offered, to be added with their names, so that what ProPresenter's actions name
// is found. MIDI Map opens the map of the notes that work the show, as ProPresenter's
// does, with Auto Fill and the last note heard.
//
// Every change is kept at once (Devices keeps them for this computer, for the
// workspace). Nothing here is ProPresenter's file: that is only read.
Item {
    id: page

    // Shows a menu (see PopupMenu) by an item
    property var showMenu: (rows, item, x, y) => {}
    // The MIDI Map's commands that are not done here, by key (see MidiControl)
    property var notHere: []
    // The device being looked at, by id, "" for the list; and whether the map is open
    property string editing: ""
    property bool mapOpen: false
    // The note Auto Fill starts from, as typed
    property int firstNote: 0

    readonly property var device: editing === "" ? null : Devices.connections.find(c => c.id === editing) ?? null

    function summary(device) {
        if (device.kind === "midi") {
            const parts = []
            if (device.sources.length > 0)
                parts.push("hears " + device.sources.join(", "))
            if (device.destinations.length > 0)
                parts.push("sends to " + device.destinations.join(", "))
            return parts.length > 0 ? parts.join("; ") : "no ports chosen"
        }
        if (device.kind === "rosstalk")
            return (device.address !== "" ? device.address : "no address") + ":" + device.port
        return device.baseUrl
    }

    function stateColor(device) {
        return device.state === "connected" || device.state === "ready" ? "#43a047" : device.state === "connecting" ? "#ffb300" : "#5c5f66"
    }

    // A port's tick in a device's list of sources or destinations, given or taken
    function toggled(list, name, on) {
        const now = list.filter(n => n !== name)
        if (on)
            now.push(name)
        return now
    }

    // ---- The list of devices
    Flickable {
        anchors.fill: parent
        visible: page.editing === "" && !page.mapOpen
        contentHeight: listColumn.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: listColumn

            width: parent.width - 16
            spacing: 8

            Repeater {
                model: Devices.connections

                delegate: Rectangle {
                    id: row

                    required property var modelData

                    objectName: "deviceRow:" + modelData.name
                    width: listColumn.width
                    height: 58
                    radius: 8
                    color: rowMouse.containsMouse ? "#383b41" : "#313338"

                    MouseArea {
                        id: rowMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: page.editing = row.modelData.id
                    }

                    // Connected, connecting, or not
                    Rectangle {
                        x: 0
                        width: 4
                        height: parent.height
                        radius: 2
                        color: page.stateColor(row.modelData)
                    }

                    Column {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 220
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
                            color: row.modelData.problem !== "" ? "#ff8a80" : "#9a9da3"
                            font.pixelSize: 12
                            text: row.modelData.kindName + "   " + (row.modelData.problem !== "" ? row.modelData.problem : page.summary(row.modelData))
                        }
                    }

                    AppButton {
                        objectName: "deviceConnect:" + row.modelData.name
                        anchors.right: chevron.left
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 110
                        height: 28
                        font.pixelSize: 13
                        visible: row.modelData.kind !== "http"
                        text: row.modelData.connected ? "Disconnect" : "Connect"
                        onClicked: Devices.setConnected(row.modelData.id, !row.modelData.connected)
                    }

                    Text {
                        anchors.right: chevron.left
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.modelData.kind === "http"
                        color: "#9a9da3"
                        font.pixelSize: 12
                        text: "Ready"
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
                visible: Devices.connections.length === 0
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "No devices yet. A device is something outside the app that a slide or a macro can send to: "
                      + "notes to a MIDI port, a Custom Control to a Ross switcher over RossTalk, or a request to a web "
                      + "address. Once there is one, Add Action has Communication for it. A MIDI device's sources also "
                      + "work the show, by the MIDI Map."
            }

            Row {
                spacing: 10

                AppButton {
                    objectName: "deviceAdd"
                    height: 30
                    font.pixelSize: 14
                    text: "+  Add a Device"
                    onClicked: page.showMenu([{ header: "Add a Device" },
                        { label: "MIDI", run: () => page.editing = Devices.add("midi").id },
                        { label: "RossTalk", run: () => page.editing = Devices.add("rosstalk").id },
                        { label: "HTTP Requests", run: () => page.editing = Devices.add("http").id }], this, 0, height)
                }

                AppButton {
                    objectName: "midiMapButton"
                    height: 30
                    font.pixelSize: 14
                    text: "MIDI Map"
                    onClicked: page.mapOpen = true
                }
            }

            // ProPresenter's devices for this workspace that are not here
            Text {
                visible: Devices.offered.length > 0
                topPadding: 10
                color: "#e6e6e6"
                font.pixelSize: 13
                font.bold: true
                text: "In ProPresenter's settings for this workspace"
            }

            Repeater {
                model: Devices.offered

                delegate: Row {
                    id: offer

                    required property var modelData

                    spacing: 10

                    Text {
                        width: 300
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        color: "#b0b3b8"
                        font.pixelSize: 13
                        text: offer.modelData.name + "   (" + offer.modelData.kindName + ")"
                    }

                    AppButton {
                        objectName: "deviceOffer:" + offer.modelData.name
                        height: 26
                        font.pixelSize: 13
                        text: "Add"
                        onClicked: page.editing = Devices.add(offer.modelData.kind, offer.modelData.id).id
                    }
                }
            }

            Text {
                width: listColumn.width
                visible: Devices.offered.length > 0
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 12
                text: "Added, a device keeps ProPresenter's name and id, so that the actions ProPresenter made for it "
                      + "send through it. Its ports and addresses are this computer's to choose."
            }
        }
    }

    // ---- One device
    Flickable {
        anchors.fill: parent
        visible: page.device !== null
        contentHeight: deviceColumn.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: deviceColumn

            width: parent.width - 16
            spacing: 12

            component Caption: Text {
                width: 140
                color: "#b0b3b8"
                font.pixelSize: 13
            }

            AppButton {
                objectName: "deviceBack"
                height: 28
                font.pixelSize: 13
                text: "‹  Devices"
                onClicked: page.editing = ""
            }

            Row {
                spacing: 10

                Caption {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Name"
                }

                AppTextField {
                    objectName: "deviceName"
                    width: 280
                    height: 30
                    font.pixelSize: 13
                    text: page.device ? page.device.name : ""
                    onEditingFinished: {
                        if (page.device && text.trim() !== "" && text.trim() !== page.device.name)
                            Devices.change(page.device.id, { name: text.trim() })
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: page.device ? page.device.kindName : ""
                }
            }

            // MIDI: the ports it hears from and sends to
            Repeater {
                model: page.device && page.device.kind === "midi" ? [{ key: "sources", caption: "Sources", note: "Notes heard from these work the show, by the MIDI Map." },
                                                                      { key: "destinations", caption: "Destinations", note: "Notes the actions send go to these." }] : []

                delegate: Column {
                    id: ports

                    required property var modelData
                    readonly property var chosen: page.device ? page.device[modelData.key] : []
                    readonly property var there: modelData.key === "sources" ? Devices.midiSources : Devices.midiDestinations
                    // Every port there is, and any that is chosen and not there
                    readonly property var all: there.concat(chosen.filter(name => !there.includes(name)))

                    spacing: 4

                    Text {
                        color: "#e6e6e6"
                        font.pixelSize: 13
                        font.bold: true
                        text: ports.modelData.caption
                    }

                    Text {
                        color: "#9a9da3"
                        font.pixelSize: 12
                        text: ports.modelData.note
                    }

                    Repeater {
                        model: ports.all

                        delegate: AppCheck {
                            required property string modelData

                            objectName: "devicePort:" + ports.modelData.key + ":" + modelData
                            text: modelData + (ports.there.includes(modelData) ? "" : "  (not here)")
                            checked: ports.chosen.includes(modelData)
                            onToggled: (on) => Devices.change(page.device.id, { [ports.modelData.key]: page.toggled(ports.chosen, modelData, on) })
                        }
                    }

                    Text {
                        visible: ports.all.length === 0
                        color: "#9a9da3"
                        font.pixelSize: 12
                        text: Devices.midiProblem !== "" ? Devices.midiProblem : "No MIDI ports here yet. Connect a MIDI device, or start the program that has the port."
                    }
                }
            }

            // RossTalk: the switcher's address and port
            Row {
                spacing: 10
                visible: page.device !== null && page.device.kind === "rosstalk"

                Caption {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Address"
                }

                AppTextField {
                    objectName: "deviceAddress"
                    width: 220
                    height: 30
                    font.pixelSize: 13
                    placeholderText: "The switcher's address"
                    text: page.device ? page.device.address : ""
                    onEditingFinished: {
                        if (page.device && text.trim() !== page.device.address)
                            Devices.change(page.device.id, { address: text.trim() })
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#b0b3b8"
                    font.pixelSize: 13
                    text: "Port"
                }

                NumberField {
                    objectName: "devicePort"
                    width: 80
                    from: 1
                    to: 65535
                    value: page.device ? page.device.port : 7788
                    onEdited: (value) => Devices.change(page.device.id, { port: Math.round(value) })
                }
            }

            // HTTP: the base address, and the headers every request has
            Row {
                spacing: 10
                visible: page.device !== null && page.device.kind === "http"

                Caption {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Base address"
                }

                AppTextField {
                    objectName: "deviceBaseUrl"
                    width: 380
                    height: 30
                    font.pixelSize: 13
                    placeholderText: "http://192.168.1.20:8080/api"
                    text: page.device ? page.device.baseUrl : ""
                    onEditingFinished: {
                        if (page.device && text.trim() !== page.device.baseUrl)
                            Devices.change(page.device.id, { baseUrl: text.trim() })
                    }
                }
            }

            Row {
                spacing: 10
                visible: page.device !== null && page.device.kind === "http"

                Caption {
                    text: "Headers"
                }

                AppTextArea {
                    objectName: "deviceHeaders"
                    width: 380
                    maximumLines: 4
                    placeholderText: "Sent with every request, one to a line: Name: value"
                    text: page.device ? page.device.headers.join("\n") : ""
                    onEditingFinished: {
                        const lines = text.split("\n").map(line => line.trim()).filter(line => line !== "")
                        if (page.device && lines.join("\n") !== page.device.headers.join("\n"))
                            Devices.change(page.device.id, { headers: lines })
                    }
                }
            }

            AppCheck {
                objectName: "deviceAutoReconnect"
                visible: page.device !== null && page.device.kind !== "http"
                text: "Auto Reconnect"
                checked: page.device ? page.device.autoReconnect : false
                onToggled: (on) => Devices.change(page.device.id, { autoReconnect: on })
            }

            Text {
                width: deviceColumn.width
                visible: page.device !== null && page.device.kind !== "http"
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 12
                text: "With Auto Reconnect, a device that drops, or whose port goes, is connected again when it can be, "
                      + "and one that was connected when the workspace was last left is connected when it is next opened."
            }

            Text {
                width: deviceColumn.width
                visible: page.device !== null && page.device.problem !== ""
                wrapMode: Text.Wrap
                color: "#ff8a80"
                font.pixelSize: 13
                text: page.device ? page.device.problem : ""
            }

            Row {
                spacing: 10

                AppButton {
                    objectName: "deviceConnect"
                    height: 30
                    font.pixelSize: 13
                    visible: page.device !== null && page.device.kind !== "http"
                    text: page.device && page.device.connected ? "Disconnect" : "Connect"
                    onClicked: Devices.setConnected(page.device.id, !page.device.connected)
                }

                AppButton {
                    objectName: "deviceRemove"
                    height: 30
                    font.pixelSize: 13
                    text: "Remove Device"
                    onClicked: {
                        const id = page.editing
                        page.editing = ""
                        Devices.remove(id)
                    }
                }
            }
        }
    }

    // ---- The MIDI Map
    Flickable {
        anchors.fill: parent
        visible: page.mapOpen
        contentHeight: mapColumn.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: mapColumn

            width: parent.width - 16
            spacing: 12

            AppButton {
                objectName: "midiMapBack"
                height: 28
                font.pixelSize: 13
                text: "‹  Devices"
                onClicked: page.mapOpen = false
            }

            Row {
                spacing: 10

                NumberField {
                    objectName: "midiMapFirst"
                    width: 70
                    from: 0
                    to: 127
                    value: page.firstNote
                    onEdited: (value) => page.firstNote = Math.round(value)
                }

                AppButton {
                    objectName: "midiMapAutoFill"
                    height: 28
                    font.pixelSize: 13
                    text: "Auto Fill"
                    onClicked: Devices.autoFillMidiMap(page.firstNote)
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 12
                    text: "Enter the starting note for the notes to be filled in from."
                }
            }

            Text {
                objectName: "midiLastNote"
                width: mapColumn.width
                elide: Text.ElideRight
                color: "#e6e6e6"
                font.pixelSize: 13
                text: "Last heard: " + (Devices.lastNote !== "" ? Devices.lastNote : "nothing yet")
            }

            Flow {
                width: mapColumn.width
                spacing: 18

                Repeater {
                    model: ["Clear Commands", "Video Controls", "Presentation Actions", "Select by Index"]

                    delegate: Rectangle {
                        id: group

                        required property string modelData

                        width: 300
                        height: groupColumn.height + 20
                        radius: 8
                        color: "#313338"

                        Column {
                            id: groupColumn

                            x: 12
                            y: 10
                            width: parent.width - 24
                            spacing: 4

                            Text {
                                color: "#e6e6e6"
                                font.pixelSize: 13
                                font.bold: true
                                bottomPadding: 4
                                text: group.modelData
                            }

                            Repeater {
                                model: Devices.midiMap.filter(command => command.group === group.modelData)

                                delegate: Row {
                                    id: entry

                                    required property var modelData
                                    readonly property bool here: !page.notHere.includes(modelData.key)

                                    spacing: 8
                                    opacity: here ? 1 : 0.45

                                    Text {
                                        width: 150
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                        color: "#b0b3b8"
                                        font.pixelSize: 12
                                        text: entry.modelData.label + ":"
                                    }

                                    NumberField {
                                        objectName: "midiNote:" + entry.modelData.key
                                        width: 56
                                        from: -1
                                        to: 127
                                        value: entry.modelData.note
                                        onEdited: (value) => Devices.setMidiNote(entry.modelData.key, Math.round(value))
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#9a9da3"
                                        font.pixelSize: 12
                                        text: entry.here ? entry.modelData.noteName : "not here"
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                width: mapColumn.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 12
                text: "A note heard from a MIDI device's sources, on any channel, does what it is set to here. For those "
                      + "under Select by Index, the note's velocity says which one: from 1 to 127, the first to the 127th. "
                      + "A note of -1 is none. The map is this computer's, for every workspace."
            }
        }
    }
}
