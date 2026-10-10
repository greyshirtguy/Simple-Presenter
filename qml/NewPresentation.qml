import QtQuick
import SimplePresenterApp

// The panel a new presentation is named in: its name, the library it goes into (the one
// that is open), and Create. It is put up from the + beside Libraries (see
// newPresentation in Main.qml), over the operator window; Cancel, Esc or a click outside
// makes nothing.
//
// The name is asked first, and not given afterwards in the list as a new library's is,
// because a presentation's name is its file's name and is what playlists find it by:
// nothing here renames one once it is made.
//
// ProPresenter asks for the name of a new presentation before it makes one as well. A
// GUESS about the rest, which has not been looked at there: its own window is thought
// to ask more than the name (where the presentation is to go, and a theme to dress its
// first slide in). This asks only the name. The presentation goes into the library
// that is open, with one slide with nothing on it (ProDocument::create), and a theme
// can be put on it afterwards like on any other.
Rectangle {
    id: panel

    required property var win
    property bool open: false
    readonly property string library: win.libraryPath
    readonly property string libraryName: library.substring(library.lastIndexOf("/") + 1)
    // What is typed, as it would be kept
    readonly property string name: field.text.trim()

    signal closed

    function show() {
        open = true
        field.text = "New Presentation"
        field.forceActiveFocus()
        field.selectAll()
    }

    function close() {
        open = false
        closed()
    }

    function create() {
        if (name === "")
            return
        const wanted = name
        open = false
        win.newPresentation(wanted)
        closed()
    }

    visible: open
    color: "#99000000"

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    Rectangle {
        objectName: "newPresentationPanel"
        anchors.centerIn: parent
        width: 440
        height: content.height + 36
        radius: 10
        color: "#2b2d31"
        border.width: 1
        border.color: "#45484e"

        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: content

            x: 20
            y: 18
            width: parent.width - 40
            spacing: 12

            Text {
                width: parent.width
                color: "#f0f0f0"
                font.pixelSize: 17
                font.bold: true
                text: "New Presentation"
            }

            AppTextField {
                id: field

                objectName: "newPresentationName"
                width: parent.width
                maximumLength: 120
                onAccepted: panel.create()
                Keys.onEscapePressed: panel.close()
            }

            Text {
                width: parent.width
                elide: Text.ElideRight
                color: "#b4b7bd"
                font.pixelSize: 13
                text: "One slide with nothing on it, in the library " + panel.libraryName
            }

            Row {
                anchors.right: parent.right
                spacing: 10

                AppButton {
                    text: "Cancel"
                    onClicked: panel.close()
                }

                AppButton {
                    objectName: "newPresentationButton"
                    text: "Create"
                    enabled: panel.name !== ""
                    onClicked: panel.create()
                }
            }
        }
    }
}
