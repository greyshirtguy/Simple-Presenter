import QtQuick
import QtQuick.Shapes
import SimplePresenterApp

// The thin strip of tabs over the transport, which pick what it works, as ProPresenter
// has tabs over its own: the video on the media layer (what a slide or the media bin
// plays), the video on the announcements layer, and the audio bin's music (not its sound
// effects, which have no transport). Pictures and no words.
//
// The pictures are those of clearing each of the three layers, without the crossed
// circle: ProPresenter's megaphone for the announcements, which its pack has; and,
// where the pack has none but the clear (see icons/README.md), this app's own drawings
// of the same, the picture in a frame for the media and the joined notes of the audio
// bin's tab for the music.
//
// The tab picked is lit, and a tab whose layer has something playing that the transport
// could work is drawn brighter than one with nothing, so that what is playing can be
// seen without going to it. Which tab is picked is the operator's to choose; nothing
// here turns to a layer by itself. (A GUESS at ProPresenter, which may.)
Row {
    id: tabs

    // "media", "announcements" or "audio"
    property string picked: "media"
    // Whether each has something playing for the transport to work
    property bool mediaLive: false
    property bool announcementsLive: false
    property bool audioLive: false

    signal chosen(string layer)

    height: 22
    spacing: 4

    Repeater {
        model: ["media", "announcements", "audio"]

        delegate: Rectangle {
            id: tab

            required property string modelData
            readonly property bool picked: tabs.picked === modelData
            readonly property bool live: modelData === "media" ? tabs.mediaLive : modelData === "announcements" ? tabs.announcementsLive : tabs.audioLive
            readonly property color ink: picked || live ? "#eceef1" : "#7d8088"

            objectName: "transportTab:" + modelData
            width: (tabs.width - 2 * tabs.spacing) / 3
            height: tabs.height
            radius: 4
            color: picked ? "#3a3c42" : mouse.containsMouse ? "#2a2c31" : "transparent"

            // The media's: a picture in a frame, a hill and the sun in it
            Shape {
                anchors.centerIn: parent
                width: 18
                height: 16
                visible: tab.modelData === "media"
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: tab.ink
                    strokeWidth: 1.4
                    fillColor: "transparent"
                    joinStyle: ShapePath.RoundJoin

                    PathSvg { path: "M 3 1.5 H 15 A 1.5 1.5 0 0 1 16.5 3 V 13 A 1.5 1.5 0 0 1 15 14.5 H 3 A 1.5 1.5 0 0 1 1.5 13 V 3 A 1.5 1.5 0 0 1 3 1.5 Z" }
                }

                ShapePath {
                    strokeColor: "transparent"
                    fillColor: tab.ink

                    PathSvg { path: "M 3 13 L 7 7.5 L 9.8 11 L 11.8 8.8 L 15 13 Z M 7.6 5 A 1.4 1.4 0 1 1 4.8 5 A 1.4 1.4 0 1 1 7.6 5 Z" }
                }
            }

            ProIcon {
                anchors.centerIn: parent
                visible: tab.modelData === "announcements"
                name: "Announcements"
                ink: tab.ink
                size: 18
            }

            AudioGlyph {
                anchors.centerIn: parent
                visible: tab.modelData === "audio"
                kind: "music"
                ink: tab.ink
                size: 17
            }

            MouseArea {
                id: mouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: tabs.chosen(tab.modelData)
            }
        }
    }
}
