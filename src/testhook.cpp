#include "testhook.h"

#include "richtext.h"
#include "textlayout.h"

#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

// (Qt's own way of sending a key by way of an application's shortcuts, which it keeps
// for its tests.)
Q_GUI_EXPORT bool qt_sendShortcutOverrideEvent(QObject *o, ulong timestamp, int k, Qt::KeyboardModifiers mods, const QString &text, bool autorep, ushort count);

namespace {

// What a test script works the windows with: the mouse, the keyboard, files dragged in,
// and ways of looking at what the windows then show.
class TestInput : public QObject
{
    Q_OBJECT
public:
    QQuickWindow *window = nullptr;
    QString directory;

    // The first audience screen's window and the first stage screen's, as they are now:
    // looked for each time, since a screen's window is made again when the screen is
    // set to something else.
    static QQuickWindow *named(const char *name)
    {
        const auto all = QGuiApplication::allWindows();
        for (QWindow *candidate : all) {
            if (candidate->objectName() == QLatin1String(name))
                return qobject_cast<QQuickWindow *>(candidate);
        }
        return nullptr;
    }

    // type: 0 press, 1 move with the button down, 2 release, 3 double click, 4 move with no button
    Q_INVOKABLE void mouse(int type, qreal x, qreal y, int modifiers = 0, int button = Qt::LeftButton)
    {
        const QPointF at(x, y);
        const auto send = [&](QEvent::Type kind, Qt::MouseButtons held) {
            QMouseEvent event(kind, at, window->mapToGlobal(at), kind == QEvent::MouseMove ? Qt::NoButton : Qt::MouseButton(button),
                              held, Qt::KeyboardModifiers(modifiers));
            QCoreApplication::sendEvent(window, &event);
        };
        if (type == 0)
            send(QEvent::MouseButtonPress, Qt::MouseButton(button));
        else if (type == 1)
            send(QEvent::MouseMove, Qt::MouseButton(button));
        else if (type == 2)
            send(QEvent::MouseButtonRelease, Qt::NoButton);
        else if (type == 4)
            send(QEvent::MouseMove, Qt::NoButton);
        else {
            send(QEvent::MouseButtonPress, Qt::MouseButton(button));
            send(QEvent::MouseButtonRelease, Qt::NoButton);
            send(QEvent::MouseButtonPress, Qt::MouseButton(button));
            send(QEvent::MouseButtonDblClick, Qt::MouseButton(button));
            send(QEvent::MouseButtonRelease, Qt::NoButton);
        }
    }
    // A key as the desktop would deliver it, by way of the app's shortcuts
    Q_INVOKABLE bool shortcut(int key, int modifiers = 0)
    {
        return qt_sendShortcutOverrideEvent(window, 0, key, Qt::KeyboardModifiers(modifiers), QString(), false, 1);
    }
    Q_INVOKABLE void key(int key, int modifiers = 0, const QString &text = QString())
    {
        QKeyEvent press(QEvent::KeyPress, key, Qt::KeyboardModifiers(modifiers), text);
        QCoreApplication::sendEvent(window, &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::KeyboardModifiers(modifiers), text);
        QCoreApplication::sendEvent(window, &release);
    }
    // One half of a key press: the key going down, or coming up. `repeat` marks it as one
    // of the pairs a held key sends over and over; `scanCode` is where the key is on the
    // keyboard, as the desktop numbers it.
    Q_INVOKABLE void keyDown(int key, bool repeat = false, const QString &text = QString(), int modifiers = 0, int scanCode = 0)
    {
        QKeyEvent press(QEvent::KeyPress, key, Qt::KeyboardModifiers(modifiers), scanCode, 0, 0, text, repeat);
        QCoreApplication::sendEvent(window, &press);
    }
    Q_INVOKABLE void keyUp(int key, bool repeat = false, const QString &text = QString(), int modifiers = 0, int scanCode = 0)
    {
        QKeyEvent release(QEvent::KeyRelease, key, Qt::KeyboardModifiers(modifiers), scanCode, 0, 0, text, repeat);
        QCoreApplication::sendEvent(window, &release);
    }
    Q_INVOKABLE void type(const QString &text)
    {
        for (const QChar c : text)
            key(c == u'\n' ? Qt::Key_Return : 0, 0, c == u'\n' ? QString() : QString(c));
    }
    // The window behaves as if it had the keyboard, whatever other window was shown since.
    Q_INVOKABLE void focus()
    {
        QFocusEvent focusIn(QEvent::FocusIn);
        QCoreApplication::sendEvent(window, &focusIn);
    }
    Q_INVOKABLE void grab(const QString &name) { window->grabWindow().save(directory + "/" + name + ".png"); }
    Q_INVOKABLE void grabOutput(const QString &name)
    {
        if (QQuickWindow *output = named("output"))
            output->grabWindow().save(directory + "/" + name + ".png");
    }
    Q_INVOKABLE void grabStage(const QString &name)
    {
        if (QQuickWindow *stage = named("stage"))
            stage->grabWindow().save(directory + "/" + name + ".png");
    }
    // The colour of a pixel of the output or the stage window, as "#rrggbb"; x and y from 0 to 1
    // A pixel of the operator window, at a point of it, as "#rrggbb"
    Q_INVOKABLE QString windowPixel(qreal x, qreal y)
    {
        const QImage image = window->grabWindow();
        const qreal ratio = image.width() / qreal(window->width());
        return image.pixelColor(qBound(0, int(x * ratio), image.width() - 1), qBound(0, int(y * ratio), image.height() - 1)).name();
    }
    Q_INVOKABLE QString pixel(bool ofStage, qreal x, qreal y)
    {
        QQuickWindow *window = named(ofStage ? "stage" : "output");
        if (!window)
            return QString();
        const QImage image = window->grabWindow();
        return image.pixelColor(qBound(0, int(x * image.width()), image.width() - 1),
                                qBound(0, int(y * image.height()), image.height() - 1)).name();
    }
    // How much of a part of the output or the stage window is not black, from 0 to 1
    Q_INVOKABLE double lit(bool ofStage, qreal x, qreal y, qreal width, qreal height)
    {
        QQuickWindow *window = named(ofStage ? "stage" : "output");
        if (!window)
            return -1;
        const QImage image = window->grabWindow().convertToFormat(QImage::Format_RGB32);
        const QRect part = QRect(int(x * image.width()), int(y * image.height()), int(width * image.width()),
                                 int(height * image.height())).intersected(image.rect());
        qint64 lit = 0;
        for (int row = part.top(); row <= part.bottom(); ++row) {
            const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(row));
            for (int column = part.left(); column <= part.right(); ++column)
                lit += qGray(line[column]) > 40 ? 1 : 0;
        }
        return part.isEmpty() ? 0 : double(lit) / (double(part.width()) * part.height());
    }
    // Keeps a copy of a file as it is now, beside the pictures
    Q_INVOKABLE bool snapshot(const QString &path, const QString &name)
    {
        const QString target = directory + "/" + name;
        QFile::remove(target);
        return QFile::copy(path, target);
    }
    // What a text is scaled by to suit a box, and how tall it stands laid out in one of that width
    Q_INVOKABLE double fitScale(const QVariant &richText, double width, double height, int fit)
    {
        return fittingScale(richText.value<RichText>(), QSizeF(width, height), fit);
    }
    Q_INVOKABLE double laidOutHeight(const QVariant &richText, double width) { return layoutText(richText.value<RichText>(), width).height; }
    Q_INVOKABLE QString readText(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }
    Q_INVOKABLE void say(const QString &line) { QTextStream(stdout) << line << "\n"; }
    Q_INVOKABLE qint64 now() { return QDateTime::currentMSecsSinceEpoch(); }
    // CPU time this process has used so far, in milliseconds
    Q_INVOKABLE double cpu()
    {
        QFile stat("/proc/self/stat");
        if (!stat.open(QIODevice::ReadOnly))
            return 0;
        const QList<QByteArray> fields = stat.readAll().split(' ');
        return (fields.value(13).toDouble() + fields.value(14).toDouble()) * 10;
    }
    Q_INVOKABLE bool exists(const QString &path) { return QFile::exists(path); }
    Q_INVOKABLE QString fileHash(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1).toHex());
    }
    Q_INVOKABLE QString plain(const QVariant &richText) const { return richText.value<RichText>().plainText(); }
    Q_INVOKABLE QVariantList runsOf(const QVariant &richText) const
    {
        QVariantList runs;
        const RichText text = richText.value<RichText>();
        for (const TextParagraph &paragraph : text.paragraphs) {
            for (const TextRun &run : paragraph.runs) {
                QVariantMap map = run.format();
                map.insert("text", run.text);
                map.insert("color", run.fill.name());
                runs.append(map);
            }
        }
        return runs;
    }
    Q_INVOKABLE void quit() { QCoreApplication::quit(); }

    // A drag of files in from another application, sent to the operator window as the
    // desktop would send it. type 0: over the point (entering the window if it had not);
    // 1: dropped there; 2: taken away again. Says what the window answered: "copy",
    // "move" or "link" (what it asks the other application to do with the files), or
    // "refused". `modifiers` are the keys held (Shift asks for a move).
    Q_INVOKABLE QString fileDrag(int type, qreal x, qreal y, const QStringList &paths = {}, int modifiers = 0)
    {
        static QMimeData *data = nullptr;
        const Qt::DropActions offered = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
        const Qt::KeyboardModifiers held(modifiers);
        const auto answer = [](const QDropEvent &event) {
            if (!event.isAccepted())
                return QStringLiteral("refused");
            return event.dropAction() == Qt::CopyAction ? QStringLiteral("copy") : event.dropAction() == Qt::MoveAction ? QStringLiteral("move")
                 : event.dropAction() == Qt::LinkAction ? QStringLiteral("link") : QStringLiteral("refused");
        };
        if (type == 2) {
            QDragLeaveEvent leave;
            QCoreApplication::sendEvent(window, &leave);
            delete data;
            data = nullptr;
            return QStringLiteral("left");
        }
        if (!data) {
            data = new QMimeData;
            QList<QUrl> urls;
            for (const QString &path : paths)
                urls << QUrl::fromLocalFile(path);
            data->setUrls(urls);
            QDragEnterEvent enter(QPoint(qRound(x), qRound(y)), offered, data, Qt::LeftButton, held);
            QCoreApplication::sendEvent(window, &enter);
        }
        if (type == 0) {
            QDragMoveEvent move(QPoint(qRound(x), qRound(y)), offered, data, Qt::LeftButton, held);
            QCoreApplication::sendEvent(window, &move);
            return answer(move);
        }
        QDropEvent drop(QPointF(x, y), offered, data, Qt::LeftButton, held);
        QCoreApplication::sendEvent(window, &drop);
        const QString result = answer(drop);
        delete data;
        data = nullptr;
        return result;
    }
};

