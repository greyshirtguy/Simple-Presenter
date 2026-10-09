import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The Looks section of the settings: the workspace's looks, and what each gives each
// audience screen.
//
// A look is a named table: a line for each audience screen, with a switch for each layer
// of the show (the slides, the media under them, the props over them) and a theme for
// that screen's slides to be dressed in, if any. The look that is live decides what each
// screen shows; it is changed here, by an action on a slide or in a macro, or from the
// menu of the toolbar's Output button. The looks are the workspace's, and are written to
// its file as they are changed (see Looks).
Item {
    id: section

    // The look whose table is shown, by id
    property string chosen: ""
    readonly property var look: Looks.looks.find(candidate => candidate.id === chosen) ?? null
    // Every slide of every theme, to choose from: [{ label, place, slideId }]
    readonly property var themeChoices: {
        const all = [{ label: "As the slides are", place: "", slideId: "" }]
        for (const theme of Themes.themes) {
            for (const slide of theme.slides)
                all.push({ label: theme.name + " — " + slide.name, place: theme.place, slideId: slide.id })
        }
        return all
    }

    signal failed(string text)

    function say(error) {
        if (error !== "")
            failed(error)
    }

    function add() {
        let name = "Look"
        for (let number = 2; Looks.looks.some(candidate => candidate.name === name); ++number)
            name = "Look " + number
        const made = Looks.add(name, Screens.audience.map(screen => screen.id))
        say(made.error)
        if (made.error === "")
            chosen = made.id
    }

    // A look is always chosen while there is one: the live one, to start with.
    function settle() {
        if (!Looks.looks.some(candidate => candidate.id === chosen))
            chosen = Looks.looks.some(candidate => candidate.id === Show.lookId) ? Show.lookId : Looks.looks.length > 0 ? Looks.looks[0].id : ""
    }

    Connections {
        target: Looks

        function onChanged() {
            section.settle()
        }
    }

    onVisibleChanged: settle()
    Component.onCompleted: settle()

    Text {
        id: about

        width: parent.width
        wrapMode: Text.WordWrap
        color: "#b0b3b8"
        font.pixelSize: 13
        text: "A look says which layers of the show each audience screen gets, and can dress a screen's slides in a theme as they "
            + "are shown: the room can have the words over the media while a stream has the words alone, as a line across the foot "
            + "of the picture, with nothing about the presentation changed. One look is live at a time."
    }

    // ---- The looks
    Column {
        id: list

        anchors.top: about.bottom
        anchors.topMargin: 12
        width: 190
        spacing: 4

        Repeater {
            model: Looks.looks

            Rectangle {
                id: row

                required property var modelData

                objectName: "lookRow"
                width: list.width
                height: 28
                radius: 5
                color: section.chosen === modelData.id ? "#1e88e5" : rowMouse.containsMouse ? "#33353a" : "#2b2d31"

                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 16 - (live.visible ? live.width + 6 : 0)
                    elide: Text.ElideRight
                    color: "#e6e6e6"
                    font.pixelSize: 13
                    text: row.modelData.name
                }

                Text {
                    id: live

                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Show.lookId === row.modelData.id
                    color: "#ff8a1f"
                    font.pixelSize: 10
                    font.bold: true
                    text: "LIVE"
                }

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: section.chosen = row.modelData.id
                }
            }
        }

        AppButton {
            objectName: "addLook"
            height: 26
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 12
            text: "+ Add"
            onClicked: section.add()
        }
    }

    Text {
        anchors.left: list.right
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.top: list.top
        wrapMode: Text.WordWrap
        visible: Looks.looks.length === 0
        color: "#9a9da3"
        font.pixelSize: 13
        text: "This workspace has no looks, so every audience screen gets everything. Add one to say otherwise."
    }

    // ---- The one chosen: its name, and its table
    Column {
        id: table

        anchors.left: list.right
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.top: list.top
        spacing: 8
        visible: section.look !== null

        Row {
            spacing: 8

            AppTextField {
                objectName: "lookName"
                width: 200
                height: 28
                font.pixelSize: 13
                text: section.look ? section.look.name : ""
                onEditingFinished: {
                    const typed = text.trim()
                    const id = section.chosen
                    const changed = section.look !== null && typed !== section.look.name
                    text = Qt.binding(() => section.look ? section.look.name : "")
                    if (changed)
                        section.say(Looks.rename(id, typed))
                }
            }

            AppButton {
                objectName: "lookMakeLive"
                height: 28
                leftPadding: 10
                rightPadding: 10
                font.pixelSize: 12
                enabled: Show.lookId !== section.chosen
                text: Show.lookId === section.chosen ? "Live" : "Make Live"
                onClicked: Show.lookId = section.chosen
            }

            AppButton {
                objectName: "lookRemove"
                height: 28
                leftPadding: 10
                rightPadding: 10
                font.pixelSize: 12
                text: "Remove"
                onClicked: section.say(Looks.remove(section.chosen))
            }
        }

        // What the columns are
        Row {
            spacing: 0

            Repeater {
                model: [["Screen", 150], ["Slides", 62], ["Media", 62], ["Props", 62], ["Theme its slides are dressed in", 220]]

                Text {
                    required property var modelData

                    width: modelData[1]
                    color: "#8a8d93"
                    font.pixelSize: 11
                    text: modelData[0]
                }
            }
        }

        Repeater {
            model: section.look !== null ? Screens.audience : []

            Rectangle {
                id: line

                required property var modelData
                readonly property var gets: section.look && section.look.screens[modelData.id] !== undefined
                                            ? section.look.screens[modelData.id]
                                            : ({ slide: true, media: true, props: true, theme: "", themeSlide: "" })

                function set(changes) {
                    section.say(Looks.setScreen(section.chosen, modelData.id, changes))
                }

                objectName: "lookLine"
                width: table.width
                height: 34
                radius: 5
                color: "#2b2d31"

                Row {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0

                    Text {
                        width: 142
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        color: "#e6e6e6"
                        font.pixelSize: 13
                        text: line.modelData.name
                    }

                    Repeater {
                        model: ["slide", "media", "props"]

                        Item {
                            required property string modelData

                            width: 62
                            height: 22

                            AppCheck {
                                objectName: "look-" + parent.modelData
                                anchors.verticalCenter: parent.verticalCenter
                                checked: line.gets[parent.modelData] === true
                                onToggled: (checked) => line.set({ [parent.modelData]: checked })
                            }
                        }
                    }

                    AppComboBox {
                        objectName: "lookTheme"
                        width: Math.max(140, line.width - 142 - 186 - 24)
                        height: 26
                        font.pixelSize: 12
                        model: section.themeChoices.map(choice => choice.label)
                        currentIndex: Math.max(0, section.themeChoices.findIndex(choice => choice.place === line.gets.theme
                                                                                           && (choice.place === "" || choice.slideId === line.gets.themeSlide)))
                        onActivated: (index) => line.set({ theme: section.themeChoices[index].place, themeSlide: section.themeChoices[index].slideId })
                    }
                }
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#8a8d93"
            font.pixelSize: 12
            text: "ProPresenter has layers this app has not yet (announcements, messages, video inputs, masks). What its looks say of "
                + "those is kept in the workspace's file as it was."
        }
    }
}
