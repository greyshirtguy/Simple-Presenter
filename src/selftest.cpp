#include "selftest.h"

#include <QCoreApplication>
#include <QPointingDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>
#include <QWheelEvent>

#include <functional>

// Drives the output through the first media file and slide, a ripple to the next slide,
// clearing the slide layer and clearing the media layer, then the editor, saving frames
// read back from the windows, then quits. Exercises the same readback path that NDI
// output will need.
void runSelfTest(QQuickWindow *operatorWindow, QQuickWindow *output, QQuickWindow *stage, const QString &dir)
{
    const auto grab = [dir](QQuickWindow *window, const QString &name) {
        const QString path = dir + "/" + name + ".png";
        if (!window->grabWindow().save(path))
            qWarning("Could not write %s", qPrintable(path));
    };
    const auto at = [operatorWindow](int ms, std::function<void()> step) {
        QTimer::singleShot(ms, operatorWindow, step);
    };
    at(1000, [=] {
        QMetaObject::invokeMethod(operatorWindow, "showFirstMedia");
        QMetaObject::invokeMethod(operatorWindow, "goLive", Q_ARG(QVariant, 0));
    });
    at(2500, [=] {
        grab(operatorWindow, "operator-1");
        grab(output, "output-1-resting");
        operatorWindow->setProperty("transitionIndex", 2);
        operatorWindow->setProperty("transitionDuration", 1.4);
        QMetaObject::invokeMethod(operatorWindow, "step", Q_ARG(QVariant, 1));
    });
    at(3300, [=] { grab(output, "output-2-mid-transition"); });
    at(4800, [=] {
        grab(output, "output-3-settled");
        grab(operatorWindow, "operator-2");
        grab(stage, "stage");
        operatorWindow->setProperty("settingsOpen", true);
    });
    at(5050, [=] {
        grab(operatorWindow, "operator-3-settings");
        operatorWindow->setProperty("settingsOpen", false);
        operatorWindow->setProperty("transitionIndex", 0);
        QMetaObject::invokeMethod(operatorWindow, "clearSlide");
    });
    at(5300, [=] {
        grab(output, "output-4-slide-cleared");
        QMetaObject::invokeMethod(operatorWindow, "clearMedia");
    });
    at(5800, [=] { grab(output, "output-5-all-cleared"); });

    // A trackpad swipe over the slide grid, as Wayland delivers one: a begin, a run of
    // pixel deltas and an end. Momentum means the grid is still moving after the end.
    auto *grid = operatorWindow->findChild<QQuickItem *>("slideGrid");
    static const QPointingDevice trackpad("selftest trackpad", 99, QInputDevice::DeviceType::TouchPad,
                                          QPointingDevice::PointerType::Finger,
                                          QInputDevice::Capability::Position | QInputDevice::Capability::Scroll
                                              | QInputDevice::Capability::PixelScroll, 1, 3);
    const auto scroll = [=](Qt::ScrollPhase phase, int pixels) {
        const QPointF at = grid->mapToScene(QPointF(grid->width() / 2, grid->height() / 2));
        QWheelEvent event(at, operatorWindow->mapToGlobal(at), QPoint(0, pixels), QPoint(0, pixels * 4), Qt::NoButton,
                          Qt::NoModifier, phase, false, Qt::MouseEventSynthesizedBySystem, &trackpad);
        QCoreApplication::sendEvent(operatorWindow, &event);
    };
    const auto report = [=](const char *when) {
        QTextStream(stdout) << "scroll test: " << when << " contentY = " << grid->property("contentY").toDouble() << "\n";
    };
    // Starts a second after the last frame grab, then runs on precise timers: the long
    // single-shot timers above are coarse, which would bunch the swipe's events together.
    at(7000, [=] {
        const auto after = [operatorWindow](int ms, std::function<void()> step) {
            QTimer::singleShot(ms, Qt::PreciseTimer, operatorWindow, step);
        };
        report("before swipe,");
        scroll(Qt::ScrollBegin, 0);
        for (int i = 0; i < 10; ++i)
            after(20 + i * 16, [=] { scroll(Qt::ScrollUpdate, -20); });
        after(190, [=] { scroll(Qt::ScrollEnd, 0); report("at finger lift,"); });
        after(350, [=] { report("160 ms after lift,"); });
        // Then the preview panel at its narrowest, where the clear buttons are tightest.
        // Where the coasting stopped depends on how the frames happened to fall, so the
        // grid is put at a fixed place for the frame that follows: a run is then the
        // same every time, and two runs can be compared.
        after(1200, [=] {
            report("1 s after lift,");
            grid->setProperty("contentY", 280);
            operatorWindow->setProperty("sidePanelWidth", 250);
        });
        // Last, a playlist, if the folder has any.
        after(1500, [=] {
            grab(operatorWindow, "operator-4-narrow-panel");
            operatorWindow->setProperty("sidePanelWidth", 360);
            QMetaObject::invokeMethod(operatorWindow, "openBusiestPlaylist");
        });
        // Then the editor: up, an element picked, its text being edited, and down
        // again. Nothing is changed, so nothing is written.
        const auto editor = [operatorWindow](int step) {
            QMetaObject::invokeMethod(operatorWindow, "selfTestEditor", Q_ARG(QVariant, step));
        };
        after(1900, [=] {
            grab(operatorWindow, "operator-5-playlist");
            editor(0);
        });
        after(2500, [=] {
            grab(operatorWindow, "operator-6-editor");
            editor(1);
        });
        after(2900, [=] {
            grab(operatorWindow, "operator-7-editor-picked");
            editor(2);
        });
        after(3400, [=] {
            grab(operatorWindow, "operator-8-editor-text");
            editor(3);
        });
        after(3700, [=] { QCoreApplication::quit(); });
    });
}
