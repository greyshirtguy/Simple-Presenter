#pragma once

#include "slide.pb.h"

#include <QList>
#include <QVariantMap>

// ProPresenter's Scrolling Text: a text box whose words travel across it, left, right,
// up or down, once or over and over. Plain rules on ProPresenter's own message of it
// (Slide.Element.TextScroller), and on where the words are as they go, with nothing of
// the windows in them, so that they can be tried by themselves (tests/unit/tst_textscroller).
// ScrollingText draws it, on the screens and in the operator's preview; a thumbnail and
// the editor show the words still, where they are.
//
// What the file holds (two of ProPresenter's own were looked at, in the maintainer's
// presentations: one going up, starting off the box, once; one going left, over and over):
// `should_scroll`; `scrolling_direction` (left, right, up, down); `scroll_rate`, the
// Speed slider from 0 to 1; `should_repeat`; `repeat_distance`, the Distance slider from
// 0 to 1, the gap between one going of the words and the next; `starts_off_screen`,
// ProPresenter's Start Position "Off-Screen" (otherwise "Automatic": the words start
// where they are); `fade_left` and `fade_right`, the Feathering, 0 to 1. A text box that
// does not scroll has the message too, with ProPresenter's settings for a new one (rate
// 0.5, repeat, distance 0.05: see proconvert::makeTextElement).
//
// GUESSES, the file saying only where the sliders were:
//   - how fast the Speed is: from 20 slide units a second at 0 (a crawl) to 500 at 1,
//     each tenth of the slider about a third faster than the last; 100 at the middle, a
//     1920-wide screen crossed in about 20 seconds. The maintainer is to compare it with
//     ProPresenter and say.
//   - that the Distance is a share of the box, along the way the words go: 0.05 of a box
//     1920 wide is a gap of 96.
//   - that the Feathering is a share of the box at each end along the way the words go:
//     at the left and the right of one going across, and at the top (the left's) and the
//     bottom (the right's) of one going up or down.
//   - that words going across are one line, as long as they are, and words going up or
//     down are as wide as the box; and that words that do not repeat go once and are
//     then gone.
namespace textscroller {

using Scroller = rv::data::Slide::Element::TextScroller;

// What the file says, for a box that scrolls: { direction ("left", "right", "up",
// "down"), rate, repeat, distance, offScreen, fadeLeft, fadeRight }
QVariantMap describe(const Scroller &scroller);
// Whether `changes` (as the editor's inspector makes them) change scrolling, and the
// changes made: scrollOn, scrollDirection, scrollRate, scrollRepeat, scrollDistance,
// scrollOffScreen, scrollFadeLeft, scrollFadeRight. A box switched to scrolling that has
// never had its settings set is given ProPresenter's for a new box.
bool asks(const QVariantMap &changes);
void change(Scroller *scroller, const QVariantMap &changes);

// How fast words go at a Speed of `rate`, in slide units a second (see the GUESSES)
double speed(double rate);
// Where the words start, along the way they go, in a box `room` long, as if they went
// towards the start of the box (left, or up): at its far end for Off-Screen, otherwise
// where they are, at 0
double start(bool offScreen, double room);
// Where each going of the words is, `travelled` slide units after they started from
// `start`, as above: the near edge of each that can be seen in a box `room` long, of
// words `length` long with a gap of `gap` between one going and the next. Over and over
// with `repeat`; otherwise once, and then none. At most `most` of them.
QList<double> positions(double travelled, double start, double length, double gap, double room, bool repeat, int most);

} // namespace textscroller
