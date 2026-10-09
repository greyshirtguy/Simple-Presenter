import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// Search: find a presentation in the workspace's libraries by its name or its words,
// and open it or add it to the playlist that is open.
//
// What is typed is searched for as it is typed (see Search). The presentations found
// are listed, those found by their names first; one found only by its words has the
// line that has them under its name. Beside the list is the one picked: its words, or
// its slides as they look.
//
//   Up, Down      pick another
//   Enter         open the one picked, in its library
//   Ctrl+Enter    add it to the playlist that is open, and stay, to add another
//   Esc           put the search away
Rectangle {
    id: search

    // The operator window
    required property var win

    readonly property var found: Search.count >= 0 ? Search.find(field.text) : []
    property int picked: 0
    readonly property var pick: found.length > 0 ? found[Math.min(picked, found.length - 1)] : null
    // The playlist that is open, to add to; null where a library is being looked at
    readonly property var playlist: win.playlistId === "" ? null : (win.catalog.playlists.find(p => p.path === win.playlistId) ?? null)
    // Whether the one picked is shown as its slides, and not as its words
    property bool asSlides: false
    // What was last added, to say so
    property string added: ""

    signal closed

    function open() {
        field.text = ""
        picked = 0
        added = ""
        field.forceActiveFocus()
    }

    function openPicked() {
        if (pick === null)
            return
        const library = win.catalog.libraries.find(l => pick.path.startsWith(l.path + "/"))
        if (library) {
            win.openLibrary(library.path)
            win.openDocument(pick.path)
        }
        closed()
    }

    function addPicked() {
        if (pick === null || playlist === null)
            return
        win.addToPlaylist(playlist.path, pick.path)
        added = pick.name
    }

    onFoundChanged: picked = 0
    color: "#c0101114"

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: search.closed()
    }

    Rectangle {
        objectName: "searchPanel"
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(12, Math.min(70, parent.height - height - 12))
        width: Math.min(860, parent.width - 40)
        height: Math.min(560, parent.height - 24)
        radius: 8
        color: "#25272b"
        border.width: 1
        border.color: "#4a4d55"

        MouseArea {
            anchors.fill: parent
        }

        AppTextField {
            id: field

            objectName: "searchField"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            height: 34
            font.pixelSize: 15
            placeholderText: "Search the libraries: a name, or some of the words"
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Down) {
                    search.picked = Math.min(search.found.length - 1, search.picked + 1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Up) {
                    search.picked = Math.max(0, search.picked - 1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    if (event.modifiers & Qt.ControlModifier)
                        search.addPicked()
                    else
                        search.openPicked()
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    search.closed()
                    event.accepted = true
                }
            }
        }

        // ---- What was found
        ListView {
            id: list

            objectName: "searchResults"
            anchors.left: parent.left
            anchors.top: field.bottom
            anchors.bottom: foot.top
            anchors.margins: 14
            anchors.topMargin: 10
            width: Math.round(parent.width * 0.42)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: search.found
            currentIndex: search.picked
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

            delegate: Rectangle {
                id: row

                required property var modelData
                required property int index

                objectName: "searchRow"
                width: list.width
                height: modelData.line !== "" ? 46 : 30
                radius: 5
                color: index === search.picked ? "#1e88e5" : rowMouse.containsMouse ? "#33353a" : "transparent"

                Text {
                    x: 8
                    y: 6
                    width: parent.width - library.width - 24
                    elide: Text.ElideRight
                    color: "#e6e6e6"
                    font.pixelSize: 13
                    font.bold: row.modelData.byName
                    text: row.modelData.name
                }

                Text {
                    id: library

                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    y: 7
                    color: row.index === search.picked ? "#d7e9fb" : "#8a8d93"
                    font.pixelSize: 11
                    text: row.modelData.library
                }

                Text {
                    x: 8
                    y: 25
                    width: parent.width - 16
                    visible: row.modelData.line !== ""
                    elide: Text.ElideRight
                    color: row.index === search.picked ? "#d7e9fb" : "#9a9da3"
                    font.pixelSize: 12
                    font.italic: true
                    text: row.modelData.line
                }

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        search.picked = row.index
                        field.forceActiveFocus()
                    }
                    onDoubleClicked: search.openPicked()
                }
            }
        }

        Text {
            anchors.centerIn: list
            width: list.width - 20
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: search.found.length === 0
            color: "#8a8d93"
            font.pixelSize: 13
            text: Search.reading && Search.count === 0 ? "Reading the libraries…"
                : field.text.trim() === "" ? Search.count + (Search.count === 1 ? " presentation" : " presentations") + " to search."
                : "Nothing has all of that in its name or its words."
        }

        // ---- The one picked
        Rectangle {
            id: preview

            anchors.left: list.right
            anchors.right: parent.right
            anchors.top: field.bottom
            anchors.bottom: foot.top
            anchors.margins: 14
            anchors.topMargin: 10
            radius: 6
            color: "#1b1c1f"

            Row {
                id: views

                x: 8
                y: 8
                spacing: 4
                visible: search.pick !== null

                Repeater {
                    model: ["Text", "Slides"]

                    AppButton {
                        required property string modelData
                        required property int index

                        objectName: index === 1 ? "searchAsSlides" : "searchAsText"
                        height: 24
                        leftPadding: 10
                        rightPadding: 10
                        font.pixelSize: 12
                        text: modelData
                        highlighted: search.asSlides === (index === 1)
                        onClicked: {
                            search.asSlides = index === 1
                            field.forceActiveFocus()
                        }
                    }
                }
            }

            // Its words, a slide after another
            Flickable {
                anchors.fill: parent
                anchors.margins: 10
                anchors.topMargin: 40
                visible: search.pick !== null && !search.asSlides
                contentHeight: words.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: words

                    objectName: "searchWords"
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#c9cdd6"
                    font.pixelSize: 13
                    lineHeight: 1.15
                    textFormat: Text.PlainText
                    text: search.pick !== null && !search.asSlides ? Search.wordsOf(search.pick.path).join("\n\n") : ""
                }
            }

            // Its slides, as they look
            GridView {
                id: slides

                objectName: "searchSlides"
                anchors.fill: parent
                anchors.margins: 10
                anchors.topMargin: 40
                visible: search.pick !== null && search.asSlides
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cellWidth: Math.floor(width / 3)
                cellHeight: Math.round(cellWidth * 9 / 16) + 4
                model: visible ? (search.win.catalog.open(search.pick.path).slides ?? []) : []

                delegate: Item {
                    required property var modelData

                    width: slides.cellWidth
                    height: slides.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        color: "black"

                        Slide {
                            anchors.fill: parent
                            slide: parent.parent.modelData
                            effects: false
                        }
                    }
                }
            }
        }

        // ---- What can be done with it
        Item {
            id: foot

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 14
            height: 30

            Text {
                objectName: "searchHint"
                anchors.left: parent.left
                anchors.right: buttons.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                color: "#8a8d93"
                font.pixelSize: 12
                text: search.added !== "" && search.playlist !== null ? "Added “" + search.added + "” to “" + search.playlist.name + "”."
                    : search.playlist !== null ? "Enter opens it. Ctrl+Enter adds it to “" + search.playlist.name + "”."
                    : "Enter opens it. To add to a playlist, have the playlist open first."
            }

            Row {
                id: buttons

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                AppButton {
                    objectName: "searchAdd"
                    height: 28
                    leftPadding: 12
                    rightPadding: 12
                    font.pixelSize: 13
                    enabled: search.pick !== null && search.playlist !== null
                    text: "Add to Playlist"
                    onClicked: {
                        search.addPicked()
                        field.forceActiveFocus()
                    }
                }

                AppButton {
                    objectName: "searchOpen"
                    height: 28
                    leftPadding: 12
                    rightPadding: 12
                    font.pixelSize: 13
                    enabled: search.pick !== null
                    text: "Open"
                    onClicked: search.openPicked()
                }

                AppButton {
                    objectName: "searchClose"
                    height: 28
                    leftPadding: 12
                    rightPadding: 12
                    font.pixelSize: 13
                    text: "Close"
                    onClicked: search.closed()
                }
            }
        }
    }
}
