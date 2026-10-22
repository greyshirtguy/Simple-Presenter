import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The stage message, and the messages. The stage message: its box and button along the bottom of the stage tab; a
// message shown, which every text box linked to the stage message then shows, in the stage window and in what the
// operator sees of the stage, a box set to flash flashing its colour for two seconds; other words typed, which wait
// to be shown; hidden; and Stage Message among what a text box can show in the editor, with its flash. The messages:
// their tab, the last of the show controls, and the button that clears their layer; a file of them as ProPresenter
// might have left it, read; a message made and named; its words typed, with tokens put among them (one picked, one
// of a name typed, the clock, a timer) and what a token is given typed in its box; its theme and how it goes away,
// each from its menu; shown, over the props, on the output and in the operator's preview, with what its tokens were
// given in its words; that what is typed while it is shown waits to be shown; that it goes by itself after its
// time; two at once; the clear button, F6, a clear group and an action; a token taken out of the words; and a
// message removed. What the files then hold is looked at when the test is over, by after_messages in run.py.
QtObject {
    id: t

//COMMON
    function tabs() {
        return Lib.findAll(sidePanel, item => item.chosen !== undefined && item.modelData !== undefined && item.modelData.id !== undefined)
    }

    function tab(id) {
        return tabs().find(item => item.modelData.id === id)
    }

    function layout(name) {
        return StageLayouts.layouts.find(l => l.name === name)
    }

    // The stage as the operator sees it, and the boxes of the layout drawn in that
    function stagePreview() {
        return Lib.findAll(sidePanel, item => item.slide !== undefined && item.slideWidth !== undefined && item.showsNow !== undefined)
                  .find(item => item.visible && stageLayout !== null && item.slide !== null && item.slide.label === stageLayout.name)
    }

    function stageBox(test) {
        const preview = stagePreview()
        return preview ? Lib.findAll(preview, item => item.source !== undefined && item.textBleed !== undefined && item.boxX !== undefined).find(item => test(item.source)) : undefined
    }

    // How much of an item is red, and how much lighter than a grey, as the operator window draws it
    function red(item) {
        const at = item.mapToItem(null, 0, 0)
        return testInput.windowRed(at.x, at.y, item.width, item.height)
    }

    function lighter(item, grey) {
        const at = item.mapToItem(null, 0, 0)
        return testInput.windowLighter(at.x, at.y, item.width, item.height, grey)
    }

    // The messages as the file has them now, read by the app's own reader
    function onDisk(name) {
        return Messages.messages.find(m => m.name === name)
    }

    function panel() {
        return Lib.find(sidePanel, item => item.objectName === "messageDetails") !== null ? Lib.find(sidePanel, item => item.objectName === "messageDetails").parent : null
    }

    function listRow(name) {
        const list = named("messageList")
        let index = -1
        for (let i = 0; i < list.count && index < 0; ++i) {
            if (list.model[i].name === name)
                index = i
        }
        if (index < 0)
            return null
        list.positionViewAtIndex(index, ListView.Contain)
        return list.itemAtIndex(index)
    }

    // The boxes for what the tokens are given, as { name, field }
    function tokenFields() {
        return Lib.findAll(named("messageDetails"), item => item.objectName === "messageToken")
                  .map(row => ({ name: row.name, id: row.tokenId, field: Lib.find(row, item => item.objectName === "messageTokenValue") }))
    }

    // The message's text box on the output and in the operator's preview: the thing that draws the words
    function shownOn(root, id) {
        return Lib.findAll(root, item => item.source !== undefined && item.liveLinkText !== undefined && item.source.linkKind === "message")
                  .find(item => item.source.linkMessageId === id)
    }

    // How far apart two colours are, as the most any of their three parts differs
    function apart(a, b) {
        const part = (c, i) => parseInt(String(c).substring(1 + 2 * i, 3 + 2 * i), 16)
        return Math.max(Math.abs(part(a, 0) - part(b, 0)), Math.abs(part(a, 1) - part(b, 1)), Math.abs(part(a, 2) - part(b, 2)))
    }

    // The colour of the window at the left of whatever is at a place in the box of words: a token's label, if one is there
    function labelAt(position) {
        const words = named("messageWords")
        const at = words.positionToRectangle(position)
        const point = words.mapToItem(null, at.x + 4, at.y + at.height / 2)
        return String(testInput.windowPixel(point.x, point.y))
    }

    // Something of the editor's inspector, by its name, if it is showing
    function shown(name) {
        const found = Lib.find(editScreen.inspector, item => item.objectName === name)
        return found !== null && found.visible ? found : null
    }

    readonly property string obj: String.fromCharCode(0xfffc)

    // Something of what a message is made of, scrolled into sight (there is not much room: see MessagesPanel), and
    // where its middle then is on the window
    function sight(item) {
        const scroll = named("messageScroll")
        const at = item.mapToItem(scroll.contentItem, 0, 0)
        if (at.y < scroll.contentY)
            scroll.contentY = Math.max(0, at.y - 2)
        else if (at.y + item.height > scroll.contentY + scroll.height)
            scroll.contentY = Math.min(Math.max(0, scroll.contentHeight - scroll.height), at.y + item.height - scroll.height + 2)
        return centre(item)
    }

    function run() {
        steps = [
            // ---- The stage message
            () => {
                // (The stage in the operator's preview, which is what these look at: the first audience screen is what it shows otherwise.)
                pickPreviewScreen(Screens.stage[0].id)
                click(centre(tab("stage")))
                return 500
            },
            () => {
                const area = named("stageMessage")
                const field = named("stageMessageText")
                const show = named("stageMessageShow")
                const control = tab("stage").parent.parent
                const at = area.mapToItem(control, 0, 0)
                check("the stage tab has the stage message along its bottom: a box to type in and a button", area !== null && area.visible && field.visible && show.visible
                      && at.y > control.height * 0.5 && Math.abs(at.y + area.height - control.height) < 2, Math.round(at.y) + " of " + Math.round(control.height))
                check("with nothing typed there is nothing to show", field.text === "" && !show.enabled && show.text === "Show" && Show.stageMessage === "")
                Show.setStageLayout(Show.stageScreens[0].id, layout("Singing").id)
                return 900
            },
            () => {
                const told = stageBox(e => e.linkKind === "stageMessage")
                kept.box = told
                check("ProPresenter's own layout has a box linked to the stage message, set to flash red, which shows nothing yet", told !== undefined && told.source.linkFlash === true
                      && String(told.source.linkFlashColor) === "#ff2d21" && told.liveLinkText === "" && red(told) === 0,
                      told ? JSON.stringify([told.source.linkFlash, String(told.source.linkFlashColor), told.liveLinkText]) : "no such box")
                kept.stageDark = testInput.red(true, 0, 0, 1, 1)
                click(centre(named("stageMessageText")))
                return 300
            },
            () => {
                testInput.type("Two minutes")
                return 300
            },
            () => {
                check("what is typed is not shown until it is asked for", Show.stageMessage === "" && named("stageMessageShow").enabled && stageMessageText === "Two minutes")
                testInput.key(Qt.Key_Return)
                return 60
            },
            () => {
                const told = stageBox(e => e.linkKind === "stageMessage")
                check("Enter shows it: every box linked to the stage message has its words", Show.stageMessage === "Two minutes" && told.liveLinkText === "Two minutes"
                      && named("stageMessageShow").text === "Hide", Show.stageMessage)
                check("and a box set to flash has its flash colour for a fill at once, in the stage window and in what the operator sees of it", Show.stageFlash === true
                      && testInput.red(true, 0, 0, 1, 1) > kept.stageDark + 0.01 && red(told) > 0.3, testInput.red(true, 0, 0, 1, 1) + " and " + red(told))
                testInput.grabStage("1-stage-message-flash")
                testInput.grab("1-operator-stage-message")
                kept.flashes = 0
                kept.was = Show.stageFlash
                return 130
            },
            // (The beat is four a second for two seconds: looked at eight times, a little off the beat.)
            ...Array.from({ length: 15 }, () => () => {
                if (Show.stageFlash !== kept.was)
                    ++kept.flashes
                kept.was = Show.stageFlash
                return 130
            }),
            () => {
                const told = stageBox(e => e.linkKind === "stageMessage")
                check("it goes on and off for two seconds", kept.flashes >= 5, kept.flashes + " changes seen")
                return 500
            },
            () => {
                const told = stageBox(e => e.linkKind === "stageMessage")
                check("and then the box is as it was, with the words still in it", Show.stageFlash === false && red(told) < 0.02 && told.liveLinkText === "Two minutes"
                      && lighter(told, 120) > 0.01, red(told) + " red, " + lighter(told, 120) + " light")
                testInput.grabStage("2-stage-message")
                // Other words, which wait
                click(centre(named("stageMessageText")))
                return 300
            },
            () => {
                testInput.type(" left")
                return 300
            },
            () => {
                const told = stageBox(e => e.linkKind === "stageMessage")
                check("other words typed while it is shown wait to be shown, and the button says Show again", Show.stageMessage === "Two minutes" && told.liveLinkText === "Two minutes"
                      && named("stageMessageShow").text === "Show" && stageMessageText === "Two minutes left")
                click(centre(named("stageMessageShow")))
                check("the button shows them, and the box flashes again", Show.stageMessage === "Two minutes left" && Show.stageFlash === true && named("stageMessageShow").text === "Hide",
                      Show.stageMessage + " " + Show.stageFlash)
                return 300
            },
            () => {
                click(centre(named("stageMessageShow")))
                return 400
            },
            () => {
                const told = stageBox(e => e.linkKind === "stageMessage")
                check("and then hides them: the box shows nothing, and does not flash", Show.stageMessage === "" && told.liveLinkText === "" && Show.stageFlash === false
                      && red(told) === 0 && named("stageMessageShow").text === "Show")
                check("what was typed is kept in the box", named("stageMessageText").text === "Two minutes left")
            },

            // ---- The messages: their tab, the button that clears them, and ProPresenter's file read
            () => {
                const clear = named("clearMessages")
                check("the row of clears has a fifth button, for the messages, grey while none is shown", clear !== null && clear.visible && !clear.live && clear.picture === "ClearMessages")
                check("and the show controls have a sixth tab, with ProPresenter's paper plane on it", tabs().length === 6 && tab("messages") !== undefined
                      && tab("messages").modelData.picture === "Message", tabs().length)
                click(centre(tab("messages")))
                return 800
            },
            () => {
                const list = named("messageList")
                const words = named("messageWords")
                check("the tab lists the messages of ProPresenter's file, the first of them picked", list !== null && list.count === 2 && list.model[0].name === "Service Countdown"
                      && list.model[1].name === "New Message" && list.selectedPath === Messages.messages[0].id, list ? list.count : "no list")
                check("its words are in the box, its token one thing among them", words.text === "Starting in " + obj, JSON.stringify(words.text))
                check("drawn as a label: a timer's is grey-blue", apart(labelAt(12), "#56677f") < 40, labelAt(12))
                check("a timer's token is given nothing to type", tokenFields().length === 0)
                check("its theme and how it goes away are said", named("messageTheme").says.startsWith("New Life Chapel") && named("messageHide").says.startsWith("Manually"),
                      named("messageTheme").says + " / " + named("messageHide").says)
                testInput.grab("3-messages-tab")
                click(centre(named("showControlAdd")))
                return 600
            },
            () => {
                const made = Messages.messages[2]
                check("the + makes a message, picked, with its name there to be typed over", Messages.messages.length === 3 && made.name === "New Message 2"
                      && named("messageList").editingPath === made.id && sidePanel.renaming === true, Messages.messages.map(m => m.name).join("|"))
                testInput.type("Parent Wanted")
                testInput.key(Qt.Key_Return)
                return 500
            },
            () => {
                kept.id = onDisk("Parent Wanted") ? onDisk("Parent Wanted").id : ""
                check("and it is called what was typed", kept.id !== "" && sidePanel.renaming === false && named("messageList").selectedPath === kept.id)
                check("a new message has no words, no tokens and no theme, and goes by hand", named("messageWords").text === "" && tokenFields().length === 0
                      && named("messageTheme").says === "None" && named("messageHide").says === "Manually", named("messageTheme").says + " / " + named("messageHide").says)
                // ---- Its words, and tokens among them
                click(sight(named("messageWords")))
                return 300
            },
            () => {
                testInput.type("Would ")
                click(sight(named("messageTokenPick")))
                return 400
            },
            () => {
                check("the arrow beside the box for a token has the tokens there are: three that are given words, the clock, and the timers",
                      labels() === "[Add Token], Name, Room, Message, --, System Clock, Timer", labels())
                const timers = menu.shown.find(i => i.label === "Timer")
                check("the timers being the workspace's", timers.items.filter(i => i.label !== undefined).length === Timers.timers.length && Timers.timers.length > 0,
                      timers.items.map(i => i.label ?? i.header).join("|"))
                click(menuRow("Name"))
                return 600
            },
            () => {
                const m = onDisk("Parent Wanted")
                const fields = tokenFields()
                check("picked, a token goes in where the caret is", named("messageWords").text === "Would " + obj, JSON.stringify(named("messageWords").text))
                check("and is the message's, in the file, with a box for what it is to be given", m.tokens.length === 1 && m.tokens[0].kind === "text" && m.tokens[0].name === "Name"
                      && m.text === "Would ${" + m.tokens[0].id + "}" && fields.length === 1 && fields[0].name === "Name", m.text + " " + fields.map(f => f.name).join("|"))
                check("it is drawn as a label: one that is given words is blue", apart(labelAt(6), "#1e6fc4") < 40, labelAt(6))
                check("and the caret is after it, in the box, to go on typing", named("messageWords").activeFocus && named("messageWords").cursorPosition === 7,
                      named("messageWords").activeFocus + " " + named("messageWords").cursorPosition)
                testInput.type(" come to the ")
                // A token of a name that is typed
                click(sight(named("messageTokenName")))
                return 300
            },
            () => {
                testInput.type("Seat")
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                const m = onDisk("Parent Wanted")
                check("a name typed in the box for a token, and Enter, makes a token of that name", named("messageWords").text === "Would " + obj + " come to the " + obj
                      && m.tokens.length === 2 && m.tokens[1].name === "Seat" && tokenFields().map(f => f.name).join("|") === "Name|Seat" && named("messageTokenName").text === "",
                      JSON.stringify(named("messageWords").text) + " " + tokenFields().map(f => f.name).join("|"))
                testInput.type(" at ")
                click(sight(named("messageTokenPick")))
                return 400
            },
            () => {
                click(menuRow("System Clock"))
                return 600
            },
            () => {
                const m = onDisk("Parent Wanted")
                check("the clock is a token that is given nothing to type", m.tokens.length === 3 && m.tokens[2].kind === "clock" && tokenFields().length === 2,
                      m.tokens.map(x => x.kind).join("|"))
                testInput.type(". ")
                click(sight(named("messageTokenPick")))
                return 400
            },
            () => {
                click(menuRow("Name"))
                return 600
            },
            () => {
                const m = onDisk("Parent Wanted")
                const places = m.pieces.filter(p => p.token).map(p => p.text)
                check("a token the message has, asked for again, is that token again: what is typed for it fills both places",
                      named("messageWords").text === "Would " + obj + " come to the " + obj + " at " + obj + ". " + obj && m.tokens.length === 3 && places.length === 4
                      && places[0] === places[3] && tokenFields().length === 2, JSON.stringify(named("messageWords").text) + " " + m.tokens.length)
                kept.saved = m.text
                testInput.type("!")
                return 150
            },
            () => {
                check("what is typed is not written at every key", onDisk("Parent Wanted").text === kept.saved)
                return 1000
            },
            () => {
                check("and is, a moment after the typing stops", onDisk("Parent Wanted").text === kept.saved + "!", onDisk("Parent Wanted").text)
                // ---- What its tokens are given
                click(sight(tokenFields()[0].field))
                return 300
            },
            () => {
                testInput.type("Sam Lee")
                click(sight(tokenFields()[1].field))
                return 500
            },
            () => {
                check("words typed for a token are kept with the message when the box is left", onDisk("Parent Wanted").tokens[0].value === "Sam Lee" && tokenFields()[1].field.activeFocus,
                      JSON.stringify(onDisk("Parent Wanted").tokens[0].value) + " " + tokenFields()[1].field.activeFocus)
                testInput.type("Row 12")
                testInput.key(Qt.Key_Return)
                return 500
            },
            () => {
                check("and with Enter", onDisk("Parent Wanted").tokens[1].value === "Row 12" && tokenFields()[0].field.text === "Sam Lee" && tokenFields()[1].field.text === "Row 12")
                // ---- Its theme, and how it goes away
                click(sight(named("messageTheme")))
                return 400
            },
            () => {
                check("its theme is picked from the menu of themes, or is none", labels().startsWith("[Theme], *None, --, ") && labels().includes("New Life Chapel"), labels())
                click(menuRow("New Life Chapel"))
                return 600
            },
            () => {
                kept.themeSlide = menu.shown.find(i => i.label !== undefined && i.run !== undefined).label
                click(menuRow(kept.themeSlide))
                return 600
            },
            () => {
                const m = onDisk("Parent Wanted")
                check("a slide of a theme that is picked is the message's, and the button says so", m.theme === "New Life Chapel" && m.themeSlideName === kept.themeSlide && m.themeSlide !== ""
                      && named("messageTheme").says === "New Life Chapel: " + kept.themeSlide, m.theme + " / " + m.themeSlideName + " / " + named("messageTheme").says)
                click(sight(named("messageHide")))
                return 400
            },
            () => {
                check("how it goes away is by hand, or after a time", labels() === "[Hide], *Manually, After Time", labels())
                const after = menu.shown.find(i => i.label === "After Time")
                check("which is one of seven", after.items.filter(i => i.label !== undefined).map(i => i.label).join("|")
                      === "5 seconds|10 seconds|15 seconds|20 seconds|30 seconds|45 seconds|60 seconds", after.items.map(i => i.label ?? i.header).join("|"))
                click(menuRow("After Time"))
                return 600
            },
            () => {
                click(menuRow("5 seconds"))
                return 600
            },
            () => {
                const m = onDisk("Parent Wanted")
                check("after five seconds, say", m.clear === 1 && m.seconds === 5 && named("messageHide").says === "After 5 seconds", m.clear + " " + m.seconds + " " + named("messageHide").says)
                testInput.grab("5-message-made")
                // ---- Shown (and the audience screen in the operator's preview again, which is what this looks at)
                pickPreviewScreen(Screens.audience[0].id)
                kept.outputDark = testInput.lit(false, 0, 0, 1, 1)
                click(centre(named("messageShow")))
                return 900
            },
            () => {
                const out = shownOn(output.contentItem, kept.id)
                const preview = shownOn(named("previewMessages"), kept.id)
                const words = out ? String(out.liveLinkText) : ""
                kept.shownAt = Date.now()
                check("the button shows it: it is on the messages' layer", Show.liveMessages.length === 1 && Show.liveMessages[0] === kept.id && liveMessages.length === 1, Show.liveMessages.join())
                check("its words are the message's, with what each token was given in them and the time of day", /^Would Sam Lee come to the Row 12 at \d{1,2}:\d\d.*\. Sam Lee!$/.test(words), words)
                check("drawn on the output, and in what the operator sees of the output", out !== undefined && preview !== undefined && testInput.lit(false, 0, 0, 1, 1) > kept.outputDark + 0.002
                      && lighter(preview, 100) > 0.01, testInput.lit(false, 0, 0, 1, 1) + " from " + kept.outputDark + ", " + (preview ? lighter(preview, 100) : "no preview"))
                check("the button that clears the messages is lit, the message is marked in the list, and its own button says Hide", named("clearMessages").live
                      && listRow("Parent Wanted").modelData.live === true && named("messageShow").hides)
                testInput.grabOutput("6-output-message")
                testInput.grab("6-operator-message")
                // ---- What is typed while it is shown waits to be shown
                click(sight(tokenFields()[0].field))
                return 300
            },
            () => {
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Kim Wu")
                testInput.key(Qt.Key_Return)
                return 500
            },
            () => {
                const out = shownOn(output.contentItem, kept.id)
                check("words typed for a token while the message is shown are kept, and are not on the screens", onDisk("Parent Wanted").tokens[0].value === "Kim Wu"
                      && String(out.liveLinkText).startsWith("Would Sam Lee"), out ? out.liveLinkText : "not shown")
                check("and the button says Show, for showing the message as it now is", Show.liveMessages.length === 1 && !named("messageShow").hides)
                click(centre(named("messageShow")))
                return 600
            },
            () => {
                const out = shownOn(output.contentItem, kept.id)
                check("which it then does", out !== undefined && String(out.liveLinkText).startsWith("Would Kim Wu") && named("messageShow").hides, out ? out.liveLinkText : "not shown")
                return 3000
            },
            () => {
                check("three seconds on it is still there (its five seconds began again when it was shown again)", Show.liveMessages.length === 1)
                return 2700
            },
            () => {
                check("and after its five seconds it has gone by itself", Show.liveMessages.length === 0 && !named("clearMessages").live && !named("messageShow").hides, Show.liveMessages.join())
                return 700
            },
            () => {
                check("and is drawn no longer", Math.abs(testInput.lit(false, 0, 0, 1, 1) - kept.outputDark) < 0.001, testInput.lit(false, 0, 0, 1, 1) + " from " + kept.outputDark)
                // ---- Two at once, and the clears
                report(Messages.setClear(kept.id, 0, 0))
                showMessage(kept.id)
                showMessage(Messages.messages[0].id)
                return 900
            },
            () => {
                const first = shownOn(output.contentItem, Messages.messages[0].id)
                check("two messages can be shown at once, the later in front", Show.liveMessages.join() === kept.id + "," + Messages.messages[0].id && shownOn(output.contentItem, kept.id) !== undefined
                      && first !== undefined, Show.liveMessages.join())
                check("a timer in a message is the timer's time", first !== undefined && /^Starting in \d+:\d\d$/.test(String(first.liveLinkText)), first ? first.liveLinkText : "not shown")
                testInput.grabOutput("7-output-two-messages")
                click(centre(named("clearMessages")))
                return 600
            },
            () => {
                check("the button clears them all", Show.liveMessages.length === 0 && !named("clearMessages").live)
                showMessage(kept.id)
                testInput.key(Qt.Key_F6)
                return 500
            },
            () => {
                check("so does F6", Show.liveMessages.length === 0)
                showMessage(kept.id)
                testInput.key(Qt.Key_F1)
                return 500
            },
            () => {
                check("and the first clear group, which has the messages among its layers", ClearGroups.groups[0].layers.includes("messages") && Show.liveMessages.length === 0,
                      ClearGroups.groups[0].layers.join())
                check("which are a layer a clear group can be given here", ClearGroups.layers.find(l => l.key === "messages").ours === true)
                // ---- A token taken out of the words
                const words = named("messageWords")
                click(sight(words))
                words.cursorPosition = 21
                testInput.key(Qt.Key_Backspace)
                return 1100
            },
            () => {
                const m = onDisk("Parent Wanted")
                check("a token deleted from the words is the message's no longer, and its box goes", m.tokens.length === 2 && !m.tokens.some(x => x.name === "Seat")
                      && tokenFields().map(f => f.name).join("|") === "Name", m.tokens.map(x => x.name).join("|") + " / " + tokenFields().map(f => f.name).join("|"))
                // ---- A message removed
                click(centre(listRow("New Message")), Qt.RightButton)
                return 400
            },
            () => {
                check("a right click on a message has showing it, renaming it and removing it", labels() === "Show, --, Rename, Remove…", labels())
                click(menuRow("Remove…"))
                return 400
            },
            () => {
                click(menuRow("Remove"))
                return 500
            },
            () => {
                check("removed, it is gone from the list and from the file, and the one that was picked still is", Messages.messages.length === 2 && onDisk("New Message") === undefined
                      && named("messageList").selectedPath === kept.id, Messages.messages.map(m => m.name).join("|"))
                // ---- In the editor: the stage message among what a text box shows
                startEditingStage(layout("Slide Notes").id)
                return 900
            },
            () => {
                editScreen.canvas.addText()
                return 600
            },
            () => {
                kept.box = editScreen.canvas.selectedId
                editScreen.inspector.tab = "text"
                return 400
            },
            () => {
                click(centre(named("linkChoice")))
                return 500
            },
            () => {
                check("the menu of what a text box shows has the stage message", labels().endsWith("System Clock, Video Countdown, Playback Marker, Stage Message"), labels())
                click(menuRow("Stage Message"))
                return 700
            },
            () => {
                const made = editScreen.canvas.elements.find(e => e.id === kept.box)
                check("picked, the box is linked to the stage message, and set to flash, in ProPresenter's red, to begin with", made.linkKind === "stageMessage" && made.linkFlash === true
                      && String(made.linkFlashColor) === "#ff2d21" && named("linkChoice").shown === "Stage Message", JSON.stringify([made.linkKind, made.linkFlash, String(made.linkFlashColor)]))
                check("with a tick box for the flash, and its colour, under the button", shown("linkFlash") !== null && shown("linkFlash").checked && shown("linkFlashColor") !== null)
                click(centre(shown("linkFlash")))
                return 600
            },
            () => {
                const made = editScreen.canvas.elements.find(e => e.id === kept.box)
                check("unticked, it does not flash", made.linkFlash === false && !shown("linkFlash").checked)
                stopEditing()
                return 700
            },
            () => {
                check("and the layouts' file has the link", StageLayouts.reload() === "" && layout("Slide Notes").slide.elements.some(e => e.id === kept.box && e.linkKind === "stageMessage" && e.linkFlash === false))
            }
        ]
        next()
    }
}
