import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import SimplePresenterApp

// Bible mode's bar, over the slides: a passage looked up in a Bible and made into slides
// in a theme, which are then shown in the grid under it like the slides of any
// presentation, and clicked to go live, stepped through with the keys, saved as a
// presentation or put into one. ProPresenter's Bibles, as its help describes them
// ("Using Bibles in ProPresenter"), with its names for every option; src/bibles.h does
// the work.
//
// How it is laid out is a guess: ProPresenter's help says what the Bible view has and not
// where each thing is. Here, as the maintainer rearranged it (2026-10-15): a line for
// finding a passage (the Bible, ProPresenter's Book menu, the passage typed and found
// with Enter, and the cog, which opens a panel of how the slides are made:
// ProPresenter's four Bible Slide Options, its Verse References and how the words are
// flowed); a line for the theme and the text boxes the verse and its reference go in;
// and a line saying what was found and its copyright, which api.bible asks to be shown
// with its words. Under the slides (SlideGrid.qml) are Save, at the left of the
// transition, which asks this bar's keepMenu(), and ← Verse and Verse →, between the
// transition and the size of the thumbnails, which ask its step().
//
// ← Verse and Verse → add the verse before or after the passage to it, within its book,
// as the maintainer asked; the slides up to (or from) the one that is live stay as they
// are (Bibles::make), so that nothing on the screen changes under the person.
//
// The boxes chosen for the verse and the reference are kept for each theme slide, by the
// box's id, so that going to another theme and back keeps them, and a box with no name or
// with another's name can be chosen too.
//
// Not here yet, of ProPresenter's: searching by keyword, and a second translation beside
// the first (its +).
Rectangle {
    id: panel

    required property var win
    // How slides are made, kept by the window from one start to the next:
    // { translation, theme, themeSlide, boxes, verseNumbers, breakOnNewVerse,
    //   translationShown, color, references, flow }, `boxes` being for each theme slide
    // ("<place>|<slide id>") the boxes chosen: { verse, reference } by id ("" reference
    // for With Verse). (0.95 kept one verseBox and referenceBox by name for every theme:
    // still read, where a theme slide has no choice of its own.)
    property var settings: ({})
    signal settingsEdited(var settings)
    // The passage found last, as Bibles.found gave it, and the lookup waited for, and
    // whether that is ← Verse or Verse → growing the passage
    property var passage: null
    property int request: -1
    property bool growing: false
    // What went wrong, or ""; and what was made: { path, count, name, request, live }
    property string problem: ""
    property var made: null
    // The passage as it was typed, which the field keeps
    property alias typed: passageField.text
    // (The panel of options, which is drawn in the window's overlay and not in the bar)
    readonly property alias optionsPanel: optionsPanel

    // The Bible chosen, or until one is, one to begin with, so that there is nothing to
    // choose first: the first imported, else api.bible's NIV, NKJV or KJV in that order,
    // else the first English one it gives, else its first. (A guess at what most will
    // want: FreeShow starts at the NIV, and ProPresenter at the Bible last installed.)
    readonly property var translation: Bibles.translations.find(t => t.id === settings.translation) ?? firstChoice(Bibles.translations)

    function firstChoice(all) {
        const imported = all.find(t => t.source === "file")
        if (imported)
            return imported
        for (const name of ["NIV", "NKJV", "KJV"]) {
            const found = all.find(t => t.abbreviation.toUpperCase() === name)
            if (found)
                return found
        }
        return all.find(t => t.language === "English") ?? all[0] ?? null
    }
    readonly property var themeChoices: {
        const all = [{ place: "", slide: "", label: "Plain (no theme)" }]
        for (const theme of Themes.themes) {
            for (const slide of theme.slides ?? [])
                all.push({ place: theme.place, slide: slide.id, label: theme.slides.length > 1 ? theme.name + " — " + slide.name : theme.name })
        }
        return all
    }
    readonly property var themeChoice: themeChoices.find(c => c.place === (settings.theme ?? "") && c.slide === (settings.themeSlide ?? "")) ?? themeChoices[0]
    readonly property string themeKey: themeChoice.place + "|" + themeChoice.slide
    // The theme slide's text boxes, [{ id, name }]: read again whenever the theme
    // chosen or the workspace's themes change, Bible mode is come to, or a menu of them
    // is opened, so that a box added in the editor is there
    property var boxes: []
    // This theme slide's own choice of boxes, if it has one
    readonly property var chosenBoxes: (settings.boxes ?? {})[themeKey] ?? null
    readonly property string verseBox: {
        if (chosenBoxes && boxes.some(b => b.id === chosenBoxes.verse))
            return chosenBoxes.verse
        const named = boxes.find(b => b.name === settings.verseBox)
        if (named)
            return named.id
        return guessBox(["verse", "scripture", "bible", "text", "lyrics"], "") ?? boxes[0]?.id ?? ""
    }
    readonly property string referenceBox: {
        if (chosenBoxes && chosenBoxes.reference !== undefined)
            return chosenBoxes.reference === "" || (chosenBoxes.reference !== verseBox && boxes.some(b => b.id === chosenBoxes.reference))
                   ? chosenBoxes.reference : ""
        if (settings.referenceBox === "")
            return ""
        const named = boxes.find(b => b.name === settings.referenceBox && b.id !== verseBox)
        if (named)
            return named.id
        return guessBox(["reference", "ref"], verseBox) ?? ""
    }
    readonly property string verseBoxName: boxName(verseBox)
    readonly property var referenceNames: ({ verse: "Verse References", each: "Passage Each", last: "Passage Last", none: "No Reference" })
    readonly property var referenceKeys: ["verse", "each", "last", "none"]

    function refreshBoxes() {
        boxes = Bibles.textBoxes(themeChoice.place, themeChoice.slide)
    }

    onThemeKeyChanged: refreshBoxes()
    onVisibleChanged: if (visible) refreshBoxes()

    Connections {
        target: Themes

        function onChanged() {
            panel.refreshBoxes()
        }
    }

    function boxName(id) {
        return boxes.find(b => b.id === id)?.name ?? ""
    }

    // The id of the text box a theme most likely means for something, by its name, not
    // `taken`; undefined
    function guessBox(words, taken) {
        for (const word of words) {
            const found = boxes.find(b => b.name.toLowerCase().includes(word) && b.id !== taken)
            if (found !== undefined)
                return found.id
        }
        return undefined
    }

    // Another Bible looks the passage up again in that one, as ProPresenter changes the
    // slides to the translation chosen; anything else makes them again
    function edit(change) {
        const otherBible = change.translation !== undefined && (translation === null || change.translation !== translation.id)
        settingsEdited(Object.assign({}, settings, change))
        Qt.callLater(otherBible ? lookup : remake)
    }

    // The boxes chosen for the theme slide being used: { verse } or { reference }
    function editBoxes(change) {
        const all = Object.assign({}, settings.boxes ?? {})
        all[themeKey] = Object.assign({ verse: verseBox, reference: referenceBox }, change)
        edit({ boxes: all })
    }

    function options(live) {
        return {
            theme: themeChoice.place, themeSlide: themeChoice.slide, verseBox: verseBox, referenceBox: referenceBox,
            verseNumbers: settings.verseNumbers !== false, breakOnNewVerse: settings.breakOnNewVerse === true,
            translation: settings.translationShown !== false, color: settings.color !== false,
            references: settings.references ?? "last", flow: settings.flow ?? "even", live: live
        }
    }

    function lookup() {
        if (passageField.text.trim() === "")
            return
        if (!translation) {
            problem = "Choose a Bible first."
            return
        }
        problem = ""
        growing = false
        request = Bibles.lookup(translation.id, passageField.text)
    }

    // ← Verse and Verse →: the passage grows by the verse before or after it
    function step(delta) {
        if (passage && passage.error === "") {
            growing = true
            request = Bibles.step(passage.request, delta)
        }
    }

    // The slide of the last ones made that is live, or -1: the one kept where the passage
    // grows (Bibles::make)
    function liveSlide() {
        return made && win.liveKey === made.path && win.livePlaylistId === "" && win.cueLive ? win.liveIndex : -1
    }

    // The slides made again, and shown in the grid; `live` is the slide kept, or -1
    function remake(live) {
        if (!passage || passage.error !== "")
            return
        const result = Bibles.make(passage.request, options(typeof live === "number" ? live : -1))
        if (result.error !== "") {
            problem = result.error
            return
        }
        made = result
        win.showBibleSlides(result)
    }

    // ProPresenter's three ways to keep the slides, from the Save button under them
    function keepMenu(item) {
        const libraries = win.catalog.libraries.map(library => ({ label: library.name, run: () => saveIn(library.path, "") }))
        const playlists = win.catalog.playlists.filter(p => !p.folder).map(playlist => ({
            label: playlist.name, items: () => win.catalog.libraries.map(library => ({ label: "In " + library.name, run: () => saveIn(library.path, playlist.path) }))
        }))
        const current = win.bibleReturnEntry
        win.showMenu([
            { label: "Save as Presentation", items: libraries.length > 0 ? libraries : [{ label: "There is no library", disabled: true }], disabled: !made },
            { label: "Copy to Current Presentation", disabled: !made || !current, run: () => copyToCurrent() },
            { label: "Save to Playlist", items: playlists.length > 0 ? playlists : [{ label: "There is no playlist", disabled: true }], disabled: !made },
            { divider: true },
            { note: current ? "The current presentation is " + current.name + "." : "Open a presentation first to copy into it." }
        ], item)
    }

    function saveIn(library, playlist) {
        const saved = Bibles.saveAs(library)
        if (saved.error !== "") {
            win.report(saved.error)
            return
        }
        if (playlist !== "")
            win.report(win.catalog.addToPlaylist(playlist, saved.path))
        win.tell((made ? made.name : "The passage") + " saved" + (playlist !== "" ? " to the playlist" : ""))
    }

    function copyToCurrent() {
        const entry = win.bibleReturnEntry
        if (!entry)
            return
        const into = win.catalog.open(entry.file ?? entry.path)
        const slides = into.slides ?? []
        if (slides.length === 0) {
            win.report("There is no slide in " + entry.name + " to put them after.")
            return
        }
        // After the slide of it that is live, where one is; or else after its last
        const after = win.bibleReturnSlide !== "" && slides.some(s => s.id === win.bibleReturnSlide) ? win.bibleReturnSlide : slides[slides.length - 1].id
        if (win.report(Bibles.copyInto(entry.file ?? entry.path, after)))
            win.tell((made ? made.count : 0) + " slides put into " + entry.name)
    }

    function translationMenu(item) {
        const rows = []
        const files = Bibles.translations.filter(t => t.source === "file")
        const api = Bibles.translations.filter(t => t.source === "api")
        rows.push({ header: "Imported" })
        if (files.length === 0)
            rows.push({ note: "None yet" })
        for (const t of files)
            rows.push({ label: t.name + (t.abbreviation !== "" ? " (" + t.abbreviation + ")" : ""), current: translation !== null && t.id === translation.id, run: () => edit({ translation: t.id }) })
        rows.push({ label: "Import XML Bible…", run: () => importDialog.open() })
        rows.push({ divider: true })
        rows.push({ header: "api.bible" })
        if (!Bibles.hasKey)
            rows.push({ note: "api.bible needs a key of your own: register at api.bible for one, and type it in the settings, under Bibles." })
        else if (Bibles.apiLoading)
            rows.push({ note: "Asking api.bible…" })
        else if (Bibles.apiProblem !== "")
            rows.push({ note: Bibles.apiProblem }, { label: "Ask Again", run: () => Bibles.refreshApi() })
        // By language, the person's own first where it is English
        let language = null
        for (const t of api) {
            if (t.language !== language && api.some(o => o.language !== t.language)) {
                language = t.language
                rows.push({ note: language })
            }
            rows.push({ label: t.name + (t.abbreviation !== "" ? " (" + t.abbreviation + ")" : ""), current: translation !== null && t.id === translation.id, run: () => edit({ translation: t.id }) })
        }
        win.showMenu(rows, item)
    }

    // ProPresenter's Book menu: a book, then a chapter of it
    function bookMenu(item) {
        if (!translation)
            return
        const books = Bibles.books(translation.id)
        const rowsOf = (from, to) => books.filter(b => b.number >= from && b.number <= to).map(book => ({
            label: book.name,
            items: () => Array.from({ length: Math.max(1, book.chapters) }, (_, c) => ({
                label: "Chapter " + (c + 1), run: () => { passageField.text = book.name + " " + (c + 1); lookup() }
            }))
        }))
        const old = rowsOf(1, 39)
        const newer = rowsOf(40, 66)
        const others = rowsOf(67, 999)
        const rows = []
        if (old.length > 0)
            rows.push({ label: "Old Testament", items: old })
        if (newer.length > 0)
            rows.push({ label: "New Testament", items: newer })
        if (others.length > 0)
            rows.push({ label: "Other Books", items: others })
        win.showMenu(rows, item)
    }

    function themeMenu(item) {
        win.showMenu(themeChoices.map(c => ({ label: c.label, current: c === themeChoice, run: () => edit({ theme: c.place, themeSlide: c.slide }) })), item)
    }

    function verseBoxMenu(item) {
        refreshBoxes()
        win.showMenu(boxes.length > 0 ? boxes.map(box => ({ label: box.name, current: box.id === verseBox, run: () => editBoxes({ verse: box.id }) }))
                                      : [{ note: "This theme slide has no text box" }], item)
    }

    function referenceBoxMenu(item) {
        refreshBoxes()
        const rows = [{ label: "With Verse", current: referenceBox === "", run: () => editBoxes({ reference: "" }) }, { divider: true }]
        for (const box of boxes.filter(b => b.id !== verseBox))
            rows.push({ label: box.name, current: box.id === referenceBox, run: () => editBoxes({ reference: box.id }) })
        win.showMenu(rows, item)
    }

    color: win.panelColor
    implicitHeight: content.implicitHeight + 16

    // The workspace whose themes the slides are made in, and the person's own key. (Given
    // here, and not by the window at its start: nothing of Bibles is made until Bible
    // mode is first come to, or its settings are looked at.)
    Binding {
        target: Bibles
        property: "workspace"
        value: panel.win.catalog.workspacePath
    }

    Binding {
        target: Bibles
        property: "ownKey"
        value: panel.win.apiBibleKey
    }

    // api.bible's Bibles, asked for once Bible mode is first come to; and the theme
    // slide's boxes, read
    Component.onCompleted: {
        refreshBoxes()
        if (Bibles.hasKey && !Bibles.translations.some(t => t.source === "api"))
            Bibles.refreshApi()
    }

    Connections {
        target: Bibles

        function onFound(request, result) {
            if (request !== panel.request)
                return
            // A passage that could not grow (the end of its book) stays as it was
            if (panel.growing && result.error !== "") {
                panel.problem = result.error
                return
            }
            // The slide that is live, kept where the passage grew round it
            const live = panel.growing ? panel.liveSlide() : -1
            panel.passage = result
            panel.problem = result.error
            if (result.error === "") {
                // (The field says what was found, as it was found: "John 3:16–18".)
                passageField.text = result.reference
                panel.remake(live)
            }
        }

        function onImported(error, id) {
            if (error !== "") {
                panel.win.report(error)
                return
            }
            panel.edit({ translation: id })
        }
    }

    FileDialog {
        id: importDialog

        title: "Import XML Bible"
        nameFilters: ["XML Bibles (*.xml *.osis *.zefania)", "All files (*)"]
        onAccepted: Bibles.importFile(selectedFile)
    }

    // (A choice shown as what is chosen, which opens a menu)
    component Pick: Rectangle {
        id: pick

        property string caption
        property string value
        signal opened(var item)

        width: Math.min(280, pickRow.implicitWidth + 22)
        height: 30
        radius: 6
        color: pickMouse.pressed ? "#50535a" : pickMouse.containsMouse ? "#45484e" : "#34363c"

        Row {
            id: pickRow

            x: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Text {
                visible: pick.caption !== ""
                color: "#9a9da3"
                font.pixelSize: 12
                text: pick.caption
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                width: Math.min(implicitWidth, 220)
                elide: Text.ElideRight
                color: "#e6e6e6"
                font.pixelSize: 13
                text: pick.value + "  ▾"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            id: pickMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: pick.opened(pick)
        }
    }

    Column {
        id: content

        x: 12
        y: 8
        width: parent.width - 24
        spacing: 8

        // Finding a passage
        Row {
            spacing: 8

            Pick {
                objectName: "bibleTranslation"
                caption: "Bible"
                value: panel.translation ? (panel.translation.abbreviation !== "" ? panel.translation.abbreviation : panel.translation.name) : "Choose…"
                onOpened: (item) => panel.translationMenu(item)
            }

            Pick {
                objectName: "bibleBook"
                value: "Book"
                opacity: panel.translation ? 1 : 0.5
                onOpened: (item) => panel.bookMenu(item)
            }

            AppTextField {
                id: passageField

                objectName: "biblePassage"
                width: Math.max(160, Math.min(420, content.width - 300))
                height: 30
                font.pixelSize: 14
                placeholderText: "A passage, then Enter: John 3:16-18"
                // (Found with Enter alone: no Find button, as the maintainer asked)
                Keys.onReturnPressed: panel.lookup()
                Keys.onEnterPressed: panel.lookup()
                // (Esc gives the keys back to the slides)
                Keys.onEscapePressed: panel.win.takeFocus()
            }

            Pick {
                id: optionsButton

                objectName: "bibleOptions"
                value: "⚙"
                onOpened: optionsPanel.open()
            }
        }

        // The theme, and the boxes of its slide the words go in
        Flow {
            width: parent.width
            spacing: 8

            Pick {
                objectName: "bibleTheme"
                caption: "Theme"
                value: panel.themeChoice.label
                onOpened: (item) => panel.themeMenu(item)
            }

            Pick {
                objectName: "bibleVerseBox"
                caption: "Verse"
                value: panel.verseBox !== "" ? panel.verseBoxName : "—"
                onOpened: (item) => panel.verseBoxMenu(item)
            }

            Pick {
                objectName: "bibleReferenceBox"
                caption: "Reference"
                value: panel.referenceBox !== "" ? panel.boxName(panel.referenceBox) : "With Verse"
                onOpened: (item) => panel.referenceBoxMenu(item)
            }
        }

        // What was found, and its copyright
        Text {
            objectName: "bibleStatus"
            width: parent.width
            elide: Text.ElideRight
            color: panel.problem !== "" ? "#e07070" : "#9a9da3"
            font.pixelSize: 12
            text: panel.problem !== "" ? panel.problem
                : panel.passage && panel.made ? panel.passage.reference + (panel.passage.translation !== "" ? " · " + panel.passage.translation : "")
                                                + " · " + panel.passage.verses.length + (panel.passage.verses.length === 1 ? " verse" : " verses")
                                                + " on " + panel.made.count + (panel.made.count === 1 ? " slide" : " slides")
                                                + (panel.passage.copyright !== "" ? "   ·   " + panel.passage.copyright : "")
                : !panel.translation ? "Choose a Bible, and type a passage."
                : "Type a passage and press Enter, or choose one from Book."
        }
    }

    // How the slides are made, opened from the cog: ProPresenter's four Bible Slide
    // Options, its references, and how the words are flowed. In the look of the panel
    // the transition's options open in (TransitionOptions.qml). The two lists are the
    // app's drop-downs, which open over it; a menu of the window's own would open
    // under it.
    Popup {
        id: optionsPanel

        objectName: "bibleOptionsPanel"
        width: 300
        // Under the cog, its right edge at the cog's, placed as it opens
        onAboutToShow: {
            x = Math.max(0, Math.min(optionsButton.mapToItem(panel, optionsButton.width, 0).x - width, panel.width - width))
            y = optionsButton.mapToItem(panel, 0, optionsButton.height).y + 6
        }
        padding: 14
        focus: true
        onClosed: panel.win.takeFocus()

        background: Rectangle {
            radius: 9
            color: "#33363d"
            border.width: 1.5
            border.color: "#8b8f98"
        }

        contentItem: Column {
            spacing: 10

            AppCheck {
                objectName: "bibleVerseNumbers"
                width: parent.width
                text: "Show Verse Numbers"
                checked: panel.settings.verseNumbers !== false
                onToggled: (checked) => panel.edit({ verseNumbers: checked })
            }

            AppCheck {
                objectName: "bibleBreakOnNewVerse"
                width: parent.width
                text: "Break on New Verse"
                checked: panel.settings.breakOnNewVerse === true
                onToggled: (checked) => panel.edit({ breakOnNewVerse: checked })
            }

            AppCheck {
                objectName: "bibleDisplayTranslation"
                width: parent.width
                text: "Display Translation"
                checked: panel.settings.translationShown !== false
                onToggled: (checked) => panel.edit({ translationShown: checked })
            }

            AppCheck {
                objectName: "biblePreserveColor"
                width: parent.width
                text: "Preserve Font Color"
                checked: panel.settings.color !== false
                onToggled: (checked) => panel.edit({ color: checked })
            }

            Text {
                topPadding: 4
                color: "#9a9da3"
                font.pixelSize: 12
                text: "References"
            }

            AppComboBox {
                objectName: "bibleReferences"
                width: parent.width
                height: 30
                font.pixelSize: 13
                model: panel.referenceKeys.map(key => panel.referenceNames[key])
                currentIndex: Math.max(0, panel.referenceKeys.indexOf(panel.settings.references ?? "last"))
                onActivated: (index) => panel.edit({ references: panel.referenceKeys[index] })
            }

            Text {
                topPadding: 4
                color: "#9a9da3"
                font.pixelSize: 12
                text: "Flow"
            }

            AppComboBox {
                objectName: "bibleFlow"
                width: parent.width
                height: 30
                font.pixelSize: 13
                model: ["Even, at Natural Pauses", "Fill Each Slide"]
                currentIndex: panel.settings.flow === "fill" ? 1 : 0
                onActivated: (index) => panel.edit({ flow: index === 1 ? "fill" : "even" })
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 11
                text: "Even shares the words out across the slides, breaking at a full stop or a comma where there is one near: "
                      + "no slide is left with a few words. Fill puts as much on each slide as goes, as ProPresenter does."
            }
        }
    }
}
