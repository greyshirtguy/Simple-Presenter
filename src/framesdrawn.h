#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

class QQuickWindow;

// How many frames the window this is in has drawn in the last second, and the longest
// time between two of them: for a screen's performance statistics (OutputScene), as
// ProPresenter's Show Performance Statistics On Screen has its frame rates.
//
// Counted as each frame is done (the window's afterFrameEnd, which a window on a display
// says on its drawing thread, and a screen drawn out of sight for NDI or a card on the
// app's own), into a count and a time kept where that thread can reach them without a
// lock; read and begun again once a second here. A frame drawn is one the window made:
// the app draws only when something has changed, so a screen standing still draws once a
// second, for the statistics' own change, and one playing a video at each of its frames.
// Nothing at all is done while it is not `counting`.
class FramesDrawn : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool counting READ counting WRITE setCounting NOTIFY countingChanged)
    // In the last whole second: how many frames, and the longest between two, in
    // milliseconds (0 before a second has been counted)
    Q_PROPERTY(int perSecond READ perSecond NOTIFY counted)
    Q_PROPERTY(double longestGap READ longestGap NOTIFY counted)

public:
    explicit FramesDrawn(QQuickItem *parent = nullptr);
    ~FramesDrawn() override;

    bool counting() const { return m_counting; }
    void setCounting(bool counting);
    int perSecond() const { return m_perSecond; }
    double longestGap() const { return m_longestGap; }

signals:
    void countingChanged();
    void counted();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    // What the drawing thread counts into: kept by whichever of it and this lasts longer
    // (a frame can be being counted as this goes)
    struct Count
    {
        std::atomic<int> frames {0};
        std::atomic<qint64> last {0};
        std::atomic<qint64> longest {0};
    };

    void watch();
    void read();

    bool m_counting = false;
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_watching;
    std::shared_ptr<Count> m_count = std::make_shared<Count>();
    QTimer m_second;
    int m_perSecond = 0;
    double m_longestGap = 0;
};
