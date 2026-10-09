#include "ndi.h"

#include "sessionlog.h"

#include <Processing.NDI.Lib.h>

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLibrary>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QWindow>

namespace {

// NDI's library, found once and kept for as long as the app runs.
struct Library
{
    const NDIlib_v6 *functions = nullptr;
    QString path;
    QString version;
    QString problem;
    QString folder;
};

const Library &library()
{
    static const Library found = [] {
        Library lib;
        lib.folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/ndi");
        // Where NDI's own installer says it has put it; the app's own folder for it;
        // and wherever the system keeps its libraries.
        QStringList places;
        const QString installed = qEnvironmentVariable(NDILIB_REDIST_FOLDER);
        if (!installed.isEmpty())
            places << installed + u'/' + QLatin1String(NDILIB_LIBRARY_NAME);
        places << lib.folder + u'/' + QLatin1String(NDILIB_LIBRARY_NAME) << QLatin1String(NDILIB_LIBRARY_NAME);
        for (const QString &place : std::as_const(places)) {
            if (place.contains(u'/') && !QFileInfo::exists(place))
                continue;
            QLibrary file(place);
            // (Left loaded: what it gives are addresses inside it.)
            auto load = reinterpret_cast<const NDIlib_v6 *(*)()>(file.resolve("NDIlib_v6_load"));
            if (!load)
                continue;
            const NDIlib_v6 *functions = load();
            if (!functions)
                continue;
            if (!functions->initialize()) {
                lib.problem = QStringLiteral("NDI's library was found (%1) but says it cannot run on this computer's processor.").arg(file.fileName());
                return lib;
            }
            lib.functions = functions;
            lib.path = file.fileName();
            lib.version = QString::fromUtf8(functions->version());
            SessionLog::write("ndi", QStringLiteral("NDI's library found: %1, at %2").arg(lib.version, lib.path));
            return lib;
        }
        lib.problem = QStringLiteral("NDI's library (%1) is not on this computer. It is NDI's own and free: get it from https://ndi.video "
                                     "(the NDI SDK, or NDI Tools), and either install it for the whole system or put the file in %2.")
                          .arg(QLatin1String(NDILIB_LIBRARY_NAME), lib.folder);
        SessionLog::write("ndi", QStringLiteral("NDI's library was not found, so no screen can be sent over NDI"));
        return lib;
    }();
    return found;
}

}

bool Ndi::available() const
{
    return library().functions != nullptr;
}

QString Ndi::version() const
{
    return library().version;
}

QString Ndi::path() const
{
    return library().path;
}

QString Ndi::problem() const
{
    return library().problem;
}

QString Ndi::folder() const
{
    return library().folder;
}

NdiScreen::NdiScreen(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &NdiScreen::tick);
    connect(this, &NdiScreen::rateChanged, this, [this] { m_due = 0; });
    m_clock.start();
}

NdiScreen::~NdiScreen()
{
    stop();
    // The scene goes first, while the window it is in is still there: parts of it (a
    // layer drawn by way of a picture of itself, as a transition does) hold on to their
    // window, and would be left holding one that is gone.
    delete m_scene.data();
    releaseDrawing();
}

void NdiScreen::setName(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    emit nameChanged();
    settle();
}

void NdiScreen::setWidth(int width)
{
    width = qBound(16, width, 7680);
    if (m_size.width() == width)
        return;
    m_size.setWidth(width);
    emit sizeChanged();
    settle();
}

void NdiScreen::setHeight(int height)
{
    height = qBound(16, height, 4320);
    if (m_size.height() == height)
        return;
    m_size.setHeight(height);
    emit sizeChanged();
    settle();
}

void NdiScreen::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
    settle();
}

void NdiScreen::setScene(QQuickItem *scene)
{
    if (m_scene == scene)
        return;
    m_scene = scene;
    // The scene lives in the window nobody sees from the start, sending or not, so
    // that it is told what to show like any other and has it ready.
    if (m_scene && prepareDrawing()) {
        m_scene->setParentItem(m_window->contentItem());
        m_scene->setPosition({0, 0});
        m_scene->setSize(m_size);
    }
    emit sceneChanged();
    settle();
}

void NdiScreen::fail(const QString &why)
{
    if (m_problem == why)
        return;
    m_problem = why;
    if (!why.isEmpty())
        SessionLog::write("PROBLEM", QStringLiteral("The screen \"%1\" is not being sent over NDI: %2").arg(m_name, why));
    emit statusChanged();
}

void NdiScreen::settle()
{
    const bool wanted = m_active && m_scene && !m_name.isEmpty();
    if (m_sender && (!wanted || m_sendingAs != m_name || m_sendingSize != m_size))
        stop();
    if (wanted && !m_sender)
        start();
    if (!wanted)
        fail(QString());
}

