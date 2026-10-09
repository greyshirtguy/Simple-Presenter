import QtCore
import QtQuick
import QtQuick.Window
import SimplePresenterApp

// Base for the output and stage windows: a small floating window that stays in front of
// the operator window, with a slim title bar of its own in place of the desktop's (drag
// it to move, drag the edges to resize), or fullscreen with nothing but its content.
// The floating window can be moved by dragging any part of it, not only the title bar:
// a desktop places a window where it sees fit, and that can be with the top of it under
// something else, where the title bar cannot be got at.
// Moving the mouse over a fullscreen window brings up a control to leave fullscreen.
// Remembers its size, position and fullscreen state under its objectName.
Window {
    id: win

    // Index into Qt.application.screens to start fullscreen on, or -1 to start as remembered.
    property int fullScreenOn: -1
    // The display it is to fill, by its name, for a screen that has been set to one (see
    // Screens); "" for a window that floats, and fills whatever display it is on when
    // asked. While that display is not plugged in the window is not shown at all.
    property string display: ""
    readonly property var displayScreen: {
        for (const candidate of Qt.application.screens) {
            if (display !== "" && candidate.name === display)
                return candidate
        }
        return null
    }
    // The window to stay in front of while floating.
    property Window owner: null
    // Key presses in this window are passed on to this item.
    property Item keyTarget: null
    // Whether the window is on screen at all.
    property bool shown: true
    // Whether to restore the last session's geometry and save this one's.
    property bool remember: true
    default property alias content: body.data

    readonly property int titleBarHeight: 20
    readonly property bool fullScreen: visibility === Window.FullScreen
    property bool wantFullScreen: false
    property bool restored: false
    property bool controlsShown: false
    // The floating size, kept apart from width and height so it survives being hidden
    // or fullscreen and can be put back when the window floats again.
    property real floatWidth: 300
    property real floatHeight: 189

    function present() {
        if (!shown || (display !== "" && displayScreen === null)) {
            hide()
        } else if (display !== "") {
            // (A window that is showing is not moved to another display by being told
            // of it: it is taken off and put up again there.)
            if (visible && screen !== displayScreen)
                hide()
            screen = displayScreen
            showFullScreen()
        } else if (wantFullScreen) {
            showFullScreen()
        } else {
            width = floatWidth
            height = floatHeight
            showNormal()
        }
    }

    function setFullScreen(on) {
        wantFullScreen = on
        save("fullScreen", on)
        present()
    }

    function save(key, value) {
        if (remember && restored)
            settings.setValue(key, value)
    }

    // Only the floating size and position are worth keeping, not the fullscreen ones.
    function saveGeometry(key, value) {
        if (wantFullScreen || visibility !== Window.Windowed || !restored)
            return
        floatWidth = width
        floatHeight = height
        save(key, value)
    }

    minimumWidth: 160
    minimumHeight: 110
    color: "black"
    // Wayland has no always-on-top, so there the window floats by being transient for
    // its owner; elsewhere the hint does it. Neither applies to an output started on its
    // own screen, which must not follow the operator window when that is minimised.
    flags: Qt.Window | Qt.FramelessWindowHint | (fullScreenOn < 0 && display === "" ? Qt.WindowStaysOnTopHint : 0)
    transientParent: fullScreenOn < 0 && display === "" ? owner : null

    Component.onCompleted: {
        if (remember) {
            floatWidth = Number(settings.value("width", floatWidth))
            floatHeight = Number(settings.value("height", floatHeight))
            // Wayland neither reports nor accepts positions; these take effect elsewhere.
            if (settings.value("x") !== undefined && settings.value("y") !== undefined) {
                x = Number(settings.value("x"))
                y = Number(settings.value("y"))
            }
            // Settings stores booleans as text.
            wantFullScreen = String(settings.value("fullScreen", false)) === "true"
        }
        if (fullScreenOn >= 0) {
            screen = Qt.application.screens[fullScreenOn]
            wantFullScreen = true
        }
        restored = true
        // What draws it, and when it is shown, hidden or moved, goes into the log.
        Log.watch(win, title.toLowerCase() + " window")
        // Not a window to switch to: kept out of Alt+Tab and the desktop's other lists
        // of windows, where the app can see to that itself (see src/windowlists.h).
        WindowLists.leaveOut(win)
        present()
    }
    onShownChanged: if (restored) present()
    onDisplayChanged: if (restored) present()
    onDisplayScreenChanged: if (restored) present()
    onWidthChanged: saveGeometry("width", width)
    onHeightChanged: saveGeometry("height", height)
    onXChanged: saveGeometry("x", x)
    onYChanged: saveGeometry("y", y)

    Settings {
        id: settings

        category: win.objectName
    }

    Rectangle {
        id: titleBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: visible ? win.titleBarHeight : 0
        visible: !win.fullScreen
        color: "#15161a"

        DragHandler {
            target: null
            onActiveChanged: if (active) win.startSystemMove()
        }

        TapHandler {
            onDoubleTapped: win.setFullScreen(true)
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: titleButtons.left
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: "#9a9da3"
            font.pixelSize: 11
            text: win.title
        }

        Row {
            id: titleButtons

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            component TitleButton: Rectangle {
                id: button

                property alias text: label.text
                // Draws a small outlined box in place of text: box glyphs fall back to a
                // colour emoji font on some systems.
                property bool box: false

                signal clicked

                width: 26
                height: parent.height
                color: mouse.containsMouse ? "#3a3c42" : "transparent"

                Text {
                    id: label

                    anchors.centerIn: parent
                    visible: !button.box
                    color: "#e6e6e6"
                    font.pixelSize: 11
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 9
                    height: 8
                    visible: button.box
                    color: "transparent"
                    border.width: 1
                    border.color: "#e6e6e6"
                }

                MouseArea {
                    id: mouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: button.clicked()
                }
            }

            TitleButton {
                text: "–"
                onClicked: win.showMinimized()
            }

            // Maximising means fullscreen here: content only, no chrome.
            TitleButton {
                box: true
                onClicked: win.setFullScreen(true)
            }
        }
    }

    Item {
        id: body

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        focus: true
        Keys.forwardTo: win.keyTarget ? [win.keyTarget] : []
        Keys.onEscapePressed: if (win.fullScreen) win.setFullScreen(false)

        // The picture is a handle to move the floating window by, as the title bar is.
        // Nothing in it is there to be clicked, so a drag that starts on it can only
        // mean that.
        DragHandler {
            objectName: "bodyDrag"
            target: null
            enabled: !win.fullScreen
            onActiveChanged: if (active) win.startSystemMove()
        }
    }

    // While fullscreen: mouse movement shows the control and the pointer; both go away
    // again after a moment of stillness.
    MouseArea {
        anchors.fill: body
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: win.fullScreen && !win.controlsShown ? Qt.BlankCursor : Qt.ArrowCursor
        onPositionChanged: {
            if (!win.fullScreen)
                return
            win.controlsShown = true
            hideControls.restart()
        }
    }

    Timer {
        id: hideControls

        interval: 2500
        onTriggered: {
            if (exitMouse.containsMouse)
                restart()
            else
                win.controlsShown = false
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        width: exitLabel.implicitWidth + 28
        height: 36
        radius: 8
        visible: win.fullScreen && win.controlsShown && win.display === ""
        color: exitMouse.containsMouse ? "#45484e" : "#2b2d31"
        border.width: 1
        border.color: "#6c6f75"

        Text {
            id: exitLabel

            anchors.centerIn: parent
            color: "#e6e6e6"
            font.pixelSize: 14
            text: "Exit full screen"
        }

        MouseArea {
            id: exitMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                win.controlsShown = false
                win.setFullScreen(false)
            }
        }
    }

    ResizeGrips {
        anchors.fill: parent
        target: win
    }
}
