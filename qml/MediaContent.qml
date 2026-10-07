import QtQuick
import QtQuick.Window
import QtMultimedia
import SimplePresenterApp

// What the media layer shows: an image or a silent video, scaled to fit. A video goes
// round again at its end if the media says it loops, as a background does, and
// otherwise plays once and stays on its last frame, as a foreground does (see
// workspace::MediaBehaviour in src/workspacefiles.h).
//
// The image or the video is made when there is content and unmade when there is none,
// so that a layer with nothing on it holds no decoder and no picture. That is how a
// video stops decoding the moment a transition away from it has finished: the layer
// sets its content to null.
//
// A still is read from its file on another thread. Decoding one takes a twentieth of a
// second for an ordinary picture and a third of a second for a large one on a modest
// processor, and done here the whole window, and any transition running in it, would
// stand still for as long. A video, likewise, has no picture until its file has been
// opened and its first frame decoded, a tenth of a second or so. So there is a moment
// when there is content but nothing to show yet: `ready` says when that is over, and
// TransitionLayer waits for it before it brings the content in. Without the wait, a cut
// to a video was a cut to black first.
Item {
    id: root

    // { source, video, loops }, or null for nothing
    property var content: null
    // Whether what there is to show can be shown yet
    readonly property bool ready: loader.item === null || loader.item.ready
    // The QVideoSink frames are delivered to while a video is showing, else null
    readonly property var videoSink: content !== null && content.video && loader.item ? loader.item.videoSink : null
    // The MediaPlayer playing the video while one is showing, else null: what a
    // transport control works
    readonly property var player: content !== null && content.video && loader.item ? loader.item.player : null

    Loader {
        id: loader

        anchors.fill: parent
        sourceComponent: root.content === null ? null : root.content.video ? video : image
    }

    Component {
        id: image

        Item {
            // Once the file has been read, or has turned out not to be readable
            readonly property bool ready: picture.status !== Image.Loading

            Image {
                id: picture

                onStatusChanged: {
                    if (status === Image.Error)
                        Log.problem("The picture \"" + (root.content?.name ?? source) + "\" could not be read")
                }

                // What the picture is scaled by to fit, keeping its shape. Worked out here
                // and not left to fillMode, which would also have a small picture decoded
                // *up* to the size below.
                readonly property real fit: implicitWidth > 0 && implicitHeight > 0
                                            ? Math.min(parent.width / implicitWidth, parent.height / implicitHeight) : 0

                // In the middle, on a whole pixel.
                x: Math.floor((parent.width - width) / 2)
                y: Math.floor((parent.height - height) / 2)
                width: implicitWidth * fit
                height: implicitHeight * fit
                source: root.content?.source ?? ""
                asynchronous: true
                // A picture larger than a 4K output could show (or than this one, if it
                // is larger still) is scaled down as it is read. A photograph straight
                // from a camera would otherwise sit in memory, and in the graphics chip's,
                // at several times the size of anything it will be shown at.
                sourceSize: Qt.size(Math.max(3840, Math.ceil(root.width * Screen.devicePixelRatio)),
                                    Math.max(2160, Math.ceil(root.height * Screen.devicePixelRatio)))
            }
        }
    }

    Component {
        id: video

        Item {
            id: clip

            readonly property var videoSink: output.videoSink
            readonly property var player: mediaPlayer
            // Once the first frame is there to be seen, or the file has turned out not
            // to play
            readonly property bool ready: firstFrame.arrived || failed
            property bool failed: false
            // For the log: what the file is called, and when it was asked for
            readonly property string name: root.content?.name ?? ""
            readonly property double askedAt: Date.now()

            MediaPlayer {
                id: mediaPlayer

                source: root.content?.source ?? ""
                videoOutput: output
                loops: root.content?.loops === false ? 1 : MediaPlayer.Infinite
                onErrorOccurred: (error, errorString) => {
                    Log.problem("The video \"" + clip.name + "\" will not play: " + errorString)
                    clip.failed = true
                }
                // Only a video that plays once comes to an end.
                onMediaStatusChanged: {
                    if (mediaStatus === MediaPlayer.EndOfMedia)
                        Log.note("media", "video \"" + clip.name + "\" has played to its end")
                }
                Component.onCompleted: play()
            }

            VideoOutput {
                id: output

                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
                // A video that has played once stays on its last frame until it is
                // cleared or something replaces it.
                endOfStreamPolicy: VideoOutput.KeepLastFrame
            }

            FirstFrame {
                id: firstFrame

                sink: output.videoSink
                // What the video turned out to be, which is what there is to go on when
                // one stutters: how large it is, how it is encoded, and whether the
                // graphics chip is decoding it.
                onArrivedChanged: {
                    if (!arrived)
                        return
                    const codec = mediaPlayer.metaData.stringValue(MediaMetaData.VideoCodec)
                    const rate = Number(mediaPlayer.metaData.value(MediaMetaData.VideoFrameRate))
                    Log.note("media", "video \"" + clip.name + "\": first picture after " + Math.round(Date.now() - clip.askedAt) + " ms; "
                             + description + (codec ? "; " + codec : "") + (rate > 0 ? ", " + (+rate.toFixed(2)) + " frames a second" : "")
                             + (mediaPlayer.duration > 0 ? ", " + (mediaPlayer.duration / 1000).toFixed(1) + " s long" : ""))
                }
            }
        }
    }
}
