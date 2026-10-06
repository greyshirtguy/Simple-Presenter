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
    property var setFormat: (format, interim) => {}
    property var settle: () => {}
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

            CheckLine {
                text: "Fill"
                checked: inspector.element ? inspector.element.fillOn : false
                onToggled: (checked) => inspector.setProperties({ fillOn: checked }, false)

                // Picking a colour turns the fill on, and makes it a plain colour.
                ColorButton {
                    value: inspector.element ? inspector.element.fillColor : "black"
                    onChanging: (value) => inspector.setProperties({ fillColor: value, fillOn: true }, true)
                    onPicked: (value) => inspector.setProperties({ fillColor: value, fillOn: true }, false)
                    onClosed: inspector.finished()
                }
            }

            AppCheck {
                width: parent.width
                text: "Only behind the lines of text"
                checked: inspector.element ? inspector.element.fillLinesOnly : false
                onToggled: (checked) => inspector.setProperties({ fillLinesOnly: checked }, false)
            }

            Note {
                topPadding: 4
                visible: inspector.element !== null && inspector.element.fillKind !== "color"
                         && inspector.element.fillKind !== "none"
                text: inspector.element && inspector.element.fillKind === "gradient"
                      ? "This element has a gradient fill, which is not drawn here yet. It is kept as it is unless a colour is picked, which replaces it."
                      : "This element is filled with something that is not drawn here yet. It is kept as it is unless a colour is picked, which replaces it."
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
            // the text of another element of the slide, the words of the slide that is
            // live or of the one after it, or the time of a timer
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
                    // Linked to a slide's text, but not to all of it: to its notes, or
                    // to its elements of some name. Those are ProPresenter's to set
                    // up; one that is there is listed, so that what it is can be seen.
                    readonly property bool slidePart: kind === "slideText" && inspector.element.linkSlideSource !== Show.Words
                    readonly property string slidePartName: !slidePart ? ""
                        : (inspector.element.linkSlideNext ? "the next slide's " : "the current slide's ")
                          + (inspector.element.linkSlideSource === Show.Notes
                             ? "notes" : "“" + inspector.element.linkSlideName + "” text")
                    // Where the two slides start in the list below, and the timers
                    readonly property int firstSlide: 1 + (elementGone ? 1 : 0) + inspector.others.length
                    readonly property int firstTimer: firstSlide + 2 + (slidePart ? 1 : 0) + (timerGone ? 1 : 0)

                    caption: "Shows"

                    Choice {
                        width: textTab.width - 78
                        model: ["its own text"]
                            .concat(linkLine.elementGone ? ["“" + inspector.element.linkElementName + "” (gone)"] : [])
                            .concat(inspector.otherNames.map(name => name !== "" ? "the text of “" + name + "”"
                                                                                  : "the text of an unnamed element"))
                            .concat(["the current slide's text", "the next slide's text"])
                            .concat(linkLine.slidePart ? [linkLine.slidePartName] : [])
                            .concat(linkLine.timerGone ? ["the timer “" + inspector.element.linkTimerName + "” (not here)"] : [])
                            .concat(inspector.timerNames.map(name => name !== "" ? "the timer “" + name + "”" : "an unnamed timer"))
                        choice: linkLine.kind === "element" ? (linkLine.elementGone ? 1 : linkLine.place + 1)
                              : linkLine.kind === "timer" ? linkLine.firstTimer + (linkLine.timerGone ? -1 : linkLine.timerPlace)
                              : linkLine.kind === "slideText" ? linkLine.firstSlide + (linkLine.slidePart ? 2
                                                                : inspector.element.linkSlideNext ? 1 : 0)
                              : 0
                        onChosen: (index) => {
                            const other = inspector.others[index - 1 - (linkLine.elementGone ? 1 : 0)]
                            const timer = index >= linkLine.firstTimer ? inspector.timers[index - linkLine.firstTimer] : undefined
                            // (A row that only says what the element is linked to
                            // already, or was, is none of these: nothing to change.)
                            if (index === 0) {
                                inspector.setProperties({ linkKind: "none" }, false)
                            } else if (index < linkLine.firstSlide) {
                                if (other)
                                    inspector.setProperties({ linkKind: "element", linkElementId: other.id }, false)
                            } else if (index <= linkLine.firstSlide + 1) {
                                // All of the slide's words: of the one that is live,
                                // or of the one after it
                                inspector.setProperties({ linkKind: "slideText", linkSlideNext: index > linkLine.firstSlide,
                                                          linkSlideSource: Show.Words }, false)
                            } else if (timer) {
                                // The first time, it is written the usual way; after
                                // that, the way it was set to be.
                                const changes = { linkKind: "timer", linkTimerId: timer.id, linkTimerName: timer.name }
                                if (linkLine.kind !== "timer")
                                    inspector.timerParts.forEach(part => changes[part.key] = part.usual)
                                inspector.setProperties(changes, false)
                            }
                        }
                    }
                }

                Line {
                    caption: "Set"
                    visible: linkLine.kind === "element" || linkLine.kind === "slideText"

                    Choice {
                        width: textTab.width - 78
                        model: inspector.transforms
                        choice: inspector.element ? inspector.element.linkTransform : 0
                        onChosen: (index) => inspector.setProperties({ linkTransform: index }, false)
                    }
                }

                // How each part of the timer's time is written, as ProPresenter has it:
                // a drop-down for each of the four
                Repeater {
                    model: linkLine.kind === "timer" ? inspector.timerParts : []

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

                Note {
                    visible: linkLine.kind === "element"
                    text: "The text is the other element's; the font, colour and everything else here are this one's."
                }

                Note {
                    visible: linkLine.kind === "slideText"
                    text: inspector.element !== null && inspector.element.linkSlideSource === Show.Notes
                          ? "This app does not read a slide's notes, so there is nothing for this element to show."
                          : "The words are those of the slide that is " + (inspector.element !== null && inspector.element.linkSlideNext
                                                                          ? "to come next" : "live")
                            + " while this is shown, without their formatting: the font, colour and everything else here"
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
