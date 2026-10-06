#pragma once

#include <QString>

class QQuickWindow;

// The self-test (--selftest <dir>): works the three windows through a fixed sequence on
// timers, saving a frame of each window into `dir` as it goes, then quits the app. It
// changes nothing on disk but those frames. The steps are functions of the operator
// window (qml/Main.qml), called by name.
//
// It is how a change to the rendering is checked: run it before and after, and compare
// the frames.
void runSelfTest(QQuickWindow *operatorWindow, QQuickWindow *output, QQuickWindow *stage, const QString &dir);
