import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// What comes up when a screen is set to NDI and NDI's library is not on this computer:
// why it is needed, and the two ways of getting it.
//
// The library is NDI's own and cannot come with the app. The quick way is to let the
// app fetch it: it downloads NDI's installer from NDI's site, shows NDI's licence, and
// if that is agreed to puts the library where the app looks for it (see Ndi). The other
// is by hand, for a computer that is not on the internet or for whoever would rather:
// the steps are here too, with the folder to put the file in. Either way the app goes
// on without being started again, and the screen is on the network.
Rectangle {
    id: setup

    signal closed

    readonly property string state: Ndi.fetching
    readonly property bool busy: state === "downloading" || state === "installing"

    function close() {
        if (state !== "done")
            Ndi.giveUp()
        closed()
    }

    color: "#e0101114"

    // (Nothing behind it is to be clicked through it.)
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        id: panel

        objectName: "ndiSetup"
        anchors.centerIn: parent
        width: Math.min(620, parent.width - 24)
        height: Math.min(body.implicitHeight + 36, parent.height - 24)
        radius: 8
        color: "#25272b"
        border.width: 1
        border.color: "#4a4d55"

        Flickable {
            anchors.fill: parent
            anchors.margins: 18
            contentHeight: body.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: body

                width: parent.width
                spacing: 12

                Text {
                    width: parent.width
                    color: "#e6e6e6"
                    font.pixelSize: 17
                    font.bold: true
                    text: Ndi.available ? "NDI is ready" : "NDI needs its own library"
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: "#b0b3b8"
                    font.pixelSize: 13
                    visible: !Ndi.available
                    text: "A screen is sent over NDI by NDI's own library. It is free, but it is NDI's and not part of this app, "
                        + "so it has to be fetched once, under NDI's licence. Everything else in the app works without it."
                }

                // ---- The quick way
                Rectangle {
                    width: parent.width
                    height: quick.implicitHeight + 24
                    radius: 6
                    color: "#2f3136"

                    Column {
                        id: quick

                        x: 12
                        y: 12
                        width: parent.width - 24
                        spacing: 10

                        Text {
                            objectName: "ndiState"
                            width: parent.width
                            wrapMode: Text.WordWrap
                            color: setup.state === "failed" ? "#ff9a8a" : "#e6e6e6"
                            font.pixelSize: 13
                            text: setup.state === "done" || Ndi.available ? "NDI's library is in place: " + Ndi.version + ". Screens set to NDI are on the network."
                                : setup.state === "downloading" ? "Downloading NDI's installer… " + Math.round(Ndi.fetched * 100) + "%"
                                : setup.state === "licence" ? "Downloaded. This is NDI's licence for its library. To use the library, you agree to it."
                                : setup.state === "installing" ? "Putting the library in place…"
                                : setup.state === "failed" ? "That did not work: " + Ndi.fetchProblem + " The steps below do the same by hand."
                                : "Let the app fetch it: it downloads NDI's installer (about 60 MB) from NDI's own site, shows you NDI's licence, "
                                  + "and if you agree puts the library in place."
                        }

                        // How far the download has got
                        Rectangle {
                            width: parent.width
                            height: 6
                            radius: 3
                            color: "#1b1c1f"
                            visible: setup.state === "downloading"

                            Rectangle {
                                width: parent.width * Math.max(0.02, Ndi.fetched)
                                height: parent.height
                                radius: 3
                                color: "#ff8a1f"
                            }
                        }

                        // NDI's licence
                        Rectangle {
                            width: parent.width
                            height: 190
                            radius: 4
                            color: "#1b1c1f"
                            visible: setup.state === "licence"

                            Flickable {
                                id: licenceView

                                anchors.fill: parent
                                anchors.margins: 8
                                contentHeight: licenceText.implicitHeight
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AlwaysOn
                                }

                                Text {
                                    id: licenceText

                                    objectName: "ndiLicence"
                                    width: licenceView.width - 14
                                    wrapMode: Text.Wrap
                                    color: "#c9cdd6"
                                    font.pixelSize: 12
                                    textFormat: Text.PlainText
                                    text: Ndi.licence
                                }
                            }
                        }

                        Row {
                            spacing: 8

                            AppButton {
                                objectName: "ndiFetch"
                                height: 28
                                leftPadding: 12
                                rightPadding: 12
                                font.pixelSize: 13
                                visible: !Ndi.available && (setup.state === "" || setup.state === "failed")
                                text: setup.state === "failed" ? "Try Again" : "Download NDI's Library"
                                onClicked: Ndi.fetch()
                            }

                            AppButton {
                                objectName: "ndiAgree"
                                height: 28
                                leftPadding: 12
                                rightPadding: 12
                                font.pixelSize: 13
                                visible: setup.state === "licence"
                                text: "I Agree to NDI's Licence"
                                onClicked: Ndi.agree()
                            }

                            AppButton {
                                objectName: "ndiDecline"
                                height: 28
                                leftPadding: 12
                                rightPadding: 12
                                font.pixelSize: 13
                                visible: setup.state === "licence" || setup.state === "downloading"
                                text: setup.state === "licence" ? "I Do Not" : "Stop"
                                onClicked: Ndi.giveUp()
                            }
                        }
                    }
                }

                // ---- By hand
                Text {
                    objectName: "ndiByHand"
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#b0b3b8"
                    font.pixelSize: 13
                    visible: !Ndi.available
                    textFormat: Text.StyledText
                    linkColor: "#6fb3ff"
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                    text: "<b>Or by hand</b>, on a computer that is not on the internet, say:<br>"
                        + "1. Get the <i>NDI SDK for Linux</i> from <a href=\"https://ndi.video/for-developers/ndi-sdk/\">ndi.video</a>, "
                        + "and run the installer it gives you, which shows NDI's licence and unpacks a folder.<br>"
                        + "2. In that folder, find <code>lib/" + Ndi.sdkFolder + "/libndi.so.6.x.x</code>.<br>"
                        + "3. Copy it into the folder below, named <code>" + Ndi.fileName + "</code>, and press Look Again."
                }

                Text {
                    objectName: "ndiFolder"
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                    color: "#e6e6e6"
                    font.pixelSize: 12
                    font.family: "monospace"
                    visible: !Ndi.available
                    text: Ndi.folder
                }

                Row {
                    spacing: 8

                    AppButton {
                        objectName: "ndiShowFolder"
                        height: 28
                        leftPadding: 12
                        rightPadding: 12
                        font.pixelSize: 13
                        visible: !Ndi.available
                        enabled: !setup.busy
                        text: "Open That Folder"
                        onClicked: Ndi.showFolder()
                    }

                    AppButton {
                        objectName: "ndiLookAgain"
                        height: 28
                        leftPadding: 12
                        rightPadding: 12
                        font.pixelSize: 13
                        visible: !Ndi.available
                        enabled: !setup.busy
                        text: "Look Again"
                        onClicked: Ndi.lookAgain()
                    }

                    AppButton {
                        objectName: "ndiClose"
                        height: 28
                        leftPadding: 12
                        rightPadding: 12
                        font.pixelSize: 13
                        text: Ndi.available ? "Done" : "Not Now"
                        onClicked: setup.close()
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#9a9da3"
                    font.pixelSize: 11
                    textFormat: Text.StyledText
                    linkColor: "#6fb3ff"
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                    text: "NDI® is a registered trademark of Vizrt NDI AB. <a href=\"https://ndi.video\">ndi.video</a>"
                }
            }
        }
    }
}
