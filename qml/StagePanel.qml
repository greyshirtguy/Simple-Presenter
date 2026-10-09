import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The stage screens, and the layout each shows. There is one stage screen so far, the
// stage window; ProPresenter has as many as are wanted, which is why this is a list.
//
// A screen shows either the plain layout the app has always had (the words of the live
// slide over those of the next) or one of the workspace's stage layouts, which are
// slides whose text boxes are linked to what is live (see StageLayouts in
// src/stagelayouts.h). The layouts are made and changed in the editor, as slides are.
Item {
    id: panel

    // The operator window: which layout the stage has is its state.
    required property var win
    // The layouts there are to choose from, by name, after the plain one
    readonly property var layouts: StageLayouts.layouts
    readonly property string plain: "Words, now and next"

    // Adds a layout to start from, gives it to the stage, and opens the editor on it.
    function add() {
        const added = StageLayouts.add()
        if (!win.report(added.error))
            return
        Show.stageLayoutId = added.id
        win.startEditingStage(added.id)
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 6

        Rectangle {
            id: screen

            objectName: "stageScreenRow"
            width: parent.width
            height: 64
            radius: 6
            color: "#2b2d31"

            // The screen can be dragged onto a slide, by its name and its picture, to
            // give the slide an action that changes the layout it shows.
            DragSource {
                objectName: "stageScreenDrag"
                width: parent.width
                height: 30
                win: panel.win
                payload: ({ kind: "stage", id: "", name: "Stage" })
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
                text: "Stage"
            }

            // On or off, as the toolbar has it
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: icon.verticalCenter
                color: "#9a9da3"
                font.pixelSize: 11
                text: panel.win.stageEnabled ? "" : "window off"
            }

            // The layout it shows
            AppComboBox {
                id: choice

                objectName: "stageLayoutChoice"
                x: 8
                y: 32
                width: parent.width - edit.width - 22
                height: 24
                font.pixelSize: 12
                model: [panel.plain].concat(panel.layouts.map(layout => layout.name))
                currentIndex: Math.max(0, panel.layouts.findIndex(layout => layout.id === panel.win.stageLayoutId) + 1)
                onActivated: (index) => Show.stageLayoutId = index === 0 ? "" : panel.layouts[index - 1].id
            }

            AppButton {
                id: edit

                objectName: "stageLayoutEdit"
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
                onClicked: panel.win.startEditingStage(panel.win.stageLayout ? panel.win.stageLayout.id : panel.layouts[0].id)
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
