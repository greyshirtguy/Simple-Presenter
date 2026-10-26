#include "framesdrawn.h"

#include <QQuickWindow>

#include <chrono>

namespace {

// Nanoseconds on a steady clock, the same on every thread
qint64 nanoseconds()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}

FramesDrawn::FramesDrawn(QQuickItem *parent)
    : QQuickItem(parent)
{
    m_second.setInterval(1000);
    connect(&m_second, &QTimer::timeout, this, &FramesDrawn::read);
}

FramesDrawn::~FramesDrawn()
{
    disconnect(m_watching);
}

void FramesDrawn::setCounting(bool counting)
{
    if (counting == m_counting)
        return;
    m_counting = counting;
    watch();
    emit countingChanged();
}

void FramesDrawn::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemSceneChange)
        watch();
    QQuickItem::itemChange(change, value);
}

// Watches the window it is in while counting, and nothing otherwise
void FramesDrawn::watch()
{
    QQuickWindow *window = m_counting ? this->window() : nullptr;
    if (window == m_window && (window != nullptr) == m_second.isActive())
        return;
    disconnect(m_watching);
    m_window = window;
    m_second.stop();
    if (!window)
        return;
    m_count->frames.store(0, std::memory_order_relaxed);
    m_count->last.store(0, std::memory_order_relaxed);
    m_count->longest.store(0, std::memory_order_relaxed);
    // (On the thread that draws, and so with what it counts into held by the call itself)
    m_watching = connect(window, &QQuickWindow::afterFrameEnd, this, [count = m_count] {
        const qint64 now = nanoseconds();
        const qint64 last = count->last.exchange(now, std::memory_order_relaxed);
        if (last > 0 && now - last > count->longest.load(std::memory_order_relaxed))
            count->longest.store(now - last, std::memory_order_relaxed);
        count->frames.fetch_add(1, std::memory_order_relaxed);
    }, Qt::DirectConnection);
    m_second.start();
}

void FramesDrawn::read()
{
    m_perSecond = m_count->frames.exchange(0, std::memory_order_relaxed);
    m_longestGap = double(m_count->longest.exchange(0, std::memory_order_relaxed)) / 1e6;
    emit counted();
}
