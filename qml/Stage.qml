import QtQuick

// A stage (confidence) screen in a window: a StageScene in a small window of its own
// that can fill a display, or filling the display the screen is set to.
AuxWindow {
    id: win

    property alias layout: scene.layout
    property alias currentText: scene.currentText
    property alias nextText: scene.nextText

    objectName: "stage"
    title: "Stage"

    StageScene {
        id: scene

        anchors.fill: parent
    }
}
