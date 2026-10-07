#include "windowlists.h"

#include <QGuiApplication>

#include <cstdlib>
#include <cstring>
#include <xcb/xcb.h>

namespace {

xcb_atom_t atom(xcb_connection_t *connection, const char *name)
{
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, 0, std::strlen(name), name), nullptr);
    const xcb_atom_t found = reply ? reply->atom : XCB_ATOM_NONE;
    std::free(reply);
    return found;
}

// The X11 connection the app is drawing through, or null if it is drawing through
// something else.
xcb_connection_t *x11()
{
    const auto *application = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return application ? application->connection() : nullptr;
}

// Before the window is on the screen: the states it wants are a list it carries, read
// by the window manager when the window is shown. The two are added to whatever is
// there, and Qt then adds its own (fullscreen, on top) to the same list.
void markBeforeShown(QWindow *window)
{
    xcb_connection_t *connection = x11();
    if (!connection)
        return;
    // winId() has the window made, if it has not been yet.
    const xcb_window_t id = xcb_window_t(window->winId());
    const xcb_atom_t state = atom(connection, "_NET_WM_STATE");
    const xcb_atom_t wanted[] = {atom(connection, "_NET_WM_STATE_SKIP_TASKBAR"), atom(connection, "_NET_WM_STATE_SKIP_PAGER")};

    QList<xcb_atom_t> states;
    xcb_get_property_reply_t *reply = xcb_get_property_reply(
        connection, xcb_get_property(connection, 0, id, state, XCB_ATOM_ATOM, 0, 1024), nullptr);
    if (reply && reply->format == 32 && reply->type == XCB_ATOM_ATOM) {
        const auto *held = static_cast<const xcb_atom_t *>(xcb_get_property_value(reply));
        for (uint32_t i = 0; i < reply->value_len; ++i)
            states.append(held[i]);
    }
    std::free(reply);
    for (const xcb_atom_t one : wanted) {
        if (!states.contains(one))
            states.append(one);
    }
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, id, state, XCB_ATOM_ATOM, 32, uint32_t(states.size()), states.constData());
    xcb_flush(connection);
}

// Once the window is on the screen its list of states is the window manager's to keep,
// and a change to it is asked for with a message. Asked as well as marked, because a
// window manager clears the list of a window that has been hidden, in its own time, and
// could do that after the marks for the next showing have been put there.
void askOnceShown(QWindow *window)
{
    xcb_connection_t *connection = x11();
    if (!connection || !window->isVisible() || !window->handle())
        return;
    const xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (!screen)
        return;
    xcb_client_message_event_t message = {};
    message.response_type = XCB_CLIENT_MESSAGE;
    message.format = 32;
    message.window = xcb_window_t(window->winId());
    message.type = atom(connection, "_NET_WM_STATE");
    message.data.data32[0] = 1;   // add
    message.data.data32[1] = atom(connection, "_NET_WM_STATE_SKIP_TASKBAR");
    message.data.data32[2] = atom(connection, "_NET_WM_STATE_SKIP_PAGER");
    message.data.data32[3] = 1;   // asked by an ordinary application
    xcb_send_event(connection, 0, screen->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&message));
    xcb_flush(connection);
}

} // namespace

void WindowLists::leaveOut(QQuickWindow *window) const
{
    if (!window || !x11())
        return;
    // Qt says a window is visible just before it puts it on the screen, which is when
    // the marks have to be there; the asking waits until it has.
    connect(window, &QWindow::visibleChanged, window, [window](bool visible) {
        if (!visible)
            return;
        markBeforeShown(window);
        QMetaObject::invokeMethod(window, [window] { askOnceShown(window); }, Qt::QueuedConnection);
    });
    if (window->isVisible())
        askOnceShown(window);
}
