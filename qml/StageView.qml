import QtQuick

// What the stage (confidence) display shows: the current slide's text on the top half
// and the next slide's on the bottom half. Plain text only, so it costs next to nothing
// to draw at any size.
Rectangle {
    id: view

    property string currentText
    property string nextText

    color: "black"

    component StageText: Text {
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        fontSizeMode: Text.Fit
        minimumPixelSize: 4
        font.pixelSize: 400
        font.bold: true
    }

    StageText {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: divider.top
        anchors.margins: view.height * 0.04
        color: "#ffd400"
        text: view.currentText
    }

    Rectangle {
        id: divider

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 1
        color: "#333333"
    }

    StageText {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: divider.bottom
        anchors.bottom: parent.bottom
        anchors.margins: view.height * 0.04
        color: "#9a9da3"
        text: view.nextText
    }
}
