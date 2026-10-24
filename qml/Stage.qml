import QtQuick

// A stage (confidence) screen in a window: a StageScene in a small window of its own
// that can fill a display, or filling the display the screen is set to.
AuxWindow {
    id: win

    // What the scene shows: see StageScene. It is handed them only while the stage is
    // switched on (`shown`): a stage screen that is off is not seen, and what it would
    // show, its layout's linked words or the plain view's, is worked out again at every
    // slide.
    property var layout: null
    property string currentText
    property string nextText
    property alias pictures: scene.pictures

    objectName: "stage"
    title: "Stage"

    StageScene {
        id: scene

        anchors.fill: parent
        layout: win.shown ? win.layout : null
        currentText: win.shown ? win.currentText : ""
        nextText: win.shown ? win.nextText : ""
    }
}
