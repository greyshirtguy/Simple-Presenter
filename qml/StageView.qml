import QtQuick

// What the stage (confidence) display shows: the current slide's text on the top half
// and the next slide's on the bottom half. Plain text only, so it costs next to nothing
// to draw at any size.
Rectangle {
    id: view

    property string currentText
    property string nextText
    // The words shown: those handed over, a moment after they were. Second to the audience
    // screens, which change with the frames of a video playing on them: fitting the words to
    // their half of the screen is a millisecond or two of the app's thread, and done in the
    // click that changed them, it held up a frame of the video that came to the app as it
    // was being done (2026-10-31). A sixteenth of a second later, the screens have changed.
    property string shownCurrent: currentText
    property string shownNext: nextText

    color: "black"
    onCurrentTextChanged: later.restart()
    onNextTextChanged: later.restart()

    Timer {
        id: later

        interval: 60
        onTriggered: {
            view.shownCurrent = view.currentText
            view.shownNext = view.nextText
        }
    }

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
        text: view.shownCurrent
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
        text: view.shownNext
    }
}
