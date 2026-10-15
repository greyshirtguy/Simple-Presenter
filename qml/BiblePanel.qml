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
// where each thing is. Here, a line for finding a passage (the Bible, ProPresenter's Book
// menu, the passage typed, Previous Verse and Next Verse, and the cog's menu of what to do
// with the slides), a line for how the slides are made (the theme, the text boxes the
// verse and its reference go in, ProPresenter's four Bible Slide Options and its Verse
// References, and how the words are flowed), and a line saying what was found and its
// copyright, which api.bible asks to be shown with its words.
//
// Not here yet, of ProPresenter's: searching by keyword, and a second translation beside
// the first (its +).
Rectangle {
    id: panel

    required property var win
    // How slides are made, kept by the window from one start to the next:
    // { translation, theme, themeSlide, verseBox, referenceBox, verseNumbers,
    //   breakOnNewVerse, translationShown, color, references, flow }
    property var settings: ({})
    signal settingsEdited(var settings)
    // The passage found last, as Bibles.found gave it, and the lookup waited for
    property var passage: null
    property int request: -1
    // What went wrong, or ""; and what was made: { path, count, name }
    property string problem: ""
    property var made: null
    // The passage as it was typed, which the field keeps
    property alias typed: passageField.text

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
    readonly property var boxes: Bibles.textBoxes(themeChoice.place, themeChoice.slide)
    readonly property string verseBox: boxes.includes(settings.verseBox) ? settings.verseBox : guessBox(["verse", "scripture", "bible", "text", "lyrics"], "", boxes[0] ?? "")
    readonly property string referenceBox: settings.referenceBox === "" ? "" : boxes.includes(settings.referenceBox) && settings.referenceBox !== verseBox ? settings.referenceBox
                                                                         : guessBox(["reference", "ref"], verseBox, "")
    readonly property var referenceNames: ({ verse: "Verse References", each: "Passage Each", last: "Passage Last", none: "No Reference" })

    // The text box a theme most likely means for something, by its name, not `taken`;
    // `otherwise`
    function guessBox(words, taken, otherwise) {
        for (const word of words) {
            const found = boxes.find(name => name.toLowerCase().includes(word) && name !== taken)
            if (found !== undefined)
                return found
        }
        return otherwise
    }

    // Another Bible looks the passage up again in that one, as ProPresenter changes the
    // slides to the translation chosen; anything else makes them again
    function edit(change) {
        const otherBible = change.translation !== undefined && (translation === null || change.translation !== translation.id)
        settingsEdited(Object.assign({}, settings, change))
        Qt.callLater(otherBible ? lookup : remake)
    }

    function options() {
        return {
            theme: themeChoice.place, themeSlide: themeChoice.slide, verseBox: verseBox, referenceBox: referenceBox,
            verseNumbers: settings.verseNumbers !== false, breakOnNewVerse: settings.breakOnNewVerse === true,
            translation: settings.translationShown !== false, color: settings.color !== false,
            references: settings.references ?? "last", flow: settings.flow ?? "even"
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
        request = Bibles.lookup(translation.id, passageField.text)
    }

    // ProPresenter's Previous Verse and Next Verse
    function step(delta) {
        if (passage && passage.error === "")
            request = Bibles.step(passage.request, delta)
    }

    // The slides made again, and shown in the grid
    function remake() {
        if (!passage || passage.error !== "")
            return
        const result = Bibles.make(passage.request, options())
        if (result.error !== "") {
            problem = result.error
            return
        }
        made = result
        win.showBibleSlides(result.path)
    }

    // The cog's menu: ProPresenter's three ways to keep the slides
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
            rows.push({ note: "This copy of the app has no key for api.bible: one can be typed in the settings, under Bibles." })
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
        win.showMenu(boxes.length > 0 ? boxes.map(name => ({ label: name, current: name === verseBox, run: () => edit({ verseBox: name }) }))
                                      : [{ note: "This theme slide has no text box" }], item)
    }

    function referenceBoxMenu(item) {
        const rows = [{ label: "With Verse", current: referenceBox === "", run: () => edit({ referenceBox: "" }) }, { divider: true }]
        for (const name of boxes.filter(n => n !== verseBox))
            rows.push({ label: name, current: name === referenceBox, run: () => edit({ referenceBox: name }) })
        win.showMenu(rows, item)
    }

    function referencesMenu(item) {
        win.showMenu(["verse", "each", "last", "none"].map(key => ({
            label: referenceNames[key], current: (settings.references ?? "last") === key, run: () => edit({ references: key })
        })), item)
    }

    function flowMenu(item) {
        win.showMenu([
            { label: "Even, at Natural Pauses", current: (settings.flow ?? "even") === "even", run: () => edit({ flow: "even" }) },
            { label: "Fill Each Slide", current: settings.flow === "fill", run: () => edit({ flow: "fill" }) },
            { divider: true },
            { note: "Even shares the words out across the slides, breaking at a full stop or a comma where there is one near: no slide is left with a few words. Fill puts as much on each slide as goes, as ProPresenter does." }
        ], item)
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

    // api.bible's Bibles, asked for once Bible mode is first come to
    Component.onCompleted: {
        if (Bibles.hasKey && !Bibles.translations.some(t => t.source === "api"))
            Bibles.refreshApi()
    }

    Connections {
        target: Bibles

        function onFound(request, result) {
            if (request !== panel.request)
                return
            panel.passage = result
            panel.problem = result.error
            if (result.error === "") {
                // (The field says what was found, as it was found: "John 3:16–18".)
                passageField.text = result.reference
                panel.remake()
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
                width: Math.max(160, Math.min(320, content.width - 640))
                height: 30
                font.pixelSize: 14
                placeholderText: "A passage: John 3:16-18"
                Keys.onReturnPressed: panel.lookup()
                Keys.onEnterPressed: panel.lookup()
                // (Esc gives the keys back to the slides)
                Keys.onEscapePressed: panel.win.takeFocus()
            }

            AppButton {
                objectName: "bibleFind"
                height: 30
                font.pixelSize: 13
                text: "Find"
                onClicked: panel.lookup()
            }

            AppButton {
                objectName: "biblePrevious"
                height: 30
                font.pixelSize: 13
                text: "← Previous Verse"
                enabled: panel.passage !== null && panel.passage.error === ""
                onClicked: panel.step(-1)
            }

            AppButton {
                objectName: "bibleNext"
                height: 30
                font.pixelSize: 13
                text: "Next Verse →"
                enabled: panel.passage !== null && panel.passage.error === ""
                onClicked: panel.step(1)
            }

            Pick {
                objectName: "bibleKeep"
                value: "⚙"
                onOpened: (item) => panel.keepMenu(item)
            }
        }

        // How the slides are made
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
                value: panel.verseBox !== "" ? panel.verseBox : "—"
                onOpened: (item) => panel.verseBoxMenu(item)
            }

            Pick {
                objectName: "bibleReferenceBox"
                caption: "Reference"
                value: panel.referenceBox !== "" ? panel.referenceBox : "With Verse"
                onOpened: (item) => panel.referenceBoxMenu(item)
            }

            AppCheck {
                objectName: "bibleVerseNumbers"
                height: 30
                text: "Show Verse Numbers"
                checked: panel.settings.verseNumbers !== false
                onToggled: (checked) => panel.edit({ verseNumbers: checked })
            }

            AppCheck {
                objectName: "bibleBreakOnNewVerse"
                height: 30
                text: "Break on New Verse"
                checked: panel.settings.breakOnNewVerse === true
                onToggled: (checked) => panel.edit({ breakOnNewVerse: checked })
            }

            AppCheck {
                objectName: "bibleDisplayTranslation"
                height: 30
                text: "Display Translation"
                checked: panel.settings.translationShown !== false
                onToggled: (checked) => panel.edit({ translationShown: checked })
            }

            AppCheck {
                objectName: "biblePreserveColor"
                height: 30
                text: "Preserve Font Color"
                checked: panel.settings.color !== false
                onToggled: (checked) => panel.edit({ color: checked })
            }

            Pick {
                objectName: "bibleReferences"
                value: panel.referenceNames[panel.settings.references ?? "last"]
                onOpened: (item) => panel.referencesMenu(item)
            }

            Pick {
                objectName: "bibleFlow"
                caption: "Flow"
                value: (panel.settings.flow ?? "even") === "even" ? "Even" : "Fill"
                onOpened: (item) => panel.flowMenu(item)
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
}
