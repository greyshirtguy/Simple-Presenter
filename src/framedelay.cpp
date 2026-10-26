#include "framedelay.h"

#include "delayline.h"

#include <QOpenGLContext>
#include <QMatrix4x4>
#include <QOpenGLExtraFunctions>
#include <QQuickWindow>
#include <QSGRenderNode>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <QTimer>

#include <chrono>
#include <cmath>

namespace {

// Milliseconds on a steady clock, the same on every thread
double milliseconds()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

// What the item draws: copies of its source, taken as they change and put on the screen
// as they come due (see framedelay.h). Everything of it is on the thread that draws.
class DelayNode : public QSGRenderNode
{
public:
    explicit DelayNode(QQuickWindow *window)
        : m_window(window)
    {
    }

    ~DelayNode() override { releaseResources(); }

    // From the item, as the window is brought up to date with it
    void set(QSGTextureProvider *provider, double delay, double frame, const QRectF &rect)
    {
        if (provider != m_provider) {
            QObject::disconnect(m_watching);
            m_provider = provider;
            // (Said as the layer is drawn, on this thread: counted, and the copy taken as
            // this frame is prepared)
            if (provider)
                m_watching = QObject::connect(provider, &QSGTextureProvider::textureChanged, &m_context, [this] { ++m_redrawn; }, Qt::DirectConnection);
            m_redrawn = 1;
        }
        m_delay = delay;
        m_frame = frame;
        m_rect = rect;
    }

    StateFlags changedStates() const override { return ScissorState; }
    RenderingFlags flags() const override { return BoundedRectRendering; }
    QRectF rect() const override { return m_rect; }

    // Qt has drawn its layers: if the source's was drawn again, a copy of it is held.
    void prepare() override
    {
        if (!m_provider || (m_redrawn == 0 && !m_held.isEmpty()))
            return;
        QSGTexture *texture = m_provider->texture();
        auto *native = texture ? texture->nativeInterface<QNativeInterface::QSGOpenGLTexture>() : nullptr;
        QOpenGLContext *context = QOpenGLContext::currentContext();
        if (!native || !context)
            return;
        m_redrawn = 0;
        m_window->beginExternalCommands();
        QOpenGLExtraFunctions *gl = context->extraFunctions();
        const QSize size = texture->textureSize();
        // (A new size, the screen's window made larger, say: the copies held are of the
        // old one and are let go of, and the delay starts again from what is drawn now)
        if (size != m_size) {
            release(gl);
            m_size = size;
        }
        // A picture to copy it into: one let go of, or a new one; or, if more are held
        // than a delay of this length can need (frames coming oftener than the screen's,
        // as said), the newest held, which this then takes the place of
        Picture picture;
        if (!m_spare.isEmpty()) {
            picture = m_spare.takeLast();
        } else if (m_held.size() <= delayline::most(m_delay, m_frame)) {
            picture = made(gl, size);
        } else {
            picture = m_held.takeLast();
            m_drawnAt.removeLast();
        }
        GLint read = 0;
        GLint draw = 0;
        gl->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        gl->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        if (!m_reading)
            gl->glGenFramebuffers(1, &m_reading);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, m_reading);
        gl->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GLuint(native->nativeTexture()), 0);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, picture.framebuffer);
        gl->glDisable(GL_SCISSOR_TEST);
        gl->glBlitFramebuffer(0, 0, size.width(), size.height(), 0, 0, size.width(), size.height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(read));
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(draw));
        m_window->endExternalCommands();
        m_held << picture;
        m_drawnAt << milliseconds();
    }

    // The screen is being drawn: the copy that is due goes on it, in the item's place,
    // over everything under the item.
    void render(const RenderState *state) override
    {
        if (m_held.isEmpty())
            return;
        QOpenGLContext *context = QOpenGLContext::currentContext();
        if (!context)
            return;
        const int due = delayline::shown(m_drawnAt, milliseconds(), m_delay, m_frame);
        // (What is older than it is let go of: it will not be shown again)
        for (int index = 0; index < due; ++index) {
            m_spare << m_held.takeFirst();
            m_drawnAt.removeFirst();
        }
        QOpenGLExtraFunctions *gl = context->extraFunctions();
        GLint read = 0;
        GLint viewport[4] = {0, 0, 0, 0};
        gl->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        gl->glGetIntegerv(GL_VIEWPORT, viewport);
        // Where the item is on what is being drawn: its corners through Qt's own matrices,
        // to the target's pixels. (A window that floats has its title bar over the screen's
        // scene, which is then not the whole of it.) That is the right way up whichever way
        // the target has its rows, the copy having its top row first as Qt's layers do: a
        // window has its bottom row first, as OpenGL's always are, and the targets of the
        // screens drawn out of sight their top row first, which is how NDI and the card
        // take their pictures (setMirrorVertically there), and which Qt's matrix says.
        const QMatrix4x4 toTarget = *projectionMatrix() * *matrix();
        const QPointF topLeft = toTarget.map(m_rect.topLeft());
        const QPointF bottomRight = toTarget.map(m_rect.bottomRight());
        const auto across = [&](qreal ndc) { return GLint(std::lround(viewport[0] + (ndc + 1) / 2 * viewport[2])); };
        const auto down = [&](qreal ndc) { return GLint(std::lround(viewport[1] + (ndc + 1) / 2 * viewport[3])); };
        // Inside what the item is clipped to, if anything is (a blit keeps to the scissor,
        // and to nothing else of Qt's clipping: there is nothing else over a screen's scene)
        if (state->scissorEnabled()) {
            const QRect scissor = state->scissorRect();
            gl->glEnable(GL_SCISSOR_TEST);
            gl->glScissor(scissor.x(), scissor.y(), scissor.width(), scissor.height());
        } else {
            gl->glDisable(GL_SCISSOR_TEST);
        }
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, m_held.first().framebuffer);
        gl->glBlitFramebuffer(0, 0, m_size.width(), m_size.height(), across(topLeft.x()), down(topLeft.y()), across(bottomRight.x()), down(bottomRight.y()),
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(read));
        // A newer copy still to come due: a frame asked for when it is, and not before
        // (nothing is drawn while what is shown stays as it is). Asked for at the moment
        // it is due, which is half a frame before the refresh it is for, so a timer a few
        // milliseconds late still has that refresh. By a timer on this thread: a window on
        // a display is drawn on a thread of its own, which runs it between frames, and the
        // frame asked for from there is drawn without the app's thread being stopped for
        // it; a screen drawn out of sight (NDI, a card) is drawn on the app's own thread,
        // by whoever has it, and is asked after its drawing, as it must be. (One asked for
        // already, for that moment or a sooner one, does.)
        const double next = delayline::nextDue(m_drawnAt, 0, m_delay, m_frame);
        const double now = milliseconds();
        if (m_asked >= 0 && now >= m_asked - 1)
            m_asked = -1;
        if (next >= 0 && (m_asked < 0 || next < m_asked)) {
            m_asked = next;
            QTimer::singleShot(qMax(0, int(std::lround(next - now))), Qt::PreciseTimer, &m_context, [window = QPointer<QQuickWindow>(m_window)] {
                if (window)
                    window->update();
            });
        }
    }

    // Called with the graphics context current, as the node goes
    void releaseResources() override
    {
        QOpenGLContext *context = QOpenGLContext::currentContext();
        if (context)
            release(context->extraFunctions());
        m_held.clear();
        m_drawnAt.clear();
        m_spare.clear();
    }

