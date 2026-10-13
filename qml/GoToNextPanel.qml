import QtQuick
import SimplePresenterApp

// The small panel a number of seconds is typed in, for the two things that make a show
// go on by itself (see show::State::advance in src/showstate.h for what they do):
//
//  - a slide's Go To Next Timer (Go To Next Timer… in a slide's menu): the seconds the
//    slide stays before the next is triggered, and a tick box, Loop to Beginning, for
//    going to the first slide instead; with a button that takes the timer off, for a
//    slide that has one;
//  - a presentation's Slide Show, for a time that is none of the ones its menu offers
//    (Custom… in the menu of the Slide Show button over the slides): the seconds
//    alone.
//
// It is put up over the operator window. Save, or Enter, hands the seconds and the
// tick to whoever asked; Cancel, Esc or a click outside changes nothing. What is typed
// has to be a number that is not less than nothing: until it is, the box is edged in
// red and there is nothing to save. (No seconds at all is allowed for a slide: it goes
// on at once, which is how a slide is made that only leads back to the first.)
//
// What ProPresenter's own pop-up has, as the maintainer describes it: the number of
// seconds and "Loop to Beginning". A GUESS: the rest of how it is laid out, and that
// taking a timer off is a button in it.
Rectangle {
    id: panel

    property bool open: false
    property string heading: "Go To Next Timer"
    property string about: ""
    // Whether there is a tick box for going to the first slide, and whether it is ticked
    property bool loopable: true
    property bool loop: false
    // Whether there is a timer to take off
    property bool removable: false
    // The least that may be saved: nothing for a slide's timer, more for a Slide Show
    property real least: 0
    // Called with the seconds and the tick when it is saved, and with -1 for the
    // seconds when the timer is taken off
    property var done: null
    // What is typed, as a number, or -1 if it is not one that can be saved
    readonly property real seconds: {
        const typed = field.text.trim().replace(",", ".")
        const number = /^[0-9]*\.?[0-9]*$/.test(typed) && typed !== "" && typed !== "." ? parseFloat(typed) : -1
        return number >= least && number <= 86400 ? number : -1
    }

    signal closed

    function show(heading, about, seconds, loop, loopable, removable, done) {
        panel.heading = heading
        panel.about = about
        panel.loop = loop
        panel.loopable = loopable
        panel.removable = removable
        panel.least = loopable ? 0 : 0.1
        panel.done = done
        open = true
        field.text = Number(seconds.toFixed(2)).toString()
        field.forceActiveFocus()
        field.selectAll()
    }

    function close() {
        open = false
        done = null
        closed()
    }

    function finish(seconds) {
        const finished = done
        const ticked = loop
        open = false
        done = null
        if (finished)
            finished(seconds, ticked)
        closed()
    }

    function save() {
        if (seconds >= 0)
            finish(seconds)
    }

    visible: open
    color: "#99000000"

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    Rectangle {
        objectName: "goToNextPanel"
        anchors.centerIn: parent
        width: 380
        height: content.height + 36
        radius: 10
        color: "#2b2d31"
        border.width: 1
        border.color: "#45484e"

        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: content

            x: 20
            y: 18
            width: parent.width - 40
            spacing: 12

            Text {
                width: parent.width
                color: "#f0f0f0"
                font.pixelSize: 17
                font.bold: true
                text: panel.heading
            }

            Text {
                width: parent.width
                visible: panel.about !== ""
                elide: Text.ElideRight
                color: "#b4b7bd"
                font.pixelSize: 13
                text: panel.about
            }

            Row {
                spacing: 10

                AppTextField {
                    id: field

                    objectName: "goToNextSeconds"
                    width: 90
                    maximumLength: 9
                    horizontalAlignment: TextInput.AlignRight
                    onAccepted: panel.save()
                    Keys.onEscapePressed: panel.close()

                    // What cannot be saved is edged in red.
                    Rectangle {
                        anchors.fill: parent
                        visible: panel.seconds < 0
                        radius: 4
                        color: "transparent"
                        border.width: 1
                        border.color: "#e25555"
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#d6d8dc"
                    font.pixelSize: 14
                    text: "seconds"
                }
            }

            AppCheck {
                objectName: "goToNextLoop"
                visible: panel.loopable
                text: "Loop to Beginning"
                checked: panel.loop
                onToggled: (checked) => panel.loop = checked
            }

            Item {
                width: parent.width
                height: 30

                AppButton {
                    objectName: "goToNextRemove"
                    anchors.left: parent.left
                    visible: panel.removable
                    text: "Remove Timer"
                    onClicked: panel.finish(-1)
                }

                Row {
                    anchors.right: parent.right
                    spacing: 10

                    AppButton {
                        text: "Cancel"
                        onClicked: panel.close()
                    }

                    AppButton {
                        objectName: "goToNextSave"
                        opacity: panel.seconds >= 0 ? 1 : 0.45
                        text: "Save"
                        onClicked: panel.save()
                    }
                }
            }
        }
    }
}
