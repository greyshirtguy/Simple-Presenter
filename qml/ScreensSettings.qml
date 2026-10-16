import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The Screens section of the settings: the workspace's screens, of both kinds, and what
// each is sent out through on this computer (see Screens).
//
// A line for each screen: its name, where it goes (a window, one of the displays, NDI,
// an output of a Blackmagic card, or nowhere), and for NDI what the source is called,
// how large it is and how many frames a second it has. The list is the workspace's and
// is written to its file as it is changed; where a screen goes is this computer's, and
// is kept in the app's settings.
//
// Where a screen goes is chosen from a menu, as in ProPresenter: each of a Blackmagic
// card's outputs, as Desktop Video Setup names and arranges them, is a menu of the
// formats it can send, and choosing one is choosing the output (see DeckLink). An
// output that can key, which a pair of connectors linked there for fill and key is, has
// ProPresenter's alpha settings under it: the alpha type (none, premultiplied or
// straight), internal or external keying, and the blend.
Flickable {
    id: section

    // What sends each screen that goes over NDI, by the screen's id (see NdiScreen): to
    // say how that is going
    property var senders: ({})

    // Something went wrong changing the list
    signal failed(string text)
    // A screen has been set to NDI and NDI's library is not here: how to get it is wanted
    signal ndiWanted
    // The menu of where a screen can go is to be shown (by the operator window, whose
    // menus can have menus in them), under an item
    signal menuWanted(var rows, var item, real x, real y)

    readonly property var sizes: [[1280, 720], [1920, 1080], [2560, 1440], [3840, 2160]]

    function say(error) {
        if (error !== "")
            failed(error)
    }

    contentHeight: column.height
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    component ScreenRow: Rectangle {
        id: row

        required property var modelData
        readonly property var screen: modelData
        readonly property var sender: section.senders[screen.id] ?? null
        // The card output it goes to, as found now, or null
        readonly property var card: screen.output === "decklink" ? DeckLink.outputs.find(output => output.id === screen.decklinkOutput) ?? null : null
        readonly property bool keys: card !== null && (card.internal || card.external)
        readonly property bool keyed: screen.decklinkAlpha !== "none"
        // Where it goes, in a few words
        readonly property string outputLabel: {
            switch (screen.output) {
            case "window":
                return "A window of its own"
            case "display": {
                const display = Screens.displays.find(candidate => candidate.name === screen.display)
                return "Display: " + (display ? display.label : screen.display + " (not plugged in)")
            }
            case "ndi":
                return "NDI"
            case "decklink":
                return (screen.decklinkName !== "" ? screen.decklinkName : "Blackmagic") + " · " + screen.decklinkFormat
            default:
                return "Nothing"
            }
        }

        // The menu of where it can go: a window, each display, NDI, each card output with
        // its formats, nothing. (What each does is worked out from values of its own:
        // changing a screen makes the list anew, and this line of it with it.)
        function outputRows() {
            const id = screen.id
            const now = screen
            const settings = section
            const choose = (changes, ask) => {
                Screens.setOutput(id, changes)
                if (ask)
                    settings.ndiWanted()
            }
            const rows = [{ label: "A window of its own", current: now.output === "window", run: () => choose({ output: "window", display: "" }) }]
            for (const display of Screens.displays)
                rows.push({ label: "Display: " + display.label, current: now.output === "display" && now.display === display.name,
                            run: () => choose({ output: "display", display: display.name }) })
            if (now.output === "display" && !now.displayThere)
                rows.push({ label: "Display: " + now.display + " (not plugged in)", current: true, run: () => {} })
            rows.push({ label: "NDI", current: now.output === "ndi", run: () => choose({ output: "ndi", display: "" }, !Ndi.available) })
            const cards = DeckLink.outputs
            if (cards.length > 0 || now.output === "decklink")
                rows.push({ header: "Blackmagic" })
            for (const card of cards) {
                const here = now.output === "decklink" && now.decklinkOutput === card.id
                rows.push({ label: card.name, current: here, items: [{ header: card.name }].concat(card.formats.map(format => ({
                    label: format, current: here && now.decklinkFormat === format,
                    // (An output that cannot key sends the screen as it is: the alpha
                    // settings are not open for it.)
                    run: () => choose({ output: "decklink", display: "", decklinkOutput: card.id, decklinkName: card.name, decklinkFormat: format,
                                        decklinkAlpha: card.internal || card.external ? now.decklinkAlpha : "none" })
                }))) })
            }
            if (now.output === "decklink" && !cards.some(card => card.id === now.decklinkOutput))
                rows.push({ label: now.decklinkName + " (not here)", current: true, run: () => {} })
            rows.push({ label: "Nothing", current: now.output === "none", run: () => choose({ output: "none", display: "" }) })
            return rows
        }

        objectName: "screenRow"
        width: parent ? parent.width : 0
        height: body.height + 16
        radius: 6
        color: "#2b2d31"

        Column {
            id: body

            x: 10
            y: 8
            width: parent.width - 20
            spacing: 8

            Row {
                spacing: 8

                AppTextField {
                    objectName: "screenName"
                    width: 200
                    height: 28
                    font.pixelSize: 13
                    text: row.screen.name
                    onEditingFinished: {
                        const typed = text.trim()
                        const settings = section
                        const id = row.screen.id
                        const changed = typed !== row.screen.name
                        text = Qt.binding(() => row.screen.name)
                        if (changed)
                            settings.say(Screens.rename(id, typed))
                    }
                }

                AppButton {
                    id: outputButton

                    objectName: "screenOutput"
                    width: Math.min(400, body.width - 200 - remove.width - 16)
                    height: 28
                    font.pixelSize: 13
                    text: row.outputLabel + "  ▾"
                    // (The card outputs are looked for again each time: Desktop Video
                    // Setup may have arranged them otherwise since.)
                    onClicked: {
                        DeckLink.refresh()
                        section.menuWanted(row.outputRows(), outputButton, 0, outputButton.height)
                    }
                }

                // Remove
                AppButton {
                    id: remove

                    objectName: "screenRemove"
                    height: 28
                    leftPadding: 10
                    rightPadding: 10
                    font.pixelSize: 12
                    text: "Remove"
                    onClicked: section.say(Screens.remove(row.screen.id))
                }
            }

            // What an NDI source is called, its size and its rate
            Row {
                spacing: 8
                visible: row.screen.output === "ndi"

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 12
                    text: "Called"
                }

                AppTextField {
                    objectName: "screenNdiName"
                    width: 200
                    height: 26
                    font.pixelSize: 12
                    text: row.screen.ndiName
                    onEditingFinished: {
                        const typed = text.trim()
                        const id = row.screen.id
                        const changed = typed !== "" && typed !== row.screen.ndiName
                        text = Qt.binding(() => row.screen.ndiName)
                        if (changed)
                            Screens.setOutput(id, { ndiName: typed })
                    }
                }

                AppComboBox {
                    id: sizeChoice

                    // The usual sizes, and the screen's own if it is none of them
                    readonly property var choices: {
                        const all = section.sizes.slice()
                        for (const own of [[row.screen.width, row.screen.height], [row.screen.ndiWidth, row.screen.ndiHeight]]) {
                            if (!all.some(size => size[0] === own[0] && size[1] === own[1]))
                                all.push(own)
                        }
                        return all
                    }

                    objectName: "screenNdiSize"
                    width: 130
                    height: 26
                    font.pixelSize: 12
                    model: choices.map(size => size[0] + " × " + size[1])
                    currentIndex: Math.max(0, choices.findIndex(size => size[0] === row.screen.ndiWidth && size[1] === row.screen.ndiHeight))
                    onActivated: (index) => Screens.setOutput(row.screen.id, { ndiWidth: choices[index][0], ndiHeight: choices[index][1] })
                }

                AppComboBox {
                    objectName: "screenNdiRate"
                    width: 110
                    height: 26
                    font.pixelSize: 12
                    model: Screens.rates.map(rate => rate + " fps")
                    currentIndex: Math.max(0, Screens.rates.indexOf(row.screen.ndiRate))
                    onActivated: (index) => Screens.setOutput(row.screen.id, { ndiRate: Screens.rates[index] })
                }
            }

            // A Blackmagic output's keying, as ProPresenter's Alpha settings have it: for an
            // output that can key (a linked pair), and not otherwise. Made only for a screen
            // that goes to a Blackmagic output: the settings are made at start, and these
            // controls on every screen's line would be work at every start for nothing.
            Loader {
                active: row.screen.output === "decklink"
                visible: active

                sourceComponent: Row {
                    spacing: 8
                    enabled: row.keys

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#9a9da3"
                        font.pixelSize: 12
                        text: "Alpha"
                    }

                    AppComboBox {
                        readonly property var values: ["none", "premultiplied", "straight"]

                        objectName: "screenDeckLinkAlpha"
                        width: 140
                        height: 26
                        font.pixelSize: 12
                        model: ["None", "Premultiplied", "Straight"]
                        currentIndex: Math.max(0, values.indexOf(row.screen.decklinkAlpha))
                        onActivated: (index) => Screens.setOutput(row.screen.id, { decklinkAlpha: values[index] })
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#9a9da3"
                        font.pixelSize: 12
                        text: "Key"
                    }

                    AppComboBox {
                        readonly property var values: ["internal", "external"]

                        objectName: "screenDeckLinkKey"
                        width: 110
                        height: 26
                        font.pixelSize: 12
                        enabled: row.keyed
                        model: ["Internal", "External"]
                        currentIndex: Math.max(0, values.indexOf(row.screen.decklinkKey))
                        onActivated: (index) => Screens.setOutput(row.screen.id, { decklinkKey: values[index] })
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#9a9da3"
                        font.pixelSize: 12
                        text: "Blend"
                    }

                    NumberField {
                        objectName: "screenDeckLinkBlend"
                        width: 64
                        from: 0
                        to: 100
                        step: 5
                        suffix: "%"
                        enabled: row.keyed
                        value: row.screen.decklinkBlend
                        onEdited: (value) => Screens.setOutput(row.screen.id, { decklinkBlend: Math.round(value) })
                    }
                }
            }

            // How it is going
            Text {
                objectName: "screenStatus"
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 12
                readonly property bool wrong: (row.screen.output === "ndi" && row.sender !== null && row.sender.problem !== "")
                                              || (row.screen.output === "ndi" && !Ndi.available)
                                              || (row.screen.output === "display" && !row.screen.displayThere)
                                              || (row.screen.output === "decklink" && ((row.sender !== null && row.sender.problem !== "")
                                                                                       || (DeckLink.looked && !DeckLink.available)))
                color: wrong ? "#ff9a8a" : "#9a9da3"
                text: row.screen.output === "none" ? "Not drawn. Things that name this screen still find it."
                    : row.screen.output === "display" ? (row.screen.displayThere ? "Fills that display." : "That display is not plugged in, so this screen is not shown.")
                    : row.screen.output === "window" ? "A small window that floats over this one. Double-click its title bar to fill the display it is on."
                    : row.screen.output === "decklink" ? (DeckLink.looked && !DeckLink.available ? DeckLink.problem
                        : row.sender === null ? ""
                        : row.sender.problem !== "" ? row.sender.problem
                        : !row.sender.sending ? "Switched off with the " + (row.screen.kind === "stage" ? "stage" : "audience") + " screens, on the toolbar."
                        : "Going out through " + row.screen.decklinkName + " as " + row.screen.decklinkFormat
                          + (row.keyed ? ", keyed " + row.screen.decklinkKey + "ly, " + row.screen.decklinkAlpha + ", at " + row.screen.decklinkBlend + "%." : ".")
                          + (row.card === null && DeckLink.looked ? " (It is not among the outputs found now.)" : ""))
                    : !Ndi.available ? "NDI's library is not on this computer yet, so this screen is not being sent."
                    : row.sender === null ? ""
                    : row.sender.problem !== "" ? row.sender.problem
                    : !row.sender.sending ? "Switched off with the " + (row.screen.kind === "stage" ? "stage" : "audience") + " screens, on the toolbar."
                    : "On the network, " + row.sender.width + " × " + row.sender.height + ". "
                      + (row.sender.receivers === 0 ? "Nothing is taking it yet." : row.sender.receivers === 1 ? "One thing is taking it."
                                                                                                                 : row.sender.receivers + " things are taking it.")
            }

            AppButton {
                id: getNdi

                objectName: "screenGetNdi"
                height: 26
                leftPadding: 10
                rightPadding: 10
                font.pixelSize: 12
                visible: row.screen.output === "ndi" && !Ndi.available
                text: "Get NDI's Library…"
                onClicked: section.ndiWanted()
            }
        }
    }

    Column {
        id: column

        width: section.width
        spacing: 12

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#b0b3b8"
            font.pixelSize: 13
            text: "A screen is somewhere the show is drawn for. An audience screen gets the slides, the media and the props; "
                + "a stage screen gets a stage layout. The screens are the workspace's, and are the ones ProPresenter has for it. "
                + "What each is sent out through is this computer's. The two buttons at the right of the toolbar switch all the "
                + "screens of a kind on and off together."
        }

        Repeater {
            model: [{ kind: "audience", title: "Audience screens", list: Screens.audience }, { kind: "stage", title: "Stage screens", list: Screens.stage }]

            Column {
                id: group

                required property var modelData

                width: column.width
                spacing: 8

                Row {
                    spacing: 10

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#e6e6e6"
                        font.pixelSize: 15
                        font.bold: true
                        text: group.modelData.title
                    }

                    AppButton {
                        objectName: group.modelData.kind === "stage" ? "addStageScreen" : "addAudienceScreen"
                        height: 26
                        leftPadding: 10
                        rightPadding: 10
                        font.pixelSize: 12
                        text: "+ Add"
                        enabled: Screens.screens.length < Screens.limit
                        onClicked: section.say(Screens.add(group.modelData.kind))
                    }
                }

                Repeater {
                    model: group.modelData.list

                    ScreenRow {}
                }

                Text {
                    visible: group.modelData.list.length === 0
                    color: "#9a9da3"
                    font.pixelSize: 12
                    text: "None."
                }
            }
        }

        Text {
            objectName: "screenCount"
            color: "#9a9da3"
            font.pixelSize: 12
            text: Screens.screens.length + " of the " + Screens.limit + " screens a workspace can have."
        }

        Text {
            objectName: "deckLinkNote"
            width: parent.width
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 12
            text: "Blackmagic: " + (!DeckLink.looked ? "its cards are looked for when a screen's menu of where it goes is opened."
                  : !DeckLink.available ? DeckLink.problem
                  : "Desktop Video " + DeckLink.version + ", " + (DeckLink.outputs.length === 0 ? "with no outputs." : DeckLink.outputs.length === 1 ? "one output." : DeckLink.outputs.length + " outputs."))
                + " The outputs are as Blackmagic's Desktop Video Setup names and arranges them: a pair of connectors linked there for fill "
                + "and key is one output, which can key. Video only, for now. DeckLink is Blackmagic Design's."
        }

        Text {
            objectName: "ndiNote"
            width: parent.width
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 12
            textFormat: Text.StyledText
            linkColor: "#6fb3ff"
            onLinkActivated: (link) => Qt.openUrlExternally(link)
            text: (Ndi.available ? "NDI: " + Ndi.version + ". " : "")
                + "NDI sends a screen over the local network to anything that takes it: a vision mixer, OBS, a monitor on another computer. "
                + "It is drawn and compressed on this computer, which is work: 1920 × 1080 at 30 frames a second is a fair place to start. "
                + "NDI® is a registered trademark of Vizrt NDI AB: <a href=\"https://ndi.video\">ndi.video</a>."
        }
    }
}
