import QtQuick

// What a stage screen shows: the stage layout it is given, which is a slide whose text
// boxes are linked to what is live and so is drawn as any slide is; or, given none, the
// plain view the app has of its own. One for every stage screen, whatever the screen is
// sent out through (see OutputScene, which is the same for an audience screen).
Item {
    id: scene

    // The slide of the stage layout to show, as a map (see StageLayouts), or null
    property var layout: null
    property alias currentText: view.currentText
    property alias nextText: view.nextText

    StageView {
        id: view

        anchors.fill: parent
        visible: scene.layout === null
    }

    Slide {
        anchors.fill: parent
        visible: scene.layout !== null
        slide: scene.layout
    }
}
