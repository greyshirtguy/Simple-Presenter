import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import SimplePresenterApp

// The messages, a tab of the show controls: at the top the workspace's messages by
// name, and under them, with a line between that can be dragged to share out the
// height, what the one picked there is made of; at the bottom right, the button that
// shows it and hides it. They are ProPresenter's messages, kept in ProPresenter's own
// file of them (Configuration/Messages: see messagefile.h).
//
// A message is something to say on the screens over everything else: a parent wanted
// at the creche, a car with its lights on. It is shown on a layer of its own, over the
// props (see OutputScene), and any number can be shown at once.
//
// What one is made of, from the top:
//   - Its words, in a box. Among them can be TOKENS: places where something is put in
//     when the message is shown. A token is a small rounded label in the box, which is
//     moved over and deleted as one thing (see MessageText). The words are saved a
//     moment after the typing stops.
//   - The box that adds a token. Its arrow opens the tokens there are to pick from:
//     Name, Room and Message, which are given words by whoever shows the message; and
//     System Clock and Timer (which leads to the workspace's timers), which need
//     nothing typed and go on changing while the message is shown. A name typed into
//     the box, and Enter, makes a token of that name. A token goes in where the caret
//     is.
//   - A box for each token that is given words, to type them in before the message is
//     shown. They are kept, with the message, in ProPresenter's file.
//   - Its theme: the slide of a theme it is drawn in (Messages.slideOf says how), from
//     the menu of themes a slide's own menu has; or none, which is this app's own
//     plain band across the foot of the screen.
//   - How it goes away: Manually, or After Time, which leads to how long.
//
// What is on the screens is the message as it was when it was shown. Words typed while
// it is shown are not on the screens until it is shown again, and the button says
// Show, not Hide, while the message is other than what is being shown.
//
// GUESSES at ProPresenter, whose messages these are from a description and not from a
// look at them: the order of things in the pane, and what each is called; that a token
// already in a message is used again where one of its name is asked for a second
// time, so that one typing fills both places; and that a message that is shown is not
// changed by typing into it.
//
// It has not much room (see AudioPanel), so what a message is made of scrolls.
Item {
    id: panel

    // The operator window
    required property var win
    // The message being renamed in place, by id; "" for none
    readonly property string renaming: list.editingPath
    // The message picked in the list, by id, and the message itself, or null
    property string picked: ""
    readonly property var message: Messages.messages.find(candidate => candidate.id === picked) ?? null
    // How high the list is, what a message is made of having the rest: what is asked
    // for, kept within what leaves both something
    readonly property real listHeight: Math.max(36, Math.min(win.messagesSplit, height - 130))
    // Whether the message picked is shown; and whether it is shown as it now is
    readonly property bool shown: message !== null && win.liveMessages.includes(message.id)
    readonly property bool shownAsItIs: shown && JSON.stringify(win.messagesAsShown[message.id] ?? null) === JSON.stringify(message)

    // What the box of words holds: the message it was filled from and its text as the
    // file had it then, so that it is filled again only when another message is picked
    // or the words were changed from somewhere else, and never under what is being
    // typed. `typed` is whether there is typing that has not been saved, and `known`
    // the tokens there are to save, by id: the message's own and any put in since.
    property string loadedId: ""
    property string loadedText: ""
    property bool loading: false
    property bool typed: false
    property var known: ({})
    // The words typed for tokens and not yet saved, by the token's id
    property var pending: ({})

    // A new message, picked, with its name ready to be typed over.
    function add() {
        flush()
        const names = Messages.messages.map(candidate => candidate.name)
        let name = "New Message"
        for (let n = 2; names.includes(name); ++n)
            name = "New Message " + n
        const made = Messages.add(name)
        if (!win.report(made.error))
            return
        Log.note("messages", "a message added")
        picked = made.id
        list.editingPath = made.id
    }

    function pick(id) {
        if (id === picked)
            return
        flush()
        picked = id
    }

    // Fills the box of words from the message picked.
    function load() {
        loading = true
        const tokens = {}
        if (message !== null) {
            for (const token of message.tokens)
                tokens[token.id] = token
        }
        known = tokens
        bridge.load(words.textDocument, message !== null ? message.pieces : [], message !== null ? message.tokens : [], Screen.devicePixelRatio)
        loadedId = message !== null ? message.id : ""
        loadedText = message !== null ? message.text : ""
        typed = false
        pending = ({})
        loading = false
        syncFields()
    }

    // Saves the words as they are in the box, if they were typed in since they were
    // last saved. A token that is no longer in the box is no longer the message's.
    function saveWords() {
        idle.stop()
        if (!typed || loadedId === "" || !Messages.messages.some(candidate => candidate.id === loadedId))
            return
        typed = false
        const read = bridge.read(words.textDocument)
        const tokens = read.tokens.filter((id, place, all) => all.indexOf(id) === place).map(id => known[id]).filter(token => token !== undefined)
        loadedText = read.text
        win.report(Messages.setText(loadedId, read.text, tokens))
    }

    // Saves the words typed for the tokens.
    function saveValues() {
        const waiting = pending
        pending = ({})
        for (const id in waiting)
            win.report(Messages.setValue(loadedId, id, waiting[id]))
    }

    // Everything typed here and not yet saved is saved: before a message is shown,
    // before another is picked, and when the tab is left.
    function flush() {
        saveWords()
        saveValues()
    }

    // Puts a token in the box where the caret is, and saves.
    function putToken(token) {
        if (message === null)
            return
        const tokens = Object.assign({}, known)
        tokens[token.id] = token
        known = tokens
        words.cursorPosition = bridge.insert(words.textDocument, words.selectionStart, words.selectionEnd, token, Screen.devicePixelRatio)
        typed = true
        saveWords()
        words.forceActiveFocus()
    }

    // A token the message has already, of a kind and (for a text token) a name or (for
    // a timer's) a timer: it is used again, so that one thing typed fills every place
    // it stands in.
    function tokenLike(kind, name, timerId) {
        for (const id in known) {
            const token = known[id]
            if (token.kind !== kind)
                continue
            if (kind === "clock" || (kind === "text" && token.name.toLowerCase() === name.toLowerCase()) || (kind === "timer" && token.timerId === timerId))
                return token
        }
        return null
    }

    function addTextToken(name) {
        const wanted = name.replace(/\s+/g, " ").trim()
        if (wanted === "")
            return
        putToken(tokenLike("text", wanted, "") ?? { id: Messages.newId(), kind: "text", name: wanted, timerId: "", value: "" })
    }

    function addClockToken() {
        putToken(tokenLike("clock", "", "") ?? { id: Messages.newId(), kind: "clock", name: "", timerId: "", value: "" })
    }

    function addTimerToken(timer) {
        putToken(tokenLike("timer", "", timer.id) ?? { id: Messages.newId(), kind: "timer", name: timer.name, timerId: timer.id, value: "" })
    }

    // The tokens there are to pick from, as the rows of a menu
    function tokenItems() {
        const timers = Timers.timers.map(timer => ({ label: timer.name !== "" ? timer.name : "An unnamed timer", run: () => panel.addTimerToken(timer) }))
        return [{ header: "Add Token" }]
            .concat(Messages.usualTokens.map(name => ({ label: name, run: () => panel.addTextToken(name) })))
            .concat([{ divider: true },
                     { label: "System Clock", run: () => panel.addClockToken() },
                     { label: "Timer", disabled: timers.length === 0, items: [{ header: "Timer" }].concat(timers) }])
    }

    // The boxes for what the text tokens are given: kept in step with the message's
    // tokens by id, so that saving one leaves the box being typed in as it is.
    function syncFields() {
        const tokens = message !== null ? message.tokens.filter(token => token.kind === "text") : []
        for (let i = fields.count - 1; i >= 0; --i) {
            if (!tokens.some(token => token.id === fields.get(i).tokenId))
                fields.remove(i)
        }
        tokens.forEach((token, place) => {
            let row = -1
            for (let i = 0; i < fields.count; ++i) {
                if (fields.get(i).tokenId === token.id)
                    row = i
            }
            if (row < 0) {
                fields.insert(Math.min(place, fields.count), { tokenId: token.id, name: token.name, value: token.value })
            } else {
                fields.setProperty(row, "name", token.name)
                // (What is being typed for it is not typed over by what was saved.)
                if (pending[token.id] === undefined)
                    fields.setProperty(row, "value", token.value)
                if (row !== place && place < fields.count)
                    fields.move(row, place, 1)
            }
        })
    }

    // The theme a message is drawn in, as the menu's rows, and picking one
    function themeItems() {
        const id = message.id
        const pick = (place, slideId) => {
            const theme = Themes.theme(place)
            const index = theme ? theme.slides.findIndex(slide => slide.id === slideId) : -1
            if (index < 0)
                return
            Log.note("messages", "a message given the theme slide " + win.quoted(theme.slides[index].name) + " of " + win.quoted(theme.name))
            win.report(Messages.setTheme(id, theme.name, slideId, theme.slides[index].name, index))
        }
        return [{ header: "Theme" },
                { label: "None", current: message.theme === "", run: () => win.report(Messages.setTheme(id, "", "", "", 0)) },
                { divider: true }].concat(win.themePickItems(pick).slice(1))
    }

    // What the button for the theme says: the theme and which of its slides; and that
    // it is not here, for a theme the workspace has not
    readonly property string themeWords: {
        if (message === null || message.theme === "")
            return "None"
        const here = Themes.themes.some(theme => theme.name === message.theme)
        return message.theme + (message.themeSlideName !== "" ? ": " + message.themeSlideName : "") + (here ? "" : " (not here)")
    }

    readonly property var times: [5, 10, 15, 20, 30, 45, 60]

    function hideItems() {
        const id = message.id
        const set = (clear, seconds) => {
            Log.note("messages", "a message set to go " + (clear === 1 ? "after " + seconds + " seconds" : "by hand"))
            win.report(Messages.setClear(id, clear, seconds))
        }
        return [{ header: "Hide" },
                { label: "Manually", current: message.clear !== 1, run: () => set(0, 0) },
                { label: "After Time", current: message.clear === 1, items: [{ header: "After Time" }].concat(times.map(seconds => ({
                    label: seconds + " seconds", current: message.clear === 1 && message.seconds === seconds, run: () => set(1, seconds) }))) }]
    }

    // (ProPresenter has a third way, when the message's timers have run out, which is
    // not done here: such a message stays until it is hidden.)
    readonly property string hideWords: message === null ? "" : message.clear === 1 ? "After " + message.seconds + " seconds"
                                        : message.clear === 2 ? "Manually (its timers are not waited for here)" : "Manually"

    // Shows the message picked, as it now is; or, while that is what is shown, hides it.
    function toggle() {
        if (message === null)
            return
        flush()
        if (shownAsItIs)
            win.hideMessage(message.id)
        else
            win.showMessage(message.id)
        win.takeFocus()
    }

    function showItemMenu(id, item) {
        const found = Messages.messages.find(candidate => candidate.id === id)
        if (!found)
            return
        const on = win.liveMessages.includes(id)
        win.showMenu([
            { label: on ? "Hide" : "Show", run: () => {
                panel.flush()
                if (on)
                    win.hideMessage(id)
                else
                    win.showMessage(id)
            } },
            { divider: true },
            { label: "Rename", run: () => {
                panel.pick(id)
                list.editingPath = id
            } },
            { label: "Remove…", run: () => win.showMenu([
                { header: "Remove “" + found.name + "”?" },
                { label: "Remove", danger: true, run: () => {
                    Log.note("messages", "a message removed")
                    win.report(Messages.remove(id))
                } },
                { label: "Cancel", run: () => {} }
            ], item) }
        ], item)
    }

    onMessageChanged: {
        // Another message, or the same one with other words than the box was filled
        // from or last saved: the box is filled again. (What was just saved from the
        // box comes back the same, and the box is left alone.)
        if (message === null || message.id !== loadedId || (message.text !== loadedText && !typed))
            load()
        else
            syncFields()
    }
    onVisibleChanged: {
        if (!visible)
            flush()
    }
    Component.onCompleted: {
        if (Messages.messages.length > 0)
            picked = Messages.messages[0].id
    }

    Connections {
        target: Messages

        // The message that was picked has gone, or there was none: the first is.
        function onChanged() {
            if (!Messages.messages.some(candidate => candidate.id === panel.picked))
                panel.picked = Messages.messages.length > 0 ? Messages.messages[0].id : ""
        }
    }

    // A button that says what is chosen of something, under a word for what it is, and
    // opens the menu it is chosen from
    component Picker: Rectangle {
        id: picker

        property string caption
        property string says
        signal clicked

        width: 100
        height: 24
        radius: 6
        color: pickerMouse.pressed ? "#50535a" : pickerMouse.containsMouse ? "#45484e" : "#3a3c42"

        Text {
            id: pickerCaption

            x: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#9a9da3"
            font.pixelSize: 11
            text: picker.caption
        }

        Text {
            anchors.left: pickerCaption.right
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: "#e6e6e6"
            font.pixelSize: 12
            text: picker.says
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#9a9da3"
            font.pixelSize: 10
            text: "▾"
        }

        MouseArea {
            id: pickerMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: picker.clicked()
        }
    }

    MessageText {
        id: bridge
    }

    ListModel {
        id: fields
    }

    Timer {
        id: idle

        interval: 700
        onTriggered: panel.saveWords()
    }

    // ---- The messages, on the lighter ground

    Rectangle {
        id: listArea

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: panel.listHeight
        radius: 6
        color: panel.win.surfaceColor

        SidebarList {
            id: list

            objectName: "messageList"
            anchors.fill: parent
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            // (The top right corner is the place of the + that adds a message, which
            // the show controls put there for every tab.)
            anchors.rightMargin: 30
            baseIndent: 10
            model: Messages.messages.map(candidate => ({ path: candidate.id, name: candidate.name, live: panel.win.liveMessages.includes(candidate.id) }))
            selectedPath: panel.picked
            onPicked: (entry) => panel.pick(entry.path)
            onMenuRequested: (entry, item) => panel.showItemMenu(entry.path, item)
            onRenamed: (entry, name) => {
                Log.note("messages", "a message renamed")
                panel.win.report(Messages.rename(entry.path, name))
            }
            onEditingEnded: panel.win.takeFocus()
        }

        EmptyNote {
            anchors.centerIn: parent
            width: parent.width - 24
            visible: list.count === 0
            font.pixelSize: 12
            text: "No messages. Add one with the + above."
        }
    }

    // The line between the two, which is dragged to share out the height
    Item {
        id: splitter

        objectName: "messagesSplit"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: listArea.bottom
        height: 7

        Rectangle {
            anchors.centerIn: parent
            width: 36
            height: 3
            radius: 1.5
            color: splitMouse.pressed ? panel.win.accentColor : splitMouse.containsMouse ? "#8d9097" : "#50535a"
        }

        MouseArea {
            id: splitMouse

            property real startedAt
            property real startedWith

            anchors.fill: parent
            anchors.topMargin: -2
            anchors.bottomMargin: -2
            hoverEnabled: true
            cursorShape: Qt.SplitVCursor
            preventStealing: true
            onPressed: (mouse) => {
                startedAt = mapToItem(panel, mouse.x, mouse.y).y
                startedWith = panel.listHeight
            }
            onPositionChanged: (mouse) => {
                if (pressed)
                    panel.win.messagesSplit = Math.max(36, Math.min(startedWith + mapToItem(panel, mouse.x, mouse.y).y - startedAt, panel.height - 130))
            }
            onReleased: panel.win.takeFocus()
        }
    }

    // ---- What the message picked is made of, on the darker ground

    Rectangle {
        id: details

        objectName: "messageDetails"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: splitter.bottom
        anchors.bottom: parent.bottom
        radius: 6
        color: panel.win.panelColor

        EmptyNote {
            anchors.centerIn: parent
            width: parent.width - 24
            visible: panel.message === null && list.count > 0
            font.pixelSize: 12
            text: "Pick a message above."
        }

        Flickable {
            id: scroll

            objectName: "messageScroll"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: foot.top
            anchors.margins: 8
            anchors.bottomMargin: 4
            visible: panel.message !== null
            clip: true
            contentHeight: made.height
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            Column {
                id: made

                width: scroll.width - 8
                spacing: 6

                // Its words, with its tokens among them
                Rectangle {
                    width: parent.width
                    height: Math.max(46, Math.min(96, words.implicitHeight + 12))
                    radius: 6
                    color: "#15161a"
                    border.width: 1
                    border.color: words.activeFocus ? panel.win.accentColor : "#45484e"

                    Flickable {
                        id: wordsView

                        anchors.fill: parent
                        anchors.margins: 6
                        clip: true
                        contentHeight: words.height
                        boundsBehavior: Flickable.StopAtBounds

                        // Keeps the caret in sight as it moves.
                        function follow(caret) {
                            if (caret.y < contentY)
                                contentY = Math.max(0, caret.y)
                            else if (caret.y + caret.height > contentY + height)
                                contentY = caret.y + caret.height - height
                        }

                        TextEdit {
                            id: words

                            objectName: "messageWords"
                            width: wordsView.width
                            height: Math.max(wordsView.height, implicitHeight)
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.Wrap
                            color: "#e6e6e6"
                            font.pixelSize: 13
                            selectionColor: "#4d6f9a"
                            selectByMouse: true
                            onCursorRectangleChanged: wordsView.follow(cursorRectangle)
                            onTextChanged: {
                                if (panel.loading)
                                    return
                                panel.typed = true
                                idle.restart()
                            }
                            onActiveFocusChanged: {
                                if (!activeFocus)
                                    panel.saveWords()
                            }
                            // Esc gives the keys back to the show, what was typed kept.
                            Keys.onEscapePressed: {
                                panel.saveWords()
                                panel.win.takeFocus()
                            }
                        }

                        Text {
                            objectName: "messageWordsPlaceholder"
                            visible: words.length === 0 && !words.activeFocus
                            color: "#7d8088"
                            font.pixelSize: 13
                            text: "What the message says"
                        }
                    }
                }

                // A token put in: one picked from the arrow's menu, or one of a name
                // typed here
                Item {
                    width: parent.width
                    height: 24

                    AppTextField {
                        id: tokenName

                        objectName: "messageTokenName"
                        anchors.left: parent.left
                        anchors.right: tokenPick.left
                        anchors.rightMargin: 4
                        height: 24
                        font.pixelSize: 12
                        placeholderText: "Add a token: pick one, or type its name"
                        placeholderTextColor: "#7d8088"
                        onAccepted: {
                            const name = text
                            text = ""
                            panel.addTextToken(name)
                        }
                        Keys.onEscapePressed: {
                            text = ""
                            panel.win.takeFocus()
                        }
                    }

                    Rectangle {
                        id: tokenPick

                        objectName: "messageTokenPick"
                        anchors.right: parent.right
                        width: 26
                        height: 24
                        radius: 6
                        color: tokenMouse.pressed ? "#50535a" : tokenMouse.containsMouse ? "#45484e" : "#3a3c42"

                        Text {
                            anchors.centerIn: parent
                            color: "#c9cdd6"
                            font.pixelSize: 10
                            text: "▾"
                        }

                        MouseArea {
                            id: tokenMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: panel.win.showMenu(panel.tokenItems(), tokenPick)
                        }
                    }
                }

                // What each token that is given words is given
                Repeater {
                    id: fieldRows

                    model: fields

                    delegate: Item {
                        id: field

                        required property string tokenId
                        required property string name
                        required property string value

                        objectName: "messageToken"
                        width: made.width
                        height: 24

                        Text {
                            id: fieldName

                            width: Math.min(90, parent.width * 0.34)
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            color: "#c9cdd6"
                            font.pixelSize: 12
                            text: field.name
                        }

                        AppTextField {
                            objectName: "messageTokenValue"
                            anchors.left: fieldName.right
                            anchors.leftMargin: 6
                            anchors.right: parent.right
                            height: 24
                            font.pixelSize: 12
                            text: field.value
                            onTextEdited: {
                                const waiting = Object.assign({}, panel.pending)
                                waiting[field.tokenId] = text
                                panel.pending = waiting
                            }
                            onEditingFinished: panel.saveValues()
                            onAccepted: panel.win.takeFocus()
                            Keys.onEscapePressed: panel.win.takeFocus()
                        }
                    }
                }

                // Its theme, and how it goes away, each on a line of its own: what
                // either says can be long
                Column {
                    width: parent.width
                    spacing: 6

                    Picker {
                        id: themePick

                        objectName: "messageTheme"
                        width: made.width
                        caption: "Theme"
                        says: panel.themeWords
                        onClicked: panel.win.showMenu(panel.themeItems(), themePick)
                    }

                    Picker {
                        id: hidePick

                        objectName: "messageHide"
                        width: made.width
                        caption: "Hide"
                        says: panel.hideWords
                        onClicked: panel.win.showMenu(panel.hideItems(), hidePick)
                    }
                }
            }
        }

        // Along the bottom: at the right, the button that shows the message and hides
        // it. It is orange while the message is on the screens as it now is.
        Item {
            id: foot

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 30
            visible: panel.message !== null

            Text {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - showButton.width - 28
                elide: Text.ElideRight
                color: panel.shown ? "#ffd9a8" : panel.win.dimTextColor
                font.pixelSize: 11
                text: !panel.shown ? "" : panel.shownAsItIs ? "Showing" : "Showing as it was: Show shows it as it is now"
            }

            Rectangle {
                id: showButton

                objectName: "messageShow"
                readonly property bool hides: panel.shownAsItIs
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 64
                height: 24
                radius: 6
                color: hides ? (showMouse.pressed ? "#ffa64d" : panel.win.accentColor)
                             : showMouse.pressed ? "#50535a" : showMouse.containsMouse ? "#45484e" : "#3a3c42"

                Text {
                    anchors.centerIn: parent
                    color: showButton.hides ? "#1b1c1f" : "#e6e6e6"
                    font.pixelSize: 12
                    font.bold: showButton.hides
                    text: showButton.hides ? "Hide" : "Show"
                }

                MouseArea {
                    id: showMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: panel.toggle()
                }
            }
        }
    }
}
