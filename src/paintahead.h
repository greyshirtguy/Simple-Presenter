#pragma once

#include "strokedtext.h"

#include <QQuickItem>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// Paints the words of the slides an audience screen is likely to show next, before they
// are asked for: so that, asked for, a slide is ready at once, and is shown with the next
// frame of a video playing under it, and not after a frame more for its words to be painted
// (StrokedText's painting ahead, and TransitionLayer.paced, which waits for the slide to
// be ready and then for the video's next frame). Measured over a 1080p video on the laptop
// this is made on (2026-10-31), from the click to the slide on the screen: about 20 ms
// when the words were painted as the frame was drawn, holding the video up; 55 painted
// ahead when the slide was asked for; and 25 with this.
//
// It is handed the slides, as the screen will be handed them (dressed in the screen's
// theme, where its look gives it one), best first (show::State::likelyNext: the next, the
// one before, and the first slide of each group); it works out what each text of them
// will be drawn from, as SlideElement has its words drawn (StrokedText::lookOf), at the
// size it fills, which is the size of the slides it is beside (OutputScene); and has those
// painted on the painters' threads, after any a screen is waiting for, and kept. A slide
// shown whose words were painted ahead takes their pictures; one whose were not, because
// the guess was wrong or its words are drawn otherwise, is painted as it would have been.
//
// What it costs: a few milliseconds of a painter's thread for each slide not painted
// already, after each change of slide (the slides likely next after the next change are
// mostly those after this one), and the pictures kept: up to 4 MB for a slide's words on
// a 1080p screen, less as they are cut to what is not see-through of them (StrokedText),
// and no more than 24 MB of them asked for at a time, the best first.
class PaintAhead : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    // The slides, as maps, best first
    Q_PROPERTY(QVariantList slides READ slides WRITE setSlides NOTIFY slidesChanged)
    // Whether their shadows are drawn (as Slide's `effects`)
    Q_PROPERTY(bool effects MEMBER m_effects NOTIFY effectsChanged)

public:
    explicit PaintAhead(QQuickItem *parent = nullptr);

    QVariantList slides() const { return m_slides; }
    void setSlides(const QVariantList &slides);

    // Has the words of `slide`, which the screen is to show, painted before all else, from
    // its map: true if they are kept already; if not, `firstPainted` is said when they are.
    // So that the slide is given to the screen's slide layer with its words to hand, and is
    // shown in the first frame that has it: given before them, it was a frame of the
    // window's, out of sight, as it waited, and shown a frame later (the benchmark's slides
    // a frame later at worst, 2026-11-01). Asked again, for another slide, it is that one's
    // words that are waited for.
    Q_INVOKABLE bool paintFirst(const QVariantMap &slide);

signals:
    void slidesChanged();
    void effectsChanged();
    void firstPainted();

private:
    void paint();
    // What the words of `slides` are drawn from, the first till `budget` bytes of pictures
    QList<StrokedText::Look> looksOf(const QVariantList &slides, qsizetype budget) const;
    qreal ratio() const;
    void kept();

    QVariantList m_slides;
    bool m_effects = true;
    bool m_queued = false;
    // The words of the slide asked for first, till they are kept
    QList<StrokedText::Look> m_first;
};
