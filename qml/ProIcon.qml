import QtQuick

// One of ProPresenter's own pictures (see src/iconprovider.h, and icons/README.md for
// where they are from): `name` says which, as the file in icons/ is called without its
// ".png" ("Looks", "ClearMedia", "Countdown").
//
// It is drawn in `ink`. Left unset, the picture is drawn in the colours it has itself,
// which is for the few that have any (the green screen that says the screens are on).
//
// `size` is the side of the square it takes up. The drawing is in the middle of that
// square and is about two thirds as high as it, and as wide as it happens to be (a
// timer is as wide as it is high, the glasses and moustache of a look half as wide
// again). So a row of them given one size has them the sizes they are beside each
// other in ProPresenter, and something that wants a drawing of a given height asks for
// about one and a half times that.
//
// The picture is made at the size and in the colour it is asked for, once, and is then
// an ordinary small picture. So `ink` should be one of a few fixed colours, not
// something that fades: every new colour is another picture made and kept.
Image {
    id: icon

    property string name
    property color ink: "transparent"
    property real size: 24
    // The colour as a URL can carry it: "rrggbb" or "aarrggbb", without the "#"
    readonly property string inkName: ink.a > 0 ? String(ink).substring(1) : ""

    width: size
    height: size
    // (Asked for in the window's own units: for a picture from a provider QML asks for
    // as many real pixels as the screen has to each, and asks again on another screen.)
    sourceSize: Qt.size(Math.round(size), Math.round(size))
    source: name === "" ? "" : "image://icon/" + name + (inkName !== "" ? "/" + inkName : "")
}
