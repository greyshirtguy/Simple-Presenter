// SimplePresenter: where to start reading.
//
// The app is a window, some screens and a folder.
//
//   The folder is a workspace: libraries of ProPresenter 7 presentations, media files,
//   three playlists files (of presentations, of media and of audio), the timers, the
//   props and the stage layouts, laid out exactly
//   as ProPresenter lays out its own folder (src/catalog.h; src/timers.h, src/props.h
//   and src/stagelayouts.h for the last three). Nothing is imported or converted; the
//   app works on the files as they are, and writes its changes back into them.
//
//   The operator window (qml/Main.qml) is where the show is run from: pick a
//   presentation, click a slide, and it is on the output.
//
//   An audience screen (qml/OutputScene.qml) is what the audience sees: a media layer
//   with a slide layer over it, and over both whichever props are on. A stage screen
//   (qml/StageScene.qml) is what the people on stage see: the words of this slide and
//   the next, plainly, or laid out as one of the workspace's stage layouts has them.
//   A workspace has as many of each as it is given (src/screens.h), and each is sent
//   out through a window of its own (qml/Output.qml, qml/Stage.qml), a display, the
//   network as an NDI source (src/ndi.h), or an output of a Blackmagic DeckLink card
//   (src/decklink.h). Every screen has a scene of its own, drawn by itself; a video is
//   played once for all of them. A picture of a screen, which the operator's preview is
//   and a shape filled with a Screen Preview has, is the screen drawn again, small, from
//   the same things it is told (qml/ScreenPicture.qml). Media is drawn as its Media
//   Inspector (qml/MediaInspector.qml) sets it, fitted, turned, cropped and with its
//   effects, wherever it is shown (qml/MediaLook.qml); what that is is read from and
//   written to ProPresenter's own fields (src/mediasettings.h).
//
//   And ProPresenter's own network API can be switched on (src/httpapiserver.h), so that
//   what follows and operates ProPresenter over the network (Bitfocus Companion) can
//   follow and operate this: the operator window answers it (qml/HttpApiControl.qml).
//
// The two halves of the code.
//
//   src/ is C++, and does the things QML cannot: reading and writing ProPresenter's
//   documents (which are Protocol Buffers), reading and writing the RTF their text is
//   kept in, laying text out and drawing it with an outline, taking a frame from a
//   video for a thumbnail.
//
//   qml/ is QML, and is everything on screen: how the windows are laid out, and what a
//   click or a key does in them.
//
//   What passes between them is plain data: lists and maps. A presentation crosses over
//   as a list of slides, each slide a map of everything needed to draw it. QML never
//   sees a file format.
//
//   Between the two is the show itself: what is live, and the rules by which it changes
//   (src/showstate.h). It is plain values and functions, with nothing of the windows in
//   it, so that the rules can be tested by themselves (tests/unit). The operator window
//   asks for a change through Show (src/show.h) and is told what the output is then to
//   be handed. Anything that decides what the audience sees belongs there.
//
// A slide's way from the file to the screen.
//
//   1. Catalog::open() has ProDocument::load() (src/prodocument.h) parse the .pro file
//      and walk the chosen arrangement into a flat list of slides.
//   2. proconvert::toSlideMap() (src/proconvert.h) turns each slide into a map: its
//      size, its elements, and each element's box, fill, stroke, shadows and text. The
//      text is parsed from RTF (src/rtf.h) into runs of styled text (src/richtext.h).
//   3. In QML, goLive() in Main.qml asks Show for the slide to go live; Show says what
//      is to be shown, and Main.qml hands that map to the output window, whose slide
//      layer (qml/TransitionLayer.qml) gives it to a Slide (qml/Slide.qml), which makes
//      a SlideElement for each element, which draws its text with a StrokedText
//      (src/strokedtext.h).
//   4. If a transition is chosen, the layer blends the old slide into the new with a
//      fragment shader (shaders/; qml/TransitionCatalogue.qml lists them, with what
//      can be adjusted about each).
//
//   A slide's cue may do more than show the slide. It may trigger media, which goes to
//   the media layer as a background that stays or as a foreground that plays once (see
//   workspace::MediaBehaviour in src/workspacefiles.h); and it may work a timer.
//
// What is heard.
//
//   Besides the layers that are seen there is the audio layer: the pieces of the audio
//   bin (qml/AudioPanel.qml; src/audiocue.h for what ProPresenter's file says of one).
//   What is to be playing is the show's to say, as what is on the output is: music one
//   piece at a time, sound effects over it (src/showstate.h). It is played by
//   AudioLayer (src/audiolayer.h), which has the fades and the device sound goes to;
//   and what is played, a video's sound with it, is handed to the meter in the preview
//   as it goes (src/audiometer.h).
//
//   What is not settled when a file is read is text that changes while it is on show: a
//   timer's time, which SlideElement asks Timers for (src/timers.h); the words of the
//   slide that is live, or its notes, which it asks Show for (src/show.h); and the time
//   of day or the time a video has left, which it asks Clocks for (src/clocks.h), a
//   clock that goes only while some box shows it. A prop and a stage
//   layout are slides too, in files of their own. A prop is laid over the output and
//   stays until it is turned off; a stage layout is made of boxes linked to what is
//   live. Both go the same way to the screen as any slide, from step 2 on, and both
//   are edited by the editor that edits a presentation's slides
//   (src/presentationeditor.h).
//
//   Over the props is one more layer, the messages': things to say over everything
//   else, made and shown from their tab of the show controls (qml/MessagesPanel.qml;
//   src/messagefile.h for ProPresenter's file of them). Which are shown is the show's
//   to say, like everything that is seen (src/showstate.h). A message is drawn as a
//   slide too: the slide of a theme, with the message's words in its first text box
//   (Messages::slideOf, src/messages.h); and its words, which can have the time of day
//   or a timer in them, are worked out once for all the screens and asked for by that
//   box as they change. What the people on the stage are told, the stage message, is
//   the same idea and simpler: a text box linked to it asks Show what it is.
//
// Choices made for modest hardware. The app is developed on a 2017 laptop with
// integrated graphics, and is meant to run a show on one.
//
//   - Text is drawn once, on the CPU, into a texture, when a slide is shown; after that
//     a slide costs the GPU one textured rectangle for each element.
//   - A transition is one shader over two textures. When none is running, nothing is
//     blended: the output is drawn directly (see TransitionLayer.qml).
//   - Video is decoded by the graphics hardware when a driver for it is there (Qt
//     Multimedia, through FFmpeg). The preview does not decode it a second time: it
//     borrows frames from the output (src/framerelay.h). Where a driver's decoder is
//     not to be trusted, decoding is kept to the processor: by a setting
//     (src/decodingchoice.h), and without being asked on a computer where trying the
//     decoder is known to do harm (src/decodingguard.h).
//   - Media files are never read on the thread that runs the windows. Thumbnails are
//     made on worker threads, once, and kept on disk (src/thumbnailprovider.h); a
//     still put on the output is read on another thread and brought in when it is
//     ready (qml/MediaContent.qml).
//   - Nothing is drawn when nothing changes. With a still slide up, the app does
//     nothing but answer, once a second, the question of whether it is still answering
//     (src/sessionlog.h); a running timer has what shows it drawn again once a second,
//     and the transport is drawn with the preview's frames, not by itself
//     (qml/Transport.qml).
//
// What happened last. Each run keeps a log (src/sessionlog.h) of what it is running on
// and what it did, a line for each thing done, for when something goes wrong and there
// is a question to ask. It is written as things happen and never as they are drawn.
//
// Never losing what is in a file. ProPresenter's files hold far more than this app
// understands. Every change is made the same way: parse the whole file, alter only the
// fields the change is about, and write the whole thing back, in one step, to a
// temporary file that then replaces the original. Whatever the app does not know about
// goes back exactly as it came.

