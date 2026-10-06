#include "catalog.h"
#include "selftest.h"
#include "thumbnailprovider.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

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

    // Workspaces are folders side by side in here; each holds everything for one setup.
    const QString workspacesDirectory =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/SimplePresenter/WorkSpaces";

    QCommandLineParser parser;
    parser.setApplicationDescription("Shows ProPresenter 7 presentations and media on a second window or screen.");
    parser.addHelpOption();
    const QCommandLineOption listOption("list-screens", "List the available screens and exit.");
    const QCommandLineOption workspaceOption({"w", "workspace"}, "Workspace folder to open, holding Libraries/, Media/ and Playlists/. "
                                             "Default: the one used last, else the first in " + workspacesDirectory + ".", "dir");
    const QCommandLineOption screenOption({"s", "screen"}, "Screen for the fullscreen output (index or name). "
                                          "Default: a non-primary screen if there is one, otherwise a window.", "screen");
    const QCommandLineOption selfTestOption("selftest", "Drive the output through a fixed sequence, save frames as PNGs into <dir>, then quit.", "dir");
    parser.addOptions({listOption, workspaceOption, screenOption, selfTestOption});
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

    // Which workspace: the one asked for, else the one used last if it is still there,
    // else the first there is, else a new one. The self-test ignores the one used last,
    // so that it does not depend on the user's saved session.
    QString workspace = parser.value(workspaceOption);
    if (workspace.isEmpty() && !parser.isSet(selfTestOption)) {
        const QString last = QSettings("SimplePresenter", "SimplePresenter").value("workspace").toString();
        if (!last.isEmpty() && QDir(last).exists())
            workspace = last;
    }
    if (workspace.isEmpty()) {
        const QStringList existing = QDir(workspacesDirectory).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        workspace = workspacesDirectory + "/" + (existing.isEmpty() ? QStringLiteral("Default") : existing.first());
    }
    Catalog catalog(workspace);

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
