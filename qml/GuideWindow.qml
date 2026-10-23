import QtQuick
import QtQuick.Controls.Basic
import QtCore

// The guide: what is this app's own, what it does not do, its keys and where it keeps
// things (docs/guide.md, which the app carries: see Guide in src/guide.h). A window of its
// own, put up and away by the ? button at the right of the toolbar, kept to one side of
// the show as the Calendar's is; its size and place are remembered. A link in it opens
// in the browser.
Window {
    id: guide

    required property var owner

    title: "Simple Presenter, in short"
    flags: Qt.Window
    transientParent: owner
    width: 720
    height: 760
    minimumWidth: 420
    minimumHeight: 300
    color: "#1e1f23"

    Settings {
        category: "guideWindow"
        property alias x: guide.x
        property alias y: guide.y
        property alias width: guide.width
        property alias height: guide.height
    }

    Flickable {
        id: view

        objectName: "guideText"
        anchors.fill: parent
        anchors.margins: 4
        contentWidth: width
        contentHeight: words.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Esc puts it away (the keys go to it, the window being given them when it is put up)
        focus: true
        Keys.onEscapePressed: guide.close()

        ScrollBar.vertical: ScrollBar {}

        // (Rich text, made from the guide's Markdown by Guide with its links coloured)
        Text {
            id: words

            x: 24
            y: 20
            width: view.width - 48
            wrapMode: Text.Wrap
            textFormat: Text.RichText
            color: "#e6e6e6"
            font.pixelSize: 14
            text: Guide.text
            onLinkActivated: (link) => Qt.openUrlExternally(link)

            HoverHandler {
                cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    // Put up, it is the window the keys go to, so that Esc puts it away again at once
    onVisibleChanged: if (visible) requestActivate()
}
