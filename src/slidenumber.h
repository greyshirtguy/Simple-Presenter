#pragma once

#include <QString>

// A slide asked for by its number, typed at the keyboard: 1, 2, Enter. Plain rules,
// with nothing of the windows in them, so that they can be tried by themselves
// (tests/unit/tst_slidenumber): what the digits typed so far make, and which slide of a
// presentation that is.
//
// How it goes, in the operator window (Main.qml has the keys, TypedNumber.qml what is
// shown). A digit typed while showing does nothing to the show: it puts the number up,
// in a panel over the slides. More digits add to it. Enter takes the slide with that
// number live, as a click on it would. A second without a key and the panel goes, with
// nothing done. The numbers are the ones the slides wear in the grid: from 1, in the
// order the arrangement being viewed has them in.
//
// It was made as it was asked for, by someone who knows ProPresenter well; what
// ProPresenter itself does with a number typed was not looked at. So what is here and
// was not in the asking is this app's own, and a GUESS at what is wanted:
//
//   - Esc gives the number up at once, and Backspace takes its last digit off.
//   - A number has four digits at most; a fifth is not taken.
//   - A nought typed first is dropped for the digit after it, so that what is shown is
//     always the number as it will be taken.
//   - A number that no slide has (0, or more than there are slides) does nothing when
//     Enter is pressed.
//   - A digit that is a group's hotkey stays that hotkey, and does not begin a number,
//     since it was a hotkey first; once a number has been begun, every digit is a
//     digit of it. (ProPresenter's own hotkeys are letters, so in a workspace that
//     nobody has given a digit to, every digit begins a number.)
namespace slidenumber {

// The most digits a number has
inline constexpr int kMostDigits = 4;

// What is typed so far, with one more key: the digit added, where the key is a digit
// and there is room for it. Any other key changes nothing.
QString typed(const QString &soFar, const QString &key);

// What is typed so far with its last digit taken off; nothing, when that was the only
// one.
QString erased(const QString &soFar);

// The place, from 0, of the slide a number asks for in a presentation of so many
// slides; -1 where no slide has that number.
int slideFor(const QString &typed, int slides);

} // namespace slidenumber