#include "benchmark.h"
#include "catalog.h"
#include "decodingchoice.h"
#include "decodingguard.h"
#include "iconprovider.h"
#include "messagetext.h"
#include "selftest.h"
#include "sessionlog.h"
#include "thumbnailprovider.h"
#ifdef SIMPLEPRESENTER_TEST_HOOK
#include "testhook.h"
#endif

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
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

// Whether video is to be decoded by the processor alone, with the graphics chip's own
// decoders left untried: because it was asked for, or because this is a computer on
// which trying them is known to do harm (decodingchoice.h says who has the say, and
// decodingguard.h which computers those are). Qt Multimedia's FFmpeg backend reads its
// variable once, when it first decodes anything, so this is settled here, before
// anything is played; a change to the setting is for the next start.
static void chooseVideoDecoding(int argc, char *argv[])
{
    decodingchoice::Asked asked;
    bool remembers = true;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        asked.softwareFlag = asked.softwareFlag || argument == "--software-decoding";
        asked.hardwareFlag = asked.hardwareFlag || argument == "--hardware-decoding";
        remembers = remembers && !argument.startsWith("--selftest") && !argument.startsWith("--benchmark");
    }
    // The self-test and the benchmark must not depend on the user's saved session: as
    // the window reads no settings in those runs, so this does not. (The guard is not of
    // the session but of the computer, and holds for them too.)
    if (remembers)
        asked.mode = decodingchoice::modeOf(QSettings("SimplePresenter", "SimplePresenter").value("videoDecoding").toString());
    asked.environmentSet = qEnvironmentVariableIsSet(decodingchoice::kVariable);
    asked.environment = qgetenv(decodingchoice::kVariable);

    // What this computer is, for the guard
    QString root;
    std::function<int(const QString &)> chipsetOf = decodingguard::nouveauChipset;
