#include "catalog.h"
#include "thumbnailprovider.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QPointingDevice>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QWheelEvent>
#include <functional>

// Drives the output through the first media file and slide, a ripple to the next slide,
// clearing the slide layer and clearing the media layer, saving frames read back from both windows, then quits.
// Exercises the same readback path that NDI output will need.
static void runSelfTest(QQuickWindow *operatorWindow, QQuickWindow *output, QQuickWindow *stage, const QString &dir)
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
        // Last, the preview panel at its narrowest, where the clear buttons are tightest.
        after(1200, [=] {
            report("1 s after lift,");
            operatorWindow->setProperty("sidePanelWidth", 250);
        });
        after(1500, [=] {
            grab(operatorWindow, "operator-4-narrow-panel");
            QCoreApplication::quit();
        });
    });
}

static int findScreen(const QString &spec)
{
    const auto screens = QGuiApplication::screens();
    bool isIndex = false;
    const int index = spec.toInt(&isIndex);
    if (isIndex)
        return index >= 0 && index < screens.size() ? index : -1;
    for (int i = 0; i < screens.size(); ++i) {
        if (screens.at(i)->name() == spec)
            return i;
    }
    return -1;
}

// Wayland does not let an application place its own windows, so the output and stage
// windows cannot be put back where they were. X11 does, and a Wayland desktop can run
// X11 applications, at some cost in rendering (see the Windows section of the settings
// screen). This must be decided before the application object exists.
static void chooseWindowSystem(int argc, char *argv[])
{
    if (!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") || qEnvironmentVariable("XDG_SESSION_TYPE") != "wayland")
        return;
    // The self-test must not depend on the user's saved session.
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]).startsWith("--selftest"))
            return;
    }
    if (QSettings("SimplePresenter", "SimplePresenter").value("useX11", false).toBool())
        qputenv("QT_QPA_PLATFORM", "xcb");
}

int main(int argc, char *argv[])
{
    chooseWindowSystem(argc, argv);
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("SimplePresenter");
    QGuiApplication::setApplicationName("SimplePresenter");

    const QString defaultRoot =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/SimplePresenter";

    QCommandLineParser parser;
    parser.setApplicationDescription("Shows ProPresenter 7 presentations and media on a second window or screen.");
    parser.addHelpOption();
    const QCommandLineOption listOption("list-screens", "List the available screens and exit.");
    const QCommandLineOption rootOption({"r", "root"}, "Folder holding Libraries/ and Media/ (default: " + defaultRoot + ").", "dir");
    const QCommandLineOption screenOption({"s", "screen"}, "Screen for the fullscreen output (index or name). "
                                          "Default: a non-primary screen if there is one, otherwise a window.", "screen");
    const QCommandLineOption selfTestOption("selftest", "Drive the output through a fixed sequence, save frames as PNGs into <dir>, then quit.", "dir");
    parser.addOptions({listOption, rootOption, screenOption, selfTestOption});
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);
    const auto screens = QGuiApplication::screens();

    if (parser.isSet(listOption)) {
        for (int i = 0; i < screens.size(); ++i) {
            const QScreen *screen = screens.at(i);
            const QRect g = screen->geometry();
            out << i << ": " << screen->name() << "  " << g.width() << "x" << g.height()
                << "+" << g.x() << "+" << g.y() << "  scale " << screen->devicePixelRatio()
                << (screen == QGuiApplication::primaryScreen() ? "  (primary)" : "") << "\n";
        }
        return 0;
    }

    // -1 means the output is an ordinary window.
    int outputScreen = -1;
    if (parser.isSet(screenOption)) {
        outputScreen = findScreen(parser.value(screenOption));
        if (outputScreen < 0) {
            err << "No such screen: " << parser.value(screenOption) << " (try --list-screens)\n";
            return 1;
        }
    } else {
        for (int i = 0; i < screens.size(); ++i) {
            if (screens.at(i) != QGuiApplication::primaryScreen()) {
                outputScreen = i;
                break;
            }
        }
    }

    Catalog catalog(parser.isSet(rootOption) ? parser.value(rootOption) : defaultRoot);

    QQmlApplicationEngine engine;
    engine.addImageProvider("thumbnail", new ThumbnailProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({
        {"catalog", QVariant::fromValue(&catalog)},
        {"outputScreen", outputScreen},
        // The self-test must not read or disturb the user's saved session.
        {"remember", !parser.isSet(selfTestOption)},
    });
    engine.loadFromModule("SimplePresenterApp", "Main");

    auto *operatorWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!operatorWindow)
        return 1;

    if (parser.isSet(selfTestOption)) {
        QQuickWindow *output = nullptr;
        QQuickWindow *stage = nullptr;
        const auto windows = QGuiApplication::allWindows();
        for (QWindow *window : windows) {
            if (window->objectName() == "output")
                output = qobject_cast<QQuickWindow *>(window);
            else if (window->objectName() == "stage")
                stage = qobject_cast<QQuickWindow *>(window);
        }
        if (!output || !stage)
            return 1;
        runSelfTest(operatorWindow, output, stage, parser.value(selfTestOption));
    }

    return app.exec();
}
