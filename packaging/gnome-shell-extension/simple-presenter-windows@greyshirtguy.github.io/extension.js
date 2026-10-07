// Leaves Simple Presenter's output and stage windows out of GNOME's lists of windows:
// the switcher that Alt+Tab brings up, the overview, the dock.
//
// Why this is an extension. Those two windows are there to be looked at, not to be
// switched to. Run through X11, the app marks them so itself (see src/windowlists.h).
// Run through Wayland it cannot: there, GNOME alone decides what is in its lists, and
// leaves a window out only if something inside GNOME marks it. This is that something,
// and it is all this does.
//
// How it knows the windows. By the application they belong to, which GNOME knows by the
// name below, and by their titles, which the app gives them and never changes (see
// qml/Output.qml and qml/Stage.qml). A window is not always known by either when it is
// first made, so each is looked at again when it is.

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const APPLICATION = 'SimplePresenter';
const TITLES = ['Output', 'Stage'];

export default class SimplePresenterWindows extends Extension {
    enable() {
        // What is known of each window there is: the signals listened to on it, and
        // whether it is one that this has had left out
        this._windows = new Map();
        this._created = global.display.connect('window-created', (_display, window) => this._watch(window));
        for (const window of global.display.list_all_windows())
            this._watch(window);
    }

    disable() {
        global.display.disconnect(this._created);
        this._created = null;
        // Everything is put back as it was: only the windows this had left out are
        // shown again, since others may have been left out by something else.
        for (const [window, known] of this._windows) {
            known.signals.forEach(signal => window.disconnect(signal));
            if (known.leftOut)
                window.show_in_window_list();
        }
        this._windows = null;
    }

    _watch(window) {
        // (A GNOME too old to have the means is left alone.)
        if (this._windows.has(window) || !window.hide_from_window_list)
            return;
        const known = {signals: [], leftOut: false, titled: false};
        this._windows.set(window, known);
        known.signals.push(window.connect('notify::wm-class', () => this._look(window)));
        known.signals.push(window.connect('unmanaging', () => this._forget(window)));
        this._look(window);
    }

    _forget(window) {
        const known = this._windows?.get(window);
        if (!known)
            return;
        known.signals.forEach(signal => window.disconnect(signal));
        this._windows.delete(window);
    }

    _look(window) {
        const known = this._windows.get(window);
        const ours = window.get_wm_class() === APPLICATION;
        // Only the app's own windows have their titles followed: other applications
        // change theirs all day, and none of that is anything to do with this.
        if (ours && !known.titled) {
            known.titled = true;
            known.signals.push(window.connect('notify::title', () => this._look(window)));
        }
        const leaveOut = ours && TITLES.includes(window.get_title());
        if (leaveOut === known.leftOut)
            return;
        known.leftOut = leaveOut;
        if (leaveOut)
            window.hide_from_window_list();
        else
            window.show_in_window_list();
    }
}
