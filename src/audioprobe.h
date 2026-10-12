#pragma once

#include <QString>

// How long a sound file is.
//
// The audio bin shows the length of each piece of audio, and ProPresenter's file keeps
// it with the row (audiocue.h). A row ProPresenter wrote has it; for a file added here
// it has to be found out, which only whatever plays media can tell: so the file is
// opened as it would be to play it, its length read when that is known, and it is
// closed again without a sound being made.
//
// That takes a moment (some hundredths of a second for an ordinary file), and is done
// where it is asked, on the thread that asks: it is asked when files are added to the
// bin, by whoever is adding them, and not while a show is drawn by anything. While it
// waits it lets the app go on drawing and keeps what is typed or clicked for
// afterwards. A file that will not say within two seconds, or cannot be read, is 0:
// its row is then made without a length, and is found to have none until it is played.
namespace audioprobe {

// The length of the file in seconds, or 0 if it could not be told.
double seconds(const QString &file);

} // namespace audioprobe
