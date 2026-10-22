import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// Settings → Copyright: the workspace's copyright display, as ProPresenter's settings
// have it (General → "Enable Copyright Display"; see src/ccli.h and CopyrightDisplay):
// whether it is on, which slides of a song show it, the church's CCLI licence number,
// and the layout: the song's details and the words between them, in order, each a chip
// that is clicked for what can be done to it (moved, taken away, its words typed), with
// + for another. Under the layout, the line it makes of made-up details. Edit Template
// opens the editor on the slide the line is drawn in, where its place and its look are
// set as any slide's are.
//
// It is all kept in ProPresenter's own file of the workspace, Configuration/CCLI, as it
// is changed. A presentation's own details (its title, artist and the rest, and whether
// it shows them) are set in the editor, under Copyright.
Item {
    id: page

    // Shows a menu (see PopupMenu) by an item
    property var showMenu: (rows, item, x, y) => {}
    // Opens the editor on the template slide
    property var editTemplate: () => {}

    readonly property var settings: CopyrightDisplay.settings
    readonly property var tokens: settings.tokens ?? []
    // The details there are, by ProPresenter's names for them
    readonly property var details: [["title", "Title"], ["artist", "Artist"], ["author", "Author"], ["publisher", "Publisher"],
                                    ["year", "Copyright Year"], ["songNumber", "CCLI Song Number"], ["license", "License Number"]]
    // Which of a token's text is being typed, by its place; -1 for none
    property int typing: -1

    function nameOf(token) {
        if (token.kind !== "text")
            return (details.find(d => d[0] === token.kind) ?? [token.kind, token.kind])[1]
        return token.text === "\n" ? "New Line" : token.text.replace(/\n/g, "↵")
    }

    function setTokens(list) {
        const error = CopyrightDisplay.set({ tokens: list })
        if (error !== "")
            Log.problem(error)
    }

    function tokenMenu(index, item) {
        const list = tokens.slice()
        const move = (to) => {
            const [token] = list.splice(index, 1)
            list.splice(to, 0, token)
            setTokens(list)
        }
        const rows = [{ header: nameOf(tokens[index]) }]
        if (tokens[index].kind === "text" && tokens[index].text !== "\n")
            rows.push({ label: "Edit Text", run: () => page.typing = index })
        rows.push({ label: "Move Left", disabled: index === 0, run: () => move(index - 1) })
        rows.push({ label: "Move Right", disabled: index === tokens.length - 1, run: () => move(index + 1) })
        rows.push({ label: "Remove", run: () => { list.splice(index, 1); setTokens(list) } })
        showMenu(rows, item, 0, item.height)
    }

    function addMenu(item) {
        const add = (token) => {
            setTokens(tokens.concat([token]))
            if (token.kind === "text" && token.text === "")
                page.typing = tokens.length - 1
        }
        showMenu([{ header: "Add" }].concat(details.map(d => ({ label: d[1], run: () => add({ kind: d[0], text: "" }) }))).concat([
            { divider: true },
            { label: "Text", run: () => add({ kind: "text", text: " " }) },
            { label: "New Line", run: () => add({ kind: "text", text: "\n" }) }
        ]), item, 0, item.height)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column

            width: parent.width - 16
            spacing: 14

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#b4b7bd"
                font.pixelSize: 13
                text: "The copyright of the song that is live, laid over its slides on the audience screens, made of the "
                      + "details the presentation has (from SongSelect, or set in the editor under Copyright), for the "
                      + "CCLI licence a church shows songs under."
            }

            AppCheck {
                objectName: "copyrightEnabled"
                width: parent.width
                text: "Enable Copyright Display"
                checked: page.settings.enabled === true
                onToggled: (checked) => CopyrightDisplay.set({ enabled: checked })
            }

            Row {
                spacing: 12

                Text {
                    width: 150
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: "Show on"
                }

                AppComboBox {
                    readonly property var choices: ["first", "last", "firstAndLast", "all"]

                    objectName: "copyrightDisplay"
                    width: 260
                    model: ["First Slide", "Last Slide", "First and Last Slide", "All Slides"]
                    currentIndex: Math.max(0, choices.indexOf(page.settings.display ?? "first"))
                    onActivated: (index) => CopyrightDisplay.set({ display: choices[index] })
                }
            }

            Row {
                spacing: 12

                Text {
                    width: 150
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: "CCLI License Number"
                }

                AppTextField {
                    objectName: "copyrightLicense"
                    width: 260
                    height: 30
                    font.pixelSize: 14
                    text: page.settings.license ?? ""
                    onEditingFinished: {
                        if (text.trim() !== (page.settings.license ?? ""))
                            CopyrightDisplay.set({ license: text })
                        text = Qt.binding(() => page.settings.license ?? "")
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#3a3c42"
            }

            Text {
                color: "#9a9da3"
                font.pixelSize: 13
                text: "Display Layout"
            }

            Flow {
                width: parent.width
                spacing: 6

                Repeater {
                    model: page.tokens

                    delegate: Rectangle {
                        id: chip

                        required property var modelData
                        required property int index
                        readonly property bool detail: modelData.kind !== "text"

                        objectName: "copyrightToken:" + index
                        width: page.typing === index ? 160 : chipLabel.implicitWidth + 20
                        height: 28
                        radius: 6
                        color: detail ? "#2f5d8a" : chipMouse.containsMouse ? "#45484e" : "#3a3c42"

                        Text {
                            id: chipLabel

                            anchors.centerIn: parent
                            visible: page.typing !== chip.index
                            color: "#e6e6e6"
                            font.pixelSize: 13
                            text: page.nameOf(chip.modelData)
                        }

                        AppTextField {
                            anchors.fill: parent
                            visible: page.typing === chip.index
                            font.pixelSize: 13
                            text: chip.modelData.text
                            onVisibleChanged: {
                                if (visible) {
                                    forceActiveFocus()
                                    selectAll()
                                }
                            }
                            onAccepted: {
                                const list = page.tokens.slice()
                                list[chip.index] = { kind: "text", text: text }
                                page.typing = -1
                                page.setTokens(list)
                            }
                            Keys.onEscapePressed: page.typing = -1
                        }

                        MouseArea {
                            id: chipMouse

                            anchors.fill: parent
                            enabled: page.typing !== chip.index
                            hoverEnabled: true
                            onClicked: page.tokenMenu(chip.index, chip)
                        }
                    }
                }

                AppButton {
                    objectName: "copyrightAddToken"
                    height: 28
                    font.pixelSize: 13
                    text: "+"
                    onClicked: page.addMenu(this)
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#8d9097"
                font.pixelSize: 12
                text: "As it looks, with made-up details:"
            }

            Rectangle {
                width: parent.width
                height: exampleText.implicitHeight + 20
                radius: 6
                color: "#1d1e22"

                Text {
                    id: exampleText

                    objectName: "copyrightExample"
                    x: 10
                    y: 10
                    width: parent.width - 20
                    wrapMode: Text.Wrap
                    color: "#e6e6e6"
                    font.pixelSize: 13
                    text: page.settings && CopyrightDisplay.example()
                }
            }

            AppButton {
                objectName: "copyrightEditTemplate"
                height: 30
                font.pixelSize: 13
                text: "Edit Template"
                onClicked: page.editTemplate()
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#8d9097"
                font.pixelSize: 12
                text: "The template is the slide the line is drawn in: where its box is, and the box's font and colour. "
                      + "ProPresenter shows the display only with “Show House of Worship Integrations” on; here it is always offered."
            }
        }
    }
}
