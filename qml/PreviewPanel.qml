import QtQuick
import QtMultimedia
import SimplePresenterApp

// The right of the operator window: what the audience and the stage are being shown, the
// buttons that clear it, and the transport for a video that is playing.
//
// The previews are built from cheap parts instead of second copies of the outputs. The
// slide is drawn again at this small size; a still image comes from its cached
// thumbnail; and video borrows a few frames a second from the output's own decoder (see
// FrameRelay), so previewing a video costs a handful of small texture uploads, not a
// second decode of the file.
Rectangle {
    id: sidePanel

    // The operator window: what this shows is its state, and what the buttons here do is
    // call its functions.
    required property var win
    // Where the output's playing video delivers its frames, or null: the preview borrows
    // a few of them a second.
    property var liveVideoSink: null
    // The player of the output's video, or null: what the transport works
    property var livePlayer: null

    color: "black"

    // Both previews are 16:9 and as wide as the panel, unless the panel is too short
    // for that with the transport under them, in which case they shrink to fit and stay
    // centred.
    readonly property real previewWidth: Math.max(80, Math.min(
        width - 24, (height - 2 * outputTitle.height - clearButtons.height - transport.height - 60) / 2 * 16 / 9))

    SectionTitle {
        id: outputTitle

        anchors.top: parent.top
        text: "Output"
    }

    // The output, small: its media with its slide over it
    Rectangle {
        id: preview

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: outputTitle.bottom
        width: sidePanel.previewWidth
        height: width * 9 / 16
        color: "black"
        border.width: 1
        border.color: "#3a3c42"

        Item {
            anchors.fill: parent
            anchors.margins: 1

            Image {
                anchors.fill: parent
                visible: sidePanel.win.liveMedia !== null && !sidePanel.win.liveMedia.video
                source: visible ? sidePanel.win.thumbnailUrl(sidePanel.win.liveMedia.path) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            VideoOutput {
                id: previewVideo

                anchors.fill: parent
                visible: sidePanel.win.liveMedia !== null && sidePanel.win.liveMedia.video
                fillMode: VideoOutput.PreserveAspectFit
            }

            FrameRelay {
                id: relay

                source: sidePanel.liveVideoSink
                target: previewVideo.videoSink
                interval: 100
            }

            Slide {
                anchors.fill: parent
                slide: sidePanel.win.liveSlide
                effects: false
            }
        }
    }

    SectionTitle {
        id: stageTitle

        anchors.top: preview.bottom
        text: "Stage"
    }

    Rectangle {
        id: stagePreview

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: stageTitle.bottom
        width: sidePanel.previewWidth
        height: width * 9 / 16
        color: "black"
        border.width: 1
        border.color: "#3a3c42"

        StageView {
            anchors.fill: parent
            anchors.margins: 1
            currentText: sidePanel.win.stageCurrentText
            nextText: sidePanel.win.stageNextText
        }
    }

    // Sized for five buttons.
    Row {
        id: clearButtons

        readonly property real buttonWidth: (width - 4 * spacing) / 5

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: stagePreview.bottom
        anchors.margins: 12
        spacing: 6

        // The key hints are dropped from all the buttons together when the widest
        // label would no longer fit.
        readonly property bool showHints: widestLabel.width + 8 <= buttonWidth

        TextMetrics {
            id: widestLabel

            font.pixelSize: 12
            text: "F3 Media"
        }

        component ClearButton: AppButton {
            property string hint
            property string name

            width: clearButtons.buttonWidth
            leftPadding: 2
            rightPadding: 2
            font.pixelSize: 12
            text: clearButtons.showHints ? hint + " " + name : name
            // Red while its layer has something on it, grey once cleared.
            alert: true
        }

        ClearButton {
            enabled: !sidePanel.win.cleared || sidePanel.win.liveMedia !== null
            hint: "F1"
            name: "All"
            onClicked: sidePanel.win.clearAll()
        }

        ClearButton {
            enabled: !sidePanel.win.cleared
            hint: "F2"
            name: "Slide"
            onClicked: sidePanel.win.clearSlide()
        }

        ClearButton {
            enabled: sidePanel.win.liveMedia !== null
            hint: "F3"
            name: "Media"
            onClicked: sidePanel.win.clearMedia()
        }
    }

    Transport {
        id: transport

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: clearButtons.bottom
        anchors.margins: 12
        player: sidePanel.livePlayer
        media: sidePanel.win.liveMedia
        pulse: relay
    }
}
