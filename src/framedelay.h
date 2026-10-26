#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

// A screen's video delay (delayline.h has why and the rules): everything the screen shows,
// shown so many of its frames late. ProPresenter's "video frame delay", set for each
// output; here for each audience screen (OutputScene has one of these over its layers).
//
// How. What the screen shows is drawn into a layer of its own (Qt's `layer.enabled`, on
// the item given as `source`), which is moved out of the window, so that nothing of it is
// seen there and Qt still draws it. Qt draws a layer again only when something in it has
// changed, and says so (its texture's textureChanged), so that is when a copy of it is
// taken: on the graphics chip, into a texture of this item's own, with when it was drawn.
// At each frame the screen draws, the copy that is due (delayline::shown) is copied onto
// the screen, in this item's place (which is the source's), over everything under it.
// While a newer copy waits to be due, one frame is asked for at the moment it is
// (delayline::nextDue), and none before, so that nothing is drawn while what is shown
// stays as it is. In that frame nothing of the screen is drawn again, Qt's layer being as
// it was, and the cost is the one copy onto the screen. What is older than the copy shown
// is let go of, and its texture kept for the next.
//
// It is all done in a render node of the item's (QSGRenderNode), on the thread that draws,
// so nothing of it crosses from one thread to another: the copy is taken as the frame is
// prepared, after Qt has drawn its layers and before it draws the screen, and the one due
// is put on the screen as the frame is drawn. A screen drawn out of sight, for NDI or a
// DeckLink card, draws the same scene in the same way (through Qt's render control, on the
// app's own thread), and has its delay by the same means; a frame it is asked for is
// drawn at its next beat.
//
// What it costs, with a delay set (nothing at all without: the layer is not made, and the
// screen is drawn as ever): Qt drawing the screen into its layer, which is what it drew
// on the screen before; a copy of the layer each time it changes; a copy onto the screen
// at each frame drawn; and a frame more for each change, when its copy comes due (while a
// video plays, one at each of the video's frames, which with a delay of an odd number of
// frames falls between the video's own: a thirty-frame video on a sixty-frame screen then
// has it drawn at every refresh). On the graphics chip, a picture of the screen for each
// frame of the delay and three more at most (delayline::most, and one being drawn into:
// 8 MB a picture for 1080p).
//
// It needs OpenGL, which is what the app draws with on Linux: the copies are OpenGL's
// (glBlitFramebuffer), on the textures Qt's own interface for OpenGL gives the numbers of.
// With anything else (the software drawing of the self-test) there is no delay, and
// `active` says so.
class FrameDelay : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    // What is shown late: an item drawn into a layer while `active` (and moved out of the
    // window, by whoever has both)
    Q_PROPERTY(QQuickItem *source READ source WRITE setSource NOTIFY sourceChanged)
    // How many of the screen's frames late, and how long a frame of it is, in milliseconds
    Q_PROPERTY(int frames READ frames WRITE setFrames NOTIFY framesChanged)
    Q_PROPERTY(double frameInterval READ frameInterval WRITE setFrameInterval NOTIFY frameIntervalChanged)
    // Whether there is a delay: one is set, and the app draws with OpenGL
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit FrameDelay(QQuickItem *parent = nullptr);

    QQuickItem *source() const { return m_source; }
    void setSource(QQuickItem *source);
    int frames() const { return m_frames; }
    void setFrames(int frames);
    double frameInterval() const { return m_frameInterval; }
    void setFrameInterval(double milliseconds);
    bool active() const { return m_active; }

    // Whether a delay can be made at all: the app drawing with OpenGL
    static bool possible();

signals:
    void sourceChanged();
    void framesChanged();
    void frameIntervalChanged();
    void activeChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    void updateActive();

    QPointer<QQuickItem> m_source;
    int m_frames = 0;
    double m_frameInterval = 1000.0 / 60;
    bool m_active = false;
};
