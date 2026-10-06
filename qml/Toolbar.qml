import QtQuick
import QtQuick.Window

// The toolbar across the top of the operator window, which stands in for the title bar
// the window does not have: drag it to move the window, double-click it to maximise.
// From the left it holds the workspace picker, the name of what is open, the transition
// and its length, the buttons that switch things on and off (the editor, the media bin,
// the output and stage windows, the settings screen), and the window's own buttons.
Rectangle {
    id: toolbar

    // The operator window: what this shows is its state, and what the controls here do is
    // call its functions.
    required property var win

    component WindowButton: AppButton {
        width: 34
        leftPadding: 0
        rightPadding: 0
        font.pixelSize: 15
    }

    height: 48
    color: "#15161a"

    DragHandler {
        target: null
        onActiveChanged: if (active) toolbar.win.startSystemMove()
    }

    TapHandler {
        onDoubleTapped: toolbar.win.visibility = toolbar.win.visibility === Window.Maximized ? Window.Windowed : Window.Maximized
    }

    // The workspace: the folder everything shown comes from. Picking another
    // reloads the app from that one.
    Rectangle {
        id: workspacePicker

        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: workspaceRow.width + 14
        height: 38
        radius: 8
        color: "#23252b"
        border.width: 1
        border.color: "#3a3c42"

        Row {
            id: workspaceRow

            anchors.centerIn: parent
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 4
                color: toolbar.win.dimTextColor
                font.pixelSize: 11
                font.capitalization: Font.AllUppercase
                text: "Workspace"
            }

            AppComboBox {
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                height: 28
                font.pixelSize: 13
                // Not while a presentation of this one is being edited
                enabled: !toolbar.win.editing
                opacity: enabled ? 1 : 0.5
                model: toolbar.win.catalog.workspaces.map(w => w.name)
                currentIndex: toolbar.win.catalog.workspaces.findIndex(w => w.path === toolbar.win.catalog.workspacePath)
                onActivated: (index) => toolbar.win.switchWorkspace(toolbar.win.catalog.workspaces[index].path)
            }
        }
    }

    Text {
        anchors.left: workspacePicker.right
        anchors.leftMargin: 14
        anchors.right: toolbarControls.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight
        color: toolbar.win.textColor
        font.pixelSize: 15
        text: toolbar.win.title
    }

    Row {
        id: toolbarControls

        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        // The transition and its length, grouped on a panel of their own
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: transitionControls.width + 12
            height: 38
            radius: 8
            color: "#23252b"
            border.width: 1
            border.color: "#3a3c42"

            Row {
                id: transitionControls

                anchors.centerIn: parent
                spacing: 6

                // The chosen transition. A click opens the menu of them all, by category.
                Rectangle {
                    id: transitionButton

                    objectName: "transitionButton"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 150
                    height: 28
                    radius: 6
                    color: transitionMouse.pressed ? "#50535a" : transitionMouse.containsMouse ? "#45484e" : "#3a3c42"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 22
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        color: toolbar.win.textColor
                        font.pixelSize: 13
                        text: toolbar.win.transition.name
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        color: toolbar.win.dimTextColor
                        font.pixelSize: 10
                        text: "▼"
                    }

                    MouseArea {
                        id: transitionMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: toolbar.win.showTransitionMenu(transitionButton)
                    }
                }

                // What can be adjusted about it, for the transitions that have anything
                IconButton {
                    id: optionsButton

                    objectName: "transitionOptionsButton"
                    anchors.verticalCenter: parent.verticalCenter
                    height: 28
                    kind: "sliders"
                    on: options.opened
                    available: toolbar.win.transition.options.length > 0
                    onClicked: options.opened ? options.close() : options.open()

                    TransitionOptions {
                        id: options

                        y: parent.height + 9
                        x: parent.width - width
                        win: toolbar.win
                        onClosed: toolbar.win.takeFocus()
                    }
                }

                AppSlider {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 100
                    from: 0
                    to: 3
                    stepSize: 0.05
                    enabled: toolbar.win.transitionIndex !== 0
                    value: Math.min(toolbar.win.transitionDuration, to)
                    onMoved: toolbar.win.transitionDuration = Math.round(value * 100) / 100
                }

                // Seconds; accepts any value from 0 up, beyond the slider's range.
                AppTextField {
                    id: durationField

                    function reset() {
                        text = Number(toolbar.win.transitionDuration.toFixed(2)).toString()
                    }

                    anchors.verticalCenter: parent.verticalCenter
                    width: 42
                    height: 28
                    leftPadding: 4
                    rightPadding: 6
                    font.pixelSize: 13
                    horizontalAlignment: TextInput.AlignRight
                    enabled: toolbar.win.transitionIndex !== 0
                    onEditingFinished: {
                        const seconds = Number(text.replace(",", "."))
                        if (text.trim() !== "" && isFinite(seconds) && seconds >= 0)
                            toolbar.win.transitionDuration = seconds
                        reset()
                        toolbar.win.takeFocus()
                    }
                    Keys.onEscapePressed: {
                        reset()
                        toolbar.win.takeFocus()
                    }
                    Component.onCompleted: reset()

                    Connections {
                        target: toolbar.win

                        function onTransitionDurationChanged() {
                            durationField.reset()
                        }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    rightPadding: 2
                    color: toolbar.win.dimTextColor
                    font.pixelSize: 12
                    text: "s"
                }
            }
        }

        // Sets the transition controls apart from the buttons that follow
        Item {
            width: 14
            height: 1
        }

        // Into the editor for the presentation being viewed, and back out
        ToolbarIcon {
            anchors.verticalCenter: parent.verticalCenter
            kind: "edit"
            label: "Edit"
            on: toolbar.win.editing
            opacity: toolbar.win.editing || (toolbar.win.document !== null && toolbar.win.currentEntry() !== undefined) ? 1 : 0.4
            onClicked: {
                if (toolbar.win.editing)
                    toolbar.win.stopEditing()
                else
                    toolbar.win.startEditing(toolbar.win.currentEntry())
            }
        }

        ToolbarIcon {
            anchors.verticalCenter: parent.verticalCenter
            kind: "bin"
            label: "Media"
            on: toolbar.win.mediaBinVisible
            onClicked: toolbar.win.mediaBinVisible = !toolbar.win.mediaBinVisible
        }

        ToolbarIcon {
            anchors.verticalCenter: parent.verticalCenter
            label: "Output"
            on: toolbar.win.outputEnabled
            onClicked: toolbar.win.outputEnabled = !toolbar.win.outputEnabled
        }

        ToolbarIcon {
            anchors.verticalCenter: parent.verticalCenter
            label: "Stage"
            on: toolbar.win.stageEnabled
            onClicked: toolbar.win.stageEnabled = !toolbar.win.stageEnabled
        }

        ToolbarIcon {
            anchors.verticalCenter: parent.verticalCenter
            kind: "settings"
            label: "Settings"
            onClicked: toolbar.win.settingsOpen = true
        }

        Item {
            width: 8
            height: 1
        }

        WindowButton {
            anchors.verticalCenter: parent.verticalCenter
            text: "–"
            onClicked: toolbar.win.showMinimized()
        }

        WindowButton {
            anchors.verticalCenter: parent.verticalCenter
            text: toolbar.win.visibility === Window.Maximized ? "❐" : "□"
            onClicked: toolbar.win.visibility = toolbar.win.visibility === Window.Maximized ? Window.Windowed : Window.Maximized
        }

        WindowButton {
            anchors.verticalCenter: parent.verticalCenter
            text: "✕"
            onClicked: toolbar.win.close()
        }
    }
}
