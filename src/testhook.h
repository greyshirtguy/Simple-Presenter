#pragma once

// The door the scripted tests come in by (tests/ui). It is in the app only when the app
// is built for testing, with -DSIMPLEPRESENTER_TEST_HOOK=ON: a release has none of it.
//
//   --script <file.qml>   after start-up, loads that QML file inside the operator
//                         window's own scope, so that it sees the window's properties,
//                         functions and ids, and calls its run(). To the script,
//                         `testInput` is the mouse, the keyboard and a few ways of
//                         looking at what came of them (see TestInput in testhook.cpp).
//   --frames <dir>        where testInput.grab() saves pictures.
//
// A test run starts with nothing remembered from an earlier one, unless the environment
// has SP_TEST_REMEMBER=1. With SP_TEST_WINDOWS=0 the output and stage windows start
// switched off; SP_TEST_WIDTH and SP_TEST_HEIGHT size the operator window, and
// SP_TEST_SCREEN names the screen for the output.

#include <QVariantMap>

class QCommandLineParser;
class QQmlApplicationEngine;
class QQuickWindow;

namespace testhook {

void addOptions(QCommandLineParser &parser);
bool asked(const QCommandLineParser &parser);
// Before the window is made: gives the script its `testInput`, and changes what the
// window starts with.
void prepare(QQmlApplicationEngine &engine, const QCommandLineParser &parser, QVariantMap &initial);
// Once the window is there: loads the script and runs it.
void start(QQmlApplicationEngine &engine, QQuickWindow *operatorWindow, const QCommandLineParser &parser);

}
