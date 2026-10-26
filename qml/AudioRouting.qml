import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The Channel Routing window, as ProPresenter's (support.renewedvision.com, "Audio
// Routing in ProPresenter"): the app's channels down the left, the sound device's
// across the top, and a cell where each meets each, lit where the one goes to the
// other; a click on a cell sends it there, or stops sending it. Beside each channel its
// name, which a click on it lets be changed, and M (mute it), S (solo it: only the
// soloed are heard while any is) and T (a test tone on it). Over them all, M mutes every
// channel, S takes every solo off, and Mapping has Auto (each channel to the device's of
// the same number: what there is until the matrix is changed) and Clear (none anywhere).
//
// What is kept where: the channels, their names, Mute and Solo are the workspace's, in
// ProPresenter's file of how it is set up; where they go on the device is this
// computer's, for this device, in the app's settings (AudioMixer has why). A tone is
// only for now, and goes when the app is closed.
//
// GUESSES, not from ProPresenter: what the letters look like when on (the app's own
// orange, as everything switched on); the M over them all lit when every channel is
// muted, and then unmuting them all; what is said under the matrix.
Popup {
    id: routing

    modal: true
    focus: true
    padding: 18
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width - 40 : 900, matrix.implicitWidth + 2 * padding + 16)
    height: Math.min(parent ? parent.height - 40 : 700, content.implicitHeight + 2 * padding)

    // (Which device, and how many channels it has, as the window opens)
    onAboutToShow: AudioMixer.look()

    background: Rectangle {
        radius: 10
        color: "#232428"
        border.width: 1
        border.color: "#45484e"
    }

    contentItem: Flickable {
        id: content

        objectName: "audioRouting"
        clip: true
        implicitHeight: column.height
        contentWidth: Math.max(width, matrix.implicitWidth)
        contentHeight: column.height
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
            visible: content.contentHeight > content.height
        }
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AlwaysOn
            visible: content.contentWidth > content.width
        }

        Column {
            id: column

            width: content.contentWidth
            spacing: 14

            Item {
                width: content.width
                height: 28

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#e6e6e6"
                    font.pixelSize: 15
                    text: "Channel Routing"
                }

                AppButton {
                    objectName: "audioRoutingDone"
                    anchors.right: parent.right
                    height: 28
                    font.pixelSize: 13
                    text: "Done"
                    onClicked: routing.close()
                }
            }

            RoutingMatrix {
                id: matrix

                objectName: "deviceMatrix"
                rows: AudioMixer.channels.length
                columns: AudioMixer.deviceChannels
                routes: AudioMixer.deviceRoutes
                columnsTitle: AudioMixer.deviceName !== "" ? AudioMixer.deviceName.toUpperCase() : "THE SOUND DEVICE"
                onToggled: (row, column) => AudioMixer.toggleRoute(row, column)

                corner: Row {
                    spacing: 6

                    IconButton {
                        objectName: "muteAll"
                        width: 26
                        height: 26
                        text: "M"
                        on: AudioMixer.channels.length > 0 && AudioMixer.channels.every(channel => channel.muted)
                        onClicked: routing.said(AudioMixer.setAllMuted(!on))
                    }

                    IconButton {
                        objectName: "clearSolos"
                        width: 26
                        height: 26
                        text: "S"
                        on: AudioMixer.channels.some(channel => channel.solo)
                        onClicked: routing.said(AudioMixer.clearSolos())
                    }

                    AppComboBox {
                        objectName: "deviceMapping"
                        width: 120
                        height: 26
                        font.pixelSize: 13
                        model: ["Auto", "Clear"]
                        currentIndex: -1
                        displayText: "Mapping"
                        onActivated: (index) => {
                            if (index === 0)
                                AudioMixer.autoRoutes()
                            else
                                AudioMixer.clearRoutes()
                            currentIndex = -1
                        }
                    }
                }

                rowHeader: Item {
                    id: header

                    property int row: 0
                    readonly property var channel: AudioMixer.channels[row] || ({})

                    // Its name, which a click lets be changed
                    Text {
                        id: name

                        objectName: "channelName:" + header.row
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - letters.width - 8
                        visible: !editor.visible
                        elide: Text.ElideRight
                        color: "#e6e6e6"
                        font.pixelSize: 14
                        text: header.channel.name ?? ""

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                editor.text = name.text
                                editor.visible = true
                                editor.forceActiveFocus()
                                editor.selectAll()
                            }
                        }
                    }

                    AppTextField {
                        id: editor

                        objectName: "channelNameEditor:" + header.row
                        anchors.verticalCenter: parent.verticalCenter
                        width: name.width
                        height: 26
                        visible: false
                        font.pixelSize: 13
                        onAccepted: {
                            visible = false
                            if (text.trim() !== "" && text.trim() !== name.text)
                                routing.said(AudioMixer.rename(header.row, text))
                        }
                        onActiveFocusChanged: if (!activeFocus) visible = false
                        Keys.onEscapePressed: visible = false
                    }

                    Row {
                        id: letters

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        IconButton {
                            objectName: "channelMute:" + header.row
                            width: 22
                            height: 22
                            text: "M"
                            on: header.channel.muted === true
                            onClicked: routing.said(AudioMixer.setMuted(header.row, !on))
                        }

                        IconButton {
                            objectName: "channelSolo:" + header.row
                            width: 22
                            height: 22
                            text: "S"
                            on: header.channel.solo === true
                            onClicked: routing.said(AudioMixer.setSolo(header.row, !on))
                        }

                        IconButton {
                            objectName: "channelTone:" + header.row
                            width: 22
                            height: 22
                            text: "T"
                            on: header.channel.tone === true
                            onClicked: AudioMixer.setTone(header.row, !on)
                        }
                    }
                }
            }

            Text {
                id: problem

                objectName: "audioRoutingProblem"
                width: content.width
                visible: text !== ""
                wrapMode: Text.Wrap
                color: "#ff8a80"
                font.pixelSize: 13
            }

            Text {
                id: words

                width: Math.min(content.width, 640)
                wrapMode: Text.Wrap
                color: "#9a9da3"
                font.pixelSize: 13
                text: "The channels, their names, and whether each is muted or soloed are the workspace's, kept where "
                    + "ProPresenter keeps them. Where they go on the device is this computer's, for this device, and is "
                    + "Auto until it is changed. A tone is only until it is switched off, or the app is closed."
            }
        }
    }

    // What went wrong, if anything did, under the matrix
    function said(error) {
        problem.text = error
    }
}