TestInput *input()
{
    static TestInput *made = new TestInput;
    return made;
}

const QString scriptName = QStringLiteral("script");
const QString framesName = QStringLiteral("frames");

}

namespace testhook {

void addOptions(QCommandLineParser &parser)
{
    parser.addOptions({QCommandLineOption(scriptName, "For the tests: run this QML script in the operator window (see tests/ui).", "file"),
                       QCommandLineOption(framesName, "For the tests: where the script's pictures go.", "dir")});
}

bool asked(const QCommandLineParser &parser)
{
    return parser.isSet(scriptName);
}

void prepare(QQmlApplicationEngine &engine, const QCommandLineParser &parser, QVariantMap &initial)
{
    engine.rootContext()->setContextProperty("testInput", input());
    if (!asked(parser))
        return;
    initial.insert("remember", qEnvironmentVariable("SP_TEST_REMEMBER") == "1");
    initial.insert("clocksHeld", false);
    initial.insert("outputScreen", qEnvironmentVariableIsEmpty("SP_TEST_SCREEN") ? -1 : qEnvironmentVariableIntValue("SP_TEST_SCREEN"));
    if (qEnvironmentVariable("SP_TEST_WINDOWS") == "0") {
        initial.insert("outputEnabled", false);
        initial.insert("stageEnabled", false);
    }
    if (!qEnvironmentVariableIsEmpty("SP_TEST_WIDTH")) {
        initial.insert("width", qEnvironmentVariableIntValue("SP_TEST_WIDTH"));
        initial.insert("height", qEnvironmentVariableIntValue("SP_TEST_HEIGHT"));
    }
}

void start(QQmlApplicationEngine &engine, QQuickWindow *operatorWindow, const QCommandLineParser &parser)
{
    if (!asked(parser))
        return;
    TestInput *testInput = input();
    testInput->window = operatorWindow;
    testInput->directory = parser.value(framesName);
    operatorWindow->requestActivate();
    const QString file = parser.value(scriptName);
    QTimer::singleShot(1500, operatorWindow, [operatorWindow, file, &engine] {
        QFocusEvent focusIn(QEvent::FocusIn);
        QCoreApplication::sendEvent(operatorWindow, &focusIn);
        auto *component = new QQmlComponent(&engine, QUrl::fromLocalFile(file));
        QObject *test = component->create(qmlContext(operatorWindow));
        if (!test) {
            QTextStream(stdout) << "SCRIPT DID NOT LOAD: " << component->errorString() << "\n";
            QCoreApplication::exit(2);
            return;
        }
        test->setParent(operatorWindow);
        QMetaObject::invokeMethod(test, "run");
    });
}

}

#include "testhook.moc"
