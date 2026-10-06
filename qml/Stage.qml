import QtQuick

// The stage (confidence) display window. It shows the stage layout it is given, which is
// a slide whose text boxes are linked to what is live and so is drawn as any slide is;
// or, given none, the plain view the app has of its own.
AuxWindow {
    id: win

    // The slide of the stage layout to show, as a map (see StageLayouts), or null
    property var layout: null
    property alias currentText: view.currentText
    property alias nextText: view.nextText

    objectName: "stage"
    title: "Stage"

    StageView {
        id: view

        anchors.fill: parent
        visible: win.layout === null
    }

    Slide {
        anchors.fill: parent
        visible: win.layout !== null
        slide: win.layout
    }
}
