.pragma library

// The drawings for the pictures of clear groups that ProPresenter's pack has no picture
// of (see ClearGroupGlyph.qml, which draws them, and src/clearfile.h for the numbers).
// They are this app's own: plain line drawings of the thing each picture is called.
//
// Each is on a grid of twenty-four, the thing about seventeen high in the middle of it,
// and is two paths, either of which may be empty: `lines`, drawn as lines, and `solid`,
// filled in.
//
// It is here, in a library, so that the table is made once for the app, and not once
// for every button that has a picture.

// A circle as a path: the middle and how far out
function ring(x, y, r) {
    return "M " + (x - r) + " " + y + " A " + r + " " + r + " 0 1 0 " + (x + r) + " " + y + " A " + r + " " + r + " 0 1 0 " + (x - r) + " " + y + " Z "
}

const drawings = {
    // Megaphone
    12: { lines: "M 4 9.5 H 7 L 16.5 5 V 19 L 7 14.5 H 4 Z M 8.2 14.9 L 9.6 19.5 H 12 L 10.9 15.7 M 19.2 9.6 Q 20.8 12 19.2 14.4", solid: "" },
    // Play
    13: { lines: "", solid: "M 8 5 L 19.5 12 L 8 19 Z" },
    // Bulb
    14: { lines: "M 9.3 16.5 C 6.8 14.9 5.5 12.8 5.5 10.5 A 6.5 6.5 0 0 1 18.5 10.5 C 18.5 12.8 17.2 14.9 14.7 16.5 Z M 9.8 19 H 14.2 M 10.8 21.3 H 13.2", solid: "" },
    // Sunglasses
    15: { lines: "M 2.5 9.5 H 21.5", solid: "M 4 9.5 H 10.6 V 12.6 A 3.3 3.3 0 0 1 4 12.6 Z M 13.4 9.5 H 20 V 12.6 A 3.3 3.3 0 0 1 13.4 12.6 Z" },
    // Arrow
    16: { lines: "M 4 12 H 19.5 M 13.5 6 L 19.5 12 L 13.5 18", solid: "" },
    // Target
    17: { lines: ring(12, 12, 8.5) + ring(12, 12, 4.6), solid: ring(12, 12, 1.4) },
    // Star
    18: { lines: "", solid: "M 12 3.5 L 14.23 9.43 L 20.56 9.72 L 15.61 13.67 L 17.29 19.78 L 12 16.3 L 6.71 19.78 L 8.39 13.67 L 3.44 9.72 L 9.77 9.43 Z" },
    // Sun
    19: { lines: ring(12, 12, 4) + "M 18.6 12 H 21.5 M 16.7 16.7 L 18.7 18.7 M 12 18.6 V 21.5 M 7.3 16.7 L 5.3 18.7 M 5.4 12 H 2.5 M 7.3 7.3 L 5.3 5.3 M 12 5.4 V 2.5 M 16.7 7.3 L 18.7 5.3", solid: "" },
    // Bell
    20: { lines: "M 5.5 17 H 18.5 L 16.6 14.2 V 10.6 A 4.6 4.6 0 0 0 7.4 10.6 V 14.2 Z M 10.4 19.6 A 1.7 1.7 0 0 0 13.6 19.6 M 12 4.2 V 6", solid: "" },
    // Paperclip
    21: { lines: "M 15.8 8.2 L 9.6 14.4 A 1.9 1.9 0 0 0 12.3 17.1 L 18.4 11 A 3.9 3.9 0 0 0 12.9 5.5 L 6.6 11.8 A 5.8 5.8 0 0 0 14.8 20 L 19.3 15.5", solid: "" },
    // Flask
    22: { lines: "M 9.6 4 H 14.4 M 10.4 4 V 9.6 L 5.6 18.2 A 1.5 1.5 0 0 0 6.9 20.5 H 17.1 A 1.5 1.5 0 0 0 18.4 18.2 L 13.6 9.6 V 4 M 8 15.2 H 16", solid: "" },
    // Eyeglasses
    23: { lines: ring(7.5, 13.5, 3.6) + ring(16.5, 13.5, 3.6) + "M 11.1 13 Q 12 12.1 12.9 13 M 4 12.6 L 3 8.6 M 20 12.6 L 21 8.6", solid: "" },
    // Cupcake
    24: { lines: "M 7 13.6 H 17 L 15.8 20.2 H 8.2 Z M 6.6 13.6 A 2.9 2.9 0 0 1 8 8.7 A 4.2 4.2 0 0 1 16 8.7 A 2.9 2.9 0 0 1 17.4 13.6", solid: ring(12, 4.6, 1.3) },
    // Slide
    25: { lines: "M 4.5 6 H 19.5 A 1 1 0 0 1 20.5 7 V 17 A 1 1 0 0 1 19.5 18 H 4.5 A 1 1 0 0 1 3.5 17 V 7 A 1 1 0 0 1 4.5 6 Z M 7.5 10.5 H 16.5 M 9 13.5 H 15", solid: "" },
    // Hat
    26: { lines: "M 8 16.4 V 6.4 H 16 V 16.4 M 4 16.6 H 20 M 8 13.2 H 16", solid: "" },
    // Flower
    27: { lines: ring(16.9, 12, 2.5) + ring(14.45, 16.24, 2.5) + ring(9.55, 16.24, 2.5) + ring(7.1, 12, 2.5) + ring(9.55, 7.76, 2.5) + ring(14.45, 7.76, 2.5),
          solid: ring(12, 12, 1.7) },
    // Heart
    28: { lines: "", solid: "M 12 20 C 5 15 3.5 11.5 3.5 8.8 A 4.3 4.3 0 0 1 12 7.6 A 4.3 4.3 0 0 1 20.5 8.8 C 20.5 11.5 19 15 12 20 Z" },
    // Audio
    30: { lines: "M 15.2 9 Q 17.2 12 15.2 15 M 17.6 6.4 Q 21.4 12 17.6 17.6", solid: "M 4 9.6 H 7.4 L 12 5.6 V 18.4 L 7.4 14.4 H 4 Z" },
    // Cloud
    31: { lines: "M 7 18 A 4 4 0 0 1 6.6 10.1 A 5.5 5.5 0 0 1 17.2 9.2 A 4.4 4.4 0 0 1 17.5 18 Z", solid: "" }
}

// The drawing for a picture, by its number, or null if it is not one that is drawn here
function drawing(icon) {
    return drawings[icon] ?? null
}
