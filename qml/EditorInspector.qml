import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The properties of the element picked in the editor, in two parts as ProPresenter has
// them: its shape (where it is, its fill, stroke and shadow, and when it shows) and its
// text (font, colour, alignment, outline, shadow, and where the text comes from).
// Nothing here changes anything itself: each control reports what it was set to through
// setProperties or setFormat. Sliders and colours being dragged report as they go, with
// `interim` set, and settle() when they stop.
Rectangle {
    id: inspector

    // The picked element, a map as proconvert describes, or null; and all the elements
    // of its slide, which are what it can be linked to
    property var element: null
    property var elements: []
    // The format of the text being worked on: a map as RichText::formatAt gives, or null
    property var format: null
    // Whether that is a selection in text being edited, rather than all of the text
    property bool selection: false
    // The installed font families
    property var families: []
    property var setProperties: (changes, interim) => {}
    // Shows a menu (see PopupMenu) by an item, at a place in it
    property var showMenu: (items, item, x, y) => {}
    // Asks for a media file to fill the element with
    property var chooseMedia: () => {}
    // Brings up the Media Inspector on the picked element's media
    property var inspectMedia: () => {}
    property var setFormat: (format, interim) => {}
    property var settle: () => {}
    // The Color Triggers kept as the default ([{ time, color }]), in the app's own
    // settings, and how one is kept (Main.qml)
    property var colorTriggerDefault: []
    property var saveColorTriggerDefault: (list) => {}
    property string tab: "shape"

    // A value was typed in; whoever had the keyboard can have it back.
    signal finished

    readonly property var others: element ? elements.filter(e => e.id !== element.id) : []
    // Their names, as a list that only changes when a name does, so that the drop-downs
    // listing them are not rebuilt by every other change to the slide. (An element may
    // have no name, so whether there are any is counted and not read off the names.)
    readonly property int otherCount: others.length
    readonly property string otherNamesJoined: others.map(e => inspector.oneLine(e.name)).join("\n")
    readonly property var otherNames: otherCount === 0 ? [] : otherNamesJoined.split("\n")
    readonly property var transforms: ["As it is", "On one line", "A word to a line", "A letter to a line"]
    // The workspace's timers, which are the other thing an element's text can be linked
    // to, and their names as a list that only changes when a name does
    readonly property var timers: Timers.timers
    readonly property string timerNamesJoined: timers.map(t => inspector.oneLine(t.name)).join("\n")
    readonly property var timerNames: timers.length === 0 ? [] : timerNamesJoined.split("\n")
    // How a part of a timer's time can be written, in the order ProPresenter offers
    // them, and the style each is in the file (Timers.Style)
    readonly property var timerStyleNames: ["Hidden", "Two digits", "One digit", "Two digits, hidden at 0",
                                            "One digit, hidden at 0"]
    readonly property var timerStyles: [Timers.None, Timers.Long, Timers.Short, Timers.RemoveLong, Timers.RemoveShort]
    // The five ways a clock's date or time can be written, in the file's order (see
    // clocktext.h), by ProPresenter's names for them
    readonly property var clockStyles: ["None", "Short", "Medium", "Long", "Full"]
    // The four parts, with the key each is under in an element and how a newly linked
    // box has it: hours when there are any, then minutes and seconds
    readonly property var timerParts: [
        { caption: "Hours", key: "linkTimerHours", usual: Timers.RemoveShort },
        { caption: "Minutes", key: "linkTimerMinutes", usual: Timers.Long },
        { caption: "Seconds", key: "linkTimerSeconds", usual: Timers.Long },
        { caption: "Hundredths", key: "linkTimerHundredths", usual: Timers.None }
    ]

    function oneLine(name) {
        return name.replace(/\s+/g, " ").trim()
    }

    color: "#2b2d31"

    component Caption: Text {
        width: 72
        color: "#9a9da3"
        font.pixelSize: 11
        font.capitalization: Font.AllUppercase
        elide: Text.ElideRight
    }

    // A caption with controls after it, or with no caption, controls from the edge
    component Line: Item {
        property alias caption: label.text
        default property alias controls: row.data

        width: parent.width
        height: Math.max(30, row.height + 4)

        Caption {
            id: label

            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            id: row

            x: label.text === "" ? 0 : 78
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
        }
    }

    // The same with a tick box for a caption
    component CheckLine: Item {
        property alias text: check.text
        property alias checked: check.checked
        default property alias controls: row.data

        signal toggled(bool checked)

        width: parent.width
        height: 30

        AppCheck {
            id: check

            width: 76
            anchors.verticalCenter: parent.verticalCenter
            onToggled: (checked) => parent.toggled(checked)
        }

        Row {
            id: row

            x: 78
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
        }
    }

    // A drop-down that shows `choice` and reports another with chosen(). A ComboBox
    // goes back to its first entry whenever its list changes, and the lists here change
    // with the slide, so this puts it back.
    component Choice: AppComboBox {
        property int choice: 0

        signal chosen(int index)

        height: 28
        font.pixelSize: 13
        onChoiceChanged: currentIndex = choice
        onModelChanged: currentIndex = choice
        Component.onCompleted: currentIndex = choice
        // Put back first: reporting the choice may rebuild whatever this is part of.
        onActivated: (index) => {
            currentIndex = choice
            chosen(index)
        }
    }

    // A small caption in front of a control, within a Line
    component Tag: Text {
        anchors.verticalCenter: parent.verticalCenter
        color: "#9a9da3"
        font.pixelSize: 12
    }

    component Heading: Item {
        property alias text: title.text

        width: parent.width
        height: 34

        Rectangle {
            y: 8
            width: parent.width
            height: 1
            color: "#3f4248"
        }

        Text {
            id: title

            y: 15
            color: "#e6e6e6"
            font.pixelSize: 12
            font.bold: true
        }
    }

    component Note: Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#9a9da3"
        font.pixelSize: 12
    }

    // Angle, offset and blur of a shadow whose keys start with `which`
    component ShadowLine: Line {
        property string which

        Tag {
            text: "Angle"
        }

        NumberField {
            width: 44
            from: 0
            to: 360
            step: 5
            value: inspector.element ? inspector.element[which + "Angle"] : 0
            onEdited: (value) => inspector.setProperties({ [which + "Angle"]: value }, false)
            onFinished: inspector.finished()
        }

        Tag {
            text: "Offset"
        }

        NumberField {
            width: 40
            from: 0
            to: 500
            value: inspector.element ? inspector.element[which + "Offset"] : 0
            onEdited: (value) => inspector.setProperties({ [which + "Offset"]: value }, false)
            onFinished: inspector.finished()
        }

        Tag {
            text: "Blur"
        }

        NumberField {
            width: 40
            from: 0
            to: 500
            value: inspector.element ? inspector.element[which + "Radius"] : 0
            onEdited: (value) => inspector.setProperties({ [which + "Radius"]: value }, false)
            onFinished: inspector.finished()
        }
    }

    // Shape and Text
    Row {
        id: tabs

        x: 12
        y: 10
        spacing: 6

        Repeater {
            model: [{ name: "Shape", tab: "shape" }, { name: "Text", tab: "text" }]

            delegate: Rectangle {
                required property var modelData

                width: (inspector.width - 30) / 2
                height: 28
                radius: 6
                color: inspector.tab === modelData.tab ? "#ff8a1f" : tabMouse.containsMouse ? "#45484e" : "#3a3c42"

                Text {
                    anchors.centerIn: parent
                    color: inspector.tab === parent.modelData.tab ? "black" : "#e6e6e6"
                    font.pixelSize: 13
                    font.bold: inspector.tab === parent.modelData.tab
                    text: parent.modelData.name
                }

                MouseArea {
                    id: tabMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: inspector.tab = parent.modelData.tab
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 48
        visible: inspector.element === null
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: "#9a9da3"
        font.pixelSize: 13
        text: "Click an element on the slide, or in the list, to change it."
    }

    Flickable {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tabs.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        contentHeight: (inspector.tab === "shape" ? shapeTab.height : textTab.height) + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        visible: inspector.element !== null

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        // The shape
        Column {
            id: shapeTab

            x: 12
            width: parent.width - 26
            visible: inspector.tab === "shape"

            Line {
                caption: "Name"

                AppTextField {
                    id: nameField

                    function show() {
                        text = inspector.element ? inspector.element.name : ""
                    }

                    width: shapeTab.width - 78
                    height: 26
                    font.pixelSize: 12
                    onEditingFinished: {
                        const name = text.trim()
                        if (inspector.element && name !== "" && name !== inspector.element.name)
                            inspector.setProperties({ name: name }, false)
                        show()
                    }
                    onAccepted: inspector.finished()
                    Keys.onEscapePressed: {
                        show()
                        inspector.finished()
                    }
                    Component.onCompleted: show()

                    Connections {
                        target: inspector

                        function onElementChanged() {
                            if (!nameField.activeFocus)
                                nameField.show()
                        }
                    }
                }
            }

            Line {
                caption: "Position"

                Tag {
                    text: "X"
                }

                NumberField {
                    decimals: 1
                    value: inspector.element ? inspector.element.x : 0
                    onEdited: (value) => inspector.setProperties({ x: value }, false)
                    onFinished: inspector.finished()
                }

                Tag {
                    text: "Y"
                }

                NumberField {
                    decimals: 1
                    value: inspector.element ? inspector.element.y : 0
                    onEdited: (value) => inspector.setProperties({ y: value }, false)
                    onFinished: inspector.finished()
                }
            }

            Line {
                caption: "Size"

                Tag {
                    text: "W"
                }

                NumberField {
                    decimals: 1
                    from: 1
                    value: inspector.element ? inspector.element.width : 0
                    onEdited: (value) => inspector.setProperties({ width: value }, false)
                    onFinished: inspector.finished()
                }

                Tag {
                    text: "H"
                }

                NumberField {
                    decimals: 1
                    from: 1
                    value: inspector.element ? inspector.element.height : 0
                    onEdited: (value) => inspector.setProperties({ height: value }, false)
                    onFinished: inspector.finished()
                }
            }

            // How far it is turned, clockwise about its middle. (Dragging a corner
            // handle with Ctrl held turns it too.)
            Line {
                caption: "Rotation"

                NumberField {
                    id: turnField

                    objectName: "rotation"
                    decimals: 1
                    from: -360
                    to: 360
                    suffix: "°"
                    // ProPresenter writes a whole turn for none
                    value: inspector.element ? (inspector.element.rotation % 360 + 360) % 360 : 0
                    onEdited: (value) => inspector.setProperties({ rotation: value }, false)
                    onFinished: inspector.finished()
                }

                IconButton {
                    width: 26
                    text: "−"
                    onClicked: inspector.setProperties({ rotation: Math.round(turnField.value) - 1 }, false)
                }

                IconButton {
                    width: 26
                    text: "+"
                    onClicked: inspector.setProperties({ rotation: Math.round(turnField.value) + 1 }, false)
                }
            }

            Line {
                caption: "Opacity"

                AppSlider {
                    width: shapeTab.width - 78 - 60
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0
                    to: 1
                    value: inspector.element ? inspector.element.opacity : 1
                    onMoved: inspector.setProperties({ opacity: Math.round(value * 100) / 100 }, true)
                    onPressedChanged: {
                        if (!pressed)
                            inspector.settle()
                    }
                }

                NumberField {
                    width: 52
                    from: 0
                    to: 100
                    step: 5
                    suffix: "%"
                    value: inspector.element ? Math.round(inspector.element.opacity * 100) : 100
                    onEdited: (value) => inspector.setProperties({ opacity: value / 100 }, false)
                    onFinished: inspector.finished()
                }
            }

            Heading {
                text: "Fill"
            }

            // What it is filled with: a colour, a gradient from one colour to another, a
            // picture, or a Screen Preview, which is what one of the workspace's screens is
            // showing (see ScreenPicture), as ProPresenter's fill menu has it. Choosing a
            // kind turns the fill on. A Screen Preview starts as the first audience screen.
            CheckLine {
                id: fillLine

                readonly property string kind: inspector.element ? inspector.element.fillKind : "none"

                text: "Fill"
                checked: inspector.element ? inspector.element.fillOn : false
                onToggled: (checked) => inspector.setProperties({ fillOn: checked }, false)

                Choice {
                    objectName: "fillKind"
                    width: shapeTab.width - 78
                    model: ["Colour", "Gradient", "Media", "Screen Preview"]
                    choice: fillLine.kind === "gradient" ? 1 : fillLine.kind === "media" ? 2 : fillLine.kind === "screen" ? 3 : 0
                    onChosen: (index) => {
                        if (index === 2) {
                            inspector.chooseMedia()
                        } else if (index === 3) {
                            if (fillLine.kind !== "screen") {
                                const first = Screens.audience[0] ?? Screens.screens[0]
                                inspector.setProperties({ fillKind: "screen", fillScreenId: first ? first.id : "",
                                                          fillScreenName: first ? first.name : "", fillOn: true }, false)
                            }
                        } else {
                            inspector.setProperties({ fillKind: index === 1 ? "gradient" : "color", fillOn: true }, false)
                        }
                    }
                }
            }

            Line {
                caption: "Colour"
                visible: fillLine.kind !== "gradient" && fillLine.kind !== "media" && fillLine.kind !== "screen"

                // Picking a colour turns the fill on, and makes it a plain colour.
                ColorButton {
                    value: inspector.element ? inspector.element.fillColor : "black"
                    onChanging: (value) => inspector.setProperties({ fillColor: value, fillOn: true }, true)
                    onPicked: (value) => inspector.setProperties({ fillColor: value, fillOn: true }, false)
                    onClosed: inspector.finished()
                }
            }

            // From the first colour to the second, along the angle: 0 is to the right
            // and the angle turns anticlockwise, so 270 is downwards.
            Line {
                caption: "Colours"
                visible: fillLine.kind === "gradient"

                ColorButton {
                    objectName: "gradientFrom"
                    value: inspector.element ? inspector.element.fillGradientFrom : "white"
                    onChanging: (value) => inspector.setProperties({ fillGradientFrom: value }, true)
                    onPicked: (value) => inspector.setProperties({ fillGradientFrom: value }, false)
                    onClosed: inspector.finished()
                }

                ColorButton {
                    objectName: "gradientTo"
                    value: inspector.element ? inspector.element.fillGradientTo : "black"
                    onChanging: (value) => inspector.setProperties({ fillGradientTo: value }, true)
                    onPicked: (value) => inspector.setProperties({ fillGradientTo: value }, false)
                    onClosed: inspector.finished()
                }

                Tag {
                    text: "Angle"
                }

                NumberField {
                    objectName: "gradientAngle"
                    width: 52
                    from: 0
                    to: 359
                    step: 15
                    suffix: "°"
                    value: inspector.element ? Math.round(inspector.element.fillGradientAngle) : 0
                    onEdited: (value) => inspector.setProperties({ fillGradientAngle: value }, false)
                    onFinished: inspector.finished()
                }
            }

            Line {
                caption: "File"
                visible: fillLine.kind === "media"

                Text {
                    width: shapeTab.width - 78 - 90
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideMiddle
                    color: inspector.element && inspector.element.fillMediaPath === "" ? "#d07070" : "#e6e6e6"
                    font.pixelSize: 12
                    text: inspector.element ? inspector.element.fillMediaName : ""
                }

                AppButton {
                    width: 84
                    height: 26
                    font.pixelSize: 12
                    text: "Choose…"
                    onClicked: inspector.chooseMedia()
                }
            }

            // All that can be set about the media, ProPresenter's Media Inspector
            // (MediaInspector.qml), of which the scale below is one
            Line {
                caption: "Media"
                visible: fillLine.kind === "media"

                AppButton {
                    objectName: "fillInspector"
                    height: 26
                    font.pixelSize: 12
                    text: "Inspector…"
                    enabled: inspector.element !== null && inspector.element.fillMediaPath !== ""
                    opacity: enabled ? 1 : 0.45
                    onClicked: inspector.inspectMedia()
                }
            }

            Line {
                caption: "Scale"
                visible: fillLine.kind === "media"

                Choice {
                    objectName: "mediaScale"
                    width: shapeTab.width - 78
                    model: ["Scale to Fit", "Scale to Fill", "Stretch to Fill"]
                    choice: inspector.element ? inspector.element.fillMediaScale : 0
                    onChosen: (index) => inspector.setProperties({ fillMediaScale: index }, false)
                }
            }

            // Which screen a Screen Preview is of: any of the workspace's, audience or
            // stage, by name. One the link names that this workspace does not have is
            // shown as the link names it, and nothing is drawn for it.
            Line {
                caption: "Screen"
                visible: fillLine.kind === "screen"

                Choice {
                    id: screenChoice

                    readonly property var screens: Screens.screens
                    readonly property int found: {
                        const element = inspector.element
                        if (!element || element.fillKind !== "screen")
                            return -1
                        const byId = screens.findIndex(screen => screen.id === element.fillScreenId)
                        return byId >= 0 ? byId : screens.findIndex(screen => screen.name === element.fillScreenName)
                    }

                    objectName: "fillScreen"
                    width: shapeTab.width - 78
                    model: found >= 0 || !inspector.element ? screens.map(screen => screen.name)
                                                            : screens.map(screen => screen.name).concat([inspector.element.fillScreenName + " (not here)"])
                    choice: found >= 0 ? found : screens.length
                    onChosen: (index) => {
                        if (index < screens.length && index !== found)
                            inspector.setProperties({ fillScreenId: screens[index].id, fillScreenName: screens[index].name }, false)
                    }
                }
            }

            AppCheck {
                width: parent.width
                visible: fillLine.kind !== "gradient" && fillLine.kind !== "media" && fillLine.kind !== "screen"
                text: "Only behind the lines of text"
                checked: inspector.element ? inspector.element.fillLinesOnly : false
                onToggled: (checked) => inspector.setProperties({ fillLinesOnly: checked }, false)
            }

            Note {
                topPadding: 4
                visible: inspector.element !== null && (fillLine.kind === "other"
                         || (fillLine.kind === "media" && (inspector.element.fillMediaVideo || inspector.element.fillMediaPath === "")))
                text: fillLine.kind === "other"
                      ? "This element is filled with something that is not drawn here yet. It is kept as it is unless another fill is chosen, which replaces it."
                      : inspector.element && inspector.element.fillMediaPath === ""
                        ? "That file is not in this workspace or where it was when it was chosen, so nothing is drawn for it."
                        : "A video as a fill is kept in the file but not drawn here yet: only pictures are."
            }

            // The corners of a rounded rectangle, which the handle on its top edge also sets
            Line {
                caption: "Corners"
                visible: inspector.element !== null && inspector.element.shape === "roundedRectangle"

                AppSlider {
                    objectName: "roundness"
                    width: shapeTab.width - 78
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0
                    to: 0.5
                    value: inspector.element ? inspector.element.roundness : 0
                    onMoved: inspector.setProperties({ roundness: Math.round(value * 1000) / 1000 }, true)
                    onPressedChanged: {
                        if (!pressed)
                            inspector.settle()
                    }
                }
            }

            // Edges that fade out, for a rectangle, a rounded one or an ellipse
            CheckLine {
                text: "Feather"
                visible: inspector.element !== null && inspector.element.shape !== "arrow" && inspector.element.shape !== "other"
                checked: inspector.element ? inspector.element.featherOn : false
                onToggled: (checked) => inspector.setProperties({ featherOn: checked }, false)

                AppSlider {
                    objectName: "feather"
                    width: shapeTab.width - 78
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0
                    to: 0.5
                    value: inspector.element ? inspector.element.featherRadius : 0
                    onMoved: inspector.setProperties({ featherRadius: Math.round(value * 1000) / 1000, featherOn: true }, true)
                    onPressedChanged: {
                        if (!pressed)
                            inspector.settle()
                    }
                }
            }

            Heading {
                text: "Stroke"
            }

            CheckLine {
                text: "Stroke"
                checked: inspector.element ? inspector.element.strokeOn : false
                onToggled: (checked) => inspector.setProperties({ strokeOn: checked }, false)

                ColorButton {
                    value: inspector.element ? inspector.element.strokeColor : "white"
                    onChanging: (value) => inspector.setProperties({ strokeColor: value, strokeOn: true }, true)
                    onPicked: (value) => inspector.setProperties({ strokeColor: value, strokeOn: true }, false)
                    onClosed: inspector.finished()
                }

                Tag {
                    text: "Width"
                }

                NumberField {
                    width: 46
                    decimals: 1
                    from: 0
                    to: 200
                    value: inspector.element ? inspector.element.strokeWidth : 0
                    onEdited: (value) => inspector.setProperties({ strokeWidth: value }, false)
                    onFinished: inspector.finished()
                }
            }

            Heading {
                text: "Shadow"
            }

            CheckLine {
                text: "Shadow"
                checked: inspector.element ? inspector.element.shadowEnabled : false
                onToggled: (checked) => inspector.setProperties({ shadowEnabled: checked }, false)

                ColorButton {
                    value: inspector.element ? inspector.element.shadowColor : "black"
                    onChanging: (value) => inspector.setProperties({ shadowColor: value, shadowEnabled: true }, true)
                    onPicked: (value) => inspector.setProperties({ shadowColor: value, shadowEnabled: true }, false)
                    onClosed: inspector.finished()
                }
            }

            ShadowLine {
                which: "shadow"
            }

            Heading {
                text: "Visibility"
            }

            AppCheck {
                width: parent.width
                text: "Show only when…"
                checked: inspector.element ? inspector.element.visibilityRules : false
                onToggled: (checked) => {
                    // Starts with one condition, on the first other element, if there is one.
                    if (checked && inspector.others.length > 0)
                        inspector.setProperties({
                            visibilityRules: true,
                            visibilityCriterion: 0,
                            visibilityConditions: [{ kind: "element", elementId: inspector.others[0].id, hasText: true }]
                        }, false)
                    else
                        inspector.setProperties({ visibilityRules: checked }, false)
                }
            }

            Column {
                id: rules

                readonly property var conditions: inspector.element ? inspector.element.visibilityConditions : []

                // The conditions with one of them changed, or with `change` null removed.
                function edited(index, change) {
                    const list = []
                    for (let i = 0; i < conditions.length; ++i) {
                        if (i !== index)
                            list.push(conditions[i])
                        else if (change !== null)
                            list.push(Object.assign({}, conditions[i], change))
                    }
                    return list
                }

                width: parent.width
                spacing: 6
                topPadding: 4
                visible: inspector.element !== null && inspector.element.visibilityRules

                Choice {
                    width: parent.width
                    model: ["all of these are so", "any of these is so", "none of these is so"]
                    choice: inspector.element ? inspector.element.visibilityCriterion : 0
                    onChosen: (index) => inspector.setProperties({ visibilityCriterion: index }, false)
                }

                Repeater {
                    model: rules.conditions

                    delegate: Row {
                        id: condition

                        required property var modelData
                        required property int index
                        readonly property bool known: modelData.kind === "element"
                        // The element it is about may have gone from the slide.
                        readonly property int place: known ? inspector.others.findIndex(e => e.id === modelData.elementId) : -1

                        spacing: 6

                        Choice {
                            width: rules.width - 110 - 26 - 12
                            visible: condition.known
                            model: (condition.place < 0 && condition.known ? ["“" + condition.modelData.elementName + "” (gone)"] : [])
                                   .concat(inspector.otherNames.map(name => name !== "" ? name : "(unnamed)"))
                            choice: Math.max(0, condition.place)
                            onChosen: (index) => {
                                const other = inspector.others[index - (condition.place < 0 ? 1 : 0)]
                                if (other)
                                    inspector.setProperties({ visibilityConditions: rules.edited(condition.index, { elementId: other.id }) }, false)
                            }
                        }

                        Choice {
                            width: 110
                            visible: condition.known
                            model: ["has text", "is empty"]
                            choice: condition.modelData.hasText ? 0 : 1
                            onChosen: (index) => inspector.setProperties(
                                { visibilityConditions: rules.edited(condition.index, { hasText: index === 0 }) }, false)
                        }

                        // A condition on something only ProPresenter tracks
                        Text {
                            width: rules.width - 26 - 6
                            height: 28
                            visible: !condition.known
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            color: "#9a9da3"
                            font.pixelSize: 12
                            text: condition.known ? "" : condition.modelData.label + " (set up in ProPresenter)"
                        }

                        IconButton {
                            width: 26
                            height: 28
                            text: "✕"
                            onClicked: inspector.setProperties({ visibilityConditions: rules.edited(condition.index, null) }, false)
                        }
                    }
                }

                AppButton {
                    height: 28
                    font.pixelSize: 13
                    text: "Add a condition"
                    enabled: inspector.others.length > 0
                    onClicked: inspector.setProperties({
                        visibilityConditions: rules.conditions.concat([{ kind: "element", elementId: inspector.others[0].id, hasText: true }])
                    }, false)
                }

                Note {
                    text: "Conditions are checked when the slide is shown. Here the element stays in view so that it can be worked on."
                }
            }
        }

        // The text
        Column {
            id: textTab

            x: 12
            width: parent.width - 26
            visible: inspector.tab === "text" && inspector.format !== null

            Note {
                bottomPadding: 6
                text: inspector.selection ? "Changes apply to the selected text." : "Changes apply to all of the text."
            }

            Line {
                caption: "Font"

                FontPicker {
                    width: textTab.width - 78
                    family: inspector.format ? inspector.format.family : ""
                    families: inspector.families
                    onPicked: (family) => inspector.setFormat({ family: family }, false)
                    onClosed: inspector.finished()
                }
            }

            Line {
                caption: "Size"

                NumberField {
                    id: sizeField

                    width: 52
                    from: 1
                    to: 2000
                    value: inspector.format ? inspector.format.size : 0
                    onEdited: (value) => inspector.setFormat({ size: value }, false)
                    onFinished: inspector.finished()
                }

                IconButton {
                    width: 26
                    text: "−"
                    onClicked: inspector.setFormat({ size: Math.max(1, Math.round(sizeField.value) - 2) }, false)
                }

                IconButton {
                    width: 26
                    text: "+"
                    onClicked: inspector.setFormat({ size: Math.round(sizeField.value) + 2 }, false)
                }

                ColorButton {
                    value: inspector.format ? inspector.format.color : "white"
                    onChanging: (value) => inspector.setFormat({ color: value }, true)
                    onPicked: (value) => inspector.setFormat({ color: value }, false)
                    onClosed: inspector.finished()
                }
            }

            Line {
                caption: "Style"

                Repeater {
                    model: [{ kind: "bold", key: "bold" }, { kind: "italic", key: "italic" },
                            { kind: "underline", key: "underline" }, { kind: "strike", key: "strikethrough" }]

                    delegate: IconButton {
                        required property var modelData

                        kind: modelData.kind
                        on: inspector.format ? inspector.format[modelData.key] === true : false
                        onClicked: inspector.setFormat({ [modelData.key]: !on }, false)
                    }
                }
            }

            Line {
                caption: "Align"

                Repeater {
                    model: [{ kind: "alignLeft", flag: Qt.AlignLeft }, { kind: "alignCenter", flag: Qt.AlignHCenter },
                            { kind: "alignRight", flag: Qt.AlignRight }, { kind: "alignJustify", flag: Qt.AlignJustify }]

                    delegate: IconButton {
                        required property var modelData

                        kind: modelData.kind
                        on: inspector.format ? (inspector.format.alignment & modelData.flag) !== 0 : false
                        onClicked: inspector.setFormat({ alignment: modelData.flag }, false)
                    }
                }
            }

            Line {
                caption: "In the box"

                Repeater {
                    model: [{ kind: "alignTop", flag: Qt.AlignTop }, { kind: "alignMiddle", flag: Qt.AlignVCenter },
                            { kind: "alignBottom", flag: Qt.AlignBottom }]

                    delegate: IconButton {
                        required property var modelData

                        kind: modelData.kind
                        on: inspector.element ? (inspector.element.verticalAlignment & modelData.flag) !== 0 : false
                        onClicked: inspector.setProperties({ verticalAlignment: modelData.flag }, false)
                    }
                }
            }

            // Whether the text's size is changed to suit the box, in ProPresenter's
            // words for it. (ProPresenter can also have the box's height suit the text,
            // which this app does not do: a box set that way is listed as it is, and
            // drawn at the size the text has.)
            Line {
                id: scaleLine

                readonly property int scale: inspector.element ? inspector.element.textScale : 0
                readonly property var values: scale === 1 ? [0, 1, 2, 3, 4] : [0, 2, 3, 4]
                readonly property var names: ["None", "Adjust Container Height", "Scale Font Down", "Scale Font Up", "Scale Font Up/Down"]

                caption: "Scale"

                Choice {
                    width: textTab.width - 78
                    model: scaleLine.values.map(value => scaleLine.names[value])
                    choice: Math.max(0, scaleLine.values.indexOf(scaleLine.scale))
                    onChosen: (index) => inspector.setProperties({ textScale: scaleLine.values[index] }, false)
                }
            }

            Line {
                caption: "Capitals"

                Choice {
                    width: textTab.width - 78
                    model: ["As typed", "ALL CAPITALS", "Small capitals", "Title Case", "Start case"]
                    choice: inspector.format ? inspector.format.capitalization : 0
                    onChosen: (index) => inspector.setFormat({ capitalization: index }, false)
                }
            }

            Line {
                caption: "Spacing"

                NumberField {
                    width: 52
                    decimals: 1
                    from: -100
                    to: 500
                    value: inspector.format ? inspector.format.kerning : 0
                    onEdited: (value) => inspector.setFormat({ kerning: value }, false)
                    onFinished: inspector.finished()
                }

                Tag {
                    text: "between letters"
                }
            }

            Heading {
                text: "Outline"
            }

            CheckLine {
                text: "Outline"
                checked: inspector.format ? inspector.format.strokeWidth > 0 : false
                // Turned on, it is three hundredths of the text's size, as a start.
                onToggled: (checked) => inspector.setFormat(
                    { strokeWidth: checked ? Math.max(1, Math.round(inspector.format.size * 0.03)) : 0 }, false)

                ColorButton {
                    value: inspector.format ? inspector.format.strokeColor : "black"
                    onChanging: (value) => inspector.setFormat({ strokeColor: value }, true)
                    onPicked: (value) => inspector.setFormat(inspector.format.strokeWidth > 0
                        ? { strokeColor: value }
                        : { strokeColor: value, strokeWidth: Math.max(1, Math.round(inspector.format.size * 0.03)) }, false)
                    onClosed: inspector.finished()
                }

                Tag {
                    text: "Width"
                }

                NumberField {
                    width: 46
                    decimals: 1
                    from: 0
                    to: 200
                    value: inspector.format ? inspector.format.strokeWidth : 0
                    onEdited: (value) => inspector.setFormat({ strokeWidth: value }, false)
                    onFinished: inspector.finished()
                }
            }

            Heading {
                text: "Shadow"
            }

            CheckLine {
                text: "Shadow"
                checked: inspector.element ? inspector.element.textShadowEnabled : false
                onToggled: (checked) => inspector.setProperties({ textShadowEnabled: checked }, false)

                ColorButton {
                    value: inspector.element ? inspector.element.textShadowColor : "black"
                    onChanging: (value) => inspector.setProperties({ textShadowColor: value, textShadowEnabled: true }, true)
                    onPicked: (value) => inspector.setProperties({ textShadowColor: value, textShadowEnabled: true }, false)
                    onClosed: inspector.finished()
                }
            }

            ShadowLine {
                which: "textShadow"
            }

            Heading {
                text: "Margins"
            }

            Line {
                Repeater {
                    model: [{ tag: "L", key: "marginLeft" }, { tag: "T", key: "marginTop" },
                            { tag: "R", key: "marginRight" }, { tag: "B", key: "marginBottom" }]

                    delegate: Row {
                        required property var modelData

                        spacing: 4

                        Tag {
                            text: parent.modelData.tag
                        }

                        NumberField {
                            width: 44
                            from: 0
                            to: 2000
                            value: inspector.element ? inspector.element[parent.modelData.key] : 0
                            onEdited: (value) => inspector.setProperties({ [parent.modelData.key]: value }, false)
                            onFinished: inspector.finished()
                        }
                    }
                }
            }

            Heading {
                text: "Linked text"
            }

            // What this element shows in place of text of its own, in its own style:
            // the text of another element of the slide, the words or the notes of the
            // slide that is live or of the one after it, the time of a timer, the
            // time of day, how long the video that is playing has left, or what the
            // stage is being told (the stage message).
            //
            // It is picked as ProPresenter has it picked: one button that says what
            // the element shows, and opens a menu of what it can show. Where there
            // are several of a thing (the slide's other elements, the workspace's
            // timers) its row leads to a menu of them; where a thing has more to be
            // said about it (which of a slide's text, how a clock is written) that is
            // set under the button once the thing is picked.
            //
            // A GUESS at ProPresenter, whose own menu was described and not looked at:
            // the order of the rows and what each is called.
            Column {
                width: parent.width
                spacing: 6
                visible: inspector.element !== null && inspector.element.linkKind !== "other"

                Line {
                    id: linkLine

                    readonly property string kind: inspector.element !== null ? inspector.element.linkKind : "none"
                    // The element it is linked to may have gone from the slide, and the
                    // timer may not be one of this workspace's. A timer is found by its
                    // id or, failing that, by its name; reading the tick has this
                    // looked up again when the timers change.
                    readonly property int place: kind === "element"
                        ? inspector.others.findIndex(e => e.id === inspector.element.linkElementId) : -1
                    readonly property string timerId: kind === "timer" && Timers.tick >= 0
                        ? Timers.linkedTimer(inspector.element.linkTimerId, inspector.element.linkTimerName) : ""
                    readonly property int timerPlace: inspector.timers.findIndex(t => t.id === timerId)
                    readonly property bool elementGone: kind === "element" && place < 0
                    readonly property bool timerGone: kind === "timer" && timerPlace < 0
                    // Which of a slide's text it shows, where it shows a slide's
                    readonly property int slideSource: kind === "slideText" ? inspector.element.linkSlideSource : Show.Words
                    readonly property bool notes: kind === "slideText" && slideSource === Show.Notes
                    // What it shows, in a few words, for the button
                    readonly property string shown: kind === "element"
                            ? (elementGone ? "“" + inspector.oneLine(inspector.element.linkElementName) + "” (gone)"
                               : "Element: " + (inspector.otherNames[place] !== "" ? inspector.otherNames[place] : "unnamed"))
                        : kind === "timer"
                            ? (timerGone ? "Timer: " + inspector.oneLine(inspector.element.linkTimerName) + " (not here)"
                               : "Timer: " + (inspector.timerNames[timerPlace] !== "" ? inspector.timerNames[timerPlace] : "unnamed"))
                        : kind === "slideText" ? (inspector.element.linkSlideNext ? "Next Slide" : "Current Slide")
                        : kind === "clock" ? "System Clock"
                        : kind === "videoCountdown" ? "Video Countdown"
                        : kind === "stageMessage" ? "Stage Message"
                        : "Its Own Text"

                    // The rows of the menu the button opens
                    function choices() {
                        const element = inspector.element
                        // A slide's text: all of its words, as it was last set if it
                        // was set to show a slide's already
                        const slide = (next) => {
                            const changes = { linkKind: "slideText", linkSlideNext: next }
                            if (linkLine.kind !== "slideText")
                                changes.linkSlideSource = Show.Words
                            inspector.setProperties(changes, false)
                        }
                        const timer = (which) => {
                            // The first time, it is written the usual way; after
                            // that, the way it was set to be.
                            const changes = { linkKind: "timer", linkTimerId: which.id, linkTimerName: which.name }
                            if (linkLine.kind !== "timer")
                                inspector.timerParts.forEach(part => changes[part.key] = part.usual)
                            inspector.setProperties(changes, false)
                        }
                        const others = inspector.others.map((other, index) => ({
                            label: inspector.otherNames[index] !== "" ? inspector.otherNames[index] : "An unnamed element",
                            current: linkLine.kind === "element" && linkLine.place === index,
                            run: () => inspector.setProperties({ linkKind: "element", linkElementId: other.id }, false) }))
                        const timers = inspector.timers.map((which, index) => ({
                            label: inspector.timerNames[index] !== "" ? inspector.timerNames[index] : "An unnamed timer",
                            current: linkLine.kind === "timer" && linkLine.timerPlace === index,
                            run: () => timer(which) }))
                        return [
                            { header: "Shows" },
                            { label: "Its Own Text", current: linkLine.kind === "none", run: () => inspector.setProperties({ linkKind: "none" }, false) },
                            { divider: true },
                            { label: "Another Element", current: linkLine.kind === "element", disabled: others.length === 0,
                              items: [{ header: "Another Element" }].concat(others) },
                            { label: "Current Slide", current: linkLine.kind === "slideText" && !element.linkSlideNext, run: () => slide(false) },
                            { label: "Next Slide", current: linkLine.kind === "slideText" && element.linkSlideNext, run: () => slide(true) },
                            { label: "Timer", current: linkLine.kind === "timer", disabled: timers.length === 0,
                              items: [{ header: "Timer" }].concat(timers) },
                            { label: "System Clock", current: linkLine.kind === "clock", run: () => {
                                // The first time, the time alone, the short way: as
                                // ProPresenter's own stage layouts have a clock.
                                const changes = { linkKind: "clock" }
                                if (linkLine.kind !== "clock")
                                    Object.assign(changes, { linkClockDate: 0, linkClockTime: 1, linkClock24: false })
                                inspector.setProperties(changes, false)
                            } },
                            { label: "Video Countdown", current: linkLine.kind === "videoCountdown", run: () => {
                                // The first time, minutes and seconds: likewise.
                                const changes = { linkKind: "videoCountdown" }
                                if (linkLine.kind !== "videoCountdown")
                                    Object.assign(changes, { linkTimerHours: Timers.None, linkTimerMinutes: Timers.Long, linkTimerSeconds: Timers.Long,
                                                             linkTimerHundredths: Timers.None, linkVideoIgnoreLooping: false })
                                inspector.setProperties(changes, false)
                            } },
                            { label: "Stage Message", current: linkLine.kind === "stageMessage", run: () => {
                                // The first time, set to flash, in the red the boxes
                                // of ProPresenter's own stage layouts flash.
                                const changes = { linkKind: "stageMessage" }
                                if (linkLine.kind !== "stageMessage")
                                    Object.assign(changes, { linkFlash: true, linkFlashColor: "#ff2d21" })
                                inspector.setProperties(changes, false)
                            } }
                        ]
                    }

                    caption: "Shows"

                    // The button: what the element shows, and a mark that it opens
                    Rectangle {
                        id: linkButton

                        objectName: "linkChoice"
                        readonly property string shown: linkLine.shown
                        width: textTab.width - 78
                        height: 28
                        radius: 6
                        color: linkMouse.pressed ? "#50535a" : linkMouse.containsMouse ? "#45484e" : "#3a3c42"

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: parent.right
                            anchors.rightMargin: 24
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            color: "#e6e6e6"
                            font.pixelSize: 13
                            text: linkLine.shown
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 9
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#9a9da3"
                            font.pixelSize: 10
                            text: "▾"
                        }

                        MouseArea {
                            id: linkMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: inspector.showMenu(linkLine.choices(), linkButton, 0, linkButton.height + 2)
                        }
                    }
                }

                // What there is to set about the thing picked. It is made only for a box
                // that shows something other than its own text: the drop-downs are dear to
                // make, the editor is made as the app starts, and most boxes show their own.
                Loader {
                    objectName: "linkSettings"
                    width: parent.width
                    active: linkLine.kind !== "none"
                    visible: active

                    sourceComponent: Column {
                        spacing: 6

                        // A slide's text: which of it. All its words; its notes; or the words
                        // of its elements of some name, which is how a stage layout shows a
                        // song's words and leaves out whatever else is on the slide.
                        Line {
                            caption: "Source"
                            visible: linkLine.kind === "slideText"

                            Choice {
                                objectName: "linkSlideSource"
                                width: textTab.width - 78
                                model: ["Text", "Notes", "Match Element Name"]
                                choice: linkLine.slideSource
                                onChosen: (index) => inspector.setProperties({ linkSlideSource: index }, false)
                            }
                        }

                        Line {
                            caption: "Name"
                            visible: linkLine.kind === "slideText" && linkLine.slideSource === Show.ElementNamed

                            AppTextField {
                                id: matchField

                                function show() {
                                    text = inspector.element ? inspector.element.linkSlideName : ""
                                }

                                objectName: "linkSlideName"
                                width: textTab.width - 78
                                height: 26
                                font.pixelSize: 12
                                placeholderText: "An element's name"
                                placeholderTextColor: "#7d8088"
                                onEditingFinished: {
                                    if (inspector.element && linkLine.kind === "slideText" && text.trim() !== inspector.element.linkSlideName)
                                        inspector.setProperties({ linkSlideName: text.trim() }, false)
                                }
                                onAccepted: inspector.finished()
                                Keys.onEscapePressed: {
                                    show()
                                    inspector.finished()
                                }
                                Component.onCompleted: show()

                                Connections {
                                    target: inspector

                                    function onElementChanged() {
                                        if (!matchField.activeFocus)
                                            matchField.show()
                                    }
                                }
                            }
                        }

                        // A slide's notes: as they were written, in their own fonts, sizes and
                        // colours, or as words in this element's style.
                        AppCheck {
                            objectName: "linkSlidePreserve"
                            width: parent.width
                            visible: linkLine.notes
                            text: "Preserve the notes' text formatting"
                            checked: inspector.element !== null && inspector.element.linkSlidePreserve === true
                            onToggled: (checked) => inspector.setProperties({ linkSlidePreserve: checked }, false)
                        }

                        Line {
                            caption: "Set"
                            visible: linkLine.kind === "element"
                                     || (linkLine.kind === "slideText" && !(linkLine.notes && inspector.element.linkSlidePreserve === true))

                            Choice {
                                width: textTab.width - 78
                                model: inspector.transforms
                                choice: inspector.element ? inspector.element.linkTransform : 0
                                onChosen: (index) => inspector.setProperties({ linkTransform: index }, false)
                            }
                        }

                        // Chords, for a text box that shows a slide's words: whether the song's
                        // chords are drawn over them, in what colour, and how they are written.
                        // They are ProPresenter's own three settings, with its names for the
                        // four ways of writing a chord; it is on a stage layout that they are
                        // wanted. (How they are drawn is ChordedText.qml's business. Notes
                        // have no chords.)
                        Line {
                            caption: "Chords"
                            visible: linkLine.kind === "slideText" && !linkLine.notes

                            AppCheck {
                                objectName: "chordsCheck"
                                width: 76
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Show"
                                checked: inspector.element !== null && inspector.element.chordsOn === true
                                onToggled: (checked) => inspector.setProperties({ chordsOn: checked }, false)
                            }

                            ColorButton {
                                objectName: "chordColor"
                                hasAlpha: false
                                value: inspector.element ? inspector.element.chordColor : "white"
                                onChanging: (value) => inspector.setProperties({ chordColor: value }, true)
                                onPicked: (value) => inspector.setProperties({ chordColor: value }, false)
                                onClosed: inspector.finished()
                            }
                        }

                        Line {
                            caption: "As"
                            visible: linkLine.kind === "slideText" && !linkLine.notes && inspector.element !== null && inspector.element.chordsOn === true

                            Choice {
                                objectName: "chordNotation"
                                width: textTab.width - 78
                                model: ["Chords", "Numbers", "Numerals", "Do Re Mi"]
                                choice: inspector.element ? inspector.element.chordNotation : 0
                                onChosen: (index) => inspector.setProperties({ chordNotation: index }, false)
                            }
                        }

                        Note {
                            visible: linkLine.kind === "slideText" && !linkLine.notes && inspector.element !== null && inspector.element.chordsOn === true
                            text: "A song's chords are drawn over its words, in the key picked for it over its slides. So that each chord"
                                  + " stays over its word, a line is never broken: text too wide for the box is made smaller instead."
                        }

                        // How each part of a time is written, as ProPresenter has it: a
                        // drop-down for each of the four. A timer's time, and a video's.
                        Repeater {
                            model: linkLine.kind === "timer" || linkLine.kind === "videoCountdown" ? inspector.timerParts : []

                            delegate: Line {
                                id: partLine

                                required property var modelData

                                caption: modelData.caption

                                Choice {
                                    width: textTab.width - 78
                                    model: inspector.timerStyleNames
                                    choice: inspector.element !== null
                                            ? Math.max(0, inspector.timerStyles.indexOf(inspector.element[partLine.modelData.key])) : 0
                                    onChosen: (index) => inspector.setProperties({ [partLine.modelData.key]: inspector.timerStyles[index] }, false)
                                }
                            }
                        }

                        AppCheck {
                            objectName: "linkVideoIgnoreLooping"
                            width: parent.width
                            visible: linkLine.kind === "videoCountdown"
                            text: "Ignore videos that loop"
                            checked: inspector.element !== null && inspector.element.linkVideoIgnoreLooping === true
                            onToggled: (checked) => inspector.setProperties({ linkVideoIgnoreLooping: checked }, false)
                        }

                        // ProPresenter's Color Triggers, of a timer's time and of a video's:
                        // times at which the words change to a colour (src/colortriggers.h
                        // says when each is in force). Each has its time, typed as
                        // ProPresenter writes it, and its colour; + adds one, × takes one
                        // away, and the handle at a row's left drags it to another place in
                        // the list, which ProPresenter cannot do (the maintainer asked for
                        // it, 2026-10-15). A list can be kept as the default, in the app's
                        // own settings, and put on another box with Recall Default.
                        Item {
                            id: triggerHead

                            readonly property bool shown: linkLine.kind === "timer" || linkLine.kind === "videoCountdown"
                            readonly property var triggers: inspector.element && inspector.element.linkColorTriggers
                                                            ? inspector.element.linkColorTriggers : []

                            // The list with one trigger changed, taken out or moved
                            function changed(index, change) {
                                return triggers.map((t, i) => i === index ? Object.assign({ time: t.time, color: t.color }, change)
                                                                          : { time: t.time, color: t.color })
                            }

                            function set(list, interim) {
                                inspector.setProperties({ linkColorTriggers: list }, interim)
                            }

                            visible: shown
                            width: parent.width
                            height: 30

                            Caption {
                                width: 150
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Color Triggers"
                            }

                            Row {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 4

                                AppButton {
                                    id: triggerDefaults

                                    objectName: "triggerDefaults"
                                    width: 30
                                    height: 26
                                    leftPadding: 0
                                    rightPadding: 0
                                    font.pixelSize: 13
                                    text: "⋯"
                                    onClicked: inspector.showMenu([
                                        { label: "Save as Default", disabled: triggerHead.triggers.length === 0,
                                          run: () => inspector.saveColorTriggerDefault(triggerHead.triggers.map(t => ({ time: t.time, color: String(t.color) }))) },
                                        { label: "Recall Default", disabled: inspector.colorTriggerDefault.length === 0,
                                          run: () => triggerHead.set(inspector.colorTriggerDefault.map(t => ({ time: t.time, color: t.color })), false) },
                                        { divider: true },
                                        { note: inspector.colorTriggerDefault.length === 0 ? "No default is kept yet."
                                              : "The default: " + inspector.colorTriggerDefault.map(t => Timers.triggerTime(t.time)).join(", ") + "." }
                                    ], triggerDefaults, 0, triggerDefaults.height)
                                }

                                AppButton {
                                    objectName: "addTrigger"
                                    width: 30
                                    height: 26
                                    leftPadding: 0
                                    rightPadding: 0
                                    font.pixelSize: 15
                                    text: "+"
                                    // The first at nothing, in ProPresenter's red (its own stage
                                    // layouts have that one); each after it half a minute
                                    // before the latest so far, in yellow. (My choice: what
                                    // ProPresenter's + adds is not known here.)
                                    onClicked: {
                                        const list = triggerHead.changed(-1, {})
                                        const latest = list.reduce((most, t) => Math.max(most, t.time), -30)
                                        list.push(list.length === 0 ? { time: 0, color: "#ff2d21" } : { time: latest + 30, color: "#ffcc00" })
                                        triggerHead.set(list, false)
                                    }
                                }
                            }
                        }

                        Item {
                            id: triggerRows

                            readonly property real rowHeight: 32
                            // The row being dragged, and where it would land: its place
                            // among the others, the list without it
                            property int dragging: -1
                            property int landing: -1

                            // Where a row dragged to `y` lands among the others: after each
                            // whose middle its middle has passed
                            function landingAt(y) {
                                const middle = y + rowHeight / 2
                                let count = 0
                                for (let j = 0; j < triggerHead.triggers.length; ++j) {
                                    if (j !== dragging && (j + 0.5) * rowHeight < middle)
                                        ++count
                                }
                                return count
                            }

                            visible: triggerHead.shown && triggerHead.triggers.length > 0
                            width: parent.width
                            height: triggerHead.triggers.length * rowHeight

                            // Where a line shows the dragged row would land, between the
                            // others as they stand
                            Rectangle {
                                visible: triggerRows.dragging >= 0 && triggerRows.landing !== triggerRows.dragging
                                y: (triggerRows.landing < triggerRows.dragging ? triggerRows.landing : triggerRows.landing + 1) * triggerRows.rowHeight - 1
                                width: parent.width
                                height: 2
                                color: "#4da3ff"
                            }

                            Repeater {
                                model: triggerHead.triggers

                                delegate: Item {
                                    id: triggerRow

                                    required property var modelData
                                    required property int index

                                    objectName: "trigger:" + index
                                    width: triggerRows.width
                                    height: triggerRows.rowHeight
                                    // In its place, but while it is dragged, where it is held
                                    // (the drag sets it, and it is put back when let go)
                                    y: index * triggerRows.rowHeight
                                    z: grip.drag.active ? 2 : 0

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 4
                                        visible: grip.drag.active
                                        color: "#3a3c42"
                                    }

                                    // The handle
                                    Text {
                                        id: handle

                                        x: 2
                                        width: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignHCenter
                                        color: grip.containsMouse || grip.drag.active ? "#e6e6e6" : "#7d8088"
                                        font.pixelSize: 15
                                        text: "≡"

                                        MouseArea {
                                            id: grip

                                            objectName: "triggerGrip:" + triggerRow.index
                                            anchors.fill: parent
                                            anchors.margins: -4
                                            hoverEnabled: true
                                            // (The inspector scrolls: without this, its scrolling
                                            // would take an up-and-down drag for its own.)
                                            preventStealing: true
                                            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                            drag.target: triggerRow
                                            drag.axis: Drag.YAxis
                                            drag.minimumY: -triggerRows.rowHeight / 2
                                            drag.maximumY: (triggerHead.triggers.length - 0.5) * triggerRows.rowHeight
                                            onPressed: {
                                                triggerRows.dragging = triggerRow.index
                                                triggerRows.landing = triggerRow.index
                                            }
                                            onPositionChanged: {
                                                if (drag.active)
                                                    triggerRows.landing = triggerRows.landingAt(triggerRow.y)
                                            }
                                            onReleased: {
                                                const from = triggerRows.dragging
                                                const to = triggerRows.landing
                                                triggerRows.dragging = -1
                                                triggerRows.landing = -1
                                                triggerRow.y = Qt.binding(() => triggerRow.index * triggerRows.rowHeight)
                                                if (from < 0 || to < 0 || to === from)
                                                    return
                                                const list = triggerHead.changed(-1, {})
                                                const moved = list.splice(from, 1)[0]
                                                list.splice(to, 0, moved)
                                                triggerHead.set(list, false)
                                            }
                                        }
                                    }

                                    AppTextField {
                                        id: timeField

                                        objectName: "triggerTime:" + triggerRow.index
                                        x: 24
                                        width: 104
                                        height: 26
                                        anchors.verticalCenter: parent.verticalCenter
                                        font.pixelSize: 13
                                        text: Timers.triggerTime(triggerRow.modelData.time)
                                        // Kept when it is a time, put back when it is not
                                        function keep() {
                                            const seconds = Timers.triggerSeconds(text)
                                            if (seconds < 0) {
                                                text = Timers.triggerTime(triggerRow.modelData.time)
                                                return
                                            }
                                            if (Math.abs(seconds - triggerRow.modelData.time) > 1e-9)
                                                triggerHead.set(triggerHead.changed(triggerRow.index, { time: seconds }), false)
                                            else
                                                text = Timers.triggerTime(seconds)
                                        }
                                        // (The inspector held first: keeping the time makes the
                                        // list anew, this row with it.)
                                        Keys.onReturnPressed: { const held = inspector; keep(); held.finished() }
                                        Keys.onEnterPressed: { const held = inspector; keep(); held.finished() }
                                        Keys.onEscapePressed: { text = Timers.triggerTime(triggerRow.modelData.time); inspector.finished() }
                                        onActiveFocusChanged: if (!activeFocus) keep()
                                    }

                                    ColorButton {
                                        objectName: "triggerColor:" + triggerRow.index
                                        x: timeField.x + timeField.width + 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        hasAlpha: false
                                        value: triggerRow.modelData.color
                                        onChanging: (value) => triggerHead.set(triggerHead.changed(triggerRow.index, { color: value }), true)
                                        onPicked: (value) => triggerHead.set(triggerHead.changed(triggerRow.index, { color: value }), false)
                                        onClosed: inspector.finished()
                                    }

                                    AppButton {
                                        objectName: "removeTrigger:" + triggerRow.index
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 28
                                        height: 26
                                        leftPadding: 0
                                        rightPadding: 0
                                        font.pixelSize: 13
                                        text: "×"
                                        onClicked: triggerHead.set(triggerHead.triggers.filter((t, i) => i !== triggerRow.index)
                                                                   .map(t => ({ time: t.time, color: t.color })), false)
                                    }
                                }
                            }
                        }

                        Note {
                            visible: triggerHead.shown
                            text: triggerHead.triggers.length === 0
                                  ? "None. With + a time's words can be made to change colour as it comes to a time: a countdown as it comes down to it,"
                                    + " and on through an overrun, an elapsed time as it goes up past it."
                                  : "The words take a trigger's colour when the time comes down to it (or up to it, for an elapsed time), and keep it"
                                    + " through an overrun. Times are minutes:seconds.hundredths, with hours before them if wanted."
                        }

                        // The clock: how the date is written and how the time is, each one of
                        // five ways (see clocktext.h), and which clock the time is on
                        Line {
                            caption: "Date"
                            visible: linkLine.kind === "clock"

                            Choice {
                                objectName: "linkClockDate"
                                width: textTab.width - 78
                                model: inspector.clockStyles
                                choice: inspector.element ? inspector.element.linkClockDate ?? 0 : 0
                                onChosen: (index) => inspector.setProperties({ linkClockDate: index }, false)
                            }
                        }

                        Line {
                            caption: "Time"
                            visible: linkLine.kind === "clock"

                            Choice {
                                objectName: "linkClockTime"
                                width: textTab.width - 78
                                model: inspector.clockStyles
                                choice: inspector.element ? inspector.element.linkClockTime ?? 0 : 0
                                onChosen: (index) => inspector.setProperties({ linkClockTime: index }, false)
                            }
                        }

                        AppCheck {
                            objectName: "linkClock24"
                            width: parent.width
                            visible: linkLine.kind === "clock"
                            text: "24 hour time"
                            checked: inspector.element !== null && inspector.element.linkClock24 === true
                            onToggled: (checked) => inspector.setProperties({ linkClock24: checked }, false)
                        }

                        // The stage message: whether the box flashes when a message is
                        // shown, and the colour its fill flashes
                        Line {
                            caption: "Flash"
                            visible: linkLine.kind === "stageMessage"

                            AppCheck {
                                objectName: "linkFlash"
                                width: 150
                                anchors.verticalCenter: parent.verticalCenter
                                text: "When it is shown"
                                checked: inspector.element !== null && inspector.element.linkFlash === true
                                onToggled: (checked) => inspector.setProperties({ linkFlash: checked }, false)
                            }

                            ColorButton {
                                objectName: "linkFlashColor"
                                hasAlpha: false
                                enabled: inspector.element !== null && inspector.element.linkFlash === true
                                opacity: enabled ? 1 : 0.4
                                value: inspector.element && inspector.element.linkFlashColor !== undefined ? inspector.element.linkFlashColor : "#ff2d21"
                                onChanging: (value) => inspector.setProperties({ linkFlashColor: value }, true)
                                onPicked: (value) => inspector.setProperties({ linkFlashColor: value }, false)
                                onClosed: inspector.finished()
                            }
                        }

                        Note {
                            visible: linkLine.kind === "stageMessage"
                            text: "What the stage is being told: the stage message, which is typed and shown from the Stage tab of the show controls."
                                  + " Until one is shown, the element shows nothing. Set to flash, the element has that colour for its"
                                  + " fill, on and off, for two seconds each time a message is shown."
                        }

                        Note {
                            visible: linkLine.kind === "element"
                            text: linkLine.elementGone ? "The element this showed the text of is no longer on the slide, so it shows nothing. Choose another."
                                : "The text is the other element's; the font, colour and everything else here are this one's."
                        }

                        Note {
                            visible: linkLine.kind === "slideText"
                            text: linkLine.notes
                                  ? "The notes are those of the slide that is " + (inspector.element !== null && inspector.element.linkSlideNext
                                                                                  ? "to come next" : "live")
                                    + " while this is shown" + (inspector.element !== null && inspector.element.linkSlidePreserve === true
                                      ? ", as they were written: in their own fonts, sizes and colours, and not this element's."
                                      : ": their words, in this element's font and colour.")
                                    + " A slide's notes are written under the slide, in this editor. Until a slide is live, the element's own text stands in for them here."
                                  : "The words are those of the slide that is " + (inspector.element !== null && inspector.element.linkSlideNext
                                                                                  ? "to come next" : "live")
                                    + " while this is shown" + (linkLine.slideSource === Show.ElementNamed ? ", from its elements of that name" : "")
                                    + ", without their formatting: the font, colour and everything else here"
                                    + " are this element's. Until a slide is live, the element's own text stands in for them here."
                        }

                        Note {
                            visible: linkLine.kind === "timer"
                            text: linkLine.timerGone
                                  ? "This workspace has no timer of that name, so the time stays at nothing. Add one called “"
                                    + (inspector.element ? inspector.element.linkTimerName : "") + "” to the timers, or choose another."
                                  : "The time is the timer's; the font, colour and everything else here are this element's."
                                    + " A part that is hidden is counted in the next one shown, so seconds alone count past"
                                    + " sixty. Timers are set up and run from the show controls of the main window."
                        }

                        Note {
                            visible: linkLine.kind === "clock"
                            text: "The date and the time are this computer's, in the words and the order its settings have them."
                                  + " The font, colour and everything else here are this element's."
                        }

                        Note {
                            visible: linkLine.kind === "videoCountdown"
                            text: "How long the video that is playing on the output has left. With none playing, the time is at nothing."
                                  + " A video that loops is counted to the end of each time round, unless it is ignored."
                        }
                    }
                }
            }

            Note {
                visible: inspector.element !== null && inspector.element.linkKind === "other"
                text: !inspector.element ? ""
                      : "In ProPresenter this element shows something this app does not: " + inspector.element.linkLabel.toLowerCase()
                        + ". The link is kept. Shown, the element has nothing in it; here it has "
                        + (inspector.element.linkPicture ? "the fill" : "the text") + " it was left with, to place it and set its look by."
            }
        }
    }
}