// The window nobody sees, and what draws it. Made once; what it is drawn into is made
// for the size in start().
bool NdiScreen::prepareDrawing()
{
    if (m_window)
        return true;
    if (QQuickWindow::sceneGraphBackend() == QLatin1String("software") || QGuiApplication::platformName() == QLatin1String("offscreen")) {
        fail(QStringLiteral("the app is drawing without the graphics card, and a screen cannot be drawn for NDI that way."));
        return false;
    }
    m_control = new QQuickRenderControl(this);
    m_window = new QQuickWindow(m_control);
    m_window->setColor(Qt::black);
    m_context = new QOpenGLContext(this);
    m_context->setFormat(QSurfaceFormat::defaultFormat());
    if (QOpenGLContext *shared = QOpenGLContext::globalShareContext())
        m_context->setShareContext(shared);
    if (!m_context->create()) {
        fail(QStringLiteral("the graphics card would not give the app somewhere to draw it (no OpenGL context)."));
        releaseDrawing();
        return false;
    }
    // What the graphics card is told the drawing is for is the window itself, made but
    // never shown. (A surface that is nowhere would be the usual thing, and what Qt
    // falls back on if the window has not been made; but a Wayland desktop's graphics
    // will not always draw for one, and will for a window, shown or not.)
    m_window->setSurfaceType(QSurface::OpenGLSurface);
    m_window->setFormat(m_context->format());
    m_window->setGeometry(0, 0, m_size.width(), m_size.height());
    m_window->create();
    m_surface = m_window;
    if (!m_context->makeCurrent(m_surface)) {
        fail(QStringLiteral("the graphics card would not let the app draw out of sight."));
        releaseDrawing();
        return false;
    }
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(m_context));
    connect(m_control, &QQuickRenderControl::renderRequested, this, [this] { m_changed = true; });
    connect(m_control, &QQuickRenderControl::sceneChanged, this, [this] { m_changed = true; });
    return true;
}

void NdiScreen::releaseDrawing()
{
    if (m_context && m_surface && m_context->isValid())
        m_context->makeCurrent(m_surface);
    delete m_target;
    m_target = nullptr;
    if (m_scene && m_window && m_scene->parentItem() == m_window->contentItem())
        m_scene->setParentItem(nullptr);
    // The window goes before what was made to draw it, as Qt asks.
    delete m_window;
    m_window = nullptr;
    delete m_control;
    m_control = nullptr;
    if (m_context)
        m_context->doneCurrent();
    m_surface = nullptr;
    delete m_context;
    m_context = nullptr;
    m_initialised = false;
}

bool NdiScreen::start()
{
    const Library &ndi = library();
    if (!ndi.functions) {
        fail(ndi.problem);
        return false;
    }
    if (!prepareDrawing())
        return false;
    if (QQuickWindow::graphicsApi() != QSGRendererInterface::OpenGL && QQuickWindow::graphicsApi() != QSGRendererInterface::Unknown) {
        fail(QStringLiteral("the app is drawing with something other than OpenGL, which is what a screen is drawn for NDI with."));
        return false;
    }
    if (!m_context->makeCurrent(m_surface)) {
        fail(QStringLiteral("the graphics card would not let the app draw it."));
        return false;
    }
    if (!m_initialised) {
        if (!m_control->initialize()) {
            fail(QStringLiteral("the app's drawing could not be started for it."));
            return false;
        }
        m_initialised = true;
    }
    delete m_target;
    m_target = new QOpenGLFramebufferObject(m_size, QOpenGLFramebufferObject::CombinedDepthStencil);
    if (!m_target->isValid()) {
        fail(QStringLiteral("the graphics card has no room for a picture of %1 x %2.").arg(m_size.width()).arg(m_size.height()));
        return false;
    }
    QQuickRenderTarget target = QQuickRenderTarget::fromOpenGLTexture(m_target->texture(), m_size);
    // Drawn upside down, as the graphics card counts rows, so that read back it is the
    // right way up.
    target.setMirrorVertically(true);
    m_window->setRenderTarget(target);
    m_window->setGeometry(0, 0, m_size.width(), m_size.height());
    m_window->contentItem()->setSize(m_size);
    if (m_scene) {
        m_scene->setParentItem(m_window->contentItem());
        m_scene->setSize(m_size);
    }
    for (auto &picture : m_pictures)
        picture.assign(size_t(m_size.width()) * size_t(m_size.height()) * 4, 0);
    m_hasPicture = false;
    m_changed = true;

    const QByteArray name = m_name.toUtf8();
    NDIlib_send_create_t settings;
    settings.p_ndi_name = name.constData();
    settings.p_groups = nullptr;
    // The app keeps time itself (see tick()).
    settings.clock_video = false;
    settings.clock_audio = false;
    m_sender = ndi.functions->send_create(&settings);
    if (!m_sender) {
        fail(QStringLiteral("NDI would not make a source called \"%1\" (is there one of that name already?).").arg(m_name));
        return false;
    }
    m_sendingAs = m_name;
    m_sendingSize = m_size;
    m_framesSent = 0;
    m_receivers = 0;
    m_due = 0;
    m_lastSent = 0;
    m_problem.clear();
    SessionLog::write("ndi", QStringLiteral("the screen \"%1\" is on the network as an NDI source, %2 x %3 at %4 frames a second")
                                 .arg(m_name).arg(m_size.width()).arg(m_size.height())
                                 .arg(double(m_rateNumerator) / qMax(1, m_rateDenominator), 0, 'g', 4));
    emit statusChanged();
    m_timer.start(0);
    return true;
}

