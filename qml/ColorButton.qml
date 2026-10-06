import QtQuick
import QtQuick.Controls.Basic

// A swatch showing a colour; clicking it opens a picker. It does not change `value`
// itself. While something in the picker is being dragged it reports each colour passed
// through with changing(), and the one settled on with picked(); a swatch clicked or a
// colour typed is reported with picked() alone.
Rectangle {
    id: button

    property color value: "white"
    // Whether the colour may be see-through
    property bool hasAlpha: true
    property bool available: true

    signal changing(color value)
    signal picked(color value)
    signal closed

    width: 46
    height: 26
    radius: 5
    color: "#15161a"
    border.width: 1
    border.color: popup.opened ? "#ff8a1f" : mouse.containsMouse ? "#8b8f98" : "#5c5f66"
    opacity: available ? 1 : 0.45

    // White and black behind the colour, so that how see-through it is can be seen.
    Row {
        anchors.fill: parent
        anchors.margins: 3

        Rectangle {
            width: parent.width / 2
            height: parent.height
            color: "white"
        }

        Rectangle {
            width: parent.width / 2
            height: parent.height
            color: "black"
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 3
        color: button.value
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        enabled: button.available
        onClicked: popup.open()
    }

    Popup {
        id: popup

        // Hue 0 to 1, saturation, value and opacity, set from the colour when opened
        // and from then on by the picker alone, so that nothing drifts as the colour
        // makes its way round through whatever is being coloured.
        property real hue: 0
        property real saturation: 0
        property real brightness: 1
        property real alpha: 1
        property bool dragged: false
        readonly property color current: Qt.hsva(hue, saturation, brightness, button.hasAlpha ? alpha : 1)
        readonly property var swatches: [
            "#ffffff", "#d0d0d0", "#9e9e9e", "#616161", "#303030", "#000000",
            "#e53935", "#fb8c00", "#fdd835", "#43a047", "#00acc1", "#1e88e5",
            "#3949ab", "#8e24aa", "#d81b60", "#6d4c41", "#ffe0b2", "#b3e5fc"
        ]

        function load(color) {
            // A grey has no hue to speak of; the picker stays at the one it was at.
            if (color.hsvHue >= 0 && color.hsvSaturation > 0.01)
                hue = color.hsvHue
            saturation = color.hsvSaturation
            brightness = color.hsvValue
        }

        function settle() {
            dragged = false
            button.picked(current)
        }

        y: button.height + 4
        // Opens leftwards from the swatch, and is kept inside the window.
        x: button.width - width
        margins: 6
        padding: 10
        focus: true
        onAboutToShow: {
            load(button.value)
            alpha = button.value.a
            dragged = false
        }
        onClosed: {
            if (dragged)
                settle()
            button.closed()
        }

        background: Rectangle {
            radius: 8
            color: "#2b2d31"
            border.width: 1
            border.color: "#8b8f98"
        }

        // A bar dragged along its length, reporting 0 to 1.
        component Bar: Item {
            id: bar

            property real position: 0
            default property alias content: track.data

            signal moved(real position)
            signal released

            width: 204
            height: 16

            Rectangle {
                id: track

                anchors.fill: parent
                radius: 4
                color: "#15161a"
                clip: true
            }

            Rectangle {
                x: bar.position * (bar.width - width)
                anchors.verticalCenter: parent.verticalCenter
                width: 6
                height: bar.height + 4
                radius: 3
                color: "white"
                border.width: 1
                border.color: "black"
            }

            MouseArea {
                function report(mouse) {
                    bar.moved(Math.max(0, Math.min(1, (mouse.x - 3) / (bar.width - 6))))
                }

                anchors.fill: parent
                preventStealing: true
                onPressed: (mouse) => report(mouse)
                onPositionChanged: (mouse) => report(mouse)
                onReleased: bar.released()
            }
        }

        contentItem: Column {
            spacing: 10

            // Saturation across, brightness down
            Item {
                width: 204
                height: 130

                Rectangle {
                    anchors.fill: parent
                    radius: 4

                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0
                            color: "white"
                        }

                        GradientStop {
                            position: 1
                            color: Qt.hsva(popup.hue, 1, 1, 1)
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 4

                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "transparent"
                        }

                        GradientStop {
                            position: 1
                            color: "black"
                        }
                    }
                }

                Rectangle {
                    x: popup.saturation * parent.width - width / 2
                    y: (1 - popup.brightness) * parent.height - height / 2
                    width: 12
                    height: 12
                    radius: 6
                    color: "transparent"
                    border.width: 2
                    border.color: popup.brightness > 0.6 && popup.saturation < 0.5 ? "black" : "white"
                }

                MouseArea {
                    function report(mouse) {
                        popup.saturation = Math.max(0, Math.min(1, mouse.x / width))
                        popup.brightness = 1 - Math.max(0, Math.min(1, mouse.y / height))
                        popup.dragged = true
                        button.changing(popup.current)
                    }

                    anchors.fill: parent
                    preventStealing: true
                    onPressed: (mouse) => report(mouse)
                    onPositionChanged: (mouse) => report(mouse)
                    onReleased: popup.settle()
                }
            }

            Bar {
                position: popup.hue
                onMoved: (position) => {
                    popup.hue = position
                    popup.dragged = true
                    button.changing(popup.current)
                }
                onReleased: popup.settle()

                Rectangle {
                    anchors.fill: parent

                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop { position: 0; color: "#ff0000" }
                        GradientStop { position: 1 / 6; color: "#ffff00" }
                        GradientStop { position: 2 / 6; color: "#00ff00" }
                        GradientStop { position: 3 / 6; color: "#00ffff" }
                        GradientStop { position: 4 / 6; color: "#0000ff" }
                        GradientStop { position: 5 / 6; color: "#ff00ff" }
                        GradientStop { position: 1; color: "#ff0000" }
                    }
                }
            }

            // How solid the colour is
            Row {
                spacing: 8
                visible: button.hasAlpha

                Bar {
                    width: 150
                    position: popup.alpha
                    onMoved: (position) => {
                        popup.alpha = Math.round(position * 100) / 100
                        popup.dragged = true
                        button.changing(popup.current)
                    }
                    onReleased: popup.settle()

                    Rectangle {
                        anchors.fill: parent

                        gradient: Gradient {
                            orientation: Gradient.Horizontal

                            GradientStop {
                                position: 0
                                color: "transparent"
                            }

                            GradientStop {
                                position: 1
                                color: Qt.hsva(popup.hue, popup.saturation, popup.brightness, 1)
                            }
                        }
                    }
                }

                Text {
                    width: 46
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    color: "#c9cbd0"
                    font.pixelSize: 12
                    text: Math.round(popup.alpha * 100) + "%"
                }
            }

            Grid {
                columns: 6
                spacing: 6

                Repeater {
                    model: popup.swatches

                    delegate: Rectangle {
                        required property string modelData

                        width: 29
                        height: 22
                        radius: 4
                        color: modelData
                        border.width: 1
                        border.color: "#5c5f66"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                popup.load(Qt.color(parent.modelData))
                                if (popup.alpha === 0)
                                    popup.alpha = 1
                                popup.settle()
                            }
                        }
                    }
                }
            }

            // Any other colour, as #rrggbb
            AppTextField {
                id: hex

                function show() {
                    text = Qt.hsva(popup.hue, popup.saturation, popup.brightness, 1).toString()
                }

                width: 204
                height: 28
                font.pixelSize: 13
                onEditingFinished: {
                    const typed = text.trim()
                    if (/^#?[0-9a-fA-F]{6}$/.test(typed)) {
                        popup.load(Qt.color(typed.startsWith("#") ? typed : "#" + typed))
                        popup.settle()
                    }
                    show()
                }

                Connections {
                    target: popup

                    function onCurrentChanged() {
                        if (!hex.activeFocus)
                            hex.show()
                    }

                    function onAboutToShow() {
                        hex.show()
                    }
                }
            }
        }
    }
}
