import QtQuick
import SimplePresenterApp

// The thin strip of tabs over the transport, which pick what it works, as ProPresenter
// has tabs over its own: the presentation's, which is the video on the media layer (what
// a slide or the media bin plays), the video on the announcements layer, and the audio
// bin's music (not its sound effects, which have no transport). Pictures and no words.
//
// The pictures are those of clearing each of the three layers, without the crossed
// circle, as ProPresenter's tabs have them: its slide for the presentation's (the
// presentation destination, whose media is the media layer's), its megaphone for the
// announcements, and its joined notes for the audio (see icons/README.md for where
// each is from).
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

            ProIcon {
                anchors.centerIn: parent
                name: tab.modelData === "media" ? "Presentation" : tab.modelData === "announcements" ? "Announcements" : "Audio"
                ink: tab.ink
                size: 18
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