void NdiScreen::stop()
{
    m_timer.stop();
    if (!m_sender)
        return;
    const Library &ndi = library();
    // (A picture NDI was handed to send in its own time is let go of first.)
    ndi.functions->send_send_video_async_v2(static_cast<NDIlib_send_instance_t>(m_sender), nullptr);
    ndi.functions->send_destroy(static_cast<NDIlib_send_instance_t>(m_sender));
    m_sender = nullptr;
    SessionLog::write("ndi", QStringLiteral("the screen \"%1\" is off the network, after %2 pictures").arg(m_sendingAs).arg(m_framesSent));
    m_receivers = 0;
    emit statusChanged();
}

// Draws the scene and reads the picture back into the one of the two that NDI is not
// holding. Says whether there is a picture.
bool NdiScreen::draw()
{
    if (!m_context->makeCurrent(m_surface))
        return false;
    m_control->polishItems();
    m_control->beginFrame();
    m_control->sync();
    m_control->render();
    m_control->endFrame();
    m_changed = false;

    std::vector<uchar> &picture = m_pictures[1 - m_picture];
    QOpenGLFunctions *gl = m_context->functions();
    gl->glBindFramebuffer(GL_FRAMEBUFFER, m_target->handle());
    gl->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    gl->glReadPixels(0, 0, m_size.width(), m_size.height(), GL_RGBA, GL_UNSIGNED_BYTE, picture.data());
    gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_picture = 1 - m_picture;
    m_hasPicture = true;
    return true;
}

void NdiScreen::tick()
{
    if (!m_sender)
        return;
    const Library &ndi = library();
    const qint64 now = m_clock.nsecsElapsed();
    const qint64 period = qint64(1'000'000'000) * qMax(1, m_rateDenominator) / qMax(1, m_rateNumerator);

    // Who is taking it is asked once a second, which is often enough to start in time.
    if (now - m_lastCounted >= 1'000'000'000 || m_lastCounted == 0) {
        m_lastCounted = now;
        const int receivers = ndi.functions->send_get_no_connections(static_cast<NDIlib_send_instance_t>(m_sender), 0);
        if (receivers != m_receivers) {
            m_receivers = receivers;
            SessionLog::write("ndi", QStringLiteral("the NDI source \"%1\" is being taken by %2").arg(m_sendingAs).arg(receivers));
        }
        emit statusChanged();
    }

    // With nothing taking it, a picture a second: enough to be found and looked at.
    const bool wanted = m_receivers > 0 || now - m_lastSent >= 1'000'000'000 || m_lastSent == 0;
    if (wanted) {
        if ((m_changed || !m_hasPicture) && !draw()) {
            fail(QStringLiteral("the graphics card stopped letting the app draw it."));
        } else if (m_hasPicture) {
            NDIlib_video_frame_v2_t frame;
            frame.xres = m_size.width();
            frame.yres = m_size.height();
            frame.FourCC = NDIlib_FourCC_video_type_RGBX;
            frame.frame_rate_N = m_rateNumerator;
            frame.frame_rate_D = qMax(1, m_rateDenominator);
            frame.picture_aspect_ratio = float(m_size.width()) / float(m_size.height());
            frame.frame_format_type = NDIlib_frame_format_type_progressive;
            frame.timecode = NDIlib_send_timecode_synthesize;
            frame.p_data = m_pictures[m_picture].data();
            frame.line_stride_in_bytes = m_size.width() * 4;
            frame.p_metadata = nullptr;
            frame.timestamp = 0;
            // Handed over to be compressed and sent in NDI's own time: it keeps the
            // picture until it is handed the next, which is why there are two.
            ndi.functions->send_send_video_async_v2(static_cast<NDIlib_send_instance_t>(m_sender), &frame);
            m_lastSent = now;
            ++m_framesSent;
        }
    }

    // The next is due a frame after this one was, not after it was done, so that the
    // rate holds; unless that has been missed by so much that it is better to start
    // counting again from now.
    if (m_due == 0 || now - m_due > 4 * period)
        m_due = now;
    m_due += period;
    const qint64 wait = qMax<qint64>(0, m_due - m_clock.nsecsElapsed());
    m_timer.start(int(wait / 1'000'000));
}
