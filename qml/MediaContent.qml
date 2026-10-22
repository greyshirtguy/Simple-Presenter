import QtQuick
import QtQuick.Window
import QtMultimedia
import SimplePresenterApp

// What the media layer shows: an image or a video, drawn as its Media Inspector sets it
// (fitted, cropped, turned, flipped, with its effects: see MediaLook.qml), which for
// media with no settings is scaled to fit. A video goes round again at its end if the
// media says it loops, as a background mostly does: for good, or a number of times, or
// for a length of time, after which it stays on the frame it has reached. Otherwise it
// plays once and stays on its last frame, as a foreground mostly does. A video that is
// trimmed plays from its in point to its out point, which are then its start and its
// end. And it is played with its sound if the media gives it a volume, and silently if
// not. How a piece of media is to play is all decided where it is read
// (workspace::MediaBehaviour in src/workspacefiles.h, and src/mediasettings.h), and
// what is here only does as it is told.
//
// A video's sound goes to the system's own audio output. It rises and falls with the
// picture: the layer says how much of this instance is on show (`level`), which a
// transition takes from nothing to all of it or back, and the sound is played that
// much quieter. So a video dissolving in or out fades in or out to the ear as well,
// and one that is cut to or from starts or stops at once. A video with no sound to
// play is given no audio output at all, and costs nothing for it. Where a device has
// been chosen for sound in the settings, it is played through that one (AudioLayer has
// it); and what is played is handed to the meter in the preview as it goes (AudioTap).
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
//
// With several screens there are several of these showing the same video, one in each
// screen's scene, and the video is still played once: by the scene that `leads`, which
// offers the frames it is sent to the others (see MediaFeeds). In a scene that does not
// lead, a video is a picture fed from there, with no player and no sound of its own, so
// the screens cannot drift apart and the sound is heard once. (A still each scene reads
// for itself: it is read once whatever asks, and kept.)
Item {
    id: root

    // { source, video, loops, volume, playback, loopCount, loopSeconds, settings }, or
    // null for nothing (the behaviour as workspace::MediaBehaviour has it, the settings
    // as mediasettings::describe has them)
    property var content: null
    // Whether this plays a video itself, or is fed the frames of the scene that does
    property bool leads: true
    // How much of this instance is on show, from 0 to 1: kept by the layer it is in
    // (see TransitionLayer), and all of it when it is used by itself
    property real level: 1
    // Whether what there is to show can be shown yet
    readonly property bool ready: loader.item === null || loader.item.ready
    // A video has played to its end, and stays where it is: its end, its out point, or
    // the end of the count or the time it goes round for. Said by the scene that plays
    // it, for what follows it in a media playlist (see Show.mediaFinished), with the
    // number of its playing (`playId`), so that a playing that has gone since is not
    // taken for the one there is now.
    signal ended(int playId)
    // Playback has reached one of the video's playback markers (`marker` as
    // playbackmarkers::describe has it), for its actions to be run (Show.runMarker); said
    // by the scene that plays it, with the number of its playing, as `ended` is.
    signal markerReached(int playId, var marker)
    // The QVideoSink frames are delivered to while a video is showing, else null
    readonly property var videoSink: content !== null && content.video && loader.item ? loader.item.videoSink : null
    // The MediaPlayer playing the video while one is showing, else null: what a
    // transport control works
    readonly property var player: content !== null && content.video && loader.item ? loader.item.player : null

    Loader {
        id: loader

        anchors.fill: parent
        sourceComponent: root.content === null ? null : !root.content.video ? image : root.leads ? video : fed
    }

    Component {
        id: image

        Item {
            // Once the file has been read, or has turned out not to be readable. (Not as
            // "not loading": while the picture is being made there is a moment when it
            // is not loading yet, and a transition started then had the picture decoded
            // half way through it, which the benchmark's dissolves showed as a stall.)
            readonly property bool ready: picture.status === Image.Ready || picture.status === Image.Error

            // Where the picture goes, and how large, is worked out there (in the middle,
            // on a whole pixel, when it is fitted) and not left to fillMode, which would
            // also have a small picture decoded *up* to the size below.
            MediaPicture {
                id: picture

                onStatusChanged: {
                    if (status === Image.Error)
                        Log.problem("The picture \"" + (root.content?.name ?? source) + "\" could not be read")
                }

                anchors.fill: parent
                source: root.content?.source ?? ""
                settings: root.content?.settings ?? null
                fitsHere: true
                // A picture larger than a 4K output could show (or than this one, if it
                // is larger still) is scaled down as it is read. A photograph straight
                // from a camera would otherwise sit in memory, and in the graphics chip's,
                // at several times the size of anything it will be shown at.
                readSize: Qt.size(Math.max(3840, Math.ceil(root.width * Screen.devicePixelRatio)),
                                  Math.max(2160, Math.ceil(root.height * Screen.devicePixelRatio)))
            }
        }
    }

    Component {
        id: video

        Item {
            // (Not called `clip`: what is made inside the Loader below would find the
            // Loader's own property of that name first.)
            id: movie

            readonly property var videoSink: output.videoSink
            readonly property var player: mediaPlayer
            // Once the first frame is there to be seen, or the file has turned out not
            // to play
            readonly property bool ready: firstFrame.arrived || failed
            property bool failed: false
            // For the log: what the file is called, and when it was asked for
            readonly property string name: root.content?.name ?? ""
            readonly property double askedAt: Date.now()
            // How loud its sound is to be, 0 being not at all
            readonly property real volume: Math.max(0, Math.min(1, Number(root.content?.volume ?? 0)))
            // The part of it that plays, if it is trimmed (see mediasettings.h): from the
            // in point, in milliseconds, to the out point, which is its end then; -1 for
            // the end of the file. A trimmed video is taken round by hand, from its out
            // point back to its in point, rather than by the player, which only goes
            // round the whole of it.
            readonly property real inAt: Math.max(0, Number(root.content?.settings?.inPoint ?? 0)) * 1000
            readonly property real outAt: root.content?.settings?.trimmedOut ? Number(root.content.settings.outPoint) * 1000 : -1
            readonly property bool trimmed: inAt > 0 || outAt > 0
            // How many more times it is to go round from its out point (for one that goes
            // round a number of times), and whether it has reached its end
            property int roundsLeft: root.content?.playback === 2 ? Math.max(1, root.content.loopCount) - 1 : 0
            property bool atEnd: false
            // Set once it has been started: at once, or once the file is open, for one
            // that starts at an in point (the player can only be sent there then)
            property bool started: false

            // Starts it, from its in point if it has one.
            function start() {
                if (started)
                    return
                if (inAt > 0)
                    mediaPlayer.setPosition(inAt)
                started = true
                mediaPlayer.play()
            }

            // At the out point: round again, or to an end, as at the end of the file.
            function reachedOut() {
                const loops = root.content?.loops === true && !(root.content?.playback === 2 && roundsLeft <= 0)
                if (loops && !lapse.lapsed) {
                    if (root.content?.playback === 2)
                        --roundsLeft
                    mediaPlayer.setPosition(inAt)
                    // (At the end of its file the player has stopped.)
                    if (mediaPlayer.playbackState !== MediaPlayer.PlayingState)
                        mediaPlayer.play()
                    return
                }
                atEnd = true
                mediaPlayer.pause()
                Log.note("media", "video \"" + movie.name + "\" has played to its out point")
                root.ended(root.content?.playId ?? 0)
            }

            MediaPlayer {
                id: mediaPlayer

                source: root.content?.source ?? ""
                videoOutput: output
                audioOutput: sound.item ? sound.item.output : null
                audioBufferOutput: sound.item ? sound.item.tap.output : null
                loops: root.content?.loops === false || movie.trimmed ? 1
                     : root.content?.playback === 2 ? Math.max(1, root.content.loopCount) : MediaPlayer.Infinite
                onErrorOccurred: (error, errorString) => {
                    Log.problem("The video \"" + movie.name + "\" will not play: " + errorString)
                    movie.failed = true
                }
                // Only a video that plays once comes to an end. (One that is trimmed only
                // comes to the end of its file when its out point is the end, and goes
                // round from there by hand.)
                onMediaStatusChanged: {
                    if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia)
                        movie.start()
                    if (mediaStatus === MediaPlayer.EndOfMedia) {
                        if (movie.trimmed && !movie.atEnd)
                            movie.reachedOut()
                        else {
                            Log.note("media", "video \"" + movie.name + "\" has played to its end")
                            root.ended(root.content?.playId ?? 0)
                        }
                    }
                }
                Component.onCompleted: {
                    if (movie.inAt <= 0)
                        movie.start()
                }
            }

            // Its playback markers (see playbackmarkers.h), for a video that has any. As
            // playback reaches one it is said, and its actions run. Looked at only for such
            // a video, as the player says where it is, which is twenty times a second or
            // so: nothing is asked of the others as they play. A jump forward of more than
            // a second (the transport moving it) passes the markers in between unsaid, and
            // one back does too unless it is to the first second, as going round is: then
            // the markers from the start are said again. (GUESSES at ProPresenter, whose
            // help says only that they run when playback reaches them.)
            readonly property var markers: root.content?.markers ?? []
            property real lastAt: -1

            function passMarkers(now) {
                const was = lastAt
                lastAt = now
                const from = now < was ? (now < 1 ? -1 : now) : now - was > 1 ? now : was
                for (const marker of markers) {
                    if (marker.time > from && marker.time <= now)
                        root.markerReached(root.content?.playId ?? 0, marker)
                }
            }

            Connections {
                target: mediaPlayer
                enabled: movie.markers.length > 0

                function onPositionChanged() {
                    movie.passMarkers(mediaPlayer.position / 1000)
                }
            }

            // Where it has got to, looked at only for a video with an out point before its
            // end: nothing is asked of the others as they play.
            Connections {
                target: mediaPlayer
                enabled: movie.outAt > 0

                function onPositionChanged() {
                    if (!movie.atEnd && mediaPlayer.position >= movie.outAt && mediaPlayer.playbackState === MediaPlayer.PlayingState)
                        movie.reachedOut()
                }
            }

            // The other screens' scenes take their frames from here.
            readonly property int playId: root.content?.playId ?? 0
            Component.onCompleted: MediaFeeds.offer(playId, output.videoSink)
            Component.onDestruction: MediaFeeds.withdraw(playId, output.videoSink)

            // A video that goes round for a length of time stops where it is when the
            // time is up.
            Timer {
                id: lapse

                interval: Math.max(1, (root.content?.loopSeconds ?? 0) * 1000)
                running: root.content?.playback === 3 && mediaPlayer.playbackState === MediaPlayer.PlayingState
                         && interval > 1 && !lapsed
                property bool lapsed: false
                onTriggered: {
                    lapsed = true
                    Log.note("media", "video \"" + movie.name + "\" has gone round for its " + root.content.loopSeconds + " seconds, and stops")
                    mediaPlayer.pause()
                    root.ended(root.content?.playId ?? 0)
                }
            }

            // Only a video that is to be heard has anything to be heard through.
            Loader {
                id: sound

                active: movie.volume > 0

                sourceComponent: QtObject {
                    readonly property AudioOutput output: AudioOutput {
                        objectName: "mediaSound"
                        volume: movie.volume * root.level
                    }
                    // How loud it is, for the meter: as loud as it is played here,
                    // while it is playing.
                    readonly property AudioTap tap: AudioTap {
                        gain: movie.volume * root.level
                        playing: mediaPlayer.playbackState === MediaPlayer.PlayingState
                    }
                    // (Only where a device has been chosen: otherwise the output is
                    // left to go where the system sends sound, and to follow it.)
                    readonly property Binding chosen: Binding {
                        target: output
                        property: "device"
                        value: AudioLayer.outputDevice
                        when: AudioLayer.device !== ""
                    }
                }
            }

            MediaLook {
                id: movieLook

                anchors.fill: parent
                settings: root.content?.settings ?? null
                sourceWidth: output.sourceRect.width
                sourceHeight: output.sourceRect.height

                VideoOutput {
                    id: output

                    anchors.fill: parent
                    fillMode: movieLook.videoFill
                    // A video that has played once stays on its last frame until it is
                    // cleared or something replaces it.
                    endOfStreamPolicy: VideoOutput.KeepLastFrame
                }
            }

            FirstFrame {
                id: firstFrame

                // (From when it is started: a video with an in point may show its first
                // frame as its file is opened, before it has been sent to the in point.)
                sink: movie.started ? output.videoSink : null
                // What the video turned out to be, which is what there is to go on when
                // one stutters: how large it is, how it is encoded, and whether the
                // graphics chip is decoding it.
                onArrivedChanged: {
                    if (!arrived)
                        return
                    const codec = mediaPlayer.metaData.stringValue(MediaMetaData.VideoCodec)
                    const rate = Number(mediaPlayer.metaData.value(MediaMetaData.VideoFrameRate))
                    // Whether there is sound in the file, and whether it is being played:
                    // what there is to go on when a video is silent that should not be
                    const tracks = mediaPlayer.audioTracks.length
                    const heard = tracks === 0 ? "no sound in the file"
                                : movie.volume <= 0 ? "its sound not played"
                                : "its sound played" + (movie.volume < 1 ? " at " + Math.round(movie.volume * 100) + "%" : "")
                                  + " through " + (sound.item.output.device.description || "no audio output")
                    Log.note("media", "video \"" + movie.name + "\": first picture after " + Math.round(Date.now() - movie.askedAt) + " ms; "
                             + description + (codec ? "; " + codec : "") + (rate > 0 ? ", " + (+rate.toFixed(2)) + " frames a second" : "")
                             + (mediaPlayer.duration > 0 ? ", " + (mediaPlayer.duration / 1000).toFixed(1) + " s long" : "")
                             + "; " + heard)
                }
            }
        }
    }

    // A video another scene is playing, shown from its frames.
    Component {
        id: fed

        Item {
            readonly property var videoSink: output.videoSink
            readonly property var player: null
            // Once a frame has been handed over; or, the scene that plays it having none
            // to hand over after a while (the file will not play), without one, so that
            // this screen is not left showing what was there before.
            readonly property bool ready: firstFrame.arrived || waited.lapsed

            MediaLook {
                id: fedLook

                anchors.fill: parent
                settings: root.content?.settings ?? null
                sourceWidth: output.sourceRect.width
                sourceHeight: output.sourceRect.height

                VideoOutput {
                    id: output

                    anchors.fill: parent
                    fillMode: fedLook.videoFill
                    endOfStreamPolicy: VideoOutput.KeepLastFrame
                }
            }

            FrameRelay {
                source: MediaFeeds.revision >= 0 ? MediaFeeds.sink(root.content?.playId ?? 0) : null
                target: output.videoSink
                interval: 0
            }

            FirstFrame {
                id: firstFrame

                sink: output.videoSink
            }

            Timer {
                id: waited

                property bool lapsed: false

                interval: 3000
                running: !firstFrame.arrived
                onTriggered: lapsed = true
            }
        }
    }
}
