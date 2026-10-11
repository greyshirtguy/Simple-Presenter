import QtQuick
import SimplePresenterApp

// The notice that hardware video decoding is being held back on this computer. It is
// put up once, over the operator window, the first time the app runs on a computer the
// guard holds (see src/decodingguard.h for which those are): the app has decided
// something for whoever runs it, and they are owed being told, with the way to decide
// otherwise.
//
// It is kept short: what was found, in the guard's own words
// (VideoDecoding.guardReason), what is done about it, and that nothing needs doing. Of
// its two buttons, one leads to the part of the settings where the choice is, which is
// the way to decide otherwise; the other, Enter and Esc put it away. Either way it is
// not shown again (Main.qml keeps that it was, in the app's settings). A click beside it
// does not put it away: it is not to be dismissed by a click that was meant for a slide.
Rectangle {
    id: panel

    property bool open: false

    // Put away: `settingsWanted` says whether it was by the button that leads to the
    // settings
    signal closed(bool settingsWanted)

    function show() {
        open = true
        forceActiveFocus()
    }

    // Gives the notice the keyboard, for as long as it is up.
    function takeKeys() {
        forceActiveFocus()
    }

    function close(settingsWanted) {
        open = false
        closed(settingsWanted)
    }

    visible: open
    color: "#99000000"

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
            close(false)
        // (Nothing typed here is a key of the show's.)
        event.accepted = true
    }

    // Nothing behind it is clicked or scrolled while it is up.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        objectName: "decodingNotice"
        anchors.centerIn: parent
        width: Math.min(panel.width - 40, 560)
        height: content.height + 36
        radius: 10
        color: "#2b2d31"
        border.width: 1
        border.color: "#45484e"

        Column {
            id: content

            x: 20
            y: 18
            width: parent.width - 40
            spacing: 12

            Text {
                objectName: "decodingNoticeTitle"
                width: parent.width
                wrapMode: Text.Wrap
                color: "#f0f0f0"
                font.pixelSize: 17
                font.bold: true
                text: "Video is being decoded by the processor"
            }

            Text {
                objectName: "decodingNoticeWords"
                width: parent.width
                wrapMode: Text.Wrap
                color: "#c9ccd1"
                font.pixelSize: 13
                text: "On this computer " + VideoDecoding.guardReason + ". Trying that decoder has been seen to hang such a "
                    + "computer, so it is left alone: video is decoded by the processor, and drawn by the graphics card as ever.\n\n"
                    + "Nothing needs doing. To have the decoder tried all the same, see the settings, under Video. This is said once."
            }

            Row {
                anchors.right: parent.right
                spacing: 10

                AppButton {
                    objectName: "decodingNoticeSettings"
                    text: "Video Settings…"
                    onClicked: panel.close(true)
                }

                AppButton {
                    objectName: "decodingNoticeOk"
                    text: "OK"
                    onClicked: panel.close(false)
                }
            }
        }
    }
}
