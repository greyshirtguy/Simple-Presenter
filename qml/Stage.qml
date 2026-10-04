import QtQuick

// The stage (confidence) display window.
AuxWindow {
    id: win

    property alias currentText: view.currentText
    property alias nextText: view.nextText

    objectName: "stage"
    title: "Stage"

    StageView {
        id: view

        anchors.fill: parent
    }
}
