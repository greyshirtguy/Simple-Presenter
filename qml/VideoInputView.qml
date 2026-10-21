import QtQuick
import QtMultimedia
import SimplePresenterApp

// One of the workspace's video inputs, live: what the video input layer of a screen
// shows, what a shape filled with an input shows, and the preview of one in the
// settings.
//
// It holds the input while it is here (VideoInputFeed), which is what has it captured
// at all, and is handed the frames the input's one capture brings in (see VideoInputs):
// every one of them for a screen, `interval` apart for something small. Until the
// first arrives, or with an input that has no device on this computer, it shows
// nothing, which on a screen is the black under everything. (A GUESS at ProPresenter,
// which may show something of its own for an input that sends nothing.)
//
// Or, `still`, the input's thumbnail on its colour, as the media bin shows it, holding
// nothing: what a shape filled with an input shows in a thumbnail of its slide, where a
// live picture in every thumbnail would cost more than the show. (A GUESS at what
// ProPresenter's thumbnails show.) Each of the two is made only where it is wanted.
Item {
    id: view

    // The input: by its place in the workspace's list, as ProPresenter's files name it,
    // or by its id
    property int index: -1
    property string inputId: ""
    // Whether what this is on plays the input's sound: an audience screen does
    property bool sound: false
    // How many milliseconds apart frames are taken: 0 for every one
    property int interval: 0
    // How it is fitted, as ProPresenter's media are: 0 to fit, 1 to fill, 2 stretched
    property int fitting: 0
    property bool still: false
    // Whether what it is on is seen: while it is not (a look has taken the layer off
    // the screen), it is handed no frames, which would have the window drawn for
    // nothing (a hidden item that changes is still a redraw), but the input is still
    // held, so that it is there again at once
    property bool shown: true
    // Whether the input is sending pictures
    readonly property bool sending: feed.sending

    VideoInputFeed {
        id: feed

        index: view.index
        inputId: view.inputId
        sound: view.sound
        active: !view.still
    }

    Loader {
        anchors.fill: parent
        active: !view.still

        sourceComponent: Item {
            VideoOutput {
                id: picture

                anchors.fill: parent
                fillMode: view.fitting === 2 ? VideoOutput.Stretch : view.fitting === 1 ? VideoOutput.PreserveAspectCrop : VideoOutput.PreserveAspectFit
            }

            FrameRelay {
                source: view.shown ? feed.sink : null
                target: picture.videoSink
                interval: view.interval
            }
        }
    }

    // (The input read from the list itself, so that it follows changes to it)
    Loader {
        anchors.fill: parent
        active: view.still

        sourceComponent: Rectangle {
            readonly property var input: view.inputId !== "" ? VideoInputs.inputs.find(i => i.id === view.inputId) ?? null
                                                             : VideoInputs.inputs[view.index] ?? null

            objectName: "videoInputStill"
            color: input !== null && input.color !== "" ? input.color : "black"

            Image {
                anchors.fill: parent
                visible: parent.input !== null && parent.input.thumbnail !== ""
                source: parent.input !== null ? parent.input.thumbnail : ""
                fillMode: view.fitting === 2 ? Image.Stretch : view.fitting === 1 ? Image.PreserveAspectCrop : Image.PreserveAspectFit
                asynchronous: true
                cache: false
            }
        }
    }
}
