import QtQuick
import QtQuick.Shapes

// A small icon for a row of a list, drawn as shapes so it stays sharp at any scale.
// `kind` is "library", "folder", "playlist" or "presentation"; anything else draws
// nothing. Libraries and playlists are white glyphs on a coloured rounded square;
// folders and presentations are plain muted glyphs, since they label rows and are not
// the content.
Item {
    id: icon

    property string kind
    readonly property bool badge: kind === "library" || kind === "playlist"
    readonly property color ink: badge ? "white" : kind === "folder" ? "#c2b280" : "#b9bcc2"

    width: badge ? 18 : 16
    height: badge ? 18 : 15

    Rectangle {
        anchors.fill: parent
        visible: icon.badge
        radius: 4
        color: icon.kind === "library" ? "#f08a24" : "#3d8be0"
    }

    component Outline: Shape {
        property alias points: line.path

        width: 16
        height: 15
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: icon.ink
            strokeColor: "transparent"

            PathPolyline {
                id: line
            }
        }
    }

    // Folder: a body with a tab at the top left.
    Outline {
        visible: icon.kind === "folder"
        points: [Qt.point(0, 2), Qt.point(6, 2), Qt.point(7.5, 4), Qt.point(16, 4),
                 Qt.point(16, 14), Qt.point(0, 14), Qt.point(0, 2)]
    }

    // Playlist: three entries, each a marker and a line.
    Repeater {
        model: icon.kind === "playlist" ? 3 : 0

        delegate: Item {
            required property int index

            x: 4
            y: 4.5 + index * 3.75
            width: 10
            height: 2

            Rectangle {
                width: 2
                height: 2
                color: icon.ink
            }

            Rectangle {
                x: 3.5
                width: parent.width - 3.5
                height: 2
                radius: 1
                color: icon.ink
            }
        }
    }

    // Presentation, and library: an upright panel beside a bracket that opens towards
    // it, after the mark ProPresenter uses for itself, since these are its documents.
    // On a library's badge it is drawn smaller, to leave a margin.
    Item {
        visible: icon.kind === "presentation" || icon.kind === "library"
        width: 16
        height: 15
        scale: icon.badge ? 0.66 : 1
        anchors.centerIn: parent

        Outline {
            points: [Qt.point(0, 1.9), Qt.point(4.5, 2.9), Qt.point(4.5, 12.4), Qt.point(0, 13.5), Qt.point(0, 1.9)]
        }

        Outline {
            points: [Qt.point(6.15, 2.4), Qt.point(16, 0), Qt.point(16, 15), Qt.point(6.15, 12.5),
                     Qt.point(11, 11.25), Qt.point(11.6, 10.65), Qt.point(11.6, 4.65), Qt.point(11, 4.05),
                     Qt.point(6.15, 2.9), Qt.point(6.15, 2.4)]
        }
    }
}
