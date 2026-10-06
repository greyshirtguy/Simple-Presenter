import QtQuick

// The title of a pane, in small capitals. Give it a colour to make it one of the
// coloured, bold titles of the three browsing areas.
Text {
    readonly property color plain: "#9a9da3"

    leftPadding: 12
    topPadding: 12
    bottomPadding: 6
    color: plain
    font.pixelSize: 12
    font.capitalization: Font.AllUppercase
    // Coloured titles are also bold, to stand out from the plain ones.
    font.bold: color !== plain
}
