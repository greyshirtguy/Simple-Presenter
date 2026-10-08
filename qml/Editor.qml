import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import SimplePresenterApp

// The editor, laid over the operator window below its toolbar. On the left are the
// presentation's slides and, under them, the elements of the slide being worked on, front
// one first; in the middle is that slide, where elements are picked, moved, resized and
// their text edited in place; on the right are the picked element's properties.
//
// Every change is saved to the presentation file as it is made, and can be undone.
//
// The same editor works on the workspace's props and on its stage layouts, which are
// slides too: the list on the left is then of the props or the layouts in place of a
// presentation's slides, and has what a presentation's list does not, a way to add one
// and to rename, copy or remove it (a presentation's slides are added to in
// ProPresenter). Those are done by what keeps the props and the layouts (Props,
// StageLayouts), which changes the file; the editor then opens the file again.
//
// How it is put together.
//
//   PresentationEditor (src/presentationeditor.h) holds the file, parsed, and is the
//   only thing that changes it; it is also the list model of the slides. A change is a
//   call to it, which alters the one slide, writes the file, and describes that slide
//   afresh for whatever draws it. Undo is a list of that slide's cue as it was before
//   and after each change.
//
//   EditorCanvas draws the slide being worked on with the same SlideElement that draws
//   the output, and lays over it what editing needs: the handles, the lines snapped
//   to, and a TextEdit for typing into a text box where it stands.
//
//   EditorInspector shows the properties of the picked element and reports what its
//   controls are set to. It changes nothing itself.
//
//   This file joins them: the lists on the left, the menus and the keys, and handing
//   what the inspector reports to the canvas, which applies it to the picked element.
//
// Dragging something (an element, a slider, a colour) would save a change and leave a
// step to undo for every pixel of the way. So a change comes in two kinds: one that is
// shown but not saved ("interim" here, a preview in PresentationEditor), of which any
// number may follow one another, and settling, which saves where it ended up as one
// change.
Rectangle {
    id: screen

    property real sidebarWidth: 260
    // The colour a slide's frame takes from its group, as the slide grid has it
    property var groupColor: (slide) => "#2b2d31"
    // The url of an image to show behind the slide with this id, or ""
    property var backdropFor: (slideId) => ""
    // Opens a menu of rows, as the operator window's menu takes them, at a point of an item
    property var showMenu: (items, item, x, y) => {}
    // Something to tell the user, and whether it is a failure
    property string notice
    property bool noticeIsError: true
    // The element being renamed in the list, by id; "" for none
    property string renamingId
    // The prop or stage layout being renamed in the list on the left, by id; "" for none
    property string renamingRow
    // The workspace folder what is open is in
    property string workspace

    // What is open: "presentation", "props" or "stage"; and what a row of the list on
    // the left is then called
    readonly property string kind: editor.kind
    readonly property string rowWord: kind === "props" ? "Prop" : kind === "stage" ? "Layout" : "Slide"

    readonly property alias editor: editor
    readonly property alias canvas: canvas
    readonly property alias inspector: inspector
    readonly property var families: bridge.fontFamilies()
    readonly property color accentColor: "#ff8a1f"

    // What the toolbar button under the pointer does, in words, or ""
    readonly property string toolHint: {
        for (let i = 0; i < actions.children.length; ++i) {
            const tool = actions.children[i]
            if (tool.hovered === true && tool.hint)
                return tool.hint
        }
        return ""
    }
    // A filter for a file dialog showing the media files the app can use, and the
    // folder such a dialog starts in
    property string mediaFilter: "All files (*)"
    property string mediaFolder: ""

    // Asks for a media file: to "add" as an element of its own, or to "fill" the picked
    // element with.
    function chooseMedia(purpose) {
        finish()
        mediaDialog.purpose = purpose
        mediaDialog.open()
    }

    // The user asked to go back to showing
    signal done
    // A key that belongs to the operator window, not the editor, was pressed
    signal keyPassed(var event)

    // Opens a presentation file from a workspace folder, at the slide with the given id
    // if it has one. Returns an error message, empty on success.
    function open(path, workspace, slideId) {
        return opened(editor.open(path, workspace), workspace, slideId)
    }

    // Opens the props of a workspace, or its stage layouts, at the one with the given
    // id, or failing that at the given row: `path` is the file they are in.
    function openProps(path, workspace, id, row) {
        return opened(editor.openProps(path, workspace), workspace, id, row)
    }

    function openStage(path, workspace, id, row) {
        return opened(editor.openStageLayouts(path, workspace), workspace, id, row)
    }

    // What follows opening any of them: the row with the given id is shown, or failing
    // that the one at `row`, or the first.
    function opened(error, workspace, id, row) {
        if (error !== "")
            return error
        screen.workspace = workspace
        notice = ""
        renamingId = ""
        renamingRow = ""
        const found = id ? editor.rowOf(id) : -1
        canvas.showRow(found >= 0 ? found : Math.max(0, Math.min(row ?? 0, editor.count - 1)))
        slideList.positionViewAtIndex(canvas.row, ListView.Center)
        return ""
    }

    // Makes a change to which props or stage layouts there are, or to what one is
    // called: `change` is a call to what keeps them, and answers with an error message
    // or with { id, error } for something it made. The file is then opened again, at
    // what was made, or at what was being shown, or at the row that was. Such a change
    // is not one of the editor's own: it cannot be undone here, and what could be
    // undone before it no longer can.
    function restructure(change) {
        finish()
        const row = canvas.row
        const shown = canvas.slide ? canvas.slide.id : ""
        const result = change()
        const error = typeof result === "string" ? result : result.error
        const made = typeof result === "string" ? "" : result.id
        const reopened = kind === "props" ? openProps(editor.path, workspace, made !== "" ? made : shown, row)
                                          : openStage(editor.path, workspace, made !== "" ? made : shown, row)
        report(error !== "" ? error : reopened)
        takeFocus()
        return error === "" ? made : ""
    }

    // Adds a prop, to the collection of the one being shown, or a stage layout.
    function addRow() {
        if (kind === "props") {
            const beside = canvas.slide ? Props.find(canvas.slide.id) : ({})
            restructure(() => Props.add(beside.collection ?? ""))
        } else if (kind === "stage") {
            restructure(() => StageLayouts.add())
        }
    }

    // Ends a rename in the list on the left, with the name typed.
    function renameRow(id, name) {
        const row = editor.rowOf(id)
        renamingRow = ""
        if (row >= 0 && name !== "" && name !== editor.slideAt(row).label)
            restructure(() => kind === "props" ? Props.rename(id, name) : StageLayouts.rename(id, name))
        else
            takeFocus()
    }

    // The menu of a prop or a stage layout in the list on the left, at a point of the
    // list. (The list, and not the row, is what it is opened on: the changes it offers
    // rebuild the rows.)
    function showRowMenu(slide, x, y) {
        if (kind === "presentation")
            return
        const props = kind === "props"
        showMenu([
            { header: slide.label !== "" ? slide.label : rowWord },
            { label: "Rename", run: () => renamingRow = slide.id },
            { label: "Duplicate", run: () => restructure(() => props ? Props.duplicate(slide.id)
                                                                       : StageLayouts.duplicate(slide.id)) },
            { label: "Remove…", danger: true, run: () => showMenu([
                { note: "“" + slide.label + "” will be removed. That cannot be undone." },
                { label: "Remove", danger: true,
                  run: () => restructure(() => props ? Props.remove(slide.id) : StageLayouts.remove(slide.id)) }
            ], slideList, x, y) }
        ], slideList, x, y)
    }

    // Finishes whatever is under way, so that all of it is in the file.
    function finish() {
        canvas.finishText()
        canvas.settle()
    }

    function close() {
        finish()
        editor.close()
    }

    function takeFocus() {
        canvas.takeFocus()
    }

    function report(error) {
        notice = error
        noticeIsError = true
        if (error !== "") {
            noticeTimer.restart()
            Log.problem("Shown in the editor: " + error)
        }
        return error === ""
    }

    function tell(message) {
        notice = message
        noticeIsError = false
        noticeTimer.restart()
    }

    function showRow(row) {
        if (row < 0 || row >= editor.count || row === canvas.row)
            return
        canvas.showRow(row)
        slideList.positionViewAtIndex(row, ListView.Contain)
    }

    // A press on an element's row in the list: picks it, and with the right button
    // opens its menu at that point of the list.
    function listPressed(id, menu, x, y) {
        canvas.pick(id)
        takeFocus()
        if (menu)
            showElementMenu(elementList, x, y)
    }

    // Ends a rename in the list, with the name typed.
    function rename(id, name) {
        const element = canvas.elements.find(e => e.id === id)
        renamingId = ""
        if (element && name !== "" && name !== element.name)
            report(editor.setProperties(canvas.row, id, { name: name }))
        takeFocus()
    }

    // The menu for the picked element, or for the slide if none is.
    function showElementMenu(item, x, y) {
        const element = canvas.selected
        const items = []
        if (element) {
            const index = canvas.elements.findIndex(e => e.id === element.id)
            const last = canvas.elements.length - 1
            items.push({ header: element.name !== "" ? element.name : "Element" })
            if (!element.locked && !element.hidden)
                items.push({ label: "Edit Text", run: () => canvas.editText(element.id) })
            items.push({ label: "Rename", run: () => renamingId = element.id })
            items.push({ label: "Duplicate", run: () => canvas.duplicate() })
            if (last > 0)
                items.push({ header: "Order" })
            if (index < last) {
                items.push({ label: "Bring to Front", run: () => canvas.reorder(last) })
                items.push({ label: "Bring Forward", run: () => canvas.reorder(1) })
            }
            if (index > 0) {
                items.push({ label: "Send Backward", run: () => canvas.reorder(-1) })
                items.push({ label: "Send to Back", run: () => canvas.reorder(-last) })
            }
            items.push({ header: "Element" })
            items.push({ label: element.hidden ? "Show" : "Hide",
                         run: () => canvas.setProperties({ hidden: !element.hidden }, false) })
            items.push({ label: element.locked ? "Unlock" : "Lock",
                         run: () => canvas.setProperties({ locked: !element.locked }, false) })
            items.push({ label: "Delete", danger: true, run: () => canvas.removeSelected() })
        } else {
            items.push({ label: "Add Text", run: () => canvas.addText() })
        }
        showMenu(items, item, x, y)
    }

    color: "#1e1f22"
    // Keys nothing in the editor took: the clears still work from here.
    Keys.onPressed: (event) => keyPassed(event)

    PresentationEditor {
        id: editor

        // Undoing a change made to another slide shows that slide.
        onRestored: (row) => screen.showRow(row)
    }

    RichTextBridge {
        id: bridge
    }

    Timer {
        id: noticeTimer

        interval: 8000
        onTriggered: screen.notice = ""
    }

    // Swallows clicks and scrolling that nothing here takes, so that the operator
    // window underneath does not.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onPressed: screen.takeFocus()
    }

    component PanelTitle: Text {
        leftPadding: 12
        topPadding: 10
        bottomPadding: 6
        color: "#9a9da3"
        font.pixelSize: 12
        font.bold: true
        font.capitalization: Font.AllUppercase
    }

    // Slides, and the elements of the one being worked on
    Rectangle {
        id: left

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.max(200, Math.min(screen.sidebarWidth, 360))
        color: "#2b2d31"

        PanelTitle {
            id: slidesTitle

            color: "#4da3ff"
            text: screen.kind === "props" ? "Props" : screen.kind === "stage" ? "Stage Layouts" : "Slides"
        }

        // Adds a prop or a stage layout. (A presentation's slides are added to in
        // ProPresenter.)
        AppButton {
            objectName: "editorAddRow"
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: slidesTitle.verticalCenter
            anchors.verticalCenterOffset: 2
            width: 26
            height: 22
            leftPadding: 0
            rightPadding: 0
            font.pixelSize: 14
            text: "+"
            visible: screen.kind !== "presentation"
            onClicked: screen.addRow()
        }

        ListView {
            id: slideList

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: slidesTitle.bottom
            anchors.bottom: elementsHeader.top
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: editor

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            delegate: Item {
                id: cell

                required property var slide
                required property int index
                readonly property bool current: index === canvas.row
                readonly property bool renaming: screen.renamingRow !== "" && screen.renamingRow === slide.id
                readonly property color frame: screen.groupColor(slide)
                readonly property bool lightFrame: 0.299 * frame.r + 0.587 * frame.g + 0.114 * frame.b > 0.6

                width: ListView.view.width
                height: (width - 36 - 6) * 9 / 16 + 3 + 22 + 10

                // The slide being worked on is ringed.
                Rectangle {
                    anchors.fill: frameRect
                    anchors.margins: -4
                    radius: 7
                    visible: cell.current
                    color: "transparent"
                    border.width: 2.5
                    border.color: screen.accentColor
                }

                Rectangle {
                    id: frameRect

                    x: 18
                    y: 5
                    width: parent.width - 36
                    height: parent.height - 10
                    radius: 4
                    color: cell.frame

                    Rectangle {
                        id: thumbnail

                        x: 3
                        y: 3
                        width: parent.width - 6
                        height: width * 9 / 16
                        color: "black"

                        Image {
                            anchors.fill: parent
                            source: screen.backdropFor(cell.slide.id)
                            visible: source != ""
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }

                        // As it will be shown, its visibility rules applied
                        Slide {
                            anchors.fill: parent
                            slide: cell.slide
                            effects: false
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: thumbnail.bottom
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: !cell.renaming
                        color: cell.lightFrame ? "black" : "white"
                        font.pixelSize: 11
                        text: (cell.index + 1)
                            + (cell.slide.groupStart && cell.slide.group !== "" ? "  " + cell.slide.group : "")
                            + (cell.slide.label !== "" && cell.slide.label !== cell.slide.group ? "  " + cell.slide.label : "")
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        screen.showRow(cell.index)
                        screen.takeFocus()
                        if (mouse.button === Qt.RightButton) {
                            const at = mapToItem(slideList, mouse.x, mouse.y)
                            screen.showRowMenu(cell.slide, at.x, at.y)
                        }
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton && screen.kind !== "presentation")
                            screen.renamingRow = cell.slide.id
                    }
                }

                // Renaming a prop or a layout where its name is
                AppTextField {
                    x: frameRect.x + 3
                    y: frameRect.y + frameRect.height - 23
                    width: frameRect.width - 6
                    height: 22
                    leftPadding: 5
                    rightPadding: 5
                    font.pixelSize: 12
                    visible: cell.renaming
                    onVisibleChanged: {
                        if (visible) {
                            text = cell.slide.label
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    Component.onCompleted: {
                        if (visible) {
                            text = cell.slide.label
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    // One call and nothing after it: the change rebuilds the list,
                    // and this row with it.
                    onEditingFinished: {
                        if (cell.renaming)
                            screen.renameRow(cell.slide.id, text.trim())
                    }
                    Keys.onEscapePressed: screen.renameRow(cell.slide.id, "")
                }
            }
        }

        Text {
            anchors.centerIn: slideList
            width: slideList.width - 40
            visible: screen.kind !== "presentation" && editor.count === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 13
            text: screen.kind === "props" ? "No props. Add one with the + above."
                                          : "No stage layouts. Add one with the + above."
        }

        // The elements of the slide, the front one first
        Rectangle {
            id: elementsHeader

            anchors.left: parent.left
            anchors.right: parent.right
            y: Math.round(parent.height * 0.56)
            height: 34
            color: "#4f5665"

            Text {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: 12
                font.bold: true
                font.capitalization: Font.AllUppercase
                text: "Elements"
            }

            AppButton {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                height: 24
                leftPadding: 10
                rightPadding: 10
                font.pixelSize: 12
                text: "+ Text"
                enabled: canvas.slide !== null
                onClicked: canvas.addText()
            }
        }

        ListView {
            id: elementList

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: elementsHeader.bottom
            anchors.bottom: parent.bottom
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // The last element is drawn in front, so it comes first here.
            model: canvas.elements.slice().reverse()

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            delegate: Rectangle {
                id: entry

                required property var modelData
                readonly property bool selected: modelData.id === canvas.selectedId
                readonly property bool renaming: screen.renamingId !== "" && screen.renamingId === modelData.id

                width: ListView.view.width
                height: 32
                color: selected ? "#22ffffff" : entryMouse.containsMouse ? "#10ffffff" : "transparent"

                Rectangle {
                    width: parent.width
                    height: 1
                    visible: entry.selected
                    color: "#55ffffff"
                }

                Rectangle {
                    y: parent.height - 1
                    width: parent.width
                    height: 1
                    visible: entry.selected
                    color: "#55ffffff"
                }

                MouseArea {
                    id: entryMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    // Picking can rebuild the list (it finishes any text being typed),
                    // so the list, not this row, is what a menu is opened on.
                    onPressed: (mouse) => {
                        const at = mapToItem(elementList, mouse.x, mouse.y)
                        screen.listPressed(entry.modelData.id, mouse.button === Qt.RightButton, at.x, at.y)
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton)
                            screen.renamingId = entry.modelData.id
                    }
                }

                IconButton {
                    id: eye

                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    kind: "eye"
                    flat: true
                    on: !entry.modelData.hidden
                    onClicked: {
                        canvas.pick(entry.modelData.id)
                        canvas.setProperties({ hidden: !entry.modelData.hidden }, false)
                    }
                }

                Text {
                    anchors.left: eye.right
                    anchors.leftMargin: 6
                    anchors.right: marks.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !entry.renaming
                    elide: Text.ElideRight
                    color: entry.modelData.hidden ? "#7d8088" : "#e6e6e6"
                    font.pixelSize: 13
                    // An element with no name goes by its words, as in ProPresenter
                    text: entry.modelData.name !== "" ? entry.modelData.name
                        : entry.modelData.words !== "" ? entry.modelData.words
                        : entry.modelData.fillKind === "media" && entry.modelData.fillMediaName !== "" ? entry.modelData.fillMediaName
                        : "(unnamed)"
                }

                // What sets it apart: its text comes from elsewhere, or it has rules for
                // when it shows.
                Text {
                    id: marks

                    anchors.right: lock.left
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !entry.renaming
                    color: "#9a9da3"
                    font.pixelSize: 11
                    text: (entry.modelData.linkKind !== "none" ? "linked" : "")
                        + (entry.modelData.linkKind !== "none" && entry.modelData.visibilityRules ? " · " : "")
                        + (entry.modelData.visibilityRules ? "rules" : "")
                }

                AppTextField {
                    anchors.left: eye.right
                    anchors.leftMargin: 2
                    anchors.right: lock.left
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    height: 26
                    font.pixelSize: 13
                    visible: entry.renaming
                    onVisibleChanged: {
                        if (visible) {
                            text = entry.modelData.name
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    Component.onCompleted: {
                        if (visible) {
                            text = entry.modelData.name
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    // One call and nothing after it: the change rebuilds the list,
                    // and this row with it.
                    onEditingFinished: {
                        if (entry.renaming)
                            screen.rename(entry.modelData.id, text.trim())
                    }
                    Keys.onEscapePressed: {
                        screen.renamingId = ""
                        screen.takeFocus()
                    }
                }

                IconButton {
                    id: lock

                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26
                    kind: "lock"
                    flat: true
                    on: entry.modelData.locked
                    onClicked: {
                        canvas.pick(entry.modelData.id)
                        canvas.setProperties({ locked: !entry.modelData.locked }, false)
                    }
                }
            }
        }

        Text {
            anchors.centerIn: elementList
            width: elementList.width - 40
            visible: canvas.slide !== null && canvas.elements.length === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 13
            text: "This " + screen.rowWord.toLowerCase() + " has nothing on it. Add a text box with “+ Text”."
        }
    }

    // The picked element's properties
    // A picture or a video: to be an element of its own, or what the picked one is
    // filled with. The file is referred to where it is, not copied.
    FileDialog {
        id: mediaDialog

        property string purpose: "add"

        title: purpose === "add" ? "Add Media" : "Fill With Media"
        currentFolder: screen.mediaFolder !== "" ? "file://" + screen.mediaFolder : ""
        nameFilters: [screen.mediaFilter, "All files (*)"]
        onAccepted: {
            if (purpose === "add")
                canvas.addMedia(selectedFile)
            else
                canvas.setProperties({ fillMediaPath: decodeURIComponent(String(selectedFile).replace(/^file:\/\//, "")), fillOn: true }, false)
            screen.takeFocus()
        }
        onRejected: screen.takeFocus()
    }

    EditorInspector {
        id: inspector

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 308
        element: canvas.selected
        elements: canvas.elements
        format: canvas.format
        selection: canvas.selection
        families: screen.families
        setProperties: (changes, interim) => canvas.setProperties(changes, interim)
        chooseMedia: () => screen.chooseMedia("fill")
        setFormat: (format, interim) => canvas.setFormat(format, interim)
        settle: () => canvas.settle()
        onFinished: screen.takeFocus()
    }

    // What can be done to the slide, and the way back
    Rectangle {
        id: strip

        anchors.left: left.right
        anchors.right: inspector.left
        anchors.top: parent.top
        height: 46
        color: "#23252b"

        Row {
            id: actions

            x: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            // What can be added: a text box, a shape from a short list of them, and
            // a picture or a video. Underneath they are all the one kind of element:
            // any of them can be given words by double-clicking it, and any fill.
            EditorTool {
                objectName: "addTextTool"
                kind: "text"
                hint: "Add a text box"
                available: canvas.slide !== null
                onClicked: canvas.addText()
            }

            EditorTool {
                id: shapesTool

                objectName: "addShapeTool"
                kind: "shapes"
                hint: "Add a shape: a rectangle, a rounded rectangle, an ellipse or an arrow"
                available: canvas.slide !== null
                onClicked: screen.showMenu([
                    { header: "Shapes" },
                    { label: "Rectangle", run: () => canvas.addShape("rectangle") },
                    { label: "Rounded Rectangle", run: () => canvas.addShape("roundedRectangle") },
                    { label: "Ellipse", run: () => canvas.addShape("ellipse") },
                    { label: "Arrow", run: () => canvas.addShape("arrow") }
                ], shapesTool, 0, shapesTool.height + 4)
            }

            EditorTool {
                objectName: "addMediaTool"
                kind: "media"
                hint: "Add a picture or a video from a file"
                available: canvas.slide !== null
                onClicked: screen.chooseMedia("add")
            }

            Item {
                width: 10
                height: 1
            }

            EditorTool {
                kind: "duplicate"
                hint: "Duplicate the picked element (Ctrl+D)"
                available: canvas.selected !== null
                onClicked: canvas.duplicate()
            }

            EditorTool {
                kind: "delete"
                hint: "Delete the picked element (Delete)"
                available: canvas.selected !== null
                onClicked: canvas.removeSelected()
            }

            Item {
                width: 10
                height: 1
            }

            EditorTool {
                kind: "undo"
                hint: "Undo (Ctrl+Z)"
                available: editor.canUndo || canvas.typed
                onClicked: canvas.undo()
            }

            EditorTool {
                kind: "redo"
                hint: "Redo (Ctrl+Shift+Z)"
                available: editor.canRedo
                onClicked: canvas.redo()
            }
        }

        Text {
            anchors.left: actions.right
            anchors.leftMargin: 14
            anchors.right: doneButton.left
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: "#9a9da3"
            font.pixelSize: 13
            text: canvas.slide ? screen.rowWord + " " + (canvas.row + 1) + " of " + editor.count : ""
        }

        AppButton {
            id: doneButton

            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            height: 30
            font.pixelSize: 13
            text: "Done"
            onClicked: screen.done()
        }
    }

    EditorCanvas {
        id: canvas

        anchors.left: left.right
        anchors.right: inspector.left
        anchors.top: strip.bottom
        anchors.bottom: hint.top
        clip: true
        editor: editor
        bridge: bridge
        backdrop: slide ? screen.backdropFor(slide.id) : ""
        onFailed: (error) => screen.report(error)
        onTold: (message) => screen.tell(message)
        onMenuRequested: (x, y) => screen.showElementMenu(canvas, x, y)
        onSlideStepRequested: (delta) => screen.showRow(canvas.row + delta)
    }

    // Whatever there is to tell the user, and otherwise how to work the canvas, for
    // whatever is going on
    Rectangle {
        id: hint

        anchors.left: left.right
        anchors.right: inspector.left
        anchors.bottom: parent.bottom
        height: Math.max(26, hintText.implicitHeight + 10)
        color: screen.notice === "" ? "#191a1d" : screen.noticeIsError ? "#4a1f1f" : "#23324a"

        Text {
            id: hintText

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            wrapMode: screen.notice === "" ? Text.NoWrap : Text.Wrap
            elide: screen.notice === "" ? Text.ElideRight : Text.ElideNone
            color: screen.notice === "" ? "#7d8088" : "#f0f0f0"
            font.pixelSize: 12
            text: screen.notice !== "" ? screen.notice
                : screen.toolHint !== "" ? screen.toolHint
                : canvas.editing ? "Select text to format part of it  ·  Esc or a click elsewhere finishes  ·  Ctrl+B, I, U"
                : canvas.selected ? "Drag to move, handles to resize  ·  Shift: straight, or in proportion  ·  Ctrl: no snapping  ·  Arrows nudge  ·  Double-click or Enter edits the text"
                : "Click an element to pick it  ·  Double-click text to edit it  ·  Arrows change slide"
        }
    }
}
