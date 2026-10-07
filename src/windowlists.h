#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

// Keeping a window out of the desktop's lists of windows: the switcher that Alt+Tab
// brings up, the overview, the dock.
//
// The output and stage windows are there to be looked at, by the audience and from the
// stage. They are not windows to switch to, and when they are in the switcher beside
// the operator window, switching back to "Simple Presenter" from another application
// lands on one of them as often as on the window that works the show.
//
// How a window is kept out depends on what the app is drawing through.
//
// Through X11 a window says so itself: it carries a list of states for the window
// manager to honour, and two of them are "leave me out of the taskbar" and "leave me out
// of the pager", which every desktop takes to mean its lists of windows. Qt has no
// setting for those two, so they are put on the window here, with the few X11 calls it
// takes. (Qt can make a window a "tool" window, which desktops also leave out, but that
// says more than is meant: some desktops hide an application's tool windows whenever the
// application is not the one in use, which is the last thing an output should do.)
//
// Through Wayland a window cannot say so. The desktop alone decides what is in its
// lists, and no desktop-neutral way exists to ask. GNOME leaves a window out only if
// something inside GNOME itself marks it, which is what the small GNOME Shell extension
// in packaging/gnome-shell-extension does for these two windows. So through Wayland
// nothing here has any effect, and the extension has to be on.
class WindowLists : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // Has the window left out of the desktop's lists of windows from now on, each time
    // it is shown, where the app can see to that itself.
    Q_INVOKABLE void leaveOut(QQuickWindow *window) const;
};
