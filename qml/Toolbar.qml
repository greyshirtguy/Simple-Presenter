import QtQuick
import QtQuick.Window

// The toolbar across the top of the operator window, which stands in for the title bar
// the window does not have: drag it to move the window, double-click it to maximise.
// On the left it holds what decides what is being worked on: the workspace picker, and
// the button into the editor and back out. Then the name of what is open. On the right,
// the buttons that switch things on and off (Simple View, which has the slides take
// the whole window, this bar included; the media bin; the output and stage windows,
// which are a pair and are drawn as one; the settings screen), and the window's own
// buttons. What works the show itself is not here but beside what it works: the
// transition under the slides, the clears, the transport and the timers under the
// previews.
Rectangle {
    id: toolbar

    // The operator window: what this shows is its state, and what the controls here do is
    // call its functions.
    required property var win
    // Where the middle of the button for Simple View is, from the left of the bar: the
    // button that leaves that view is put in the same place (see SimpleViewToggle).
    readonly property real simpleViewCentre: toolbarControls.x + simpleViewButton.x + simpleViewButton.width / 2

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

    // Into the editor for the presentation being viewed, and back out
    ToolbarIcon {
        id: editButton

        objectName: "editButton"
        anchors.left: workspacePicker.right
        anchors.leftMargin: 8
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

    // What is open, and the app: in the middle of the window where there is room for it
    // there, and otherwise in what room there is between the edit button and the buttons
    // on the right
    Text {
        readonly property real from: editButton.x + editButton.width + 14
        readonly property real to: toolbarControls.x - 16

        x: Math.max(from, Math.min((parent.width - width) / 2, to - width))
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, Math.min(implicitWidth, to - from))
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

        // The slides and nothing else: see Simple View in Main.qml. Not while the editor
        // is up, which has the window to itself.
        ToolbarIcon {
            id: simpleViewButton

            objectName: "simpleViewButton"
            anchors.verticalCenter: parent.verticalCenter
            width: 64
            kind: "expand"
            label: "Simple View"
            on: toolbar.win.simpleView
            progress: toolbar.win.holdProgress
            opacity: toolbar.win.editing ? 0.4 : 1
            onClicked: {
                if (!toolbar.win.editing)
                    toolbar.win.setSimpleView(!toolbar.win.simpleView, "its button in the toolbar")
            }
        }

        ToolbarIcon {
            anchors.verticalCenter: parent.verticalCenter
            kind: "bin"
            label: "Media"
            on: toolbar.win.mediaBinVisible
            onClicked: toolbar.win.mediaBinVisible = !toolbar.win.mediaBinVisible
        }

        // The two windows the show goes out through, each switched on and off by its
        // half: one control, with a line down the middle
        Rectangle {
            objectName: "outputToggles"
            anchors.verticalCenter: parent.verticalCenter
            width: outputToggles.width + 2
            height: 40
            radius: 8
            color: "#23252b"
            border.width: 1
            border.color: "#3a3c42"

            Row {
                id: outputToggles

                anchors.centerIn: parent

                ToolbarIcon {
                    objectName: "outputToggle"
                    height: 38
                    label: "Output"
                    on: toolbar.win.outputEnabled
                    onClicked: toolbar.win.outputEnabled = !toolbar.win.outputEnabled
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 26
                    color: "#3a3c42"
                }

                ToolbarIcon {
                    objectName: "stageToggle"
                    height: 38
                    label: "Stage"
                    on: toolbar.win.stageEnabled
                    onClicked: toolbar.win.stageEnabled = !toolbar.win.stageEnabled
                }
            }
        }

        ToolbarIcon {
            anchors.verticalCenter: parent.verticalCenter
            kind: "settings"
            label: "Settings"
            onClicked: toolbar.win.settingsOpen = true
        }

        // Sets the switches apart from the window's own buttons
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
