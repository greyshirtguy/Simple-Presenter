import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The stage screens, and the layout each shows. A workspace has as many as it has been
// given (see Screens, and the Screens section of the settings).
//
// A screen shows either the plain layout the app has always had (the words of the live
// slide over those of the next) or one of the workspace's stage layouts, which are
// slides whose text boxes are linked to what is live (see StageLayouts in
// src/stagelayouts.h). The layouts are made and changed in the editor, as slides are.
//
// Under the screens, along the bottom, is the stage message: a box to type in what the
// people on the stage are to be told, and a button that shows it and hides it. Shown,
// it is what every text box linked to the stage message shows, on whichever layout has
// one (ProPresenter's own layouts have one, across the foot), and a box set to flash
// flashes for two seconds (see Show). What is shown is what was in the box when it was
// shown: words typed since go out with Enter, or with the button, which says Show
// again while the box has other words than the stage is being told. What is in the
// box is kept from one start of the app to the next, with the app's settings; that a
// message is being shown is not.
//
// GUESSES at ProPresenter, whose stage message this is from a description: that the
// box is one line, that Enter shows what is in it, and that words changed while a
// message is shown wait to be shown.
Item {
    id: panel

    // The operator window: which layout the stage has is its state.
    required property var win
    // The layouts there are to choose from, by name, after the plain one
    readonly property var layouts: StageLayouts.layouts
    readonly property string plain: "Words, now and next"
    // Whether the stage is being told what is in the box, as it is in the box
    readonly property bool showing: Show.stageMessage !== "" && Show.stageMessage === win.stageMessageText.trim()

    // Shows what is in the box, or with nothing in it stops showing anything.
    function tell() {
        win.stageMessageText = message.text
        if (message.text.trim() === "")
            Show.hideStageMessage()
        else
            Show.showStageMessage(message.text.trim())
    }

    // Adds a layout to start from, gives it to the stage, and opens the editor on it.
    function add() {
        const added = StageLayouts.add()
        if (!win.report(added.error))
            return
        if (Screens.stage.length > 0)
            Show.setStageLayout(Screens.stage[0].id, added.id)
        win.startEditingStage(added.id)
    }

    // The screens, which scroll where there are more than there is room for over the
    // stage message
    Flickable {
        id: screens

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: telling.top
        anchors.bottomMargin: 6
        clip: true
        contentHeight: rows.height
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        Column {
            id: rows

            width: screens.width
            spacing: 6

            Repeater {
                model: Screens.stage

                Rectangle {
                    id: screen

                    required property var modelData
                    required property int index
                    // The id of the layout this screen has, "" for the plain view
                    readonly property string layoutId: Show.screenLayouts[modelData.id] ?? ""

                    objectName: index === 0 ? "stageScreenRow" : "stageScreenRow" + (index + 1)
                    width: parent.width
                    height: 64
                    radius: 6
                    color: "#2b2d31"

                    // The screen can be dragged onto a slide, by its name and its picture, to
                    // give the slide an action that changes the layout it shows.
                    DragSource {
                        objectName: screen.index === 0 ? "stageScreenDrag" : "stageScreenDrag" + (screen.index + 1)
                        width: parent.width
                        height: 30
                        win: panel.win
                        payload: ({ kind: "stage", id: screen.modelData.id, name: screen.modelData.name })
                    }

                    // A screen on its stand, and what the screen is called
                    Item {
                        id: icon

                        x: 10
                        y: 9
                        width: 18
                        height: 16

                        Rectangle {
                            width: 18
                            height: 11.5
                            radius: 2
                            color: "transparent"
                            border.width: 1.5
                            border.color: "#c9cdd6"
                        }

                        Rectangle {
                            x: 8.25
                            y: 11.5
                            width: 1.5
                            height: 3
                            color: "#c9cdd6"
                        }

                        Rectangle {
                            x: 5
                            y: 14.5
                            width: 8
                            height: 1.5
                            color: "#c9cdd6"
                        }
                    }

                    Text {
                        anchors.left: icon.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: icon.verticalCenter
                        color: "#e6e6e6"
                        font.pixelSize: 13
                        text: Screens.stage.length > 1 ? screen.modelData.name : "Stage"
                    }

                    // On or off, as the toolbar has it
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: icon.verticalCenter
                        color: "#9a9da3"
                        font.pixelSize: 11
                        text: screen.modelData.output === "none" ? "not drawn" : panel.win.stageEnabled ? "" : screen.modelData.output === "ndi" ? "off" : "window off"
                    }

                    // The layout it shows
                    AppComboBox {
                        id: choice

                        objectName: screen.index === 0 ? "stageLayoutChoice" : "stageLayoutChoice" + (screen.index + 1)
                        x: 8
                        y: 32
                        width: parent.width - edit.width - 22
                        height: 24
                        font.pixelSize: 12
                        model: [panel.plain].concat(panel.layouts.map(layout => layout.name))
                        currentIndex: Math.max(0, panel.layouts.findIndex(layout => layout.id === screen.layoutId) + 1)
                        onActivated: (index) => Show.setStageLayout(screen.modelData.id, index === 0 ? "" : panel.layouts[index - 1].id)
                    }

                    AppButton {
                        id: edit

                        objectName: screen.index === 0 ? "stageLayoutEdit" : "stageLayoutEdit" + (screen.index + 1)
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: choice.verticalCenter
                        height: 24
                        leftPadding: 10
                        rightPadding: 10
                        font.pixelSize: 12
                        text: "Edit"
                        enabled: panel.layouts.length > 0
                        // The layout the stage has, or the first there is if it has the plain one
                        onClicked: panel.win.startEditingStage(screen.layoutId !== "" ? screen.layoutId : panel.layouts[0].id)
                    }
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 12
                visible: panel.layouts.length === 0
                text: "This workspace has no stage layouts. The + above makes one to start from."
            }
        }
    }

    // The stage message, along the bottom
    Rectangle {
        id: telling

        objectName: "stageMessage"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 58
        radius: 6
        color: "#2b2d31"
        // While the stage is being told something, it has the orange of what is live.
        border.width: Show.stageMessage !== "" ? 1 : 0
        border.color: panel.win.accentColor

        Text {
            x: 10
            y: 7
            color: Show.stageMessage !== "" ? "#ffd9a8" : "#9a9da3"
            font.pixelSize: 11
            text: Show.stageMessage === "" ? "Stage Message" : panel.showing ? "Stage Message, showing" : "Stage Message, showing other words"
        }

        AppTextField {
            id: message

            objectName: "stageMessageText"
            x: 8
            y: 26
            width: parent.width - show.width - 22
            height: 24
            font.pixelSize: 12
            placeholderText: "What to tell the stage"
            placeholderTextColor: "#7d8088"
            text: panel.win.stageMessageText
            onTextEdited: panel.win.stageMessageText = text
            onAccepted: {
                panel.tell()
                panel.win.takeFocus()
            }
            Keys.onEscapePressed: panel.win.takeFocus()
        }

        // Shows what is in the box; or, while that is what the stage is being told,
        // hides it.
        AppButton {
            id: show

            objectName: "stageMessageShow"
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: message.verticalCenter
            width: 58
            height: 24
            leftPadding: 6
            rightPadding: 6
            font.pixelSize: 12
            text: panel.showing ? "Hide" : "Show"
            enabled: panel.showing || message.text.trim() !== ""
            onClicked: {
                if (panel.showing)
                    Show.hideStageMessage()
                else
                    panel.tell()
                panel.win.takeFocus()
            }
        }
    }
}