private:
    struct Picture
    {
        GLuint texture = 0;
        GLuint framebuffer = 0;
    };

    Picture made(QOpenGLExtraFunctions *gl, const QSize &size)
    {
        Picture picture;
        GLint bound = 0;
        gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        gl->glGenTextures(1, &picture.texture);
        gl->glBindTexture(GL_TEXTURE_2D, picture.texture);
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->glBindTexture(GL_TEXTURE_2D, GLuint(bound));
        GLint framebuffer = 0;
        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
        gl->glGenFramebuffers(1, &picture.framebuffer);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, picture.framebuffer);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, picture.texture, 0);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(framebuffer));
        return picture;
    }

    void release(QOpenGLExtraFunctions *gl)
    {
        for (const QList<Picture> *pictures : {&m_held, &m_spare}) {
            for (const Picture &picture : *pictures) {
                gl->glDeleteFramebuffers(1, &picture.framebuffer);
                gl->glDeleteTextures(1, &picture.texture);
            }
        }
        m_held.clear();
        m_drawnAt.clear();
        m_spare.clear();
        if (m_reading)
            gl->glDeleteFramebuffers(1, &m_reading);
        m_reading = 0;
    }

    QQuickWindow *m_window;
    // (The context the watching of the layer is in, so that it goes with the node)
    QObject m_context;
    QPointer<QSGTextureProvider> m_provider;
    QMetaObject::Connection m_watching;
    int m_redrawn = 0;
    double m_delay = 0;
    double m_frame = 1000.0 / 60;
    QRectF m_rect;
    QSize m_size;
    // When the frame last asked for was for, on the clock above; -1 if none is waited for
    double m_asked = -1;
    // The copies held, oldest first, with when each was drawn; and those let go of
    QList<Picture> m_held;
    QList<double> m_drawnAt;
    QList<Picture> m_spare;
    GLuint m_reading = 0;
};

} // namespace

FrameDelay::FrameDelay(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

bool FrameDelay::possible()
{
    return QQuickWindow::graphicsApi() == QSGRendererInterface::OpenGL;
}

void FrameDelay::setSource(QQuickItem *source)
{
    if (source == m_source)
        return;
    m_source = source;
    update();
    emit sourceChanged();
}

void FrameDelay::setFrames(int frames)
{
    frames = qBound(0, frames, delayline::kMostFrames);
    if (frames == m_frames)
        return;
    m_frames = frames;
    updateActive();
    update();
    emit framesChanged();
}

void FrameDelay::setFrameInterval(double milliseconds)
{
    // (Not a number, or none: a sixtieth of a second, the most screens' refresh)
    if (!(milliseconds > 0))
        milliseconds = 1000.0 / 60;
    if (milliseconds == m_frameInterval)
        return;
    m_frameInterval = milliseconds;
    updateActive();
    update();
    emit frameIntervalChanged();
}

void FrameDelay::updateActive()
{
    const bool active = m_frames > 0 && m_frameInterval > 0 && possible();
    if (active == m_active)
        return;
    m_active = active;
    emit activeChanged();
}

QSGNode *FrameDelay::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<DelayNode *>(old);
    // (The source is a texture provider once its layer is made, which follows `active`)
    if (!m_active || !m_source || !m_source->isTextureProvider()) {
        delete node;
        // Asked again, for when the layer has been made
        if (m_active && m_source)
            update();
        return nullptr;
    }
    if (!node)
        node = new DelayNode(window());
    node->set(m_source->textureProvider(), m_frames * m_frameInterval, m_frameInterval, boundingRect());
    return node;
}
