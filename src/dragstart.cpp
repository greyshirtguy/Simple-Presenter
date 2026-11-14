#include "dragstart.h"

#include <QElapsedTimer>

// (See dragstart.h for what this is and why.)

namespace dragstart {

bool begins(QPointF pressedAt, QPointF at, double heldFor)
{
    const QPointF by = at - pressedAt;
    return QPointF::dotProduct(by, by) >= kDistance * kDistance && heldFor >= kHold;
}

double now()
{
    // (Started the first time it is asked, which is before any press is measured by it)
    static const QElapsedTimer clock = [] {
        QElapsedTimer started;
        started.start();
        return started;
    }();
    return double(clock.nsecsElapsed()) / 1e6;
}

} // namespace dragstart
