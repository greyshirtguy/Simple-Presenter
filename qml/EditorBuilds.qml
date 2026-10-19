import QtQuick
import SimplePresenterApp

// The editor's Build tab: ProPresenter's builds of the slide being edited (src/builds.h
// has what they are and when each runs), set out as ProPresenter's Build tab has them.
//
//   - With an element picked: its Build In and its Build Out. Each is its transition
//     (its name, Change for another, and ✕ to take the build off), what can be
//     adjusted about it, its length, and Reset to Default Parameters, which puts the
//     transition back as ProPresenter has it to begin with. An element without one has
//     Add Build in its place.
//   - With nothing picked: the slide's own Build In, which is the transition the slide
//     comes in with, and every element of it that has no Build In of its own, in place
//     of its presentation's or the one in use. ProPresenter has no Build Out for a
//     whole slide. (A presentation's slide only: a theme's slide has no transition of
//     its own in ProPresenter's file, and its builds are what a screen whose look
//     dresses its slides in the theme runs.)
//   - Under either, the Build Order: every build of the slide, ins and outs, in the
//     order they run, numbered, to be dragged into another order; the picked element's
//     are lit. A click on one picks its element. And at the foot, when the build picked
//     in the list (or the picked element's first) starts and after what delay: On
//     Click, With Previous, After Previous or With Slide.
//
// Each change is saved at once and is a step to undo, as any change in the editor; a
// slider is taken when it is let go, not at every step it is dragged. Nothing here keeps
// anything itself: the slide's builds come in as `buildList` and go out through the calls
// below (PresentationEditor's builds, addBuild and the rest).
//
// Not here: ProPresenter's Delivery, which builds a text a part at a time (its words,
// its lines), and the transitions that are for that alone; nor its favourite
// transitions (the star) and the pictures of each. A build the file has with any of
// them is kept as it is.
Column {
    id: pane

    // The element picked, or null for the slide; and the slide's elements
    property var element: null
    property var elements: []
    // The slide's builds in order (builds::describe), its own transition (or undefined),
    // and its presentation's (empty for none)
    property var buildList: []
    property var slideTransition: undefined
    property var presentationTransition: ({})
    // Whether the slide can have a transition of its own: a presentation's can
    property bool ownTransition: true
    // Shows a menu (see PopupMenu) by an item
    property var showMenu: (items, item, x, y) => {}
    // The changes, as PresentationEditor has them. addBuild returns the new build's id.
    property var addBuild: (element, out, transition) => ""
    property var removeBuild: (id) => {}
    property var changeBuild: (id, changes) => {}
    property var moveBuild: (id, index) => {}
    property var setSlideTransition: (transition) => {}
    property var pickElement: (id) => {}
    // A value was typed in; whoever had the keyboard can have it back.
    signal finished

    // The build picked in the Build Order, by id; "" for the picked element's first
    property string pickedBuild: ""
    readonly property string elementId: element ? element.id : ""
    readonly property var ownIn: element ? buildList.find(b => b.element === element.id && !b.out) : undefined
    readonly property var ownOut: element ? buildList.find(b => b.element === element.id && b.out) : undefined
    // What Start and Delay are for
    readonly property var picked: buildList.find(b => b.id === pickedBuild) ?? ownIn ?? ownOut
    // What a build is given when it is added: a Dissolve, as long as ProPresenter makes one
    readonly property var dissolve: ({ renderId: catalogue.transitions[1].pro })
    // ProPresenter's ways for a build to start, in its order, by the names the file has
    readonly property var starts: [{ name: "On Click", start: "click" }, { name: "With Previous", start: "withPrevious" },
                                   { name: "After Previous", start: "afterPrevious" }, { name: "With Slide", start: "withSlide" }]
    // The row of the Build Order being dragged, where it would land (the row it would go
    // before), and how high a row is
    property int carried: -1
    property int landing: -1
    readonly property real rowHeight: 30

    onElementIdChanged: pickedBuild = ""

    spacing: 2

    function nameOf(id) {
        const found = elements.find(e => e.id === id)
        return found ? found.name.replace(/\s+/g, " ").trim() : "?"
    }

    // The menu of transitions a build can have: Cut, then the rest under their
    // categories, with the one it has ticked. Only those ProPresenter has. One chosen
    // keeps the length the build had, and has what can be adjusted about it as
    // ProPresenter has it to begin with (which the file is given: see
    // builds::writeTransition), not as the catalogue starts it.
    function changeMenu(described, item, apply) {
        const items = []
        const rowsOf = category => catalogue.transitions.forEach(t => {
            if (t.category === category && t.pro !== undefined)
                items.push({ label: t.name, current: t.pro === (described.renderId ?? "").toUpperCase(),
                             run: () => apply({ renderId: t.pro, duration: described.duration }) })
        })
        rowsOf("")
        for (const category of catalogue.categories) {
            if (catalogue.transitions.some(t => t.category === category && t.pro !== undefined)) {
                items.push({ header: category })
                rowsOf(category)
            }
        }
        showMenu(items, item, 0, item.height)
    }

    TransitionCatalogue {
        id: catalogue
    }

    component Heading: Item {
        property alias text: title.text

        width: pane.width
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
        width: pane.width
        wrapMode: Text.Wrap
        color: "#9a9da3"
        font.pixelSize: 12
    }

    // A build's transition, or Add Build where there is none
    component Block: Column {
        id: block

        property string title
        property string name
        // The transition as the file has it ({ renderId, name, duration, values }), or
        // undefined for none
        property var described: undefined
        property var add: () => {}
        property var remove: () => {}
        property var change: (described) => {}
        readonly property var kind: described !== undefined ? catalogue.byPro(described.renderId) : undefined

        width: pane.width
        spacing: 8

        Heading {
            text: block.title
        }

        AppButton {
            objectName: "add" + block.name
            visible: block.described === undefined
            height: 28
            font.pixelSize: 13
            text: "Add Build"
            onClicked: block.add()
        }

        Item {
            width: parent.width
            height: 28
            visible: block.described !== undefined

            Text {
                objectName: block.name + "Name"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - change.width - remove.width - 16
                elide: Text.ElideRight
                color: "#e6e6e6"
                font.pixelSize: 14
                font.bold: true
                text: block.described === undefined ? "" : (block.described.name || "?") + (block.kind === undefined ? " (not drawn here)" : "")
            }

            AppButton {
                id: change

                objectName: block.name + "Change"
                anchors.right: remove.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                height: 28
                font.pixelSize: 13
                text: "Change"
                onClicked: pane.changeMenu(block.described, change, block.change)
            }

            IconButton {
                id: remove

                objectName: block.name + "Remove"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 26
                height: 28
                text: "✕"
                onClicked: block.remove()
            }
        }

        TransitionOptionRows {
            width: parent.width
            visible: block.kind !== undefined
            live: false
            options: block.kind !== undefined ? block.kind.options : []
            valueOf: (option) => catalogue.valueOf(option, catalogue.chosenFrom(block.kind, block.described.values))
            choose: (option, value) => {
                const chosen = Object.assign(catalogue.chosenFrom(block.kind, block.described.values), { [option.name]: value })
                block.change(catalogue.described(block.kind, chosen, block.described.duration))
            }
        }

        // Its length: the slider to five seconds, the box to a minute
        Column {
            width: parent.width
            spacing: 5
            visible: block.described !== undefined

            Text {
                color: "#e6e6e6"
                font.pixelSize: 13
                text: "Duration"
            }

            Row {
                spacing: 8

                AppSlider {
                    id: length

                    objectName: block.name + "Duration"
                    anchors.verticalCenter: parent.verticalCenter
                    width: block.width - seconds.width - 8
                    from: 0
                    to: 5
                    value: block.described !== undefined ? block.described.duration : 0
                    onPressedChanged: {
                        const chosen = Math.round(value * 10) / 10
                        if (!pressed && chosen !== block.described.duration)
                            block.change(Object.assign({}, block.described, { duration: chosen }))
                    }
                }

                NumberField {
                    id: seconds

                    objectName: block.name + "Seconds"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 62
                    decimals: 1
                    from: 0
                    to: 60
                    step: 0.1
                    suffix: " s"
                    value: length.pressed ? Math.round(length.value * 10) / 10 : block.described !== undefined ? block.described.duration : 0
                    onEdited: (value) => block.change(Object.assign({}, block.described, { duration: value }))
                    onFinished: pane.finished()
                }
            }
        }

        AppButton {
            objectName: block.name + "Reset"
            visible: block.described !== undefined && block.kind !== undefined
            height: 28
            font.pixelSize: 13
            text: "Reset to Default Parameters"
            onClicked: block.change({ renderId: block.described.renderId, reset: true })
        }
    }

    // The slide's own Build In, with nothing picked
    Block {
        visible: pane.element === null && pane.ownTransition
        title: "Build In"
        name: "slideBuild"
        described: pane.slideTransition
        add: () => pane.setSlideTransition(pane.dissolve)
        remove: () => pane.setSlideTransition({})
        change: (described) => pane.setSlideTransition(described)
    }

    Note {
        visible: pane.element === null && pane.ownTransition && pane.slideTransition === undefined
        topPadding: 6
        text: "The slide comes in with " + (pane.presentationTransition.name ? "its presentation's transition, " + pane.presentationTransition.name
                                                                            : "the transition in use")
              + ", and so do its elements that have no Build In of their own."
    }

    // The picked element's
    Block {
        visible: pane.element !== null
        title: "Build In"
        name: "buildIn"
        described: pane.ownIn ? pane.ownIn.transition : undefined
        add: () => { pane.pickedBuild = pane.addBuild(pane.elementId, false, pane.dissolve) }
        remove: () => pane.removeBuild(pane.ownIn.id)
        change: (described) => pane.changeBuild(pane.ownIn.id, { transition: described })
    }

    Block {
        visible: pane.element !== null
        title: "Build Out"
        name: "buildOut"
        described: pane.ownOut ? pane.ownOut.transition : undefined
        add: () => { pane.pickedBuild = pane.addBuild(pane.elementId, true, pane.dissolve) }
        remove: () => pane.removeBuild(pane.ownOut.id)
        change: (described) => pane.changeBuild(pane.ownOut.id, { transition: described })
    }

    Heading {
        text: "Build Order"
    }

    Note {
        visible: pane.buildList.length === 0
        text: "No element of the slide has a build."
    }

    // The builds, in order. A row is pressed and dragged up or down to move it: it
    // lands in the gap nearest the pointer, shown by a line.
    Item {
        id: order

        objectName: "buildOrder"
        width: pane.width
        height: pane.buildList.length * pane.rowHeight

        Repeater {
            model: pane.buildList

            delegate: Rectangle {
                id: entry

                required property var modelData
                required property int index
                readonly property bool mine: pane.element !== null && modelData.element === pane.element.id
                readonly property bool chosen: pane.picked !== undefined && pane.picked.id === modelData.id

                objectName: "buildRow" + index
                y: index * pane.rowHeight
                width: order.width
                height: pane.rowHeight - 2
                radius: 4
                color: chosen ? "#5a4326" : mine ? "#3f3a33" : rowMouse.containsMouse ? "#3a3c42" : "transparent"
                opacity: pane.carried === index ? 0.5 : 1

                Text {
                    x: 6
                    width: 20
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 12
                    text: entry.index + 1
                }

                Text {
                    x: 28
                    width: parent.width - 34
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    color: "#e6e6e6"
                    font.pixelSize: 12
                    textFormat: Text.StyledText
                    text: pane.nameOf(entry.modelData.element).replace(/&/g, "&amp;").replace(/</g, "&lt;")
                          + "  <font color=\"#9a9da3\">" + (entry.modelData.out ? "Out: " : "In: ")
                          + (entry.modelData.transition.name || "?").replace(/&/g, "&amp;").replace(/</g, "&lt;") + "</font>"
                }

                MouseArea {
                    id: rowMouse

                    property real pressedAt: 0

                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: (mouse) => pressedAt = mouse.y
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return
                        if (pane.carried < 0 && Math.abs(mouse.y - pressedAt) < 6)
                            return
                        pane.carried = entry.index
                        const at = entry.y + mouse.y
                        pane.landing = Math.max(0, Math.min(pane.buildList.length, Math.round(at / pane.rowHeight)))
                    }
                    onReleased: {
                        const from = pane.carried
                        const to = pane.landing
                        pane.carried = -1
                        pane.landing = -1
                        if (from < 0) {
                            pane.pickedBuild = entry.modelData.id
                            pane.pickElement(entry.modelData.element)
                            pane.pickedBuild = entry.modelData.id
                        } else if (to >= 0 && to !== from && to !== from + 1) {
                            pane.moveBuild(entry.modelData.id, to > from ? to - 1 : to)
                        }
                    }
                    onCanceled: {
                        pane.carried = -1
                        pane.landing = -1
                    }
                }
            }
        }

        // Where the row being dragged would land
        Rectangle {
            objectName: "buildLanding"
            x: 4
            y: Math.max(0, pane.landing * pane.rowHeight - 2)
            width: parent.width - 8
            height: 2
            radius: 1
            visible: pane.landing >= 0
            color: "#ff8a1f"
        }
    }

    // When the build picked starts, and after how long
    Item {
        width: pane.width
        height: 38
        visible: pane.picked !== undefined

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 72
            color: "#9a9da3"
            font.pixelSize: 11
            font.capitalization: Font.AllUppercase
            text: "Start"
        }

        AppComboBox {
            id: start

            objectName: "buildStart"
            x: 78
            anchors.verticalCenter: parent.verticalCenter
            width: pane.width - 78
            height: 28
            font.pixelSize: 13
            model: pane.starts.map(s => s.name)
            readonly property int choice: pane.picked !== undefined ? pane.starts.findIndex(s => s.start === pane.picked.start) : 0
            onChoiceChanged: currentIndex = choice
            Component.onCompleted: currentIndex = choice
            onActivated: (index) => {
                currentIndex = choice
                if (pane.picked !== undefined && pane.starts[index].start !== pane.picked.start)
                    pane.changeBuild(pane.picked.id, { start: pane.starts[index].start })
            }
        }
    }

    Item {
        width: pane.width
        height: 34
        visible: pane.picked !== undefined

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 72
            color: "#9a9da3"
            font.pixelSize: 11
            font.capitalization: Font.AllUppercase
            text: "Delay"
        }

        NumberField {
            objectName: "buildDelay"
            x: 78
            anchors.verticalCenter: parent.verticalCenter
            width: 70
            decimals: 1
            from: 0
            to: 600
            step: 0.1
            suffix: " s"
            value: pane.picked !== undefined ? pane.picked.delay : 0
            onEdited: (value) => {
                if (pane.picked !== undefined && value !== pane.picked.delay)
                    pane.changeBuild(pane.picked.id, { delay: value })
            }
            onFinished: pane.finished()
        }
    }
}
