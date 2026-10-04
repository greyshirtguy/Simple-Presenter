import QtQuick
import QtMultimedia

// An image or a looping, silent video, scaled to fit.
Item {
    id: root

    // { source, video }, or null for nothing
    property var content: null
    // The QVideoSink frames are delivered to while a video is showing, else null
    readonly property var videoSink: content !== null && content.video && loader.item ? loader.item.videoSink : null

    Loader {
        id: loader

        anchors.fill: parent
        sourceComponent: root.content === null ? null : root.content.video ? video : image
    }

    Component {
        id: image

        Image {
            source: root.content?.source ?? ""
            fillMode: Image.PreserveAspectFit
        }
    }

    Component {
        id: video

        Item {
            readonly property var videoSink: output.videoSink

            MediaPlayer {
                source: root.content?.source ?? ""
                videoOutput: output
                loops: MediaPlayer.Infinite
                onErrorOccurred: (error, errorString) => console.warn("Media layer:", errorString)
                Component.onCompleted: play()
            }

            VideoOutput {
                id: output

                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
            }
        }
    }
}
