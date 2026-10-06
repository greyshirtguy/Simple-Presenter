import QtQuick
import QtQuick.Controls.Basic

// Names a font family and, when clicked, offers the installed ones to pick from, each
// shown in itself, with a box to narrow the list by typing. A family the presentation
// names that is not installed here is shown as it is named, marked missing; it stays
// the presentation's font until another is picked.
Rectangle {
    id: picker

    // The family in use, and all the installed ones
    property string family
    property var families: []
    readonly property bool installed: families.some(name => name.toLowerCase() === family.toLowerCase())

    signal picked(string family)
    signal closed

    height: 28
    radius: 6
    color: mouse.pressed || popup.opened ? "#50535a" : mouse.containsMouse ? "#45484e" : "#3a3c42"

    Text {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 22
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: picker.installed || picker.family === "" ? "#e6e6e6" : "#ffb300"
        font.pixelSize: 13
        text: picker.family === "" ? "—" : picker.installed ? picker.family : picker.family + "  (missing)"
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        color: "#c9cbd0"
        font.pixelSize: 9
        text: "▼"
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        onClicked: popup.open()
    }

    Popup {
        id: popup

        readonly property var matches: {
            const wanted = filter.text.trim().toLowerCase()
            return wanted === "" ? picker.families : picker.families.filter(name => name.toLowerCase().includes(wanted))
        }

        function pick(family) {
            close()
            picker.picked(family)
        }

        y: picker.height + 4
        width: Math.max(picker.width, 260)
        margins: 6
        padding: 6
        focus: true
        onAboutToShow: {
            filter.text = ""
            const current = picker.families.findIndex(name => name.toLowerCase() === picker.family.toLowerCase())
            list.currentIndex = Math.max(0, current)
            list.positionViewAtIndex(list.currentIndex, ListView.Center)
        }
        onOpened: filter.forceActiveFocus()
        onClosed: picker.closed()

        background: Rectangle {
            radius: 8
            color: "#2b2d31"
            border.width: 1
            border.color: "#8b8f98"
        }

        contentItem: Column {
            spacing: 6

            AppTextField {
                id: filter

                width: parent.width
                height: 28
                font.pixelSize: 13
                placeholderText: "Find a font"
                placeholderTextColor: "#7d8088"
                onTextChanged: list.currentIndex = 0
                onAccepted: {
                    if (list.currentIndex >= 0 && list.currentIndex < popup.matches.length)
                        popup.pick(popup.matches[list.currentIndex])
                }
                Keys.onDownPressed: list.incrementCurrentIndex()
                Keys.onUpPressed: list.decrementCurrentIndex()
            }

            ListView {
                id: list

                width: parent.width
                height: Math.min(contentHeight, 320)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: popup.matches
                highlightMoveDuration: 0

                ScrollBar.vertical: ScrollBar {}

                KineticWheel {}

                delegate: Rectangle {
                    id: option

                    required property string modelData
                    required property int index

                    width: ListView.view.width
                    height: 30
                    radius: 4
                    color: optionMouse.containsMouse ? "#565962" : ListView.isCurrentItem ? "#45484e" : "transparent"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        color: option.modelData.toLowerCase() === picker.family.toLowerCase() ? "#ff8a1f" : "#e6e6e6"
                        font.pixelSize: 15
                        font.family: option.modelData
                        text: option.modelData
                    }

                    MouseArea {
                        id: optionMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: popup.pick(option.modelData)
                    }
                }
            }
        }
    }
}
