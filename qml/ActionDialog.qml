import QtQuick
import SimplePresenterApp

// The small panel in which an action that needs more than a click is set up: one for
// a timer (which timer, what is done to it, and whether it is set up anew first), one
// for the stage (which layout each stage screen is given, if any), one for what a
// communication device is to be sent (a MIDI note, a Custom Control, an HTTP request:
// see src/communications.h), and one for the words a message is shown with (a box for
// each of its tokens that is given words, as the messages' tab has; its timers and the
// clock are said and need nothing), the last two made only when they are opened.
//
// It is opened for something to give the action to, a slide or a macro (the `target`
// that commitAction() in Main.qml takes), and for an action of theirs that is to be
// changed, or none for a new one. Add, or Enter, hands the action to the operator
// window; Cancel, Esc or a click outside leaves things as they were.
Rectangle {
    id: dialog

    required property var win
    // "" while it is shut, else "timer", "stage", "communication" or "message"
    property string mode: ""
    property var target: null
    // The action being changed, or null for a new one
    property var existing: null

    // ---- a timer action
    // The timer it is for, by id and by name: one of the workspace's, or the one the
    // action names, which may not be among them
    property string timerId: ""
    property string timerName: ""
    // What is done to it: a Timers.Action
    property int timerAction: 0
    // Whether the timer is set up anew first, and how (as a timer's own settings are)
    property bool timerSet: false
    property string setKind: "countdown"
    property real setDuration: 300
    property real setTimeOfDay: 36000
    property real setStartTime: 0
    property bool setOverrun: false

    // ---- a stage action
    // The layout this app's stage screen is given: one of the workspace's, by id, or
    // "" for no change
    property string layoutId: ""
    // And what every stage screen is given, by the screen's id: a layout's id, or ""
    // to leave that screen as it is. (`layoutId` is the first screen's.)
    property var layoutIds: ({})

    function giveLayout(screenId, id) {
        const all = Object.assign({}, layoutIds)
        all[screenId] = id
        layoutIds = all
        if (Screens.stage.length > 0 && Screens.stage[0].id === screenId)
            layoutId = id
    }
    onLayoutIdChanged: {
        if (Screens.stage.length > 0 && (layoutIds[Screens.stage[0].id] ?? "") !== layoutId)
            giveLayout(Screens.stage[0].id, layoutId)
    }

    // ---- a communication action
    // The device it is sent to ({ id, name, kind }, as Devices has it), what it sends
    // ("midi", "customControl" or "http"), and what it says, as the action map has it
    property var device: ({})
    property string command: ""
    property bool noteOn: true
    property int channel: 1
    property int note: 0
    property int intensity: 127
    property string controlValue: ""
    property string method: "GET"
    property string path: ""
    property string body: ""
    property string headers: ""

    // ---- a message action
    // The message it shows, by id and by name; its tokens, as Messages has them; and the
    // words each text token is to be given, by the token's id
    property string messageId: ""
    property string messageName: ""
    property var messageTokens: []
    property var tokenWords: ({})

    function giveWords(tokenId, words) {
        const all = Object.assign({}, tokenWords)
        all[tokenId] = words
        tokenWords = all
    }

    readonly property var timerActions: ["Start", "Stop", "Reset", "Reset and Start", "Stop and Reset"]
    readonly property var kinds: ["countdown", "countdownTo", "elapsed"]
    readonly property var kindNames: ["Countdown", "Countdown to a Time", "Elapsed Time"]
    // The timers there are to choose from, with the action's own first if it is not
    // one of them
    readonly property var timers: {
        const list = Timers.timers.map(timer => ({ id: timer.id, name: timer.name }))
        if (existing && existing.kind === "timer" && !list.some(timer => timer.id === existing.timerId || timer.name === existing.timerName))
            list.unshift({ id: existing.timerId, name: existing.timerName, missing: true })
        return list
    }

    signal closed

    function openTimer(target, existing, timerId) {
        dialog.target = target
        dialog.existing = existing
        const first = Timers.timers.find(timer => timer.id === timerId) ?? Timers.timers[0]
        const known = existing ? Timers.timers.find(timer => timer.id === existing.timerId)
                                 ?? Timers.timers.find(timer => timer.name === existing.timerName) : first
        dialog.timerId = known ? known.id : existing ? existing.timerId : ""
        dialog.timerName = known ? known.name : existing ? existing.timerName : ""
        timerAction = existing ? Math.min(existing.action, timerActions.length - 1) : 0
        timerSet = existing ? existing.set : false
        // Set up anew, it starts as the timer is now, or as the action has it.
        const like = known ? Timers.timers.find(timer => timer.id === known.id) : null
        setKind = existing && existing.set ? existing.setKind : like ? like.kind : "countdown"
        setDuration = existing && existing.set ? existing.setDuration : like ? like.duration : 300
        setTimeOfDay = existing && existing.set ? existing.setTimeOfDay : like ? like.timeOfDay : 36000
        setStartTime = existing && existing.set ? existing.setStartTime : like ? like.startTime : 0
        setOverrun = existing && existing.set ? existing.setOverrun : like ? like.overrun : false
        mode = "timer"
        forceActiveFocus()
    }

    function openStage(target, existing) {
        dialog.target = target
        dialog.existing = existing
        const given = existing ? Show.stageLayoutsOf(existing) : ({})
        const all = {}
        for (const screen of Screens.stage)
            all[screen.id] = given[screen.id] ? given[screen.id].id : ""
        layoutIds = all
        layoutId = Screens.stage.length > 0 ? all[Screens.stage[0].id] : ""
        mode = "stage"
        forceActiveFocus()
    }

    // `device` is the connection it is for; `command` what it sends; `on` whether a MIDI
    // note is a note on. An action that is there says the rest.
    function openCommunication(target, existing, device, command, on) {
        dialog.target = target
        dialog.existing = existing
        dialog.device = device
        dialog.command = command
        noteOn = existing ? existing.on !== false : on !== false
        channel = existing && existing.channel > 0 ? existing.channel : 1
        note = existing ? existing.note ?? 0 : 0
        intensity = existing ? existing.intensity ?? 127 : 127
        controlValue = existing ? existing.value ?? "" : ""
        method = existing && existing.method ? existing.method : "GET"
        path = existing ? existing.path ?? "" : ""
        body = existing ? existing.body ?? "" : ""
        headers = existing && existing.headers ? existing.headers.join("\n") : ""
        mode = "communication"
        forceActiveFocus()
    }

    // `message` is the one it shows, as Messages has it. Each of its text tokens starts
    // with the words the action gives it, or, for a new action or a token the action
    // gives nothing, what it was last given on the messages' tab.
    function openMessage(target, existing, message) {
        dialog.target = target
        dialog.existing = existing
        messageId = message.id
        messageName = message.name
        messageTokens = message.tokens
        const given = {}
        for (const token of message.tokens) {
            if (token.kind === "text")
                given[token.id] = token.value
        }
        for (const value of existing ? existing.values ?? [] : []) {
            if (value.kind === "text" && given[value.tokenId] !== undefined)
                given[value.tokenId] = value.value
        }
        tokenWords = given
        mode = "message"
        forceActiveFocus()
    }

    // What the panel says it is
    readonly property string title: mode === "message" ? "Message Action" : mode === "stage" ? "Stage Action" : mode === "timer" ? "Timer Action"
                                  : command === "midi" ? (noteOn ? "MIDI Note On" : "MIDI Note Off")
                                  : command === "customControl" ? "Custom Control" : "HTTP Request"

    function close() {
        mode = ""
        closed()
    }

    // The action as it is set up, as a map of the kind src/actions.h describes.
    function accept() {
        let action
        if (mode === "message") {
            action = win.messageAction({ id: messageId, name: messageName }, messageTokens.filter(token => token.kind === "text").map(token => ({
                tokenId: token.id, name: token.name, kind: "text", value: tokenWords[token.id] ?? "" })))
        } else if (mode === "communication") {
            action = { kind: "communication", command: command, deviceId: device.id ?? "", deviceName: device.name ?? "" }
            if (command === "midi")
                Object.assign(action, { on: noteOn, channel: channel, note: note, intensity: intensity })
            else if (command === "customControl")
                Object.assign(action, { value: controlValue.trim() })
            else
                Object.assign(action, { method: method, path: path.trim(), body: body,
                                        headers: headers.split("\n").map(line => line.trim()).filter(line => line !== "") })
            if (command === "customControl" && action.value === "")
                return
        } else if (mode === "timer") {
            if (timerName === "" && timerId === "")
                return
            action = { kind: "timer", action: timerAction, timerId: timerId, timerName: timerName, amount: 0, set: timerSet,
                       setKind: setKind, setDuration: setDuration, setTimeOfDay: setTimeOfDay, setStartTime: setStartTime,
                       setEndTime: 0, setHasEndTime: false, setOverrun: setOverrun }
        } else {
            // Every stage screen has its say: a layout, or (null) to be left as it is.
            const chosen = {}
            for (const screen of Screens.stage)
                chosen[screen.id] = StageLayouts.layouts.find(candidate => candidate.id === (layoutIds[screen.id] ?? "")) ?? null
            action = { kind: "stage", assignments: Show.stageAssignmentsFor(existing ?? null, chosen) }
        }
        const target = dialog.target
        const existingId = existing ? existing.id : ""
        close()
        win.commitAction(target, action, existingId)
    }

    visible: mode !== ""
    color: "#80000000"
    Keys.onEscapePressed: close()
    Keys.onReturnPressed: accept()
    Keys.onEnterPressed: accept()

    // Nothing behind it is clicked or scrolled; a click outside the panel shuts it.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
        onClicked: dialog.close()
    }

    Rectangle {
        id: panel

        objectName: "actionPanel"
        anchors.centerIn: parent
        width: 380
        height: content.height + 32
        radius: 10
        color: "#2b2d31"
        border.width: 1
        border.color: "#50535a"

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        component Line: Item {
            property alias caption: label.text
            default property alias controls: row.data

            width: parent.width
            height: 30

            Text {
                id: label

                // (A message's token can have a long name of its own.)
                width: 86
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                color: "#9a9da3"
                font.pixelSize: 11
                font.capitalization: Font.AllUppercase
            }

            Row {
                id: row

                x: 92
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
            }
        }

        Column {
            id: content

            x: 18
            y: 16
            width: parent.width - 36
            spacing: 6

            Text {
                color: "#e6e6e6"
                font.pixelSize: 16
                bottomPadding: 6
                text: (dialog.existing ? "" : dialog.title.startsWith("HTTP") ? "Add an " : "Add a ") + dialog.title
                      + (dialog.mode === "communication" ? ", to " + (dialog.device.name ?? "") : dialog.mode === "message" ? ": " + dialog.messageName : "")
            }

            // ---- A message action, made only while one is being set up: a line for
            // each of the message's tokens, in its order
            Loader {
                width: parent.width
                active: dialog.mode === "message"
                visible: active

                sourceComponent: Column {
                    spacing: 6

                    Repeater {
                        model: dialog.messageTokens

                        delegate: Line {
                            id: tokenLine

                            required property var modelData
                            readonly property bool given: modelData.kind === "text"

                            caption: given ? modelData.name : modelData.kind === "timer" ? "Timer" : "Clock"

                            AppTextField {
                                objectName: "actionToken:" + tokenLine.modelData.name
                                visible: tokenLine.given
                                width: content.width - 92
                                height: 26
                                font.pixelSize: 13
                                placeholderText: "What it says"
                                text: dialog.tokenWords[tokenLine.modelData.id] ?? ""
                                onTextEdited: dialog.giveWords(tokenLine.modelData.id, text)
                            }

                            Text {
                                visible: !tokenLine.given
                                width: content.width - 92
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                color: "#9a9da3"
                                font.pixelSize: 13
                                text: tokenLine.modelData.kind === "timer" ? (tokenLine.modelData.name || "A timer") + ", as it is going" : "The time of day"
                            }
                        }
                    }
                }
            }

            // ---- A communication action, made only while one is being set up
            Loader {
                width: parent.width
                active: dialog.mode === "communication"
                visible: active

                sourceComponent: Column {
                    spacing: 6

                    Line {
                        caption: "Note"
                        visible: dialog.command === "midi"

                        NumberField {
                            objectName: "actionNote"
                            width: 70
                            from: 0
                            to: 127
                            value: dialog.note
                            onEdited: (value) => dialog.note = Math.round(value)
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#9a9da3"
                            font.pixelSize: 13
                            text: Devices.noteName(dialog.note)
                        }
                    }

                    Line {
                        caption: "Velocity"
                        visible: dialog.command === "midi"

                        NumberField {
                            objectName: "actionVelocity"
                            width: 70
                            from: 0
                            to: 127
                            value: dialog.intensity
                            onEdited: (value) => dialog.intensity = Math.round(value)
                        }
                    }

                    Line {
                        caption: "Channel"
                        visible: dialog.command === "midi"

                        NumberField {
                            objectName: "actionChannel"
                            width: 70
                            from: 1
                            to: 16
                            value: dialog.channel
                            onEdited: (value) => dialog.channel = Math.round(value)
                        }
                    }

                    Line {
                        caption: "Control"
                        visible: dialog.command === "customControl"

                        AppTextField {
                            objectName: "actionControl"
                            width: content.width - 92
                            height: 26
                            font.pixelSize: 13
                            placeholderText: "Which, as the switcher numbers it"
                            text: dialog.controlValue
                            onTextEdited: dialog.controlValue = text
                        }
                    }

                    Text {
                        width: parent.width
                        visible: dialog.command === "customControl"
                        wrapMode: Text.WordWrap
                        color: "#9a9da3"
                        font.pixelSize: 12
                        text: "Sent to the switcher as CC and this."
                    }

                    Line {
                        caption: "Method"
                        visible: dialog.command === "http"

                        AppComboBox {
                            objectName: "actionMethod"
                            width: 110
                            height: 26
                            font.pixelSize: 13
                            model: ["GET", "POST", "PUT", "DELETE"]
                            currentIndex: Math.max(0, model.indexOf(dialog.method))
                            onActivated: (index) => {
                                dialog.method = model[index]
                                dialog.forceActiveFocus()
                            }
                        }
                    }

                    Line {
                        caption: "Path"
                        visible: dialog.command === "http"

                        AppTextField {
                            objectName: "actionPath"
                            width: content.width - 92
                            height: 26
                            font.pixelSize: 13
                            placeholderText: "/path, under " + (Devices.find(dialog.device.id ?? "", dialog.device.name ?? "").baseUrl ?? "the base address")
                            text: dialog.path
                            onTextEdited: dialog.path = text
                        }
                    }

                    Text {
                        visible: dialog.command === "http"
                        color: "#9a9da3"
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        text: "Body"
                    }

                    AppTextArea {
                        objectName: "actionBody"
                        width: parent.width
                        visible: dialog.command === "http"
                        placeholderText: "None, or what is sent: JSON, or words"
                        text: dialog.body
                        onTextChanged: dialog.body = text
                    }

                    Text {
                        visible: dialog.command === "http"
                        color: "#9a9da3"
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        text: "Headers"
                    }

                    AppTextArea {
                        objectName: "actionHeaders"
                        width: parent.width
                        visible: dialog.command === "http"
                        maximumLines: 4
                        placeholderText: "None, or one to a line: Name: value"
                        text: dialog.headers
                        onTextChanged: dialog.headers = text
                    }
                }
            }

            // ---- A timer action
            Line {
                caption: "Timer"
                visible: dialog.mode === "timer"

                AppComboBox {
                    objectName: "actionTimer"
                    width: content.width - 92
                    height: 26
                    font.pixelSize: 13
                    model: dialog.timers.map(timer => timer.name + (timer.missing ? " (not here)" : ""))
                    currentIndex: Math.max(0, dialog.timers.findIndex(timer => timer.id === dialog.timerId && timer.name === dialog.timerName))
                    onActivated: (index) => {
                        dialog.timerId = dialog.timers[index].id
                        dialog.timerName = dialog.timers[index].name
                        dialog.forceActiveFocus()
                    }
                }
            }

            Line {
                caption: "Do"
                visible: dialog.mode === "timer"

                AppComboBox {
                    objectName: "actionTimerDo"
                    width: content.width - 92
                    height: 26
                    font.pixelSize: 13
                    model: dialog.timerActions
                    currentIndex: dialog.timerAction
                    onActivated: (index) => {
                        dialog.timerAction = index
                        dialog.forceActiveFocus()
                    }
                }
            }

            AppCheck {
                objectName: "actionTimerSet"
                width: parent.width
                visible: dialog.mode === "timer"
                text: "Set the timer up first"
                checked: dialog.timerSet
                onToggled: (checked) => dialog.timerSet = checked
            }

            Line {
                caption: "As a"
                visible: dialog.mode === "timer" && dialog.timerSet

                AppComboBox {
                    width: content.width - 92
                    height: 26
                    font.pixelSize: 13
                    model: dialog.kindNames
                    currentIndex: Math.max(0, dialog.kinds.indexOf(dialog.setKind))
                    onActivated: (index) => {
                        dialog.setKind = dialog.kinds[index]
                        dialog.forceActiveFocus()
                    }
                }
            }

            Line {
                caption: dialog.setKind === "countdownTo" ? "To" : dialog.setKind === "elapsed" ? "From" : "Of"
                visible: dialog.mode === "timer" && dialog.timerSet

                TimeField {
                    objectName: "actionTimerTime"
                    width: 96
                    timeOfDay: dialog.setKind === "countdownTo"
                    seconds: dialog.setKind === "countdownTo" ? dialog.setTimeOfDay
                           : dialog.setKind === "elapsed" ? dialog.setStartTime : dialog.setDuration
                    onEdited: (seconds) => {
                        if (dialog.setKind === "countdownTo")
                            dialog.setTimeOfDay = seconds
                        else if (dialog.setKind === "elapsed")
                            dialog.setStartTime = seconds
                        else
                            dialog.setDuration = seconds
                    }
                    onFinished: dialog.forceActiveFocus()
                }

                AppCheck {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: dialog.setKind !== "elapsed"
                    text: "Overrun"
                    checked: dialog.setOverrun
                    onToggled: (checked) => dialog.setOverrun = checked
                }
            }

            // ---- A stage action: a line for each stage screen
            Repeater {
                model: dialog.mode === "stage" ? Screens.stage : []

                Line {
                    id: stageLine

                    required property var modelData
                    required property int index

                    caption: Screens.stage.length > 1 ? modelData.name : "Stage"

                    AppComboBox {
                        objectName: stageLine.index === 0 ? "actionStageLayout" : "actionStageLayout" + (stageLine.index + 1)
                        width: content.width - 92
                        height: 26
                        font.pixelSize: 13
                        model: ["-- No Change --"].concat(StageLayouts.layouts.map(layout => layout.name))
                        currentIndex: Math.max(0, StageLayouts.layouts.findIndex(layout => layout.id === (dialog.layoutIds[stageLine.modelData.id] ?? "")) + 1)
                        onActivated: (index) => {
                            dialog.giveLayout(stageLine.modelData.id, index === 0 ? "" : StageLayouts.layouts[index - 1].id)
                            dialog.forceActiveFocus()
                        }
                    }
                }
            }

            Text {
                width: parent.width
                visible: dialog.mode === "stage" && StageLayouts.layouts.length === 0
                wrapMode: Text.WordWrap
                color: "#9a9da3"
                font.pixelSize: 12
                text: "This workspace has no stage layouts yet. They are made on the Stage tab of the show controls."
            }

            Item {
                width: parent.width
                height: 40

                Row {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 8

                    AppButton {
                        height: 28
                        font.pixelSize: 13
                        text: "Cancel"
                        onClicked: dialog.close()
                    }

                    AppButton {
                        objectName: "actionAccept"
                        height: 28
                        font.pixelSize: 13
                        text: dialog.existing ? "Save" : "Add"
                        onClicked: dialog.accept()
                    }
                }
            }
        }
    }
}
