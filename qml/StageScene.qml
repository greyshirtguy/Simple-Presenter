import QtQuick

// What a stage screen shows: the stage layout it is given, which is a slide whose text
// boxes are linked to what is live and so is drawn as any slide is; or, given none, the
// plain view the app has of its own. One for every stage screen, whatever the screen is
// sent out through (see OutputScene, which is the same for an audience screen).
Item {
    id: scene

    // The slide of the stage layout to show, as a map (see StageLayouts), or null
    property var layout: null
    // The words of the slide on show and of the next, for the plain view. It is handed
    // them only while it is the view shown: each is fitted to its half of the screen by
    // being laid out at one size after another, which cost the app's thread a millisecond
    // or two at every slide on the laptop this is made on, behind a layout as much as in
    // front of it.
    property string currentText
    property string nextText
    // As for Slide: shadows, and the operator window, for the layout's Screen Previews
    property bool effects: true
    property var pictures: null

    StageView {
        anchors.fill: parent
        visible: scene.layout === null
        currentText: scene.layout === null ? scene.currentText : ""
        nextText: scene.layout === null ? scene.nextText : ""
    }

    Slide {
        anchors.fill: parent
        visible: scene.layout !== null
        slide: scene.layout
        effects: scene.effects
        pictures: scene.pictures
    }
}
