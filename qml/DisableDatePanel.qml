import QtQuick
import SimplePresenterApp

// The small panel a slide's disable date is typed in (Disable On… in a slide's menu): the
// date and the time the slide is to be disabled at, this app's own (see DisableDates in
// src/disabledates.h; ProPresenter has no such thing). Typed as the Calendar's are, in the
// locale's way or as 2026-10-30, and "9:00 am" or "21:00" (CalendarEvents reads them).
//
// It is put up over the operator window, as the Go To Next Timer's panel is. Save, or Enter,
// hands the time to whoever asked; Remove Date takes a date the slide has off; Cancel, Esc
// or a click outside changes nothing. What cannot be read is edged in red, and there is
// nothing to save until both can be. A time that has passed is said to be, and saved it
// disables the slide at once.
Rectangle {
    id: panel

    property bool open: false
    property string about: ""
    // Whether the slide has a date to take off
    property bool removable: false
    // Called with the time when it is saved (seconds since 1970), and with 0 when the date
    // is taken off
    property var done: null
    // What is typed, as a time, or -1 where it cannot be read
    property double start: 0
    readonly property double day: CalendarEvents.withDate(start, dateField.text)
    readonly property double at: day < 0 ? -1 : CalendarEvents.withTime(day, timeField.text)
    // (Asked again when the panel is put up)
    property double now: 0

    signal closed

    function show(about, at, removable, done) {
        panel.about = about
        panel.removable = removable
        panel.done = done
        panel.now = CalendarEvents.now()
        panel.start = at
        dateField.text = CalendarEvents.dateText(at)
        timeField.text = CalendarEvents.timeText(at)
        open = true
        dateField.forceActiveFocus()
        dateField.selectAll()
    }

    function close() {
        open = false
        done = null
        closed()
    }

    function finish(at) {
        const finished = done
        open = false
        done = null
        if (finished)
            finished(at)
        closed()
    }

    function save() {
        if (at >= 0)
            finish(at)
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
        objectName: "disableDatePanel"
        anchors.centerIn: parent
        width: 420
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
                text: "Disable On"
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
                    id: dateField

                    objectName: "disableDateDay"
                    width: 150
                    onAccepted: panel.save()
                    Keys.onEscapePressed: panel.close()

                    Rectangle {
                        anchors.fill: parent
                        visible: panel.day < 0
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
                    text: "at"
                }

                AppTextField {
                    id: timeField

                    objectName: "disableDateTime"
                    width: 110
                    onAccepted: panel.save()
                    Keys.onEscapePressed: panel.close()

                    Rectangle {
                        anchors.fill: parent
                        visible: panel.day >= 0 && panel.at < 0
                        radius: 4
                        color: "transparent"
                        border.width: 1
                        border.color: "#e25555"
                    }
                }
            }

            // What the two make, written out; or that it has passed
            Text {
                objectName: "disableDateSaid"
                width: parent.width
                wrapMode: Text.Wrap
                color: panel.at >= 0 && panel.at <= panel.now ? "#ffb84d" : "#b4b7bd"
                font.pixelSize: 13
                text: panel.at < 0 ? "That is no date and time that can be read."
                    : panel.at <= panel.now ? "That has passed: saved, the slide is disabled now."
                    : "The slide is disabled on " + CalendarEvents.whenText(panel.at) + "."
            }

            Item {
                width: parent.width
                height: 30

                AppButton {
                    objectName: "disableDateRemove"
                    anchors.left: parent.left
                    visible: panel.removable
                    text: "Remove Date"
                    onClicked: panel.finish(0)
                }

                Row {
                    anchors.right: parent.right
                    spacing: 10

                    AppButton {
                        text: "Cancel"
                        onClicked: panel.close()
                    }

                    AppButton {
                        objectName: "disableDateSave"
                        opacity: panel.at >= 0 ? 1 : 0.45
                        text: "Save"
                        onClicked: panel.save()
                    }
                }
            }
        }
    }
}
