import QtQuick
import SimplePresenterApp

// The small panel in which an action that needs more than a click is set up: one for
// a timer (which timer, what is done to it, and whether it is set up anew first), and
// one for the stage (which layout each stage screen is given, if any).
//
// It is opened for something to give the action to, a slide or a macro (the `target`
// that commitAction() in Main.qml takes), and for an action of theirs that is to be
// changed, or none for a new one. Add, or Enter, hands the action to the operator
// window; Cancel, Esc or a click outside leaves things as they were.
Rectangle {
    id: dialog

    required property var win
    // "" while it is shut, else "timer" or "stage"
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

    function close() {
        mode = ""
        closed()
    }

    // The action as it is set up, as a map of the kind src/actions.h describes.
    function accept() {
        let action
        if (mode === "timer") {
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

                anchors.verticalCenter: parent.verticalCenter
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
                text: (dialog.existing ? "" : "Add a ") + (dialog.mode === "stage" ? "Stage Action" : "Timer Action")
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
