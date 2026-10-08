import QtQuick

// One of the small icons in the top left corner of a thumbnail that say what comes with
// a slide or a media file: the key that goes to it, the media it brings and how that
// plays. (More kinds are to come.)
//
// They are all one size and have one look, so that a row of them reads as a row: a
// small rounded square, or a little wider, with a thin black edge that keeps it apart
// from a picture of its own colour, and see-through by as much as the settings say, so
// that it does not blot out what is under it. What is in it is the icon's own.
Rectangle {
    // How solid: 1 is fully, and the settings screen goes down to 0.05
    property real strength: 0.8

    width: 18
    height: 18
    radius: 3.5
    // A dark grey that is still seen against a black slide
    color: "#383b42"
    border.width: 1
    border.color: "black"
    opacity: strength
}