#ifdef SIMPLEPRESENTER_TEST_HOOK
    // (A scripted test stands a made-up system in for the real one: a folder laid out
    // as /sys, /dev and /lib/firmware are, and the number nouveau would give its chip.)
    if (qEnvironmentVariableIsSet("SP_TEST_SYSTEM_ROOT")) {
        root = qEnvironmentVariable("SP_TEST_SYSTEM_ROOT");
        const int chipset = qEnvironmentVariable("SP_TEST_NOUVEAU_CHIPSET").toInt(nullptr, 16);
        chipsetOf = [chipset](const QString &) { return chipset; };
    }
#endif
    const decodingguard::Found found = decodingguard::look(root, QSysInfo::kernelVersion(), chipsetOf);
    const decodingguard::Verdict verdict = decodingguard::judge(found);
    asked.guarded = verdict.guarded;
    asked.guardReason = verdict.reason;

    const decodingchoice::Choice choice = decodingchoice::choose(asked);
    if (!choice.set.isNull())
        qputenv(decodingchoice::kVariable, choice.set);
    decodingchoice::setSession(choice);
    SessionLog::write("video", decodingguard::described(found));
    SessionLog::write("video", decodingchoice::logLine(choice));
}

int main(int argc, char *argv[])
{
    chooseWindowSystem(argc, argv);
    // The log is opened before anything else is tried, so that whatever goes wrong in
    // starting up, down to there being no way to put a window on the screen, is in it.
    SessionLog::start(argc, argv, QStringLiteral(APP_VERSION));
    // (After the log has said what the environment was, which this may add to.)
    chooseVideoDecoding(argc, argv);
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("SimplePresenter");
    QGuiApplication::setApplicationName("SimplePresenter");
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    SessionLog::describeDisplay();

    // Workspaces are folders side by side in here; each holds everything for one setup.
    const QString workspacesDirectory =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/SimplePresenter/WorkSpaces";

    QCommandLineParser parser;
    parser.setApplicationDescription("Shows ProPresenter 7 presentations and media on a second window or screen.");
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption listOption("list-screens", "List the available screens and exit.");
    const QCommandLineOption workspaceOption({"w", "workspace"}, "Workspace folder to open, holding Libraries/, Media/ and Playlists/. "
                                             "Default: the one used last, else the first in " + workspacesDirectory + ".", "dir");
    const QCommandLineOption screenOption({"s", "screen"}, "Screen for the fullscreen output (index or name). "
                                          "Default: a non-primary screen if there is one, otherwise a window.", "screen");
    const QCommandLineOption selfTestOption("selftest", "Drive the output through a fixed sequence, save frames as PNGs into <dir>, then quit.", "dir");
    const QCommandLineOption benchmarkOption("benchmark", "Time a fixed run on a workspace of the benchmark's own making, write the times to <file>, then quit.", "file");
    // (Both acted on already, before the application was made: see chooseVideoDecoding.)
    const QCommandLineOption softwareDecodingOption("software-decoding", "Decode video with the processor only, for this run: no hardware video decoder "
                                                    "is tried. The same can be kept in the settings, under Video.");
    const QCommandLineOption hardwareDecodingOption("hardware-decoding", "Have the graphics chip's video decoder tried, for this run, on a computer where "
                                                    "the app would leave it untried. The same can be kept in the settings, under Video.");
    parser.addOptions({listOption, workspaceOption, screenOption, selfTestOption, benchmarkOption, softwareDecodingOption, hardwareDecodingOption});
#ifdef SIMPLEPRESENTER_TEST_HOOK
    testhook::addOptions(parser);
#endif
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
            SessionLog::write("PROBLEM", QStringLiteral("No such screen as the one asked for, \"%1\". The app stops here.").arg(parser.value(screenOption)));
            SessionLog::finish();
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
    QString chosenAs = QStringLiteral("the one asked for on the command line");
    if (workspace.isEmpty() && !parser.isSet(selfTestOption)) {
        const QString last = QSettings("SimplePresenter", "SimplePresenter").value("workspace").toString();
        if (!last.isEmpty() && QDir(last).exists()) {
            workspace = last;
            chosenAs = QStringLiteral("the one used last");
        }
    }
    if (workspace.isEmpty()) {
        const QStringList existing = QDir(workspacesDirectory).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        workspace = workspacesDirectory + "/" + (existing.isEmpty() ? QStringLiteral("Default") : existing.first());
        chosenAs = existing.isEmpty() ? QStringLiteral("a new one, there being none") : QStringLiteral("the first there is");
    }
    // The benchmark works on a workspace of its own, made afresh and thrown away after.
    QTemporaryDir benchmarkFolder;
    const bool benchmarking = parser.isSet(benchmarkOption);
    if (benchmarking) {
        const QString error = Benchmark::makeWorkspace(benchmarkFolder.path());
        if (!error.isEmpty()) {
            err << error << "\n";
            SessionLog::finish();
            return 1;
        }
        workspace = benchmarkFolder.path();
        chosenAs = QStringLiteral("the benchmark's own");
    }
    SessionLog::write("workspace", QStringLiteral("opening %1 (%2)").arg(workspace, chosenAs));
    Catalog catalog(workspace);

    QQmlApplicationEngine engine;
    engine.addImageProvider("thumbnail", new ThumbnailProvider);
    engine.addImageProvider("icon", new IconProvider);
    engine.addImageProvider("token", new TokenPictures);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    QVariantMap initial {
        {"catalog", QVariant::fromValue(&catalog)},
        {"outputScreen", outputScreen},
        // The self-test must not read or disturb the user's saved session,
        {"remember", !parser.isSet(selfTestOption) && !benchmarking},
        // and its pictures must not depend on when it is run or how long it takes.
        {"clocksHeld", parser.isSet(selfTestOption)},
    };
#ifdef SIMPLEPRESENTER_TEST_HOOK
    testhook::prepare(engine, parser, initial);
#endif
    engine.setInitialProperties(initial);
    engine.loadFromModule("SimplePresenterApp", "Main");

    auto *operatorWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!operatorWindow) {
        SessionLog::write("PROBLEM", QStringLiteral("The app's window could not be made: see what Qt says above. The app stops here."));
        SessionLog::finish();
        return 1;
    }

#ifdef SIMPLEPRESENTER_TEST_HOOK
    testhook::start(engine, operatorWindow, parser);
#endif

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
        if (!output || !stage) {
            SessionLog::finish();
            return 1;
        }
        runSelfTest(operatorWindow, output, stage, parser.value(selfTestOption));
    }

    if (benchmarking) {
        // The run is made in the operator window's own scope, so that it can work the
        // app by the functions a click does: see qml/Benchmark.qml.
        auto *bench = new Benchmark(parser.value(benchmarkOption), &app);
        bench->record("startup: to the windows being made", Benchmark::sinceStart(), "ms");
        QQmlComponent component(&engine, "SimplePresenterApp", "Benchmark");
        QObject *run = component.createWithInitialProperties({{"bench", QVariant::fromValue(bench)}}, qmlContext(operatorWindow));
        if (!run) {
            err << component.errorString() << "\n";
            SessionLog::finish();
            return 1;
        }
        run->setParent(operatorWindow);
    }

    SessionLog::watchForStalls();
    const int result = app.exec();
    SessionLog::finish();
    return result;
}
